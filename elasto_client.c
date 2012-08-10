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
 *
 * Author: David Disseldorp <ddiss@suse.de>
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
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "ccan/list/list.h"
#include "lib/azure_xml.h"
#include "lib/azure_req.h"
#include "lib/azure_conn.h"
#include "lib/azure_ssl.h"
#include "cli_common.h"
#include "cli_put.h"

void
cli_args_usage(const char *progname,
	       const char *msg)
{
	if (msg != NULL) {
		fprintf(stderr, "%s\n\n", msg);
	}
	fprintf(stderr, "usage: %s -s publish_settings "
				  "-a storage_account "
				  "[-l storage_location] [-g] "
				  "<cmd> <cmd args>\n\n"
		"-s publish_settings:	Azure PublishSettings file\n"
		"-a storage_account:	Storage account, created if needed\n"
		"-l storage_location:	Storage geographic location\n"
		"-g:			Enable geographic redundancy\n\n"
		"Commands:\n"
		"	put	<local path> <container>/<blob>\n",
		progname);
}

static void
cli_args_free(struct cli_args *cli_args)
{
	free(cli_args->ps_file);
	free(cli_args->blob_acc);

	if (cli_args->cmd == CLI_CMD_PUT) {
		cli_put_args_free(cli_args);
	}
}

static int
cli_cmd_parse(const char *progname,
	      int argc,
	      char * const *argv,
	      struct cli_args *cli_args)
{
	int ret;

	if (argc == 0) {
		cli_args_usage(progname, NULL);
		ret = -EINVAL;
		goto err_out;
	}

	if (!strcmp(argv[0], "put")) {
		ret = cli_put_args_parse(progname, argc, argv, cli_args);
		if (ret < 0) {
			goto err_out;
		}
	} else {
		cli_args_usage(progname, NULL);
		ret = -EINVAL;
		goto err_out;
	}

	ret = 0;
err_out:
	return ret;
}

static int
cli_args_parse(int argc,
	       char * const *argv,
	       struct cli_args *cli_args)
{
	int opt;
	int ret;
	extern char *optarg;
	extern int optind;
	char *pub_settings;
	char *store_acc = NULL;
	char *store_loc = NULL;	/* not yet supported */
	bool store_geo = false;	/* not yet supported */

	while ((opt = getopt(argc, argv, "s:a:l:g")) != -1) {
		switch (opt) {
		case 's':
			pub_settings = strdup(optarg);
			if (pub_settings == NULL) {
				ret = -ENOMEM;
				goto err_out;
			}
			break;
		case 'a':
			store_acc = strdup(optarg);
			if (store_acc == NULL) {
				ret = -ENOMEM;
				goto err_out;
			}
			break;
		case 'l':
			store_loc = strdup(optarg);
			if (store_loc == NULL) {
				ret = -ENOMEM;
				goto err_out;
			}
			break;
		case 'g':
			store_geo = true;
			break;
		default: /* '?' */
			cli_args_usage(argv[0], NULL);
			ret = -EINVAL;
			goto err_out;
			break;
		}
	}
	if ((pub_settings == NULL) || (store_acc == NULL)) {
		cli_args_usage(argv[0], NULL);
		ret = -EINVAL;
		goto err_out;
	}

	ret = cli_cmd_parse(argv[0], argc - optind, &argv[optind],
			    cli_args);
	if (ret < 0) {
		goto err_out;
	}

	cli_args->ps_file = pub_settings;
	cli_args->blob_acc = store_acc;

	return 0;
err_out:
	free(pub_settings);
	free(store_acc);
	free(store_loc);

	return ret;
}

int
main(int argc, char * const *argv)
{
	struct cli_args cli_args;
	struct azure_conn aconn;
	struct azure_op op;
	char *pem_file;
	char *sub_id;
	char *sub_name;
	int ret;

	memset(&cli_args, 0, sizeof(cli_args));

	ret = cli_args_parse(argc, argv, &cli_args);
	if (ret < 0) {
		goto err_out;
	}

	ret = azure_conn_subsys_init();
	if (ret < 0) {
		goto err_args_free;
	}
	azure_xml_subsys_init();

	memset(&op, 0, sizeof(op));

	ret = azure_ssl_pubset_process(cli_args.ps_file, &pem_file, &sub_id, &sub_name);
	if (ret < 0) {
		goto err_global_clean;
	}

	ret = azure_conn_init(pem_file, NULL, &aconn);
	if (ret < 0) {
		goto err_sub_info_free;
	}

	ret = azure_op_mgmt_get_sa_keys(sub_id, cli_args.blob_acc, &op);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = azure_conn_send_op(&aconn, &op);
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

	printf("primary key: %s\n"
	       "secondary key: %s\n",
	       op.rsp.mgmt_get_sa_keys.primary,
	       op.rsp.mgmt_get_sa_keys.secondary);

	ret = azure_conn_sign_setkey(&aconn, cli_args.blob_acc,
				     op.rsp.mgmt_get_sa_keys.primary);
	if (ret < 0) {
		goto err_op_free;
	}

	/* op freed later */

	if (cli_args.cmd == CLI_CMD_PUT) {
		ret = cli_put_handle(&aconn, &cli_args);
		if (ret < 0) {
			goto err_op_free;
		}
	}

	ret = 0;
err_op_free:
	azure_op_free(&op);
err_conn_free:
	azure_conn_free(&aconn);
err_sub_info_free:
	free(pem_file);
	free(sub_id);
	free(sub_name);
err_global_clean:
	azure_xml_subsys_deinit();
	azure_conn_subsys_deinit();
err_args_free:
	cli_args_free(&cli_args);
err_out:
	return ret;
}
