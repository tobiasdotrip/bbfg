#include "filter.h"

#include "tree.h"

void
bbfg_filter_delete_path(BbfgFilter* filter, const char* path)
{
  filter->kind = BBFG_FILTER_DELETE_PATH;
  filter->path = path;
}

int
bbfg_filter_matches_tree(int* matches, const BbfgFilter* filter, git_tree* tree)
{
  if (filter->kind == BBFG_FILTER_DELETE_PATH) {
    git_tree_entry* entry = NULL;
    int result = git_tree_entry_bypath(&entry, tree, filter->path);
    if (result == GIT_ENOTFOUND) {
      *matches = 0;
      return 0;
    }

    if (result < 0) {
      return -1;
    }

    git_tree_entry_free(entry);
    *matches = 1;
    return 0;
  }

  return -1;
}

int
bbfg_filter_apply_tree(git_oid* rewritten_tree_id,
                       git_repository* repo,
                       git_tree* tree,
                       const BbfgFilter* filter)
{
  if (filter->kind == BBFG_FILTER_DELETE_PATH) {
    return bbfg_tree_remove_path(rewritten_tree_id, repo, tree, filter->path);
  }

  return -1;
}
