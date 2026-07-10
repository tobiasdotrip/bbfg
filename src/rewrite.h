#ifndef BBFG_REWRITE_H
#define BBFG_REWRITE_H

#include <git2.h>

int
bbfg_rewrite_head_commit(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const char* path);
int
bbfg_rewrite_head_history(git_oid* rewritten_commit_id,
                          git_repository* repo,
                          const char* repo_path,
                          const char* path);
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int
bbfg_rewrite_ref_history(git_oid* rewritten_commit_id,
                         git_repository* repo,
                         const char* repo_path,
                         const char* ref_name,
                         const char* path);
// NOLINTEND(bugprone-easily-swappable-parameters)

#endif
