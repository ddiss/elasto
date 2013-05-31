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
#ifndef _CLI_COMMON_H_
#define _CLI_COMMON_H_

enum cli_cmd {
	CLI_CMD_NONE = 0,
	CLI_CMD_LS,
	CLI_CMD_PUT,
	CLI_CMD_GET,
	CLI_CMD_DEL,
	CLI_CMD_CREATE,
	CLI_CMD_EXIT,
};

enum cli_type {
	CLI_TYPE_AZURE = 1,
	CLI_TYPE_S3,
};

/*
 * @CLI_FL_BIN_ARG:	run as argument to binary
 * @CLI_FL_PROMPT:	run from elasto prompt
 */
enum cli_fl {
	CLI_FL_BIN_ARG	= 0x00000001,
	CLI_FL_PROMPT	= 0x00000002,
};

/*
 * @feature_fl: features available to this instance
 */
struct cli_args {
	enum cli_type type;
	enum cli_fl flags;
	union {
		struct {
			char *ps_file;
			char *pem_file;
			char *sub_name;
			char *sub_id;
			char *blob_acc;
			char *ctnr_name;
			char *blob_name;
		} az;
		struct {
			char *key_id;
			char *secret;
			char *bkt_name;
			char *obj_name;
		} s3;
	};
	bool insecure_http;
	enum cli_cmd cmd;
	union {
		struct {
		} ls;
		struct {
			char *local_path;
		} put;
		struct {
			char *local_path;
		} get;
		struct {
		} del;
		struct {
			char *label;
			char *desc;
			char *affin_grp;
			char *location;
		} create;
	};
};

int
cli_args_path_parse(const char *progname,
		    const char *path,
		    char **comp1_out,
		    char **comp2_out,
		    char **comp3_out);

void
cli_args_usage(const char *progname,
	       const char *msg);

#endif /* ifdef _CLI_COMMON_H_ */
