#ifndef BBFG_REFS_H
#define BBFG_REFS_H

#include <git2.h>

#include <stddef.h>

typedef struct
{
  char** names;
  size_t count;
  size_t capacity;
} BbfgRefList;

typedef struct
{
  const char* name;
  const git_oid* target_id;
} BbfgRefUpdate;

int
bbfg_collect_rewrite_refs(BbfgRefList* refs, git_repository* repo);
int
bbfg_validate_rewrite_ref(git_repository* repo, const char* ref_name);
void
bbfg_ref_list_dispose(BbfgRefList* refs);
int
bbfg_write_ref(git_repository* repo,
               const char* ref_name,
               const git_oid* target_id,
               const char* log_message);
int
bbfg_write_refs(git_repository* repo,
                const BbfgRefUpdate* updates,
                size_t update_count,
                const char* log_message);

#endif
