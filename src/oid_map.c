#include "oid_map.h"

#include <search.h>
#include <stdlib.h>

struct BbfgOidMapEntry
{
  git_oid old_id;
  git_oid new_id;
};

static int
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
compare_entries(const void* left, const void* right)
{
  const BbfgOidMapEntry* left_entry = (const BbfgOidMapEntry*)left;
  const BbfgOidMapEntry* right_entry = (const BbfgOidMapEntry*)right;

  return git_oid_cmp(&left_entry->old_id, &right_entry->old_id);
}

static int
reserve_entry(BbfgOidMap* map)
{
  if (map->count < map->capacity) {
    return 0;
  }

  size_t next_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
  BbfgOidMapEntry** next_entries = (BbfgOidMapEntry**)realloc(
    (void*)map->entries, next_capacity * sizeof(*next_entries));
  if (next_entries == NULL) {
    return -1;
  }

  map->entries = next_entries;
  map->capacity = next_capacity;
  return 0;
}

const git_oid*
bbfg_oid_map_get(const BbfgOidMap* map, const git_oid* old_id)
{
  BbfgOidMapEntry key;
  git_oid_cpy(&key.old_id, old_id);

  void* const* found = (void* const*)tfind(&key, &map->root, compare_entries);
  if (found == NULL) {
    return NULL;
  }

  const BbfgOidMapEntry* entry = *(const BbfgOidMapEntry* const*)found;
  return &entry->new_id;
}

int
bbfg_oid_map_put(BbfgOidMap* map, const git_oid* old_id, const git_oid* new_id)
{
  if (reserve_entry(map) < 0) {
    return -1;
  }

  BbfgOidMapEntry* entry = (BbfgOidMapEntry*)malloc(sizeof(*entry));
  if (entry == NULL) {
    return -1;
  }

  git_oid_cpy(&entry->old_id, old_id);
  git_oid_cpy(&entry->new_id, new_id);

  void** found = (void**)tsearch(entry, &map->root, compare_entries);
  if (found == NULL) {
    free(entry);
    return -1;
  }

  if (*found != entry) {
    BbfgOidMapEntry* existing = (BbfgOidMapEntry*)*found;
    git_oid_cpy(&existing->new_id, new_id);
    free(entry);
    return 0;
  }

  map->entries[map->count] = entry;
  map->count++;
  return 0;
}

void
bbfg_oid_map_dispose(BbfgOidMap* map)
{
  size_t i;

  for (i = 0; i < map->count; i++) {
    BbfgOidMapEntry* entry = map->entries[i];
    (void)tdelete(entry, &map->root, compare_entries);
    free(entry);
  }

  free((void*)map->entries);
  map->root = NULL;
  map->entries = NULL;
  map->count = 0;
  map->capacity = 0;
}
