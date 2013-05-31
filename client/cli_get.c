/*
 * Copyright (C) SUSE LINUX Products GmbH 2012, all rights reserved.
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

#include <curl/curl.h>
#include <apr-1/apr_xml.h>

#include "ccan/list/list.h"
#include "lib/azure_xml.h"
#include "lib/azure_req.h"
#include "lib/conn.h"
#include "lib/azure_ssl.h"
#include "cli_common.h"
#include "cli_sign.h"
#include "cli_get.h"

void
cli_get_args_free(struct cli_args *cli_args)
{
	free(cli_args->az.blob_acc);
	free(cli_args->az.ctnr_name);
	free(cli_args->az.blob_name);
	free(cli_args->get.local_path);
}

int
cli_get_args_parse(int argc,
		   char * const *argv,
		   struct cli_args *cli_args)
{
	int ret;

	ret = cli_args_path_parse(cli_args->progname, cli_args->flags,
				  argv[1],
				  &cli_args->az.blob_acc,
				  &cli_args->az.ctnr_name,
				  &cli_args->az.blob_name);
	if (ret < 0)
		goto err_out;

	if (cli_args->az.blob_name == NULL) {
		cli_args_usage(cli_args->progname, cli_args->flags,
		   "Invalid remote path, must be <account>/<container>/<blob>");
		ret = -EINVAL;
		goto err_ctnr_free;
	}

	cli_args->get.local_path = strdup(argv[2]);
	if (cli_args->get.local_path == NULL) {
		ret = -ENOMEM;
		goto err_blob_free;
	}

	cli_args->cmd = CLI_CMD_GET;
	return 0;

err_blob_free:
	free(cli_args->az.blob_name);
err_ctnr_free:
	free(cli_args->az.ctnr_name);
err_out:
	return ret;
}

int
cli_get_handle(struct cli_args *cli_args)
{
	struct elasto_conn *econn;
	struct stat st;
	struct azure_op op;
	int ret;

	if (cli_args->type == CLI_TYPE_AZURE) {
		ret = elasto_conn_init_az(cli_args->az.pem_file, NULL, &econn);
	} else {
		ret = -ENOTSUP;
	}
	if (ret < 0) {
		goto err_out;
	}

	ret = cli_sign_conn_setup(econn,
				  cli_args->az.blob_acc,
				  cli_args->az.sub_id);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = stat(cli_args->get.local_path, &st);
	if (ret == 0) {
		printf("destination already exists at %s\n",
		       cli_args->get.local_path);
		goto err_conn_free;
	}
	memset(&op, 0, sizeof(op));
	printf("getting container %s blob %s for %s\n",
	       cli_args->az.ctnr_name,
	       cli_args->az.blob_name,
	       cli_args->get.local_path);

	ret = azure_op_blob_get(cli_args->az.blob_acc,
				cli_args->az.ctnr_name,
				cli_args->az.blob_name,
				false,
				AOP_DATA_FILE,
				(uint8_t *)cli_args->get.local_path,
				0, 0,
				cli_args->insecure_http,
				&op);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = elasto_conn_send_op(econn, &op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = azure_rsp_process(&op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = 0;
err_op_free:
	/* data buffer contains cli_args->get.local_path */
	if (op.rsp.data)
		op.rsp.data->buf = NULL;
	azure_op_free(&op);
err_conn_free:
	elasto_conn_free(econn);
err_out:
	return ret;
}
