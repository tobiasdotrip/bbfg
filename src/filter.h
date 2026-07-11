#ifndef BBFG_FILTER_H
#define BBFG_FILTER_H

#include <git2.h>

typedef enum
{
  BBFG_FILTER_DELETE_PATH,
  BBFG_FILTER_DELETE_FILENAME
} BbfgFilterKind;

typedef struct
{
  BbfgFilterKind kind;
  const char* path;
} BbfgFilter;

void
bbfg_filter_delete_path(BbfgFilter* filter, const char* path);
void
bbfg_filter_delete_filename(BbfgFilter* filter, const char* filename);
int
bbfg_filter_matches_tree(int* matches,
                         git_repository* repo,
                         const BbfgFilter* filter,
                         git_tree* tree);
int
bbfg_filter_apply_tree(git_oid* rewritten_tree_id,
                       git_repository* repo,
                       git_tree* tree,
                       const BbfgFilter* filter);

#endif
