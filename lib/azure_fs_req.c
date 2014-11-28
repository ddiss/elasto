/*
 * Copyright (C) SUSE LINUX Products GmbH 2012-2014, all rights reserved.
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
#include "exml.h"
#include "data_api.h"
#include "op.h"
#include "sign.h"
#include "azure_req.h"
#include "azure_fs_req.h"

/*
 * primary Elasto-Backend Op structure for Azure File Service requests
 */
struct az_fs_ebo {
	enum az_fs_opcode opcode;
	struct az_fs_req req;
	struct az_fs_rsp rsp;
	struct op op;
};

static void
az_fs_req_free(struct op *op);
static void
az_fs_rsp_free(struct op *op);
static int
az_fs_rsp_process(struct op *op);

static void
az_fs_ebo_free(struct op *op)
{
	struct az_fs_ebo *ebo = container_of(op, struct az_fs_ebo, op);

	free(ebo);
}

static int
az_fs_ebo_init(enum az_fs_opcode opcode,
	       struct az_fs_ebo **_ebo)
{
	struct az_fs_ebo *ebo;

	ebo = malloc(sizeof(*ebo));
	if (ebo == NULL) {
		return -ENOMEM;
	}
	memset(ebo, 0, sizeof(*ebo));
	ebo->opcode = opcode;
	op_init(opcode, &ebo->op);

	ebo->op.req_free = az_fs_req_free;
	ebo->op.rsp_free = az_fs_rsp_free;
	ebo->op.rsp_process = az_fs_rsp_process;
	ebo->op.ebo_free = az_fs_ebo_free;
	/* sign callback set conditionally per-op */
	*_ebo = ebo;
	return 0;
}

static void
az_fs_req_share_create_free(struct az_fs_req_share_create *share_create_req)
{
	free(share_create_req->acc);
	free(share_create_req->share);
}

int
az_fs_req_share_create(const char *acc,
		       const char *share,
		       struct op **_op)
{
	int ret;
	struct az_fs_ebo *ebo;
	struct op *op;
	struct az_fs_req_share_create *share_create_req;

	if ((acc == NULL) || (share == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = az_fs_ebo_init(AOP_FS_SHARE_CREATE, &ebo);
	if (ret < 0) {
		goto err_out;
	}
	op = &ebo->op;
	share_create_req = &ebo->req.share_create;

	share_create_req->acc = strdup(acc);
	if (share_create_req->acc == NULL) {
		ret = -ENOMEM;
		goto err_ebo_free;
	}
	share_create_req->share = strdup(share);
	if (share_create_req->share == NULL) {
		ret = -ENOMEM;
		goto err_acc_free;
	}

	op->method = REQ_METHOD_PUT;
	ret = asprintf(&op->url_host,
		       "%s.file.core.windows.net", acc);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_share_free;
	}
	ret = asprintf(&op->url_path, "/%s?restype=share",
		       share);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_uhost_free;
	}

	ret = az_req_common_hdr_fill(op, false);
	if (ret < 0) {
		goto err_upath_free;
	}

	op->req_sign = az_req_sign;

	*_op = op;
	return 0;

err_upath_free:
	free(op->url_path);
err_uhost_free:
	free(op->url_host);
err_share_free:
	free(share_create_req->share);
err_acc_free:
	free(share_create_req->acc);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

static void
az_fs_req_share_del_free(struct az_fs_req_share_del *share_del_req)
{
	free(share_del_req->acc);
	free(share_del_req->share);
}

int
az_fs_req_share_del(const char *acc,
		    const char *share,
		    struct op **_op)
{
	int ret;
	struct az_fs_ebo *ebo;
	struct op *op;
	struct az_fs_req_share_del *share_del_req;

	if ((acc == NULL) || (share == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = az_fs_ebo_init(AOP_FS_SHARE_DEL, &ebo);
	if (ret < 0) {
		goto err_out;
	}
	op = &ebo->op;
	share_del_req = &ebo->req.share_del;

	share_del_req->acc = strdup(acc);
	if (share_del_req->acc == NULL) {
		ret = -ENOMEM;
		goto err_ebo_free;
	}
	share_del_req->share = strdup(share);
	if (share_del_req->share == NULL) {
		ret = -ENOMEM;
		goto err_acc_free;
	}

	op->method = REQ_METHOD_DELETE;
	ret = asprintf(&op->url_host,
		       "%s.file.core.windows.net", acc);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_share_free;
	}
	ret = asprintf(&op->url_path, "/%s?restype=share",
		       share);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_uhost_free;
	}

	ret = az_req_common_hdr_fill(op, false);
	if (ret < 0) {
		goto err_upath_free;
	}

	op->req_sign = az_req_sign;

	*_op = op;
	return 0;

err_upath_free:
	free(op->url_path);
err_uhost_free:
	free(op->url_host);
err_share_free:
	free(share_del_req->share);
err_acc_free:
	free(share_del_req->acc);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

static void
az_fs_req_dirs_files_list_free(
			struct az_fs_req_dirs_files_list *dirs_files_list_req)
{
	free(dirs_files_list_req->acc);
	free(dirs_files_list_req->share);
	free(dirs_files_list_req->dir_path);
}

static void
az_fs_ent_free(struct az_fs_ent **pent)
{
	struct az_fs_ent *ent = *pent;

	if (ent->type == AZ_FS_ENT_TYPE_FILE) {
		free(ent->file.name);
	} else if (ent->type == AZ_FS_ENT_TYPE_DIR) {
		free(ent->dir.name);
	}
	free(ent);
}

static void
az_fs_rsp_dirs_files_list_free(struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp)
{
	struct az_fs_ent *ent;
	struct az_fs_ent *ent_n;

	if (dirs_files_list_rsp->num_ents == 0) {
		return;
	}

	list_for_each_safe(&dirs_files_list_rsp->ents, ent, ent_n, list) {
		az_fs_ent_free(&ent);
	}
}

int
az_fs_req_dirs_files_list(const char *acc,
			  const char *share,
			  const char *dir_path,
			  struct op **_op)
{
	int ret;
	struct az_fs_ebo *ebo;
	struct op *op;
	struct az_fs_req_dirs_files_list *dirs_files_list_req;

	if ((acc == NULL) || (share == NULL) || (dir_path == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = az_fs_ebo_init(AOP_FS_DIRS_FILES_LIST, &ebo);
	if (ret < 0) {
		goto err_out;
	}
	op = &ebo->op;
	dirs_files_list_req = &ebo->req.dirs_files_list;

	dirs_files_list_req->acc = strdup(acc);
	if (dirs_files_list_req->acc == NULL) {
		ret = -ENOMEM;
		goto err_ebo_free;
	}

	dirs_files_list_req->share = strdup(share);
	if (dirs_files_list_req->share == NULL) {
		ret = -ENOMEM;
		goto err_acc_free;
	}

	dirs_files_list_req->dir_path = strdup(dir_path);
	if (dirs_files_list_req->dir_path == NULL) {
		ret = -ENOMEM;
		goto err_share_free;
	}

	op->method = REQ_METHOD_GET;
	op->url_https_only = false;	/* TODO check! */
	ret = asprintf(&op->url_host,
		       "%s.file.core.windows.net", acc);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_path_free;
	}
	ret = asprintf(&op->url_path, "/%s/%s?restype=directory&comp=list",
		       share, dir_path);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_uhost_free;
	}

	ret = az_req_common_hdr_fill(op, false);
	if (ret < 0) {
		goto err_upath_free;
	}

	op->req_sign = az_req_sign;

	*_op = op;
	return 0;
err_upath_free:
	free(op->url_path);
err_uhost_free:
	free(op->url_host);
err_path_free:
	free(dirs_files_list_req->dir_path);
err_share_free:
	free(dirs_files_list_req->share);
err_acc_free:
	free(dirs_files_list_req->acc);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

static int
az_fs_rsp_ent_file_iter_process(struct xml_doc *xdoc,
				const char *path,
				const char *val,
				void *cb_data)
{
	int ret;
	struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp
				= (struct az_fs_rsp_dirs_files_list *)cb_data;
	struct az_fs_ent *ent;

	/* request callback for subsequent file descriptors */
	ret = exml_path_cb_want(xdoc,
				"/EnumerationResults/Entries/File", false,
				az_fs_rsp_ent_file_iter_process,
				dirs_files_list_rsp, NULL);
	if (ret < 0) {
		goto err_out;
	}

	ent = malloc(sizeof(*ent));
	if (ent == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}
	memset(ent, 0, sizeof(*ent));
	ent->type = AZ_FS_ENT_TYPE_FILE;

	ret = exml_str_want(xdoc, "./Name", true, &ent->file.name, NULL);
	if (ret < 0) {
		goto err_ent_free;
	}

	ret = exml_uint64_want(xdoc, "./Properties/Content-Length", true,
			       &ent->file.size, NULL);
	if (ret < 0) {
		goto err_ent_free;
	}

	list_add_tail(&dirs_files_list_rsp->ents, &ent->list);
	dirs_files_list_rsp->num_ents++;

	return 0;

err_ent_free:
	free(ent);
err_out:
	return ret;
}

static int
az_fs_rsp_ent_dir_iter_process(struct xml_doc *xdoc,
			       const char *path,
			       const char *val,
			       void *cb_data)
{
	int ret;
	struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp
				= (struct az_fs_rsp_dirs_files_list *)cb_data;
	struct az_fs_ent *ent;

	/* request callback for subsequent dir descriptors */
	ret = exml_path_cb_want(xdoc,
				"/EnumerationResults/Entries/Directory", false,
				az_fs_rsp_ent_dir_iter_process,
				dirs_files_list_rsp, NULL);
	if (ret < 0) {
		goto err_out;
	}

	ent = malloc(sizeof(*ent));
	if (ent == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}
	memset(ent, 0, sizeof(*ent));
	ent->type = AZ_FS_ENT_TYPE_DIR;

	ret = exml_str_want(xdoc, "./Name", true, &ent->file.name, NULL);
	if (ret < 0) {
		goto err_ent_free;
	}

	list_add_tail(&dirs_files_list_rsp->ents, &ent->list);
	dirs_files_list_rsp->num_ents++;

	return 0;

err_ent_free:
	free(ent);
err_out:
	return ret;
}

static int
az_fs_rsp_dirs_files_list_process(struct op *op,
			struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp)
{
	int ret;
	struct xml_doc *xdoc;
	struct az_fs_ent *ent;
	struct az_fs_ent *ent_n;

	assert(op->opcode == AOP_FS_DIRS_FILES_LIST);
	assert(op->rsp.data->type == ELASTO_DATA_IOV);

	assert(op->rsp.data->base_off == 0);
	ret = exml_slurp((const char *)op->rsp.data->iov.buf,
			 op->rsp.data->off, &xdoc);
	if (ret < 0) {
		goto err_out;
	}

	list_head_init(&dirs_files_list_rsp->ents);

	/* request callback for first file descriptor */
	ret = exml_path_cb_want(xdoc,
				"/EnumerationResults/Entries/File", false,
				az_fs_rsp_ent_file_iter_process,
				dirs_files_list_rsp, NULL);
	if (ret < 0) {
		goto err_xdoc_free;
	}

	ret = exml_path_cb_want(xdoc,
				"/EnumerationResults/Entries/Directory", false,
				az_fs_rsp_ent_dir_iter_process,
				dirs_files_list_rsp, NULL);
	if (ret < 0) {
		goto err_xdoc_free;
	}

	ret = exml_parse(xdoc);
	if (ret < 0) {
		/* need to walk list in case cb fired */
		goto err_ents_free;
	}

	exml_free(xdoc);
	return 0;

err_ents_free:
	list_for_each_safe(&dirs_files_list_rsp->ents, ent, ent_n, list) {
		az_fs_ent_free(&ent);
	}
err_xdoc_free:
	exml_free(xdoc);
err_out:
	return ret;
}

static void
az_fs_req_dir_create_free(struct az_fs_req_dir_create *dir_create_req)
{
	free(dir_create_req->acc);
	free(dir_create_req->share);
	free(dir_create_req->parent_dir_path);
	free(dir_create_req->dir);
}

int
az_fs_req_dir_create(const char *acc,
		     const char *share,
		     const char *parent_dir_path,	/* optional */
		     const char *dir,
		     struct op **_op)
{
	int ret;
	struct az_fs_ebo *ebo;
	struct op *op;
	struct az_fs_req_dir_create *dir_create_req;

	if ((acc == NULL) || (share == NULL) || (dir == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = az_fs_ebo_init(AOP_FS_DIR_CREATE, &ebo);
	if (ret < 0) {
		goto err_out;
	}
	op = &ebo->op;
	dir_create_req = &ebo->req.dir_create;

	dir_create_req->acc = strdup(acc);
	if (dir_create_req->acc == NULL) {
		ret = -ENOMEM;
		goto err_ebo_free;
	}

	dir_create_req->share = strdup(share);
	if (dir_create_req->share == NULL) {
		ret = -ENOMEM;
		goto err_acc_free;
	}

	if (parent_dir_path != NULL) {
		dir_create_req->parent_dir_path = strdup(parent_dir_path);
		if (dir_create_req->parent_dir_path == NULL) {
			ret = -ENOMEM;
			goto err_share_free;
		}
	}

	dir_create_req->dir = strdup(dir);
	if (dir_create_req->dir == NULL) {
		ret = -ENOMEM;
		goto err_path_free;
	}

	op->method = REQ_METHOD_PUT;
	op->url_https_only = false;
	ret = asprintf(&op->url_host,
		       "%s.file.core.windows.net", acc);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_dir_free;
	}
	ret = asprintf(&op->url_path, "/%s/%s%s%s?restype=directory",
		       share,
		       (parent_dir_path ? parent_dir_path : ""),
		       (parent_dir_path ? "/" : ""), dir);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_uhost_free;
	}

	ret = az_req_common_hdr_fill(op, false);
	if (ret < 0) {
		goto err_upath_free;
	}

	op->req_sign = az_req_sign;

	*_op = op;
	return 0;
err_upath_free:
	free(op->url_path);
err_uhost_free:
	free(op->url_host);
err_dir_free:
	free(dir_create_req->dir);
err_path_free:
	free(dir_create_req->parent_dir_path);
err_share_free:
	free(dir_create_req->share);
err_acc_free:
	free(dir_create_req->acc);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

static void
az_fs_req_dir_del_free(struct az_fs_req_dir_del *dir_del_req)
{
	free(dir_del_req->acc);
	free(dir_del_req->share);
	free(dir_del_req->parent_dir_path);
	free(dir_del_req->dir);
}

int
az_fs_req_dir_del(const char *acc,
		  const char *share,
		  const char *parent_dir_path,	/* optional */
		  const char *dir,
		  struct op **_op)
{
	int ret;
	struct az_fs_ebo *ebo;
	struct op *op;
	struct az_fs_req_dir_del *dir_del_req;

	if ((acc == NULL) || (share == NULL) || (dir == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = az_fs_ebo_init(AOP_FS_DIR_DEL, &ebo);
	if (ret < 0) {
		goto err_out;
	}
	op = &ebo->op;
	dir_del_req = &ebo->req.dir_del;

	dir_del_req->acc = strdup(acc);
	if (dir_del_req->acc == NULL) {
		ret = -ENOMEM;
		goto err_ebo_free;
	}

	dir_del_req->share = strdup(share);
	if (dir_del_req->share == NULL) {
		ret = -ENOMEM;
		goto err_acc_free;
	}

	if (parent_dir_path != NULL) {
		dir_del_req->parent_dir_path = strdup(parent_dir_path);
		if (dir_del_req->parent_dir_path == NULL) {
			ret = -ENOMEM;
			goto err_share_free;
		}
	}

	dir_del_req->dir = strdup(dir);
	if (dir_del_req->dir == NULL) {
		ret = -ENOMEM;
		goto err_path_free;
	}

	op->method = REQ_METHOD_DELETE;
	op->url_https_only = false;
	ret = asprintf(&op->url_host,
		       "%s.file.core.windows.net", acc);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_dir_free;
	}
	ret = asprintf(&op->url_path, "/%s/%s%s%s?restype=directory",
		       share,
		       (parent_dir_path ? parent_dir_path : ""),
		       (parent_dir_path ? "/" : ""), dir);
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_uhost_free;
	}

	ret = az_req_common_hdr_fill(op, false);
	if (ret < 0) {
		goto err_upath_free;
	}

	op->req_sign = az_req_sign;

	*_op = op;
	return 0;
err_upath_free:
	free(op->url_path);
err_uhost_free:
	free(op->url_host);
err_dir_free:
	free(dir_del_req->dir);
err_path_free:
	free(dir_del_req->parent_dir_path);
err_share_free:
	free(dir_del_req->share);
err_acc_free:
	free(dir_del_req->acc);
err_ebo_free:
	free(ebo);
err_out:
	return ret;
}

static void
az_fs_req_free(struct op *op)
{
	struct az_fs_ebo *ebo = container_of(op, struct az_fs_ebo, op);

	switch (ebo->opcode) {
	case AOP_FS_SHARE_CREATE:
		az_fs_req_share_create_free(&ebo->req.share_create);
		break;
	case AOP_FS_SHARE_DEL:
		az_fs_req_share_del_free(&ebo->req.share_del);
		break;
	case AOP_FS_DIRS_FILES_LIST:
		az_fs_req_dirs_files_list_free(&ebo->req.dirs_files_list);
		break;
	case AOP_FS_DIR_CREATE:
		az_fs_req_dir_create_free(&ebo->req.dir_create);
		break;
	case AOP_FS_DIR_DEL:
		az_fs_req_dir_del_free(&ebo->req.dir_del);
		break;
	default:
		assert(false);
		break;
	};
}

static void
az_fs_rsp_free(struct op *op)
{
	struct az_fs_ebo *ebo = container_of(op, struct az_fs_ebo, op);

	switch (ebo->opcode) {
	case AOP_FS_DIRS_FILES_LIST:
		az_fs_rsp_dirs_files_list_free(&ebo->rsp.dirs_files_list);
		break;
	case AOP_FS_SHARE_CREATE:
	case AOP_FS_SHARE_DEL:
	case AOP_FS_DIR_CREATE:
	case AOP_FS_DIR_DEL:
		/* nothing to do */
		break;
	default:
		assert(false);
		break;
	};
}

/*
 * unmarshall response data
 */
int
az_fs_rsp_process(struct op *op)
{
	int ret;
	struct az_fs_ebo *ebo = container_of(op, struct az_fs_ebo, op);

	ret = op_hdr_val_lookup(&op->rsp.hdrs, "x-ms-request-id",
				&op->rsp.req_id);
	if (ret < 0) {
		dbg(0, "no req_id in %d response\n", op->opcode);
	} else {
		dbg(4, "req_id in %d response: %s\n",
		    op->opcode, op->rsp.req_id);
	}

	switch (op->opcode) {
	case AOP_FS_DIRS_FILES_LIST:
		ret = az_fs_rsp_dirs_files_list_process(op,
						&ebo->rsp.dirs_files_list);
		break;
	case AOP_FS_SHARE_CREATE:
	case AOP_FS_SHARE_DEL:
	case AOP_FS_DIR_CREATE:
	case AOP_FS_DIR_DEL:
		/* nothing to do */
		ret = 0;
		break;
	default:
		assert(false);
		break;
	};

	return ret;
}

struct az_fs_rsp_dirs_files_list *
az_fs_rsp_dirs_files_list(struct op *op)
{
	struct az_fs_ebo *ebo = container_of(op, struct az_fs_ebo, op);
	return &ebo->rsp.dirs_files_list;
}
