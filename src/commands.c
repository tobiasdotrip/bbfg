#include "commands.h"

#include "error.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
has_prefix(const char* value, const char* prefix)
{
  return strncmp(value, prefix, strlen(prefix)) == 0;
}

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
print_ref_name(const char* name, void* payload)
{
  (void)payload;
  printf("%s\n", name);
  return 0;
}

static int
print_rewrite_ref_name(const char* name, void* payload)
{
  (void)payload;

  if (has_prefix(name, "refs/heads/") || has_prefix(name, "refs/tags/")) {
    printf("%s\n", name);
  }

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
load_commit_parents(const git_commit*** parents,
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

static int
create_commit_without_tree_entry(git_oid* rewritten_commit_id,
                                 git_repository* repo,
                                 const char* repo_path,
                                 const char* path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  git_tree* head_tree = NULL;
  if (git_commit_tree(&head_tree, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD tree", repo_path);
    git_commit_free(head_commit);
    return -1;
  }

  git_oid rewritten_tree_id;
  if (bbfg_tree_remove_path(&rewritten_tree_id, repo, head_tree, path) < 0) {
    bbfg_print_git_error("could not remove tree path", path);
    git_tree_free(head_tree);
    git_commit_free(head_commit);
    return -1;
  }

  git_tree* rewritten_tree = NULL;
  if (git_tree_lookup(&rewritten_tree, repo, &rewritten_tree_id) < 0) {
    bbfg_print_git_error("could not read rewritten tree", repo_path);
    git_tree_free(head_tree);
    git_commit_free(head_commit);
    return -1;
  }

  const git_commit** parents = NULL;
  size_t parent_count = 0;
  if (load_commit_parents(&parents, &parent_count, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD parents", repo_path);
    git_tree_free(rewritten_tree);
    git_tree_free(head_tree);
    git_commit_free(head_commit);
    return -1;
  }

  if (git_commit_create(rewritten_commit_id,
                        repo,
                        NULL,
                        git_commit_author(head_commit),
                        git_commit_committer(head_commit),
                        git_commit_message_encoding(head_commit),
                        git_commit_message_raw(head_commit),
                        rewritten_tree,
                        parent_count,
                        parents) < 0) {
    bbfg_print_git_error("could not create rewritten commit", repo_path);
    free_commit_array(parents, parent_count);
    git_tree_free(rewritten_tree);
    git_tree_free(head_tree);
    git_commit_free(head_commit);
    return -1;
  }

  free_commit_array(parents, parent_count);
  git_tree_free(rewritten_tree);
  git_tree_free(head_tree);
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_print_head_commit_id(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(git_commit_id(head_commit)));
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_print_head_tree_id(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(git_commit_tree_id(head_commit)));
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_print_head_tree_entries(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  git_tree* tree = NULL;
  if (git_commit_tree(&tree, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD tree", repo_path);
    git_commit_free(head_commit);
    return -1;
  }

  size_t entry_count = git_tree_entrycount(tree);
  for (size_t i = 0; i < entry_count; i++) {
    const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
    printf("%06o %s %s\t%s\n",
           git_tree_entry_filemode_raw(entry),
           git_object_type2string(git_tree_entry_type(entry)),
           git_oid_tostr_s(git_tree_entry_id(entry)),
           git_tree_entry_name(entry));
  }

  git_tree_free(tree);
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_print_refs(git_repository* repo, const char* repo_path)
{
  if (git_reference_foreach_name(repo, print_ref_name, NULL) < 0) {
    bbfg_print_git_error("could not list refs", repo_path);
    return -1;
  }

  return 0;
}

int
bbfg_print_rewrite_refs(git_repository* repo, const char* repo_path)
{
  if (git_reference_foreach_name(repo, print_rewrite_ref_name, NULL) < 0) {
    bbfg_print_git_error("could not list rewrite refs", repo_path);
    return -1;
  }

  return 0;
}

int
bbfg_print_rewrite_commits(git_repository* repo, const char* repo_path)
{
  git_revwalk* walk = NULL;
  if (git_revwalk_new(&walk, repo) < 0) {
    bbfg_print_git_error("could not create revwalk", repo_path);
    return -1;
  }

  git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

  if (git_revwalk_push_glob(walk, "refs/heads/*") < 0 ||
      git_revwalk_push_glob(walk, "refs/tags/*") < 0) {
    bbfg_print_git_error("could not push rewrite refs", repo_path);
    git_revwalk_free(walk);
    return -1;
  }

  git_oid oid;
  int result = git_revwalk_next(&oid, walk);
  while (result == 0) {
    printf("%s\n", git_oid_tostr_s(&oid));
    result = git_revwalk_next(&oid, walk);
  }

  git_revwalk_free(walk);

  if (result != GIT_ITEROVER) {
    bbfg_print_git_error("could not walk rewrite commits", repo_path);
    return -1;
  }

  return 0;
}

int
bbfg_rebuild_head_tree(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  git_tree* tree = NULL;
  if (git_commit_tree(&tree, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD tree", repo_path);
    git_commit_free(head_commit);
    return -1;
  }

  git_treebuilder* builder = NULL;
  if (git_treebuilder_new(&builder, repo, tree) < 0) {
    bbfg_print_git_error("could not create tree builder", repo_path);
    git_tree_free(tree);
    git_commit_free(head_commit);
    return -1;
  }

  git_oid rebuilt_tree_id;
  if (git_treebuilder_write(&rebuilt_tree_id, builder) < 0) {
    bbfg_print_git_error("could not write rebuilt tree", repo_path);
    git_treebuilder_free(builder);
    git_tree_free(tree);
    git_commit_free(head_commit);
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rebuilt_tree_id));
  git_treebuilder_free(builder);
  git_tree_free(tree);
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_remove_head_tree_entry(git_repository* repo,
                            const char* repo_path,
                            const char* path)
{
  git_commit* head_commit = NULL;
  if (lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  git_tree* tree = NULL;
  if (git_commit_tree(&tree, head_commit) < 0) {
    bbfg_print_git_error("could not read HEAD tree", repo_path);
    git_commit_free(head_commit);
    return -1;
  }

  git_oid rebuilt_tree_id;
  if (bbfg_tree_remove_path(&rebuilt_tree_id, repo, tree, path) < 0) {
    bbfg_print_git_error("could not remove tree path", path);
    git_tree_free(tree);
    git_commit_free(head_commit);
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rebuilt_tree_id));
  git_tree_free(tree);
  git_commit_free(head_commit);
  return 0;
}

int
bbfg_commit_without_tree_entry(git_repository* repo,
                               const char* repo_path,
                               const char* path)
{
  git_oid rewritten_commit_id;
  if (create_commit_without_tree_entry(
        &rewritten_commit_id, repo, repo_path, path) < 0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
  return 0;
}

int
bbfg_write_rewrite_ref(git_repository* repo,
                       const char* repo_path,
                       const char* path)
{
  git_oid rewritten_commit_id;
  if (create_commit_without_tree_entry(
        &rewritten_commit_id, repo, repo_path, path) < 0) {
    return -1;
  }

  git_reference* rewrite_ref = NULL;
  if (git_reference_create(&rewrite_ref,
                           repo,
                           "refs/heads/bbfg-rewrite",
                           &rewritten_commit_id,
                           1,
                           "bbfg rewrite") < 0) {
    bbfg_print_git_error("could not write rewrite ref", repo_path);
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
  git_reference_free(rewrite_ref);
  return 0;
}
