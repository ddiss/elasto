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
#include "cli_del.h"

void
cli_del_args_free(struct cli_args *cli_args)
{
	free(cli_args->az.blob_acc);
	free(cli_args->az.ctnr_name);
	free(cli_args->az.blob_name);
}

int
cli_del_args_parse_az(int argc,
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

	if (cli_args->az.blob_acc == NULL) {
		cli_args_usage(cli_args->progname, cli_args->flags,
			       "Invalid remote path, must be "
			       "<account>[/<container>[/<blob>]]");
		ret = -EINVAL;
		goto err_ctnr_free;
	}

	cli_args->cmd = CLI_CMD_DEL;
	return 0;

err_ctnr_free:
	free(cli_args->az.ctnr_name);
err_out:
	return ret;
}

int
cli_del_args_parse_s3(int argc,
		      char * const *argv,
		      struct cli_args *cli_args)
{
	int ret;

	ret = cli_args_path_parse(cli_args->progname, cli_args->flags,
				  argv[1],
				  &cli_args->s3.bkt_name,
				  NULL, NULL);
	if (ret < 0)
		goto err_out;

	if (cli_args->s3.bkt_name == NULL) {
		cli_args_usage(cli_args->progname, cli_args->flags,
			       "Invalid remote path, must be <bucket name>");
		ret = -EINVAL;
		goto err_bkt_free;
	}

	cli_args->cmd = CLI_CMD_DEL;
	return 0;

err_bkt_free:
	free(cli_args->s3.bkt_name);
err_out:
	return ret;
}

int
cli_del_args_parse(int argc,
		   char * const *argv,
		   struct cli_args *cli_args)
{
	if (cli_args->type == CLI_TYPE_AZURE) {
		return cli_del_args_parse_az(argc, argv, cli_args);
	} else if (cli_args->type == CLI_TYPE_S3) {
		return cli_del_args_parse_s3(argc, argv, cli_args);
	}
	return -ENOTSUP;
}

static int
cli_del_acc_handle(struct elasto_conn *econn,
		   const char *sub_id,
		   const char *acc_name)
{
	struct azure_op op;
	int ret;

	memset(&op, 0, sizeof(op));
	ret = azure_op_acc_del(sub_id,
			       acc_name, &op);
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_conn_send_op(econn, &op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = azure_rsp_process(&op);
	if (ret < 0) {
		goto err_op_free;
	}

	if (op.rsp.is_error) {
		ret = -EIO;
		printf("failed response: %d\n", op.rsp.err_code);
		goto err_op_free;
	}

	ret = 0;
err_op_free:
	azure_op_free(&op);
err_out:
	return ret;
}

static int
cli_del_blob_handle(struct elasto_conn *econn,
		   const char *acc_name,
		   const char *ctnr_name,
		   const char *blob_name)
{
	struct azure_op op;
	int ret;

	memset(&op, 0, sizeof(op));
	ret = azure_op_blob_del(acc_name,
				ctnr_name,
				blob_name, &op);
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_conn_send_op(econn, &op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = azure_rsp_process(&op);
	if (ret < 0) {
		goto err_op_free;
	}

	if (op.rsp.is_error) {
		ret = -EIO;
		printf("failed response: %d\n", op.rsp.err_code);
		goto err_op_free;
	}

	ret = 0;
err_op_free:
	azure_op_free(&op);
err_out:
	return ret;
}

static int
cli_del_ctnr_handle(struct elasto_conn *econn,
		    const char *acc_name,
		    const char *ctnr_name)
{
	struct azure_op op;
	int ret;

	memset(&op, 0, sizeof(op));

	ret = azure_op_ctnr_del(acc_name,
				ctnr_name, &op);
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_conn_send_op(econn, &op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = azure_rsp_process(&op);
	if (ret < 0) {
		goto err_op_free;
	}

	if (op.rsp.is_error) {
		ret = -EIO;
		printf("failed response: %d\n", op.rsp.err_code);
		goto err_op_free;
	}

	ret = 0;
err_op_free:
	azure_op_free(&op);
err_out:
	return ret;
}

static int
cli_del_bkt_handle(char *bkt_name,
		   bool insecure_http,
		   struct elasto_conn *econn)
{
	struct azure_op op;
	int ret;

	memset(&op, 0, sizeof(op));
	ret = s3_op_bkt_del(bkt_name, insecure_http, &op);
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_conn_send_op(econn, &op);
	if (ret < 0) {
		goto err_op_free;
	}

	ret = azure_rsp_process(&op);
	if (ret < 0) {
		goto err_op_free;
	}

	if (op.rsp.is_error) {
		ret = -EIO;
		printf("failed response: %d\n", op.rsp.err_code);
		goto err_op_free;
	}

	ret = 0;
err_op_free:
	azure_op_free(&op);
err_out:
	return ret;
}

int
cli_del_handle(struct cli_args *cli_args)
{
	struct elasto_conn *econn;
	int ret;

	if (cli_args->type == CLI_TYPE_AZURE) {
		ret = elasto_conn_init_az(cli_args->az.pem_file, NULL, &econn);
		if (ret < 0) {
			goto err_out;
		}
	} else if (cli_args->type == CLI_TYPE_S3) {
		ret = elasto_conn_init_s3(cli_args->s3.key_id,
					  cli_args->s3.secret, &econn);
		if (ret < 0) {
			goto err_out;
		}
		ret = cli_del_bkt_handle(cli_args->s3.bkt_name,
					 cli_args->insecure_http, econn);
		elasto_conn_free(econn);
		return ret;
	} else {
		ret = -ENOTSUP;
		goto err_out;
	}

	if ((cli_args->az.blob_name == NULL)
	 && (cli_args->az.ctnr_name == NULL)) {
		/* delete account for subscription, signing setup not needed */
		ret = cli_del_acc_handle(econn, cli_args->az.sub_id,
					 cli_args->az.blob_acc);
		elasto_conn_free(econn);
		return ret;
	}

	ret = cli_sign_conn_setup(econn,
				  cli_args->az.blob_acc,
				  cli_args->az.sub_id);
	if (ret < 0) {
		goto err_conn_free;
	}

	if (cli_args->az.blob_name != NULL) {
		ret = cli_del_blob_handle(econn, cli_args->az.blob_acc,
					  cli_args->az.ctnr_name,
					  cli_args->az.blob_name);
	} else {
		ret = cli_del_ctnr_handle(econn, cli_args->az.blob_acc,
					  cli_args->az.ctnr_name);
	}

err_conn_free:
	elasto_conn_free(econn);
err_out:
	return ret;
}
