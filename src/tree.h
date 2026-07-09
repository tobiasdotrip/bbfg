#ifndef BBFG_TREE_H
#define BBFG_TREE_H

#include <git2.h>

int
bbfg_tree_remove_path(git_oid* rewritten_tree_id,
                      git_repository* repo,
                      git_tree* tree,
                      const char* path);

#endif
