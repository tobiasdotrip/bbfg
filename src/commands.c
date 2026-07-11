#include "commands.h"

#include "error.h"
#include "refs.h"
#include "repository.h"
#include "rewrite.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>

static void
init_rewrite_refs(BbfgRewriteRef* rewrite_refs, const BbfgRefList* refs)
{
  size_t i;

  for (i = 0; i < refs->count; i++) {
    rewrite_refs[i].name = refs->names[i];
  }
}

int
bbfg_rebuild_head_tree(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
                               const BbfgFilter* filter)
{
  git_oid rewritten_commit_id;
  if (bbfg_rewrite_head_commit(&rewritten_commit_id, repo, repo_path, filter) <
      0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
  return 0;
}

int
bbfg_write_rewrite_ref(git_repository* repo,
                       const char* repo_path,
                       const BbfgFilter* filter)
{
  git_oid rewritten_commit_id;
  if (bbfg_rewrite_head_commit(&rewritten_commit_id, repo, repo_path, filter) <
      0) {
    return -1;
  }

  if (bbfg_write_ref(
        repo, "refs/heads/bbfg-rewrite", &rewritten_commit_id, "bbfg rewrite") <
      0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
  return 0;
}

int
bbfg_rewrite_head_history_ref(git_repository* repo,
                              const char* repo_path,
                              const BbfgFilter* filter)
{
  git_oid rewritten_commit_id;
  if (bbfg_rewrite_head_history(&rewritten_commit_id, repo, repo_path, filter) <
      0) {
    return -1;
  }

  if (bbfg_write_ref(repo,
                     "refs/heads/bbfg-rewrite",
                     &rewritten_commit_id,
                     "bbfg rewrite history") < 0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
  return 0;
}

int
bbfg_rewrite_ref(git_repository* repo,
                 const char* repo_path,
                 BbfgRewriteRef* ref,
                 const BbfgFilter* filter)
{
  if (bbfg_rewrite_ref_history(ref, repo, repo_path, filter) < 0) {
    return -1;
  }

  if (bbfg_write_ref(
        repo, ref->name, &ref->rewritten_ref_id, "bbfg rewrite ref") < 0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&ref->rewritten_ref_id));
  return 0;
}

int
bbfg_rewrite_refs(git_repository* repo,
                  const char* repo_path,
                  const BbfgFilter* filter)
{
  BbfgRefList refs = { NULL, 0, 0 };
  if (bbfg_collect_rewrite_refs(&refs, repo) < 0) {
    return -1;
  }

  BbfgRewriteRef* rewrite_refs =
    (BbfgRewriteRef*)calloc(refs.count, sizeof(*rewrite_refs));
  if (rewrite_refs == NULL && refs.count > 0) {
    bbfg_ref_list_dispose(&refs);
    return -1;
  }
  init_rewrite_refs(rewrite_refs, &refs);

  if (bbfg_rewrite_ref_histories(
        rewrite_refs, repo, repo_path, refs.count, filter) < 0) {
    free(rewrite_refs);
    bbfg_ref_list_dispose(&refs);
    return -1;
  }

  BbfgRefUpdate* updates = NULL;
  if (refs.count > 0) {
    updates = (BbfgRefUpdate*)calloc(refs.count, sizeof(*updates));
    if (updates == NULL) {
      free(rewrite_refs);
      bbfg_ref_list_dispose(&refs);
      return -1;
    }
  }

  size_t i;
  for (i = 0; i < refs.count; i++) {
    updates[i].name = rewrite_refs[i].name;
    updates[i].target_id = &rewrite_refs[i].rewritten_ref_id;
  }

  if (bbfg_write_refs(repo, updates, refs.count, "bbfg rewrite ref") < 0) {
    free(updates);
    free(rewrite_refs);
    bbfg_ref_list_dispose(&refs);
    return -1;
  }

  for (i = 0; i < refs.count; i++) {
    printf("%s %s\n",
           rewrite_refs[i].name,
           git_oid_tostr_s(&rewrite_refs[i].rewritten_ref_id));
  }

  free(updates);
  free(rewrite_refs);
  bbfg_ref_list_dispose(&refs);
  return 0;
}
