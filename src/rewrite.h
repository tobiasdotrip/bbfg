#ifndef BBFG_REWRITE_H
#define BBFG_REWRITE_H

#include "filter.h"

#include <git2.h>

#include <stddef.h>

typedef enum
{
  BBFG_REWRITE_DIRECT_REF,
  BBFG_REWRITE_ANNOTATED_TAG
} BbfgRewriteRefKind;

typedef struct
{
  const char* name;
  BbfgRewriteRefKind kind;
  git_oid original_ref_id;
  git_oid rewritten_commit_id;
  git_oid rewritten_ref_id;
} BbfgRewriteRef;

int
bbfg_rewrite_head_commit(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const BbfgFilter* filter);
int
bbfg_rewrite_head_history(git_oid* rewritten_commit_id,
                          git_repository* repo,
                          const char* repo_path,
                          const BbfgFilter* filter);
int
bbfg_rewrite_ref_history(BbfgRewriteRef* ref,
                         git_repository* repo,
                         const char* repo_path,
                         const BbfgFilter* filter);
int
bbfg_rewrite_ref_histories(BbfgRewriteRef* refs,
                           git_repository* repo,
                           const char* repo_path,
                           size_t ref_count,
                           const BbfgFilter* filter);

#endif
