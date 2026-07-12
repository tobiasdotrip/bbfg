#ifndef BBFG_MEMPACK_H
#define BBFG_MEMPACK_H

#include "oid_map.h"

#include <git2.h>

typedef struct
{
  git_odb* odb;
  git_odb_backend* backend;
} BbfgMempack;

#define BBFG_MEMPACK_INIT { NULL, NULL }

int
bbfg_mempack_begin(BbfgMempack* mempack, git_repository* repo);

int
bbfg_mempack_commit(BbfgMempack* mempack,
                    git_repository* repo,
                    const BbfgOidMap* rewritten_tags);

void
bbfg_mempack_abort(BbfgMempack* mempack);

void
bbfg_mempack_dispose(BbfgMempack* mempack);

#endif
