#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>
#include "build.h"
#include "tests_compile.h"

/* Note: we already test safe_mode in run_tests.c */
static const char *can_run_coverage(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output;

	if (!run_command(m, &timeleft, &output, "gcov -h"))
		return talloc_asprintf(m, "No gcov support: %s", output);
	return NULL;
}

static void cov_compile(const void *ctx,
			unsigned int time_ms,
			struct manifest *m,
			struct ccan_file *file,
			bool link_with_module)
{
	char *flags = talloc_asprintf(ctx, "%s %s", cflags, COVERAGE_CFLAGS);

	file->compiled[COMPILE_COVERAGE] = temp_file(ctx, "", file->fullname);
	compile_and_link_async(file, time_ms, file->fullname, ccan_dir,
			       test_obj_list(m, link_with_module,
					     COMPILE_NORMAL,
					     COMPILE_COVERAGE),
			       compiler, flags,
			       lib_list(m, COMPILE_NORMAL),
			       file->compiled[COMPILE_COVERAGE]);
}

/* FIXME: Coverage from testable examples as well. */
static void do_compile_coverage_tests(struct manifest *m,
				      unsigned int *timeleft,
				      struct score *score)
{
	char *cmdout;
	struct ccan_file *i;
	struct list_head *h;
	bool ok;
	char *f = talloc_asprintf(score, "%s %s", cflags, COVERAGE_CFLAGS);

	/* For API tests, we need coverage version of module. */
	if (!list_empty(&m->api_tests)) {
		build_objects(m, score, f, COMPILE_COVERAGE);
		if (!score->pass) {
			score->error = talloc_strdup(score,
						     "Failed to compile module objects with coverage");
			return;
		}
	}

	foreach_ptr(h, &m->run_tests, &m->api_tests) {
		list_for_each(h, i, list) {
			cov_compile(m, *timeleft, m, i, h == &m->api_tests);
		}
	}

	while ((i = collect_command(&ok, &cmdout)) != NULL) {
		if (!ok) {
			score_file_error(score, i, 0,
					 "Failed to compile test with coverage:"
					 " %s", cmdout);
		}
	}
	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint tests_compile_coverage = {
	.key = "tests_compile_coverage",
	.name = "Module tests compile with " COVERAGE_CFLAGS,
	.check = do_compile_coverage_tests,
	.can_run = can_run_coverage,
	.needs = "tests_compile"
};

REGISTER_TEST(tests_compile_coverage);
