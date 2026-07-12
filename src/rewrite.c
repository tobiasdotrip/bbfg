#include "rewrite.h"

#include "error.h"
#include "filter.h"
#include "mempack.h"
#include "oid_map.h"
#include "repository.h"

#include <stdlib.h>

typedef struct
{
  BbfgOidMap commits;
  BbfgOidMap tags;
  BbfgOidMap trees;
} RewriteState;

typedef enum
{
  BBFG_MISSING_PATH_IS_ERROR,
  BBFG_MISSING_PATH_IS_UNCHANGED
} MissingPathMode;

static int
rewrite_history_from_walk(RewriteState* state,
                          git_repository* repo,
                          const char* repo_path,
                          git_revwalk* walk,
                          const BbfgFilter* filter,
                          const git_oid* protected_commit_ids,
                          size_t protected_commit_count);

static int
rewrite_tag(git_oid* rewritten_tag_id,
            git_repository* repo,
            const char* repo_path,
            const git_oid* original_tag_id,
            const BbfgOidMap* commits,
            BbfgOidMap* tags);

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
oid_is_protected(const git_oid* oid,
                 const git_oid* protected_commit_ids,
                 size_t protected_commit_count)
{
  size_t i;

  for (i = 0; i < protected_commit_count; i++) {
    if (git_oid_equal(oid, &protected_commit_ids[i])) {
      return 1;
    }
  }

  return 0;
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

static int
load_rewritten_parents(const git_commit*** parents,
                       size_t* parent_count,
                       git_repository* repo,
                       const BbfgOidMap* commits,
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
    const git_oid* new_parent_id = bbfg_oid_map_get(commits, old_parent_id);
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
copy_cached_tree(git_oid* rewritten_tree_id,
                 const git_oid* original_tree_id,
                 const BbfgOidMap* trees)
{
  if (trees == NULL) {
    return 0;
  }

  const git_oid* cached_tree_id = bbfg_oid_map_get(trees, original_tree_id);
  if (cached_tree_id == NULL) {
    return 0;
  }

  git_oid_cpy(rewritten_tree_id, cached_tree_id);
  return 1;
}

static int
cache_rewritten_tree(BbfgOidMap* trees,
                     const git_oid* original_tree_id,
                     const git_oid* rewritten_tree_id)
{
  return trees == NULL
           ? 0
           : bbfg_oid_map_put(trees, original_tree_id, rewritten_tree_id);
}

static int
keep_original_tree(git_oid* rewritten_tree_id,
                   const git_oid* original_tree_id,
                   git_tree* tree,
                   BbfgOidMap* trees)
{
  git_oid_cpy(rewritten_tree_id, original_tree_id);
  git_tree_free(tree);
  return cache_rewritten_tree(trees, original_tree_id, rewritten_tree_id);
}

static int
filter_commit_tree(git_oid* rewritten_tree_id,
                   git_repository* repo,
                   git_commit* commit,
                   const char* repo_path,
                   const BbfgFilter* filter,
                   MissingPathMode missing_path_mode,
                   BbfgOidMap* trees,
                   const git_oid* protected_commit_ids,
                   size_t protected_commit_count)
{
  const git_oid* original_tree_id = git_commit_tree_id(commit);
  int protect_tree =
    filter->protect_blobs && oid_is_protected(git_commit_id(commit),
                                              protected_commit_ids,
                                              protected_commit_count);
  if (!protect_tree &&
      copy_cached_tree(rewritten_tree_id, original_tree_id, trees)) {
    return 0;
  }

  git_tree* tree = NULL;
  if (git_commit_tree(&tree, commit) < 0) {
    bbfg_print_git_error("could not read commit tree", repo_path);
    return -1;
  }

  if (protect_tree) {
    git_oid_cpy(rewritten_tree_id, original_tree_id);
    git_tree_free(tree);
    return 0;
  }

  int matches = 0;
  int result = bbfg_filter_matches_tree(&matches, repo, filter, tree);
  if (result < 0) {
    bbfg_print_git_error("could not match tree filter", repo_path);
    git_tree_free(tree);
    return -1;
  }

  if (!matches && missing_path_mode == BBFG_MISSING_PATH_IS_UNCHANGED) {
    return keep_original_tree(rewritten_tree_id, original_tree_id, tree, trees);
  }

  if (bbfg_filter_apply_tree(rewritten_tree_id, repo, tree, filter) < 0) {
    bbfg_print_git_error("could not apply tree filter", repo_path);
    git_tree_free(tree);
    return -1;
  }

  git_tree_free(tree);
  return cache_rewritten_tree(trees, original_tree_id, rewritten_tree_id);
}

static int
commit_arcs_are_unchanged(git_commit* commit,
                          const git_oid* tree_id,
                          const git_commit** parents,
                          size_t parent_count)
{
  if (!git_oid_equal(tree_id, git_commit_tree_id(commit)) ||
      parent_count != git_commit_parentcount(commit)) {
    return 0;
  }

  size_t i;
  for (i = 0; i < parent_count; i++) {
    if (!git_oid_equal(git_commit_id(parents[i]),
                       git_commit_parent_id(commit, (unsigned int)i))) {
      return 0;
    }
  }

  return 1;
}

static int
create_rewritten_commit(git_oid* rewritten_commit_id,
                        git_repository* repo,
                        const char* repo_path,
                        git_commit* commit,
                        const BbfgFilter* filter,
                        MissingPathMode missing_path_mode,
                        const git_commit** parents,
                        size_t parent_count,
                        BbfgOidMap* trees,
                        const git_oid* protected_commit_ids,
                        size_t protected_commit_count)
{
  git_oid rewritten_tree_id;
  if (filter_commit_tree(&rewritten_tree_id,
                         repo,
                         commit,
                         repo_path,
                         filter,
                         missing_path_mode,
                         trees,
                         protected_commit_ids,
                         protected_commit_count) < 0) {
    return -1;
  }

  if (commit_arcs_are_unchanged(
        commit, &rewritten_tree_id, parents, parent_count)) {
    git_oid_cpy(rewritten_commit_id, git_commit_id(commit));
    return 0;
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
                         const BbfgFilter* filter)
{
  git_commit* head_commit = NULL;
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
                                       filter,
                                       BBFG_MISSING_PATH_IS_ERROR,
                                       parents,
                                       parent_count,
                                       NULL,
                                       NULL,
                                       0);

  free_commit_array(parents, parent_count);
  git_commit_free(head_commit);
  return result;
}

static int
rewrite_history_from_commit(git_oid* rewritten_commit_id,
                            git_repository* repo,
                            const char* repo_path,
                            git_commit* start_commit,
                            BbfgRewriteRef* rewrite_ref,
                            const BbfgFilter* filter)
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

  RewriteState state = { BBFG_OID_MAP_INIT,
                         BBFG_OID_MAP_INIT,
                         BBFG_OID_MAP_INIT };
  BbfgMempack mempack = BBFG_MEMPACK_INIT;
  if (bbfg_mempack_begin(&mempack, repo) < 0) {
    bbfg_print_git_error("could not start object pack", repo_path);
    git_revwalk_free(walk);
    return -1;
  }

  const git_oid* protected_commit_ids =
    filter->protect_blobs ? git_commit_id(start_commit) : NULL;
  size_t protected_commit_count = filter->protect_blobs ? 1 : 0;
  int result = rewrite_history_from_walk(&state,
                                         repo,
                                         repo_path,
                                         walk,
                                         filter,
                                         protected_commit_ids,
                                         protected_commit_count);
  if (result == 0) {
    const git_oid* rewritten_tip_id =
      bbfg_oid_map_get(&state.commits, git_commit_id(start_commit));
    if (rewritten_tip_id == NULL) {
      bbfg_print_git_error("could not find rewritten tip", repo_path);
      result = -1;
    } else {
      git_oid_cpy(rewritten_commit_id, rewritten_tip_id);
      if (rewrite_ref != NULL) {
        git_oid_cpy(&rewrite_ref->rewritten_commit_id, rewritten_tip_id);
        if (rewrite_ref->kind == BBFG_REWRITE_ANNOTATED_TAG) {
          result = rewrite_tag(&rewrite_ref->rewritten_ref_id,
                               repo,
                               repo_path,
                               &rewrite_ref->original_ref_id,
                               &state.commits,
                               &state.tags);
        } else {
          git_oid_cpy(&rewrite_ref->rewritten_ref_id, rewritten_tip_id);
        }
      }
    }
  }

  if (result == 0 && bbfg_mempack_commit(&mempack, repo, &state.tags) < 0) {
    bbfg_print_git_error("could not write rewritten objects", repo_path);
    result = -1;
  }
  if (result < 0) {
    bbfg_mempack_abort(&mempack);
  }

  bbfg_oid_map_dispose(&state.commits);
  bbfg_oid_map_dispose(&state.tags);
  bbfg_oid_map_dispose(&state.trees);
  bbfg_mempack_dispose(&mempack);
  git_revwalk_free(walk);
  return result;
}

static int
classify_ref(BbfgRewriteRef* ref, git_repository* repo)
{
  git_reference* original = NULL;
  if (git_reference_lookup(&original, repo, ref->name) < 0) {
    bbfg_print_git_error("could not resolve ref", ref->name);
    return -1;
  }

  git_reference* resolved = NULL;
  if (git_reference_resolve(&resolved, original) < 0) {
    bbfg_print_git_error("could not resolve ref", ref->name);
    git_reference_free(original);
    return -1;
  }

  const git_oid* target_id = git_reference_target(resolved);
  git_object* target = NULL;
  if (target_id == NULL ||
      git_object_lookup(&target, repo, target_id, GIT_OBJECT_ANY) < 0) {
    bbfg_print_git_error("ref does not point to an object", ref->name);
    git_reference_free(resolved);
    git_reference_free(original);
    return -1;
  }

  if (git_object_type(target) == GIT_OBJECT_COMMIT) {
    ref->kind = BBFG_REWRITE_DIRECT_REF;
  } else if (git_object_type(target) == GIT_OBJECT_TAG) {
    ref->kind = BBFG_REWRITE_ANNOTATED_TAG;
  } else {
    bbfg_print_git_error("ref does not point to a commit or tag", ref->name);
    git_object_free(target);
    git_reference_free(resolved);
    git_reference_free(original);
    return -1;
  }

  git_oid_cpy(&ref->original_ref_id, target_id);
  git_object_free(target);
  git_reference_free(resolved);
  git_reference_free(original);
  return 0;
}

static int
rewrite_tag(git_oid* rewritten_tag_id,
            git_repository* repo,
            const char* repo_path,
            const git_oid* original_tag_id,
            const BbfgOidMap* commits,
            BbfgOidMap* tags)
{
  const git_oid* cached_tag_id = bbfg_oid_map_get(tags, original_tag_id);
  if (cached_tag_id != NULL) {
    git_oid_cpy(rewritten_tag_id, cached_tag_id);
    return 0;
  }

  git_tag* tag = NULL;
  if (git_tag_lookup(&tag, repo, original_tag_id) < 0) {
    bbfg_print_git_error("could not read annotated tag", repo_path);
    return -1;
  }

  const git_oid* original_target_id = git_tag_target_id(tag);
  git_object_t target_type = git_tag_target_type(tag);
  git_oid rewritten_target_id;
  if (original_target_id == NULL ||
      (target_type != GIT_OBJECT_TAG && target_type != GIT_OBJECT_COMMIT)) {
    bbfg_print_git_error("annotated tag does not point to a commit or tag",
                         repo_path);
    git_tag_free(tag);
    return -1;
  }

  int result = 0;
  if (target_type == GIT_OBJECT_TAG) {
    result = rewrite_tag(
      &rewritten_target_id, repo, repo_path, original_target_id, commits, tags);
  } else {
    const git_oid* rewritten_commit_id =
      bbfg_oid_map_get(commits, original_target_id);
    if (rewritten_commit_id == NULL) {
      bbfg_print_git_error("could not find rewritten tag target", repo_path);
      git_tag_free(tag);
      return -1;
    } else {
      git_oid_cpy(&rewritten_target_id, rewritten_commit_id);
    }
  }

  if (result < 0) {
    git_tag_free(tag);
    return -1;
  }

  git_object* target = NULL;
  if (git_object_lookup(&target, repo, &rewritten_target_id, target_type) < 0) {
    bbfg_print_git_error("could not read rewritten tag target", repo_path);
    git_tag_free(tag);
    return -1;
  }

  result = git_tag_annotation_create(rewritten_tag_id,
                                     repo,
                                     git_tag_name(tag),
                                     target,
                                     git_tag_tagger(tag),
                                     git_tag_message(tag));
  if (result < 0) {
    bbfg_print_git_error("could not create rewritten tag", repo_path);
  } else {
    result = bbfg_oid_map_put(tags, original_tag_id, rewritten_tag_id);
  }

  git_object_free(target);
  git_tag_free(tag);
  return result;
}

static int
push_ref_tip(git_oid* tip_id,
             git_revwalk* walk,
             git_repository* repo,
             BbfgRewriteRef* ref)
{
  if (classify_ref(ref, repo) < 0) {
    return -1;
  }

  git_commit* commit = NULL;
  if (bbfg_lookup_ref_commit(&commit, repo, ref->name) < 0) {
    return -1;
  }

  git_oid_cpy(tip_id, git_commit_id(commit));
  int result = git_revwalk_push(walk, tip_id);
  git_commit_free(commit);
  return result;
}

static int
push_ref_tips(git_oid* tip_ids,
              git_revwalk* walk,
              git_repository* repo,
              BbfgRewriteRef* refs,
              size_t ref_count)
{
  size_t i;

  for (i = 0; i < ref_count; i++) {
    if (push_ref_tip(&tip_ids[i], walk, repo, &refs[i]) < 0) {
      bbfg_print_git_error("could not push rewrite tip", refs[i].name);
      return -1;
    }
  }

  return 0;
}

static int
copy_rewritten_ref_tips(BbfgRewriteRef* refs,
                        git_repository* repo,
                        const char* repo_path,
                        const git_oid* tip_ids,
                        RewriteState* state,
                        size_t ref_count)
{
  size_t i;

  for (i = 0; i < ref_count; i++) {
    const git_oid* rewritten_tip_id =
      bbfg_oid_map_get(&state->commits, &tip_ids[i]);
    if (rewritten_tip_id == NULL) {
      bbfg_print_git_error("could not find rewritten tip", refs[i].name);
      return -1;
    }

    git_oid_cpy(&refs[i].rewritten_commit_id, rewritten_tip_id);
    if (refs[i].kind == BBFG_REWRITE_ANNOTATED_TAG) {
      if (rewrite_tag(&refs[i].rewritten_ref_id,
                      repo,
                      repo_path,
                      &refs[i].original_ref_id,
                      &state->commits,
                      &state->tags) < 0) {
        return -1;
      }
    } else {
      git_oid_cpy(&refs[i].rewritten_ref_id, rewritten_tip_id);
    }
  }

  return 0;
}

static int
commit_rewrite_pack(BbfgMempack* mempack,
                    git_repository* repo,
                    const char* repo_path,
                    const BbfgOidMap* rewritten_tags)
{
  if (bbfg_mempack_commit(mempack, repo, rewritten_tags) < 0) {
    bbfg_print_git_error("could not write rewritten objects", repo_path);
    return -1;
  }

  return 0;
}

static int
rewrite_one_commit(RewriteState* state,
                   git_repository* repo,
                   const char* repo_path,
                   const BbfgFilter* filter,
                   const git_oid* protected_commit_ids,
                   size_t protected_commit_count,
                   const git_oid* old_id)
{
  git_commit* commit = NULL;
  if (git_commit_lookup(&commit, repo, old_id) < 0) {
    bbfg_print_git_error("could not read commit", repo_path);
    return -1;
  }

  const git_commit** parents = NULL;
  size_t parent_count = 0;
  if (load_rewritten_parents(
        &parents, &parent_count, repo, &state->commits, commit) < 0) {
    bbfg_print_git_error("could not read rewritten parents", repo_path);
    git_commit_free(commit);
    return -1;
  }

  git_oid new_id;
  int result = create_rewritten_commit(&new_id,
                                       repo,
                                       repo_path,
                                       commit,
                                       filter,
                                       BBFG_MISSING_PATH_IS_UNCHANGED,
                                       parents,
                                       parent_count,
                                       &state->trees,
                                       protected_commit_ids,
                                       protected_commit_count);
  if (result == 0) {
    result = bbfg_oid_map_put(&state->commits, old_id, &new_id);
  }

  free_commit_array(parents, parent_count);
  git_commit_free(commit);
  return result;
}

static int
rewrite_history_from_walk(RewriteState* state,
                          git_repository* repo,
                          const char* repo_path,
                          git_revwalk* walk,
                          const BbfgFilter* filter,
                          const git_oid* protected_commit_ids,
                          size_t protected_commit_count)
{
  git_oid old_id;
  int result = git_revwalk_next(&old_id, walk);
  while (result == 0) {
    if (rewrite_one_commit(state,
                           repo,
                           repo_path,
                           filter,
                           protected_commit_ids,
                           protected_commit_count,
                           &old_id) < 0) {
      result = -1;
      break;
    }

    result = git_revwalk_next(&old_id, walk);
  }

  if (result == GIT_ITEROVER) {
    result = 0;
  }

  if (result < 0) {
    bbfg_print_git_error("could not rewrite HEAD history", repo_path);
  }

  return result;
}

int
bbfg_rewrite_head_history(git_oid* rewritten_commit_id,
                          git_repository* repo,
                          const char* repo_path,
                          const BbfgFilter* filter)
{
  git_commit* head_commit = NULL;
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
    return -1;
  }

  int result = rewrite_history_from_commit(
    rewritten_commit_id, repo, repo_path, head_commit, NULL, filter);
  git_commit_free(head_commit);
  return result;
}

int
bbfg_rewrite_ref_history(BbfgRewriteRef* ref,
                         git_repository* repo,
                         const char* repo_path,
                         const BbfgFilter* filter)
{
  if (classify_ref(ref, repo) < 0) {
    return -1;
  }

  git_commit* commit = NULL;
  if (bbfg_lookup_ref_commit(&commit, repo, ref->name) < 0) {
    return -1;
  }

  int result = rewrite_history_from_commit(
    &ref->rewritten_commit_id, repo, repo_path, commit, ref, filter);
  git_commit_free(commit);
  return result;
}

int
bbfg_rewrite_ref_histories(BbfgRewriteRef* refs,
                           git_repository* repo,
                           const char* repo_path,
                           size_t ref_count,
                           const BbfgFilter* filter)
{
  if (ref_count == 0) {
    return 0;
  }

  git_revwalk* walk = NULL;
  if (git_revwalk_new(&walk, repo) < 0) {
    bbfg_print_git_error("could not create revwalk", repo_path);
    return -1;
  }

  git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

  git_oid* tip_ids = (git_oid*)calloc(ref_count, sizeof(*tip_ids));
  if (tip_ids == NULL) {
    git_revwalk_free(walk);
    return -1;
  }

  if (push_ref_tips(tip_ids, walk, repo, refs, ref_count) < 0) {
    free(tip_ids);
    git_revwalk_free(walk);
    return -1;
  }

  RewriteState state = { BBFG_OID_MAP_INIT,
                         BBFG_OID_MAP_INIT,
                         BBFG_OID_MAP_INIT };
  BbfgMempack mempack = BBFG_MEMPACK_INIT;
  if (bbfg_mempack_begin(&mempack, repo) < 0) {
    bbfg_print_git_error("could not start object pack", repo_path);
    free(tip_ids);
    git_revwalk_free(walk);
    return -1;
  }

  int result = rewrite_history_from_walk(&state,
                                         repo,
                                         repo_path,
                                         walk,
                                         filter,
                                         filter->protect_blobs ? tip_ids : NULL,
                                         filter->protect_blobs ? ref_count : 0);
  if (result == 0) {
    result = copy_rewritten_ref_tips(
      refs, repo, repo_path, tip_ids, &state, ref_count);
  }

  if (result == 0) {
    result = commit_rewrite_pack(&mempack, repo, repo_path, &state.tags);
  }
  if (result < 0) {
    bbfg_mempack_abort(&mempack);
  }

  bbfg_oid_map_dispose(&state.commits);
  bbfg_oid_map_dispose(&state.tags);
  bbfg_oid_map_dispose(&state.trees);
  bbfg_mempack_dispose(&mempack);
  free(tip_ids);
  git_revwalk_free(walk);
  return result;
}
