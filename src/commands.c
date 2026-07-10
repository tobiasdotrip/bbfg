#include "commands.h"

#include "error.h"
#include "refs.h"
#include "repository.h"
#include "rewrite.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>

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
                 const char* ref_name,
                 const BbfgFilter* filter)
{
  git_oid rewritten_commit_id;
  if (bbfg_rewrite_ref_history(
        &rewritten_commit_id, repo, repo_path, ref_name, filter) < 0) {
    return -1;
  }

  if (bbfg_write_ref(repo, ref_name, &rewritten_commit_id, "bbfg rewrite ref") <
      0) {
    return -1;
  }

  printf("%s\n", git_oid_tostr_s(&rewritten_commit_id));
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

  git_oid* rewritten_commit_ids =
    (git_oid*)calloc(refs.count, sizeof(*rewritten_commit_ids));
  if (rewritten_commit_ids == NULL && refs.count > 0) {
    bbfg_ref_list_dispose(&refs);
    return -1;
  }

  if (bbfg_rewrite_ref_histories(rewritten_commit_ids,
                                 repo,
                                 repo_path,
                                 (const char* const*)refs.names,
                                 refs.count,
                                 filter) < 0) {
    free(rewritten_commit_ids);
    bbfg_ref_list_dispose(&refs);
    return -1;
  }

  size_t i;
  for (i = 0; i < refs.count; i++) {
    if (bbfg_write_ref(
          repo, refs.names[i], &rewritten_commit_ids[i], "bbfg rewrite ref") <
        0) {
      free(rewritten_commit_ids);
      bbfg_ref_list_dispose(&refs);
      return -1;
    }

    printf("%s %s\n", refs.names[i], git_oid_tostr_s(&rewritten_commit_ids[i]));
  }

  free(rewritten_commit_ids);
  bbfg_ref_list_dispose(&refs);
  return 0;
}
