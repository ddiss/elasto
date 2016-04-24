/*
 * Copyright (C) SUSE LINUX GmbH 2016, all rights reserved.
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

#include "ccan/list/list.h"
#include "azure_blob_path.h"
#include "dbg.h"

static void
cm_az_blob_path_root(void **state)
{
	int ret;
	struct az_blob_path path = { 0 };

	ret = az_blob_path_parse("/", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_ROOT);
	assert_null(path.acc);
	assert_null(path.ctnr);
	assert_null(path.blob);
	az_blob_path_free(&path);
	/* double free should be ok */
	az_blob_path_free(&path);

	ret = az_blob_path_parse("///", &path);
	assert_int_equal(path.type, AZ_BLOB_PATH_ROOT);
	assert_true(ret >= 0);
	az_blob_path_free(&path);

	ret = az_blob_path_parse("", &path);
	assert_true(ret < 0);
}

static void
cm_az_blob_path_acc(void **state)
{
	int ret;
	struct az_blob_path path = { 0 };

	ret = az_blob_path_parse("/acc", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_ACC);
	assert_string_equal(path.acc, "acc");
	assert_null(path.ctnr);
	assert_null(path.blob);
	az_blob_path_free(&path);

	ret = az_blob_path_parse("//ace///", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_ACC);
	assert_string_equal(path.acc, "ace");
	az_blob_path_free(&path);

	/* no leading slash */
	ret = az_blob_path_parse("ao", &path);
	assert_true(ret < 0);
}

static void
cm_az_blob_path_ctnr(void **state)
{
	int ret;
	struct az_blob_path path = { 0 };

	ret = az_blob_path_parse("/acc/ctnr", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_CTNR);
	assert_string_equal(path.acc, "acc");
	assert_string_equal(path.ctnr, "ctnr");
	assert_null(path.blob);
	az_blob_path_free(&path);

	ret = az_blob_path_parse("//ace///ctne//", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_CTNR);
	assert_string_equal(path.acc, "ace");
	assert_string_equal(path.ctnr, "ctne");
	az_blob_path_free(&path);

	ret = az_blob_path_parse("ao/co", &path);
	assert_true(ret < 0);
}

static void
cm_az_blob_path_blob(void **state)
{
	int ret;
	struct az_blob_path path = { 0 };

	ret = az_blob_path_parse("/acc/ctnr/blob", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_BLOB);
	assert_string_equal(path.acc, "acc");
	assert_string_equal(path.ctnr, "ctnr");
	assert_string_equal(path.blob, "blob");
	az_blob_path_free(&path);

	ret = az_blob_path_parse("//ace///ctne//bloe", &path);
	assert_true(ret >= 0);
	assert_int_equal(path.type, AZ_BLOB_PATH_BLOB);
	assert_string_equal(path.acc, "ace");
	assert_string_equal(path.ctnr, "ctne");
	assert_string_equal(path.blob, "bloe");
	az_blob_path_free(&path);

	ret = az_blob_path_parse("ao/co/bo", &path);
	assert_true(ret < 0);

	/* trailing garbage */
	ret = az_blob_path_parse("/ao/co/bo/", &path);
	assert_true(ret < 0);
	ret = az_blob_path_parse("/ao/co/bo/asdf", &path);
	assert_true(ret < 0);
}

static void
cm_az_blob_path_dup(void **state)
{
	int ret;
	struct az_blob_path path = { 0 };
	struct az_blob_path path_dup = { 0 };

	ret = az_blob_path_parse("/acc/ctnr/blob", &path);
	assert_true(ret >= 0);
	ret = az_blob_path_dup(&path, &path_dup);
	assert_true(ret >= 0);

	assert_int_equal(path.type, AZ_BLOB_PATH_BLOB);
	assert_string_equal(path.acc, "acc");
	assert_string_equal(path.ctnr, "ctnr");
	assert_string_equal(path.blob, "blob");
	az_blob_path_free(&path);

	assert_int_equal(path_dup.type, AZ_BLOB_PATH_BLOB);
	assert_string_equal(path_dup.acc, "acc");
	assert_string_equal(path_dup.ctnr, "ctnr");
	assert_string_equal(path_dup.blob, "blob");
	az_blob_path_free(&path_dup);
}

static const UnitTest cm_az_blob_path_tests[] = {
	unit_test(cm_az_blob_path_root),
	unit_test(cm_az_blob_path_acc),
	unit_test(cm_az_blob_path_ctnr),
	unit_test(cm_az_blob_path_blob),
	unit_test(cm_az_blob_path_dup),
};

int
cm_az_blob_path_run(void)
{
	return run_tests(cm_az_blob_path_tests);
}