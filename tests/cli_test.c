#include <criterion/criterion.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(bugprone-command-processor)
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
// NOLINTBEGIN(readability-function-cognitive-complexity)

#define BBFG_TEST_PATH_SIZE 4096
#define BBFG_TEST_COMMAND_SIZE 8192

static const char*
bbfg_path(void)
{
  const char* path = getenv("BBFG");
  cr_assert_not_null(path);
  return path;
}

static void
format_command(char* command, size_t size, const char* format, va_list args)
{
  int length = vsnprintf(command, size, format, args);
  cr_assert(length >= 0);
  cr_assert((size_t)length < size);
}

static int
run_command(const char* format, ...)
{
  char command[BBFG_TEST_COMMAND_SIZE];
  va_list args;

  va_start(args, format);
  format_command(command, sizeof(command), format, args);
  va_end(args);

  return system(command);
}

static char*
append_output(char* output, size_t* output_size, const char* chunk)
{
  size_t chunk_size = strlen(chunk);
  char* next_output = realloc(output, *output_size + chunk_size + 1);
  cr_assert_not_null(next_output);

  memcpy(next_output + *output_size, chunk, chunk_size + 1);
  *output_size += chunk_size;
  return next_output;
}

static char*
read_command(const char* format, ...)
{
  char command[BBFG_TEST_COMMAND_SIZE];
  va_list args;

  va_start(args, format);
  format_command(command, sizeof(command), format, args);
  va_end(args);

  FILE* stream = popen(command, "r");
  cr_assert_not_null(stream);

  size_t output_size = 0;
  char* output = malloc(1);
  cr_assert_not_null(output);
  output[0] = '\0';

  char chunk[1024];
  while (fgets(chunk, sizeof(chunk), stream) != NULL) {
    output = append_output(output, &output_size, chunk);
  }

  cr_assert_eq(pclose(stream), 0);
  return output;
}

static void
strip_trailing_newline(char* value)
{
  size_t length = strlen(value);
  if (length > 0 && value[length - 1] == '\n') {
    value[length - 1] = '\0';
  }
}

static char*
format_string(const char* format, ...)
{
  char value[BBFG_TEST_COMMAND_SIZE];
  va_list args;

  va_start(args, format);
  format_command(value, sizeof(value), format, args);
  va_end(args);

  size_t length = strlen(value);
  char* copy = malloc(length + 1);
  cr_assert_not_null(copy);
  memcpy(copy, value, length + 1);
  return copy;
}

static void
write_file(const char* path, const char* content)
{
  FILE* file = fopen(path, "w");
  cr_assert_not_null(file);
  cr_assert_neq(fputs(content, file), EOF);
  cr_assert_eq(fclose(file), 0);
}

static void
init_repo(char* tmpdir, size_t tmpdir_size, char* repo, size_t repo_size)
{
  char* created_dir = read_command("mktemp -d /tmp/bbfg-criterion.XXXXXX");
  strip_trailing_newline(created_dir);

  cr_assert(strlen(created_dir) < tmpdir_size);
  strcpy(tmpdir, created_dir);
  free(created_dir);

  int length = snprintf(repo, repo_size, "%s/repo", tmpdir);
  cr_assert(length >= 0);
  cr_assert((size_t)length < repo_size);

  cr_assert_eq(run_command("git init -q %s", repo), 0);
  cr_assert_eq(run_command("git -C %s checkout -q -b main", repo), 0);
  cr_assert_eq(run_command("git -C %s config user.name 'bbfg test'", repo), 0);
  cr_assert_eq(
    run_command("git -C %s config user.email bbfg@example.invalid", repo), 0);

  char file_path[BBFG_TEST_PATH_SIZE];
  length = snprintf(file_path, sizeof(file_path), "%s/file.txt", repo);
  cr_assert(length >= 0);
  cr_assert((size_t)length < sizeof(file_path));
  write_file(file_path, "hello\n");

  cr_assert_eq(run_command("mkdir %s/dir", repo), 0);
  length = snprintf(file_path, sizeof(file_path), "%s/dir/nested.txt", repo);
  cr_assert(length >= 0);
  cr_assert((size_t)length < sizeof(file_path));
  write_file(file_path, "nested\n");

  cr_assert_eq(run_command("git -C %s add file.txt dir/nested.txt", repo), 0);
  cr_assert_eq(run_command("git -C %s commit -q -m 'Initial commit'", repo), 0);

  length = snprintf(file_path, sizeof(file_path), "%s/file.txt", repo);
  cr_assert(length >= 0);
  cr_assert((size_t)length < sizeof(file_path));
  write_file(file_path, "hello again\n");
  cr_assert_eq(run_command("git -C %s add file.txt", repo), 0);
  cr_assert_eq(run_command("git -C %s commit -q -m 'Second commit'", repo), 0);
}

Test(cli, commands_match_git)
{
  char tmpdir[BBFG_TEST_PATH_SIZE];
  char repo[BBFG_TEST_PATH_SIZE];
  const char* bbfg = bbfg_path();

  init_repo(tmpdir, sizeof(tmpdir), repo, sizeof(repo));

  char* git_dir = read_command("git -C %s rev-parse --absolute-git-dir", repo);
  strip_trailing_newline(git_dir);
  char* expected_open = format_string("bbfg: using repository: %s/\n", git_dir);
  char* actual = read_command("%s %s", bbfg, repo);
  cr_assert_str_eq(actual, expected_open);
  free(actual);
  free(expected_open);
  free(git_dir);

  actual = read_command("%s --help", bbfg);
  cr_assert(strncmp(actual, "usage: ", strlen("usage: ")) == 0);
  free(actual);

  cr_assert_neq(
    run_command("%s --head-commit --head-tree %s >/dev/null 2>&1", bbfg, repo),
    0);
  cr_assert_neq(run_command("%s %s --head-commit >/dev/null 2>&1", bbfg, repo),
                0);

  char* expected_head = read_command("git -C %s rev-parse HEAD", repo);
  actual = read_command("%s --head-commit %s", bbfg, repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  char* expected = read_command("git -C %s rev-parse 'HEAD^{tree}'", repo);
  actual = read_command("%s --head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);

  actual = read_command("%s --rebuild-head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  expected = read_command("git -C %s ls-tree HEAD", repo);
  actual = read_command("%s --list-head-tree %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  actual = read_command("%s --remove-head-entry file.txt %s", bbfg, repo);
  cr_assert_eq(run_command("git -C %s read-tree HEAD", repo), 0);
  cr_assert_eq(run_command("git -C %s rm -q --cached file.txt", repo), 0);
  expected = read_command("git -C %s write-tree", repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  actual = read_command("%s --remove-head-entry dir/nested.txt %s", bbfg, repo);
  cr_assert_eq(run_command("git -C %s read-tree HEAD", repo), 0);
  cr_assert_eq(run_command("git -C %s rm -q --cached dir/nested.txt", repo), 0);
  char* expected_removed_path = read_command("git -C %s write-tree", repo);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  char* commit_id =
    read_command("%s --commit-without-entry dir/nested.txt %s", bbfg, repo);
  strip_trailing_newline(commit_id);
  expected = read_command("git -C %s cat-file -t %s", repo, commit_id);
  cr_assert_str_eq(expected, "commit\n");
  free(expected);

  actual = read_command("git -C %s rev-parse '%s^{tree}'", repo, commit_id);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  actual = read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);
  free(commit_id);

  char* rewrite_ref =
    read_command("%s --write-rewrite-ref dir/nested.txt %s", bbfg, repo);
  strip_trailing_newline(rewrite_ref);
  expected = read_command("git -C %s rev-parse refs/heads/bbfg-rewrite", repo);
  strip_trailing_newline(expected);
  cr_assert_str_eq(rewrite_ref, expected);
  free(expected);

  actual = read_command("git -C %s rev-parse '%s^{tree}'", repo, rewrite_ref);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  actual = read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  char* history_ref =
    read_command("%s --rewrite-head-history dir/nested.txt %s", bbfg, repo);
  strip_trailing_newline(history_ref);
  expected = read_command("git -C %s rev-parse refs/heads/bbfg-rewrite", repo);
  strip_trailing_newline(expected);
  cr_assert_str_eq(history_ref, expected);
  free(expected);

  actual = read_command("git -C %s rev-parse '%s^{tree}'", repo, history_ref);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  expected = read_command("git -C %s rev-list --count refs/heads/main", repo);
  actual =
    read_command("git -C %s rev-list --count refs/heads/bbfg-rewrite", repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  actual = read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  cr_assert_eq(run_command("git -C %s branch target", repo), 0);
  char* target_before =
    read_command("git -C %s rev-parse refs/heads/target", repo);
  strip_trailing_newline(target_before);

  char* rewritten_target = read_command(
    "%s --rewrite-ref refs/heads/target --delete dir/nested.txt %s",
    bbfg,
    repo);
  strip_trailing_newline(rewritten_target);
  expected = read_command("git -C %s rev-parse refs/heads/target", repo);
  strip_trailing_newline(expected);
  cr_assert_str_eq(rewritten_target, expected);
  cr_assert_str_neq(rewritten_target, target_before);
  free(expected);

  actual =
    read_command("git -C %s rev-parse '%s^{tree}'", repo, rewritten_target);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  actual = read_command("git -C %s rev-parse HEAD", repo);
  cr_assert_str_eq(actual, expected_head);
  free(actual);

  char* rewritten_refs =
    read_command("%s --rewrite-refs --delete dir/nested.txt %s", bbfg, repo);
  expected =
    read_command("git -C %s for-each-ref --format='%%(refname) %%(objectname)' "
                 "refs/heads refs/tags",
                 repo);
  cr_assert_str_eq(rewritten_refs, expected);
  free(expected);

  actual = read_command("git -C %s rev-parse 'HEAD^{tree}'", repo);
  cr_assert_str_eq(actual, expected_removed_path);
  free(actual);

  expected =
    read_command("git -C %s for-each-ref --format='%%(refname)' refs/heads "
                 "refs/tags",
                 repo);
  actual = read_command("%s --list-rewrite-refs %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  expected = read_command(
    "git -C %s rev-list --topo-order --reverse --branches --tags", repo);
  actual = read_command("%s --walk-rewrite-commits %s", bbfg, repo);
  cr_assert_str_eq(actual, expected);
  free(actual);
  free(expected);

  free(expected_removed_path);
  free(expected_head);
  free(rewrite_ref);
  free(history_ref);
  free(target_before);
  free(rewritten_target);
  free(rewritten_refs);

  cr_assert_eq(run_command("rm -rf %s", tmpdir), 0);
}

// NOLINTEND(readability-function-cognitive-complexity)
// NOLINTEND(bugprone-easily-swappable-parameters)
// NOLINTEND(bugprone-command-processor)
