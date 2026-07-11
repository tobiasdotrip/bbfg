#include "test_helpers.h"

#include <criterion/criterion.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(bugprone-command-processor)

#define BBFG_TEST_COMMAND_SIZE 8192

typedef struct
{
  const char* path;
  const char* content;
} TestFile;

static void
format_command(char* command, size_t size, const char* format, va_list args)
{
  int length = vsnprintf(command, size, format, args);
  cr_assert(length >= 0);
  cr_assert((size_t)length < size);
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
read_stream(FILE* stream)
{
  size_t output_size = 0;
  char* output = malloc(1);
  cr_assert_not_null(output);
  output[0] = '\0';

  char chunk[1024];
  while (fgets(chunk, sizeof(chunk), stream) != NULL) {
    output = append_output(output, &output_size, chunk);
  }

  return output;
}

const char*
bbfg_test_path(void)
{
  const char* path = getenv("BBFG");
  cr_assert_not_null(path);
  return path;
}

int
bbfg_test_run_command(const char* format, ...)
{
  char command[BBFG_TEST_COMMAND_SIZE];
  va_list args;

  va_start(args, format);
  format_command(command, sizeof(command), format, args);
  va_end(args);

  return system(command);
}

char*
bbfg_test_read_command(const char* format, ...)
{
  char command[BBFG_TEST_COMMAND_SIZE];
  va_list args;

  va_start(args, format);
  format_command(command, sizeof(command), format, args);
  va_end(args);

  FILE* stream = popen(command, "r");
  cr_assert_not_null(stream);
  char* output = read_stream(stream);
  cr_assert_eq(pclose(stream), 0);
  return output;
}

void
bbfg_test_strip_trailing_newline(char* value)
{
  size_t length = strlen(value);
  if (length > 0 && value[length - 1] == '\n') {
    value[length - 1] = '\0';
  }
}

char*
bbfg_test_format_string(const char* format, ...)
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
write_file(const TestFile* file_spec)
{
  FILE* file = fopen(file_spec->path, "w");
  cr_assert_not_null(file);
  cr_assert_neq(fputs(file_spec->content, file), EOF);
  cr_assert_eq(fclose(file), 0);
}

static void
write_repo_file(const char* repo, TestFile file_spec)
{
  char path[BBFG_TEST_PATH_SIZE];
  int length = snprintf(path, sizeof(path), "%s/%s", repo, file_spec.path);
  cr_assert(length >= 0);
  cr_assert((size_t)length < sizeof(path));
  TestFile repo_file = { path, file_spec.content };
  write_file(&repo_file);
}

static void
configure_repo(const char* repo)
{
  cr_assert_eq(bbfg_test_run_command("git init -q %s", repo), 0);
  cr_assert_eq(bbfg_test_run_command("git -C %s checkout -q -b main", repo), 0);
  cr_assert_eq(
    bbfg_test_run_command("git -C %s config user.name 'bbfg test'", repo), 0);
  cr_assert_eq(bbfg_test_run_command(
                 "git -C %s config user.email bbfg@example.invalid", repo),
               0);
}

static void
create_initial_files(const char* repo)
{
  cr_assert_eq(bbfg_test_run_command("mkdir %s/dir %s/other", repo, repo), 0);
  write_repo_file(repo, (TestFile){ "file.txt", "hello\n" });
  write_repo_file(repo, (TestFile){ "dir/nested.txt", "nested\n" });
  write_repo_file(repo, (TestFile){ "other/nested.txt", "another nested\n" });

  cr_assert_eq(
    bbfg_test_run_command(
      "git -C %s add file.txt dir/nested.txt other/nested.txt", repo),
    0);
  cr_assert_eq(
    bbfg_test_run_command("git -C %s commit -q -m 'Initial commit'", repo), 0);
}

static void
create_second_commit(const char* repo)
{
  write_repo_file(repo, (TestFile){ "file.txt", "hello again\n" });
  cr_assert_eq(bbfg_test_run_command("git -C %s add file.txt", repo), 0);
  cr_assert_eq(
    bbfg_test_run_command("git -C %s commit -q -m 'Second commit'", repo), 0);
}

void
bbfg_test_init_repo(char* tmpdir,
                    size_t tmpdir_size,
                    char* repo,
                    size_t repo_size)
{
  char* created_dir =
    bbfg_test_read_command("mktemp -d /tmp/bbfg-criterion.XXXXXX");
  bbfg_test_strip_trailing_newline(created_dir);

  cr_assert(strlen(created_dir) < tmpdir_size);
  strcpy(tmpdir, created_dir);
  free(created_dir);

  int length = snprintf(repo, repo_size, "%s/repo", tmpdir);
  cr_assert(length >= 0);
  cr_assert((size_t)length < repo_size);

  configure_repo(repo);
  create_initial_files(repo);
  create_second_commit(repo);
}

void
bbfg_test_cleanup(const char* tmpdir)
{
  cr_assert_eq(bbfg_test_run_command("rm -rf %s", tmpdir), 0);
}

// NOLINTEND(bugprone-command-processor)
