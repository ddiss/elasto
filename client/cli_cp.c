/*
 * Copyright (C) SUSE LINUX GmbH 2013-2016, all rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>

#include "lib/file/file_api.h"
#include "ccan/list/list.h"
#include "cli_common.h"

void
cli_cp_args_free(struct cli_args *cli_args)
{
	free(cli_args->cp.src_path);
	free(cli_args->path);
}

int
cli_cp_args_parse(int argc,
		  char * const *argv,
		  struct cli_args *cli_args)
{
	int ret;

	if ((cli_args->auth.type != ELASTO_FILE_ABB)
	 && (cli_args->auth.type != ELASTO_FILE_APB)
	 && (cli_args->auth.type != ELASTO_FILE_AFS)
	 && (cli_args->auth.type != ELASTO_FILE_S3)) {
		ret = -ENOTSUP;
		goto err_out;
	}

	/* paths parsed by libfile on open */
	ret = cli_path_realize(cli_args->cwd, argv[1], &cli_args->cp.src_path);
	if (ret < 0) {
		goto err_out;
	}

	ret = cli_path_realize(cli_args->cwd, argv[2], &cli_args->path);
	if (ret < 0) {
		goto err_src_free;
	}

	return 0;

err_src_free:
	free(cli_args->cp.src_path);
err_out:
	return ret;
}

int
cli_cp_handle(struct cli_args *cli_args)
{
	struct elasto_fh *src_fh;
	struct elasto_fh *dest_fh;
	struct elasto_fstat fstat;
	int ret;

	/* open source without create or dir flags */
	ret = elasto_fopen(&cli_args->auth, cli_args->cp.src_path, 0, NULL,
			   &src_fh);
	if (ret < 0) {
		printf("%s path open failed with: %s\n",
		       cli_args->cp.src_path, strerror(-ret));
		goto err_out;
	}

	/* stat to determine size to copy */
	ret = elasto_fstat(src_fh, &fstat);
	if (ret < 0) {
		printf("stat failed with: %s\n", strerror(-ret));
		goto err_src_close;
	}

	/* open dest with create flag */
	ret = elasto_fopen(&cli_args->auth, cli_args->path, ELASTO_FOPEN_CREATE,
			   NULL, &dest_fh);
	if (ret < 0) {
		printf("%s path open failed with: %s\n",
		       cli_args->path, strerror(-ret));
		goto err_src_close;
	}

	printf("copying %" PRIu64 " bytes from %s to %s\n",
	       fstat.size, cli_args->cp.src_path, cli_args->path);

	ret = elasto_fsplice(src_fh, 0, dest_fh, 0, fstat.size);
	if (ret < 0) {
		printf("copy failed with: %s\n", strerror(-ret));
		goto err_dest_close;
	}

	ret = 0;
err_dest_close:
	if (elasto_fclose(dest_fh) < 0) {
		printf("dest close failed\n");
	}
err_src_close:
	if (elasto_fclose(src_fh) < 0) {
		printf("src close failed\n");
	}
err_out:
	return ret;
}

static struct cli_cmd_spec spec = {
	.name = "cp",
	.az_help = "<src_acc>/<src_ctnr>/<src_blob> "
		   "<dst_acc>/<dst_ctnr>/<dst_blob>",
	.afs_help = "<src_acc>/<src_share>/<src_file_path> "
		    "<dst_acc>/<dst_share>/<dst_file_path>",
	.s3_help = "<bucket>/<object> <bucket>/<object>",
	.arg_min = 2,
	.arg_max = 2,
	.args_parse = &cli_cp_args_parse,
	.handle = &cli_cp_handle,
	.args_free = &cli_cp_args_free,
	.feature_flags = CLI_FL_PROMPT | CLI_FL_BIN_ARG
			| CLI_FL_AZ | CLI_FL_AFS | CLI_FL_S3,
};

static cli_cmd_init cli_cp_init(void)
{
	cli_cmd_register(&spec);
}

static cli_cmd_deinit cli_cp_deinit(void)
{
	cli_cmd_unregister(&spec);
}
