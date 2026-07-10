#ifndef BBFG_REFS_H
#define BBFG_REFS_H

#include <git2.h>

int
bbfg_write_ref(git_repository* repo,
               const char* ref_name,
               const git_oid* target_id,
               const char* log_message);

#endif
