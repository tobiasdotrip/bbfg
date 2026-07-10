#include "rewrite.h"

#include "error.h"
#include "tree.h"

#include <stdlib.h>

typedef struct
{
  git_oid old_id;
  git_oid new_id;
} CommitRewrite;

typedef struct
{
  CommitRewrite* items;
  size_t count;
  size_t capacity;
} CommitRewriteMap;

typedef enum
{
  BBFG_MISSING_PATH_IS_ERROR,
  BBFG_MISSING_PATH_IS_UNCHANGED
} MissingPathMode;

static int
lookup_head_commit(git_commit** commit,
                   git_repository* repo,
                   const char* repo_path)
{
  git_reference* head = NULL;
  if (git_repository_head(&head, repo) < 0) {
    bbfg_print_git_error("could not resolve HEAD", repo_path);
    return -1;
  }

  git_object* head_object = NULL;
  if (git_reference_peel(&head_object, head, GIT_OBJECT_COMMIT) < 0) {
    bbfg_print_git_error("HEAD does not point to a commit", repo_path);
    git_reference_free(head);
    return -1;
  }

  *commit = (git_commit*)head_object;
  git_reference_free(head);
  return 0;
}

static int
lookup_ref_commit(git_commit** commit,
                  git_repository* repo,
                  const char* ref_name)
{
  git_reference* ref = NULL;
  if (git_reference_lookup(&ref, repo, ref_name) < 0) {
    bbfg_print_git_error("could not resolve ref", ref_name);
    return -1;
  }

  git_object* object = NULL;
  if (git_reference_peel(&object, ref, GIT_OBJECT_COMMIT) < 0) {
    bbfg_print_git_error("ref does not point to a commit", ref_name);
    git_reference_free(ref);
    return -1;
  }

  *commit = (git_commit*)object;
  git_reference_free(ref);
  return 0;
}

static void
free_commit_array(const git_commit** commits, size_t count)
{
  size_t i;

  for (i = 0; i < count; i++) {
    git_commit_free((git_commit*)commits[i]);
  }

  free((void*)commits);
}

static int
load_original_parents(const git_commit*** parents,
                      size_t* parent_count,
                      git_commit* commit)
{
  unsigned int count = git_commit_parentcount(commit);
  if (count == 0) {
    *parents = NULL;
    *parent_count = 0;
    return 0;
  }

  const git_commit** loaded_parents =
    (const git_commit**)calloc(count, sizeof(*loaded_parents));
  if (loaded_parents == NULL) {
    return -1;
  }

  unsigned int i;
  for (i = 0; i < count; i++) {
    git_commit* parent = NULL;
    if (git_commit_parent(&parent, commit, i) < 0) {
      free_commit_array(loaded_parents, i);
      return -1;
    }

    loaded_parents[i] = parent;
  }

  *parents = loaded_parents;
  *parent_count = count;
  return 0;
}

static void
free_rewrite_map(CommitRewriteMap* map)
{
  free(map->items);
  map->items = NULL;
  map->count = 0;
  map->capacity = 0;
}

static int
append_rewrite(CommitRewriteMap* map,
               const git_oid* old_id,
               const git_oid* new_id)
{
  if (map->count == map->capacity) {
    size_t next_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
    CommitRewrite* next_items =
      (CommitRewrite*)realloc(map->items, next_capacity * sizeof(*next_items));
    if (next_items == NULL) {
      return -1;
    }

    map->items = next_items;
    map->capacity = next_capacity;
  }

  git_oid_cpy(&map->items[map->count].old_id, old_id);
  git_oid_cpy(&map->items[map->count].new_id, new_id);
  map->count++;
  return 0;
}

static const git_oid*
find_rewrite(const CommitRewriteMap* map, const git_oid* old_id)
{
  size_t i;

  for (i = 0; i < map->count; i++) {
    if (git_oid_equal(&map->items[i].old_id, old_id)) {
      return &map->items[i].new_id;
    }
  }

  return NULL;
}

static int
load_rewritten_parents(const git_commit*** parents,
                       size_t* parent_count,
                       git_repository* repo,
                       const CommitRewriteMap* map,
                       git_commit* commit)
{
  unsigned int count = git_commit_parentcount(commit);
  if (count == 0) {
    *parents = NULL;
    *parent_count = 0;
    return 0;
  }

  const git_commit** loaded_parents =
    (const git_commit**)calloc(count, sizeof(*loaded_parents));
  if (loaded_parents == NULL) {
    return -1;
  }

  unsigned int i;
  for (i = 0; i < count; i++) {
    const git_oid* old_parent_id = git_commit_parent_id(commit, i);
    const git_oid* new_parent_id = find_rewrite(map, old_parent_id);
    git_commit* parent = NULL;
    if (new_parent_id == NULL ||
        git_commit_lookup(&parent, repo, new_parent_id) < 0) {
      free_commit_array(loaded_parents, i);
      return -1;
    }

    loaded_parents[i] = parent;
  }

  *parents = loaded_parents;
  *parent_count = count;
  return 0;
}

static int
tree_without_path(git_oid* rewritten_tree_id,
                  git_repository* repo,
                  git_commit* commit,
                  const char* repo_path,
                  const char* path,
                  MissingPathMode missing_path_mode)
{
  git_tree* tree = NULL;
  if (git_commit_tree(&tree, commit) < 0) {
    bbfg_print_git_error("could not read commit tree", repo_path);
    return -1;
  }

  git_tree_entry* entry = NULL;
  int result = git_tree_entry_bypath(&entry, tree, path);
  if (result == GIT_ENOTFOUND &&
      missing_path_mode == BBFG_MISSING_PATH_IS_UNCHANGED) {
    git_oid_cpy(rewritten_tree_id, git_commit_tree_id(commit));
    git_tree_free(tree);
    return 0;
  }

  if (result < 0) {
    bbfg_print_git_error("could not find tree path", path);
    git_tree_free(tree);
    return -1;
  }

  git_tree_entry_free(entry);

  if (bbfg_tree_remove_path(rewritten_tree_id, repo, tree, path) < 0) {
    bbfg_print_git_error("could not remove tree path", path);
    git_tree_free(tree);
    return -1;
  }

  git_tree_free(tree);
  return 0;
}

static int
create_rewritten_commit(git_oid* rewritten_commit_id,
                        git_repository* repo,
                        const char* repo_path,
                        git_commit* commit,
                        const char* path,
                        MissingPathMode missing_path_mode,
                        const git_commit** parents,
                        size_t parent_count)
{
  git_oid rewritten_tree_id;
  if (tree_without_path(
        &rewritten_tree_id, repo, commit, repo_path, path, missing_path_mode) <
      0) {
    return -1;
  }

  git_tree* rewritten_tree = NULL;
  if (git_tree_lookup(&rewritten_tree, repo, &rewritten_tree_id) < 0) {
    bbfg_print_git_error("could not read rewritten tree", repo_path);
    return -1;
  }

  if (git_commit_create(rewritten_commit_id,
                        repo,
                        NULL,
                        git_commit_author(commit),
                        git_commit_committer(commit),
                        git_commit_message_encoding(commit),
                        git_commit_message_raw(commit),
                        rewritten_tree,
                        parent_count,
                        parents) < 0) {
    bbfg_print_git_error("could not create rewritten commit", repo_path);
    git_tree_free(rewritten_tree);
    return -1;
  }

  git_tree_free(rewritten_tree);
  return 0;
}

int
bbfg_rewrite_head_commit(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const char* path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  const git_commit** parents = NULL;
  size_t parent_count = 0;
  if (load_original_parents(&parents, &parent_count, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD parents", repo_path);
    git_commit_free(head_commit);
    return -1;
  }

  int result = create_rewritten_commit(rewritten_commit_id,
                                       repo,
                                       repo_path,
                                       head_commit,
                                       path,
                                       BBFG_MISSING_PATH_IS_ERROR,
                                       parents,
                                       parent_count);

  free_commit_array(parents, parent_count);
  git_commit_free(head_commit);
  return result;
}

static int
rewrite_history_from_commit(git_oid* rewritten_commit_id,
                            git_repository* repo,
                            const char* repo_path,
                            git_commit* start_commit,
                            const char* path)
{
  git_revwalk* walk = NULL;
  if (git_revwalk_new(&walk, repo) < 0) {
    bbfg_print_git_error("could not create revwalk", repo_path);
    return -1;
  }

  git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);
  if (git_revwalk_push(walk, git_commit_id(start_commit)) < 0) {
    bbfg_print_git_error("could not push rewrite tip", repo_path);
    git_revwalk_free(walk);
    return -1;
  }

  CommitRewriteMap map = { NULL, 0, 0 };
  git_oid old_id;
  int result = git_revwalk_next(&old_id, walk);
  while (result == 0) {
    git_commit* commit = NULL;
    if (git_commit_lookup(&commit, repo, &old_id) < 0) {
      bbfg_print_git_error("could not read commit", repo_path);
      result = -1;
      break;
    }

    const git_commit** parents = NULL;
    size_t parent_count = 0;
    if (load_rewritten_parents(&parents, &parent_count, repo, &map, commit) <
        0) {
      bbfg_print_git_error("could not read rewritten parents", repo_path);
      git_commit_free(commit);
      result = -1;
      break;
    }

    git_oid new_id;
    if (create_rewritten_commit(&new_id,
                                repo,
                                repo_path,
                                commit,
                                path,
                                BBFG_MISSING_PATH_IS_UNCHANGED,
                                parents,
                                parent_count) < 0 ||
        append_rewrite(&map, &old_id, &new_id) < 0) {
      free_commit_array(parents, parent_count);
      git_commit_free(commit);
      result = -1;
      break;
    }

    free_commit_array(parents, parent_count);
    git_oid_cpy(rewritten_commit_id, &new_id);
    git_commit_free(commit);
    result = git_revwalk_next(&old_id, walk);
  }

  if (result == GIT_ITEROVER) {
    result = 0;
  }

  if (result < 0) {
    bbfg_print_git_error("could not rewrite HEAD history", repo_path);
  }

  free_rewrite_map(&map);
  git_revwalk_free(walk);
  return result;
}

int
bbfg_rewrite_head_history(git_oid* rewritten_commit_id,
                          git_repository* repo,
                          const char* repo_path,
                          const char* path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  int result = rewrite_history_from_commit(
    rewritten_commit_id, repo, repo_path, head_commit, path);
  git_commit_free(head_commit);
  return result;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int
bbfg_rewrite_ref_history(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const char* ref_name,
                         const char* path)
{
  git_commit* commit = NULL;
  if (lookup_ref_commit(&commit, repo, ref_name) < 0) {
    return -1;
  }

  int result = rewrite_history_from_commit(
    rewritten_commit_id, repo, repo_path, commit, path);
  git_commit_free(commit);
  return result;
}
// NOLINTEND(bugprone-easily-swappable-parameters)
