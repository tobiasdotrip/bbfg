#ifndef BBFG_TEST_HELPERS_H
#define BBFG_TEST_HELPERS_H

#include <stddef.h>

#define BBFG_TEST_PATH_SIZE 4096

const char*
bbfg_test_path(void);

int
bbfg_test_run_command(const char* format, ...);

char*
bbfg_test_read_command(const char* format, ...);

void
bbfg_test_strip_trailing_newline(char* value);

char*
bbfg_test_format_string(const char* format, ...);

void
bbfg_test_init_repo(char* tmpdir,
                    size_t tmpdir_size,
                    char* repo,
                    size_t repo_size);

void
bbfg_test_cleanup(const char* tmpdir);

#endif
