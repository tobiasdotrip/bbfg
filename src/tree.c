#include "tree.h"

#include <stdlib.h>
#include <string.h>

static int
copy_path_component(char** component, const char* path, size_t length)
{
  char* value = malloc(length + 1);
  if (value == NULL) {
    return -1;
  }

  memcpy(value, path, length);
  value[length] = '\0';
  *component = value;
  return 0;
}

static int
tree_is_empty(int* is_empty, git_repository* repo, const git_oid* tree_id)
{
  git_tree* tree = NULL;
  if (git_tree_lookup(&tree, repo, tree_id) < 0) {
    return -1;
  }

  *is_empty = git_tree_entrycount(tree) == 0;
  git_tree_free(tree);
  return 0;
}

int
bbfg_tree_remove_path(git_oid* rewritten_tree_id,
                      git_repository* repo,
                      git_tree* tree,
                      const char* path)
{
  git_treebuilder* builder = NULL;
  if (git_treebuilder_new(&builder, repo, tree) < 0) {
    return -1;
  }

  const char* slash = strchr(path, '/');
  if (slash == NULL) {
    int result = git_treebuilder_remove(builder, path);
    if (result == 0) {
      result = git_treebuilder_write(rewritten_tree_id, builder);
    }

    git_treebuilder_free(builder);
    return result;
  }

  if (slash == path || slash[1] == '\0') {
    git_treebuilder_free(builder);
    return -1;
  }

  char* component = NULL;
  if (copy_path_component(&component, path, (size_t)(slash - path)) < 0) {
    git_treebuilder_free(builder);
    return -1;
  }

  const git_tree_entry* entry = git_tree_entry_byname(tree, component);
  if (entry == NULL || git_tree_entry_type(entry) != GIT_OBJECT_TREE) {
    free(component);
    git_treebuilder_free(builder);
    return -1;
  }

  git_tree* subtree = NULL;
  if (git_tree_lookup(&subtree, repo, git_tree_entry_id(entry)) < 0) {
    free(component);
    git_treebuilder_free(builder);
    return -1;
  }

  git_oid rewritten_subtree_id;
  int result =
    bbfg_tree_remove_path(&rewritten_subtree_id, repo, subtree, slash + 1);
  git_tree_free(subtree);

  if (result == 0) {
    int is_empty = 0;
    result = tree_is_empty(&is_empty, repo, &rewritten_subtree_id);
    if (result == 0 && is_empty) {
      result = git_treebuilder_remove(builder, component);
    } else if (result == 0) {
      result = git_treebuilder_insert(NULL,
                                      builder,
                                      component,
                                      &rewritten_subtree_id,
                                      git_tree_entry_filemode(entry));
    }
  }

  if (result == 0) {
    result = git_treebuilder_write(rewritten_tree_id, builder);
  }

  free(component);
  git_treebuilder_free(builder);
  return result;
}
