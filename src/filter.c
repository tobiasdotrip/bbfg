#include "filter.h"

#include "tree.h"

#include <stdlib.h>

void
bbfg_filter_init(BbfgFilter* filter)
{
  filter->rules = NULL;
  filter->count = 0;
  filter->capacity = 0;
  filter->protect_blobs = 1;
}

void
bbfg_filter_dispose(BbfgFilter* filter)
{
  free(filter->rules);
  filter->rules = NULL;
  filter->count = 0;
  filter->capacity = 0;
  filter->protect_blobs = 1;
}

static int
append_rule(BbfgFilter* filter,
            BbfgFilterKind kind,
            const char* path,
            git_object_size_t max_blob_size)
{
  if (filter->count == filter->capacity) {
    size_t next_capacity = filter->capacity == 0 ? 4 : filter->capacity * 2;
    BbfgFilterRule* next_rules = (BbfgFilterRule*)realloc(
      filter->rules, next_capacity * sizeof(*next_rules));
    if (next_rules == NULL) {
      return -1;
    }

    filter->rules = next_rules;
    filter->capacity = next_capacity;
  }

  filter->rules[filter->count].kind = kind;
  filter->rules[filter->count].path = path;
  filter->rules[filter->count].max_blob_size = max_blob_size;
  filter->count++;
  return 0;
}

int
bbfg_filter_delete_path(BbfgFilter* filter, const char* path)
{
  return append_rule(filter, BBFG_FILTER_DELETE_PATH, path, 0);
}

int
bbfg_filter_delete_filename(BbfgFilter* filter, const char* filename)
{
  return append_rule(filter, BBFG_FILTER_DELETE_FILENAME, filename, 0);
}

int
bbfg_filter_delete_blobs_larger_than(BbfgFilter* filter,
                                     git_object_size_t max_size)
{
  return append_rule(filter, BBFG_FILTER_DELETE_LARGE_BLOB, NULL, max_size);
}

static int
rule_matches_tree(int* matches,
                  git_repository* repo,
                  const BbfgFilterRule* rule,
                  git_tree* tree)
{
  switch (rule->kind) {
    case BBFG_FILTER_DELETE_PATH: {
      git_tree_entry* entry = NULL;
      int result = git_tree_entry_bypath(&entry, tree, rule->path);
      if (result == GIT_ENOTFOUND) {
        *matches = 0;
        return 0;
      }

      if (result < 0) {
        return -1;
      }

      git_tree_entry_free(entry);
      *matches = 1;
      return 0;
    }
    case BBFG_FILTER_DELETE_FILENAME:
      return bbfg_tree_contains_filename(matches, repo, tree, rule->path);
    case BBFG_FILTER_DELETE_LARGE_BLOB:
      return bbfg_tree_contains_blob_larger_than(
        matches, repo, tree, rule->max_blob_size);
  }

  return -1;
}

int
bbfg_filter_matches_tree(int* matches,
                         git_repository* repo,
                         const BbfgFilter* filter,
                         git_tree* tree)
{
  size_t i;
  *matches = 0;

  for (i = 0; i < filter->count; i++) {
    int rule_matches = 0;
    int result =
      rule_matches_tree(&rule_matches, repo, &filter->rules[i], tree);
    if (result < 0) {
      return result;
    }

    if (rule_matches) {
      *matches = 1;
      return 0;
    }
  }

  return 0;
}

static int
apply_rule(git_oid* rewritten_tree_id,
           git_repository* repo,
           git_tree* tree,
           const BbfgFilterRule* rule)
{
  switch (rule->kind) {
    case BBFG_FILTER_DELETE_PATH:
      return bbfg_tree_remove_path(rewritten_tree_id, repo, tree, rule->path);
    case BBFG_FILTER_DELETE_FILENAME:
      return bbfg_tree_remove_filename(
        rewritten_tree_id, repo, tree, rule->path);
    case BBFG_FILTER_DELETE_LARGE_BLOB:
      return bbfg_tree_remove_blobs_larger_than(
        rewritten_tree_id, repo, tree, rule->max_blob_size);
  }

  return -1;
}

int
bbfg_filter_apply_tree(git_oid* rewritten_tree_id,
                       git_repository* repo,
                       git_tree* tree,
                       const BbfgFilter* filter)
{
  git_tree* current_tree = tree;
  git_tree* owned_tree = NULL;
  size_t i;

  for (i = 0; i < filter->count; i++) {
    int matches = 0;
    int result =
      rule_matches_tree(&matches, repo, &filter->rules[i], current_tree);
    if (result < 0) {
      git_tree_free(owned_tree);
      return result;
    }

    if (!matches) {
      continue;
    }

    git_oid next_tree_id;
    result = apply_rule(&next_tree_id, repo, current_tree, &filter->rules[i]);
    if (result < 0) {
      git_tree_free(owned_tree);
      return result;
    }

    if (git_oid_equal(&next_tree_id, git_tree_id(current_tree))) {
      continue;
    }

    git_tree* next_tree = NULL;
    if (git_tree_lookup(&next_tree, repo, &next_tree_id) < 0) {
      git_tree_free(owned_tree);
      return -1;
    }

    git_tree_free(owned_tree);
    owned_tree = next_tree;
    current_tree = next_tree;
  }

  git_oid_cpy(rewritten_tree_id, git_tree_id(current_tree));
  git_tree_free(owned_tree);
  return 0;
}
