#ifndef BBFG_COMMANDS_H
#define BBFG_COMMANDS_H

#include <git2.h>

int
bbfg_print_head_commit_id(git_repository* repo, const char* repo_path);
int
bbfg_print_head_tree_entries(git_repository* repo, const char* repo_path);
int
bbfg_print_head_tree_id(git_repository* repo, const char* repo_path);
int
bbfg_print_refs(git_repository* repo, const char* repo_path);

#endif
