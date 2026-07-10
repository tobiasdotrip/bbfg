#ifndef BBFG_OID_MAP_H
#define BBFG_OID_MAP_H

#include <git2.h>

#include <stddef.h>

typedef struct BbfgOidMapEntry BbfgOidMapEntry;

typedef struct
{
  void* root;
  BbfgOidMapEntry** entries;
  size_t count;
  size_t capacity;
} BbfgOidMap;

#define BBFG_OID_MAP_INIT { NULL, NULL, 0, 0 }

const git_oid*
bbfg_oid_map_get(const BbfgOidMap* map, const git_oid* old_id);

int
bbfg_oid_map_put(BbfgOidMap* map, const git_oid* old_id, const git_oid* new_id);

void
bbfg_oid_map_dispose(BbfgOidMap* map);

#endif
