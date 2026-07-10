#ifndef BBFG_REWRITE_H
#define BBFG_REWRITE_H

#include "filter.h"

#include <git2.h>

#include <stddef.h>

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
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int
bbfg_rewrite_ref_history(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const char* ref_name,
                         const BbfgFilter* filter);
int
bbfg_rewrite_ref_histories(git_oid* rewritten_commit_ids,
                           git_repository* repo,
                           const char* repo_path,
                           const char* const* ref_names,
                           size_t ref_count,
                           const BbfgFilter* filter);
// NOLINTEND(bugprone-easily-swappable-parameters)

#endif
