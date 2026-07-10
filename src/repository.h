#ifndef BBFG_REPOSITORY_H
#define BBFG_REPOSITORY_H

#include <git2.h>

int
bbfg_lookup_head_commit(git_commit** commit,
                        git_repository* repo,
                        const char* repo_path);
int
bbfg_lookup_ref_commit(git_commit** commit,
                       git_repository* repo,
                       const char* ref_name);

#endif
