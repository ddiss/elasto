/*
 * Copyright (C) SUSE LINUX Products GmbH 2014, all rights reserved.
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
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <curl/curl.h>

#include "cm_test.h"
#include "ccan/list/list.h"
#include "lib/exml.h"
#include "lib/op.h"
#include "lib/azure_mgmt_req.h"
#include "lib/azure_blob_req.h"
#include "lib/azure_fs_req.h"
#include "lib/conn.h"
#include "lib/azure_ssl.h"
#include "lib/util.h"
#include "lib/data_api.h"

static struct {
	char *pem_file;
	char *sub_id;
	char *sub_name;
	struct elasto_conn *econn;
	char *share;
} cm_op_az_fs_state = {
	.pem_file = NULL,
	.sub_id = NULL,
	.sub_name = NULL,
	.econn = NULL,
	.share = NULL,
};

/* initialise test share used for fs testing */
static void
cm_az_fs_init(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct az_mgmt_rsp_acc_keys_get *acc_keys_get_rsp;
	struct op *op;

	ret = elasto_conn_subsys_init();
	assert_true(ret >= 0);

	ret = azure_ssl_pubset_process(cm_us->ps_file,
				       &cm_op_az_fs_state.pem_file,
				       &cm_op_az_fs_state.sub_id,
				       &cm_op_az_fs_state.sub_name);
	assert_true(ret >= 0);

	ret = elasto_conn_init_az(cm_op_az_fs_state.pem_file, NULL,
				  cm_us->insecure_http,
				  &cm_op_az_fs_state.econn);
	assert_true(ret >= 0);

	/* TODO split cli_sign_conn_setup(); into a non-client helper for... */
	ret = az_mgmt_req_acc_keys_get(cm_op_az_fs_state.sub_id, cm_us->acc,
				       &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	acc_keys_get_rsp = az_mgmt_rsp_acc_keys_get(op);
	assert_true(acc_keys_get_rsp != NULL);

	ret = elasto_conn_sign_setkey(cm_op_az_fs_state.econn, cm_us->acc,
				      acc_keys_get_rsp->primary);
	assert_true(ret >= 0);

	ret = asprintf(&cm_op_az_fs_state.share, "%s%d",
		       cm_us->ctnr, cm_us->ctnr_suffix);
	assert_true(ret >= 0);

	ret = az_fs_req_share_create(cm_us->acc, cm_op_az_fs_state.share, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	cm_us->ctnr_suffix++;

	op_free(op);
}

/* cleanup test share used for fs testing */
static void
cm_az_fs_deinit(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct op *op;

	ret = az_fs_req_share_del(cm_us->acc, cm_op_az_fs_state.share, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	op_free(op);
	free(cm_op_az_fs_state.share);

	elasto_conn_free(cm_op_az_fs_state.econn);
	elasto_conn_subsys_init();
	azure_ssl_pubset_cleanup(cm_op_az_fs_state.pem_file);
}

static void
cm_az_fs_dir_create(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct op *op;
	struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp;
	struct az_fs_ent *ent;

	ret = az_fs_req_dir_create(cm_us->acc, cm_op_az_fs_state.share, NULL,
				   "truth", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* check that the newly created directory exists in the base share */
	ret = az_fs_req_dirs_files_list(cm_us->acc, cm_op_az_fs_state.share,
					"", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	dirs_files_list_rsp = az_fs_rsp_dirs_files_list(op);
	assert_true(dirs_files_list_rsp != NULL);
	assert_true(dirs_files_list_rsp->num_ents == 1);
	ent = list_tail(&dirs_files_list_rsp->ents, struct az_fs_ent, list);
	assert_int_equal(ent->type, AZ_FS_ENT_TYPE_DIR);
	assert_string_equal(ent->dir.name, "truth");
	op_free(op);

	/* create nested subdirectory */
	ret = az_fs_req_dir_create(cm_us->acc, cm_op_az_fs_state.share, "truth",
				   "is", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* confirm new subdir exists */
	ret = az_fs_req_dirs_files_list(cm_us->acc, cm_op_az_fs_state.share,
					"truth", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	dirs_files_list_rsp = az_fs_rsp_dirs_files_list(op);
	assert_true(dirs_files_list_rsp != NULL);
	assert_true(dirs_files_list_rsp->num_ents == 1);
	ent = list_tail(&dirs_files_list_rsp->ents, struct az_fs_ent, list);
	assert_int_equal(ent->type, AZ_FS_ENT_TYPE_DIR);
	assert_string_equal(ent->dir.name, "is");
	op_free(op);

	/* confirm new subdir is empty */
	ret = az_fs_req_dirs_files_list(cm_us->acc, cm_op_az_fs_state.share,
					"truth/is", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	dirs_files_list_rsp = az_fs_rsp_dirs_files_list(op);
	assert_true(dirs_files_list_rsp != NULL);
	assert_true(dirs_files_list_rsp->num_ents == 0);
	op_free(op);

	/* cleanup subdir */
	ret = az_fs_req_dir_del(cm_us->acc, cm_op_az_fs_state.share, "truth",
				"is", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* cleanup parent */
	ret = az_fs_req_dir_del(cm_us->acc, cm_op_az_fs_state.share, NULL,
				"truth", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);
}

static void
cm_az_fs_file_create(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct op *op;
	struct az_fs_rsp_dirs_files_list *dirs_files_list_rsp;
	struct az_fs_ent *ent;

	/* create base file and directory */
	ret = az_fs_req_file_create(cm_us->acc, cm_op_az_fs_state.share, NULL,
				    "file1", BYTES_IN_TB, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	ret = az_fs_req_dir_create(cm_us->acc, cm_op_az_fs_state.share, NULL,
				   "dir1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* create nested file */
	ret = az_fs_req_file_create(cm_us->acc, cm_op_az_fs_state.share, "dir1",
				    "file2", BYTES_IN_MB, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* confirm new entries exists */
	ret = az_fs_req_dirs_files_list(cm_us->acc, cm_op_az_fs_state.share,
					"", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	dirs_files_list_rsp = az_fs_rsp_dirs_files_list(op);
	assert_true(dirs_files_list_rsp != NULL);
	assert_true(dirs_files_list_rsp->num_ents == 2);
	list_for_each(&dirs_files_list_rsp->ents, ent, list) {
		if (ent->type == AZ_FS_ENT_TYPE_DIR) {
			assert_string_equal(ent->dir.name, "dir1");
		} else {
			assert_int_equal(ent->type, AZ_FS_ENT_TYPE_FILE);
			assert_string_equal(ent->file.name, "file1");
			assert_int_equal(ent->file.size, BYTES_IN_TB);
		}
	}
	op_free(op);

	/* cleanup dir, not empty */
	ret = az_fs_req_dir_del(cm_us->acc, cm_op_az_fs_state.share, NULL,
				"dir1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(op->rsp.is_error);
	assert_int_equal(op->rsp.err_code, 409);
	op_free(op);

	/* cleanup nested file */
	ret = az_fs_req_file_del(cm_us->acc, cm_op_az_fs_state.share, "dir1",
				 "file2", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	/* cleanup dir, now empty */
	ret = az_fs_req_dir_del(cm_us->acc, cm_op_az_fs_state.share, NULL,
				"dir1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	/* cleanup base file */
	ret = az_fs_req_file_del(cm_us->acc, cm_op_az_fs_state.share, NULL,
				 "file1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);
}

static void
cm_az_fs_file_io(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct op *op;
	struct elasto_data *data;
	uint8_t buf[1024];

	/* create base file and directory */
	ret = az_fs_req_file_create(cm_us->acc, cm_op_az_fs_state.share, NULL,
				    "file1", BYTES_IN_TB, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	cm_file_buf_fill(buf, ARRAY_SIZE(buf));
	ret = elasto_data_iov_new(buf, ARRAY_SIZE(buf), 0, false, &data);
	assert_true(ret >= 0);

	ret = az_fs_req_file_put(cm_us->acc, cm_op_az_fs_state.share, NULL,
				 "file1", 0, ARRAY_SIZE(buf), data, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	/* TODO the ugly data api should be improved here... */
	op->req.data = NULL;
	op_free(op);
	data->iov.buf = NULL;
	elasto_data_free(data);

	memset(buf, 0, ARRAY_SIZE(buf));

	ret = elasto_data_iov_new(buf, ARRAY_SIZE(buf), 0, false, &data);
	assert_true(ret >= 0);

	ret = az_fs_req_file_get(cm_us->acc, cm_op_az_fs_state.share, NULL,
				 "file1", 0, ARRAY_SIZE(buf), data, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	cm_file_buf_check(buf, ARRAY_SIZE(buf));
	op->rsp.data = NULL;
	op_free(op);
	data->iov.buf = NULL;
	elasto_data_free(data);

	/* read from offset after allocated range, should be zero */
	ret = elasto_data_iov_new(buf, ARRAY_SIZE(buf), 0, false, &data);
	assert_true(ret >= 0);

	ret = az_fs_req_file_get(cm_us->acc, cm_op_az_fs_state.share, NULL,
				 "file1", ARRAY_SIZE(buf), ARRAY_SIZE(buf),
				 data, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	cm_file_buf_check_zero(buf, ARRAY_SIZE(buf));
	op->rsp.data = NULL;
	op_free(op);
	data->iov.buf = NULL;
	elasto_data_free(data);

	/* cleanup base file */
	ret = az_fs_req_file_del(cm_us->acc, cm_op_az_fs_state.share, NULL,
				 "file1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);
}

static void
cm_az_fs_file_props(void **state)
{
	int ret;
	struct cm_unity_state *cm_us = cm_unity_state_get();
	struct op *op;
	struct az_fs_rsp_file_prop_get *file_prop_get;
	uint64_t relevant;

	ret = az_fs_req_file_create(cm_us->acc, cm_op_az_fs_state.share, NULL,
				    "file1", BYTES_IN_TB, &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	ret = az_fs_req_file_prop_get(cm_us->acc, cm_op_az_fs_state.share, NULL,
				       "file1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	file_prop_get = az_fs_rsp_file_prop_get(op);
	assert_true(file_prop_get->len == BYTES_IN_TB);
	assert_string_equal(file_prop_get->content_type,
			    "application/octet-stream");

	op_free(op);

	relevant = (AZ_FS_FILE_PROP_LEN | AZ_FS_FILE_PROP_CTYPE);
	ret = az_fs_req_file_prop_set(cm_us->acc, cm_op_az_fs_state.share, NULL,
				      "file1", relevant, BYTES_IN_GB, "text/plain",
				      &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);
	op_free(op);

	ret = az_fs_req_file_prop_get(cm_us->acc, cm_op_az_fs_state.share, NULL,
				       "file1", &op);
	assert_true(ret >= 0);

	ret = elasto_conn_op_txrx(cm_op_az_fs_state.econn, op);
	assert_true(ret >= 0);
	assert_true(!op->rsp.is_error);

	file_prop_get = az_fs_rsp_file_prop_get(op);
	assert_true(file_prop_get->relevant
			== (AZ_FS_FILE_PROP_LEN | AZ_FS_FILE_PROP_CTYPE));
	assert_true(file_prop_get->len == BYTES_IN_GB);
	assert_string_equal(file_prop_get->content_type, "text/plain");

	op_free(op);
}

static const UnitTest cm_az_fs_tests[] = {
	unit_test_setup_teardown(cm_az_fs_dir_create, cm_az_fs_init, cm_az_fs_deinit),
	unit_test_setup_teardown(cm_az_fs_file_create, cm_az_fs_init, cm_az_fs_deinit),
	unit_test_setup_teardown(cm_az_fs_file_io, cm_az_fs_init, cm_az_fs_deinit),
	unit_test_setup_teardown(cm_az_fs_file_props, cm_az_fs_init, cm_az_fs_deinit),
};

int
cm_az_fs_run(void)
{
	return run_tests(cm_az_fs_tests);
}
