#ifndef BBFG_COMMANDS_H
#define BBFG_COMMANDS_H

#include "rewrite.h"

#include <git2.h>

int
bbfg_print_head_commit_id(git_repository* repo, const char* repo_path);
int
bbfg_print_head_tree_entries(git_repository* repo, const char* repo_path);
int
bbfg_print_head_tree_id(git_repository* repo, const char* repo_path);
int
bbfg_print_rewrite_refs(git_repository* repo, const char* repo_path);
int
bbfg_print_rewrite_commits(git_repository* repo, const char* repo_path);
int
bbfg_rebuild_head_tree(git_repository* repo, const char* repo_path);
int
bbfg_remove_head_tree_entry(git_repository* repo,
                            const char* repo_path,
                            const char* path);
int
bbfg_commit_without_tree_entry(git_repository* repo,
                               const char* repo_path,
                               const BbfgFilter* filter);
int
bbfg_write_rewrite_ref(git_repository* repo,
                       const char* repo_path,
                       const BbfgFilter* filter);
int
bbfg_rewrite_head_history_ref(git_repository* repo,
                              const char* repo_path,
                              const BbfgFilter* filter);
int
bbfg_rewrite_ref(git_repository* repo,
                 const char* repo_path,
                 BbfgRewriteRef* ref,
                 const BbfgFilter* filter);
int
bbfg_rewrite_refs(git_repository* repo,
                  const char* repo_path,
                  const BbfgFilter* filter);
int
bbfg_print_refs(git_repository* repo, const char* repo_path);

#endif
