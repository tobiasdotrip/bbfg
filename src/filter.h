#ifndef BBFG_FILTER_H
#define BBFG_FILTER_H

#include <git2.h>

#include <stddef.h>

typedef enum
{
  BBFG_FILTER_DELETE_PATH,
  BBFG_FILTER_DELETE_FILENAME,
  BBFG_FILTER_DELETE_LARGE_BLOB
} BbfgFilterKind;

typedef struct
{
  BbfgFilterKind kind;
  const char* path;
  git_object_size_t max_blob_size;
} BbfgFilterRule;

typedef struct
{
  BbfgFilterRule* rules;
  size_t count;
  size_t capacity;
} BbfgFilter;

void
bbfg_filter_init(BbfgFilter* filter);
void
bbfg_filter_dispose(BbfgFilter* filter);
int
bbfg_filter_delete_path(BbfgFilter* filter, const char* path);
int
bbfg_filter_delete_filename(BbfgFilter* filter, const char* filename);
int
bbfg_filter_delete_blobs_larger_than(BbfgFilter* filter,
                                     git_object_size_t max_size);
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
