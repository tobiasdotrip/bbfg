#include "commands.h"

#include "error.h"
#include "repository.h"

#include <stdio.h>
#include <string.h>

static int
has_prefix(const char* value, const char* prefix)
{
  return strncmp(value, prefix, strlen(prefix)) == 0;
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

int
bbfg_print_head_commit_id(git_repository* repo, const char* repo_path)
{
  git_commit* head_commit = NULL;
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
  if (bbfg_lookup_head_commit(&head_commit, repo, repo_path) < 0) {
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
