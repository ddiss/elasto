/*
 * Copyright (C) SUSE LINUX GmbH 2017, all rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include "ccan/list/list.h"
#include "dbg.h"
#include "base64.h"
#include "util.h"
#include "exml.h"
#include "data.h"
#include "op.h"
#include "sign.h"
#include "web_path.h"
#include "web_req.h"

/*
 * primary Elasto-Backend Op structure for web download requests
 */
struct web_ebo {
	enum web_opcode opcode;
	struct web_req req;
	struct web_rsp rsp;
	struct op op;
};

static void
web_ebo_free(struct op *op)
{
	struct web_ebo *ebo = container_of(op, struct web_ebo, op);

	free(ebo);
}

static int
web_ebo_init(enum web_opcode opcode,
	     void (*req_free)(struct op *op),
	     void (*rsp_free)(struct op *op),
	     int (*rsp_process)(struct op *op),
	     struct web_ebo **_ebo)
{
	struct web_ebo *ebo;

	ebo = malloc(sizeof(*ebo));
	if (ebo == NULL) {
		return -ENOMEM;
	}
	memset(ebo, 0, sizeof(*ebo));
	ebo->opcode = opcode;
	op_init(opcode, &ebo->op);

	ebo->op.req_free = req_free;
	ebo->op.rsp_free = rsp_free;
	ebo->op.rsp_process = rsp_process;
	ebo->op.rsp_error_process = op_rsp_error_process;
	ebo->op.ebo_free = web_ebo_free;
	*_ebo = ebo;
	return 0;
}

/* no need for anything complex - dl_path already includes query string */
static int
web_req_url_encode(const struct web_path *path,
		   char **_url_host,
		   char **_url_path)
{
	int ret;
	struct web_path dup_path;

	ret = web_path_dup(path, &dup_path);
	if (ret < 0) {
		return ret;
	}

	*_url_host = dup_path.host;
	*_url_path = dup_path.dl_path;
	return 0;
}

static int
web_req_fill_hdr_common(struct op *op)
{
	int ret;
	size_t sz;
	char hdr_buf[100];
	time_t t;
	struct tm tm_gmt;

	time(&t);
	gmtime_r(&t, &tm_gmt);
	sz = strftime(hdr_buf, ARRAY_SIZE(hdr_buf),
		      "%a, %d %b %Y %T %z", &tm_gmt);
	if (sz == 0) {
		return -E2BIG;
	}

	ret = op_req_hdr_add(op, "Date", hdr_buf);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

static int
web_req_dl_get_hdr_fill(struct web_req_dl_get *dl_get_req,
			struct op *op)
{
	int ret;
	char *hdr_str;

	ret = web_req_fill_hdr_common(op);
	if (ret < 0) {
		goto err_out;
	}

	if (dl_get_req->len > 0) {
		ret = asprintf(&hdr_str, "bytes=%" PRIu64 "-%" PRIu64,
			       dl_get_req->off,
			       (dl_get_req->off + dl_get_req->len - 1));
		if (ret < 0) {
			ret = -ENOMEM;
			goto err_hdrs_free;
		}
		ret = op_req_hdr_add(op, "Range", hdr_str);
		free(hdr_str);
		if (ret < 0) {
			goto err_hdrs_free;
		}
	}

	return 0;

err_hdrs_free:
	op_hdrs_free(&op->req.hdrs);
err_out:
	return ret;
}

/*
 * If @src_len is zero then ignore @src_off and retrieve entire blob
 */
int
web_req_dl_get(const struct web_path *path,
	       uint64_t src_off,
	       uint64_t src_len,
	       struct elasto_data *dest_data,
	       struct op **_op)
{
	int ret;
	struct web_ebo *ebo;
	struct op *op;
	struct web_req_dl_get *dl_get_req;

	if ((dest_data == NULL) || (dest_data->type == ELASTO_DATA_NONE)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = web_ebo_init(WEBOP_OBJ_GET,
			   NULL,	/* req free */
			   NULL,	/* rsp free */
			   NULL,	/* rsp process */
			   &ebo);
	if (ret < 0) {
		goto err_out;
	}

	op = &ebo->op;
	dl_get_req = &ebo->req.dl_get;

	if (src_len > 0) {
		/* retrieve a specific range */
		dl_get_req->off = src_off;
		dl_get_req->len = src_len;
	}

	if (dest_data == NULL) {
		dbg(3, "no recv buffer, allocating on arrival\n");
	}
	op->rsp.data = dest_data;
	/* TODO add a foreign flag so @req.data is not freed with @op */

	op->method = REQ_METHOD_GET;

	/* no need to dup path, as it's tracked via url_host and url_path */
	ret = web_req_url_encode(path, &op->url_host, &op->url_path);
	if (ret < 0) {
		goto err_data_close;
	}

	ret = web_req_dl_get_hdr_fill(dl_get_req, op);
	if (ret < 0) {
		goto err_url_free;
	}

	*_op = op;
	return 0;
err_url_free:
	free(op->url_path);
	free(op->url_host);
err_data_close:
	op->req.data = NULL;
	free(ebo);
err_out:
	return ret;
}

static int
web_rsp_dl_head_process(struct op *op)
{
	int ret;
	struct web_ebo *ebo = container_of(op, struct web_ebo, op);
	struct web_rsp_dl_head *dl_head_rsp = &ebo->rsp.dl_head;

	assert(op->opcode == WEBOP_OBJ_HEAD);

	ret = op_hdr_u64_val_lookup(&op->rsp.hdrs, "Content-Length",
				    &dl_head_rsp->len);
	if (ret < 0) {
		dbg(0, "no clen response header\n");
		goto err_out;
	}

	ret = op_hdr_val_lookup(&op->rsp.hdrs, "Content-Type",
				&dl_head_rsp->content_type);
	if (ret < 0) {
		dbg(0, "no ctype response header\n");
		goto err_out;
	}

	ret = 0;
err_out:
	return ret;
}

static void
web_rsp_dl_head_free(struct op *op)
{
	struct web_ebo *ebo = container_of(op, struct web_ebo, op);
	struct web_rsp_dl_head *dl_head_rsp = &ebo->rsp.dl_head;
	free(dl_head_rsp->content_type);
}

int
web_req_dl_head(const struct web_path *path,
		struct op **_op)
{
	int ret;
	struct web_ebo *ebo;
	struct op *op;

	ret = web_ebo_init(WEBOP_OBJ_HEAD,
			   NULL,	/* req free */
			   web_rsp_dl_head_free,
			   web_rsp_dl_head_process,
			   &ebo);
	if (ret < 0) {
		goto err_out;
	}

	op = &ebo->op;
	op->method = REQ_METHOD_HEAD;

	ret = web_req_url_encode(path, &op->url_host, &op->url_path);
	if (ret < 0) {
		goto err_ebo_free;
	}

	ret = web_req_fill_hdr_common(op);
	if (ret < 0) {
		goto err_url_free;
	}

	*_op = op;
	return 0;
err_url_free:
	free(op->url_path);
	free(op->url_host);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

struct web_rsp_dl_head *
web_rsp_dl_head(struct op *op)
{
	struct web_ebo *ebo = container_of(op, struct web_ebo, op);
	return &ebo->rsp.dl_head;
}
