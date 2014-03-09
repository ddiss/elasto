/*
 * Copyright (C) SUSE LINUX Products GmbH 2013, all rights reserved.
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
#define _GNU_SOURCE
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
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmocka.h>

#include "ccan/list/list.h"
#include "dbg.h"
#include "util.h"
#include "exml.h"

static char *cm_xml_data_str_basic
	= "<outer><inner1><str>val</str></inner1><str>blah</str></outer>";
static char *cm_xml_data_str_multi
	= "<out><in><str>val0</str></in><in><str>val1</str></in>"
	  "<in><str>val2</str></in></out>";
static char *cm_xml_data_str_dup
	= "<outer><dup><str>val</str></dup><dup><str>blah</str></dup></outer>";
static char *cm_xml_data_num_basic
	= "<outer><num>100</num><inner1><neg>-100</neg></inner1>"
	  "<huge>18446744073709551615</huge></outer>";
static char *cm_xml_data_bool_basic
	= "<outer><inner1><bool>true</bool></inner1><next>false</next></outer>";
static char *cm_xml_data_b64_basic
	= "<outer><Label1>dGhpcyBpcyBhIGxhYmVs</Label1>"
	  "<Label2>aXN0Z3Q=</Label2></outer>";
static char *cm_xml_data_attr_basic
	= "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
	  "<PublishData>"
	  "<PublishProfile\n"
	   "PublishMethod=\"AzureServiceManagementAPI\"\n"
	   "Url=\"https://management.core.windows.net/\">"
	  "<Subscription\n"
	   "Id=\"55555555-4444-3333-2222-111111111111\"\n"
	   "Name=\"3-Month Free Trial\" />"
	  "</PublishProfile>"
	  "</PublishData>";

/*
 * TODO test:
 * - duplicate attr paths
 * - empty values
 * - empty attributes
 * - multiple parse calls
 * - valgrind memory checks
 * - index based path checks
 */

/*
 * CMocka unit tests for Elasto XML decoding
 */
static void
cm_xml_str_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/outer/inner1/str",
			   true,
			   &val,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_non_null(val);
	assert_string_equal(val, "val");

	free(val);
}

static void
cm_xml_str_dup(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;
	bool val_present = false;

	ret = exml_slurp(cm_xml_data_str_dup,
			strlen(cm_xml_data_str_dup), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/outer/dup/str",
			   true,
			   &val,
			   &val_present);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	exml_free(xdoc);

	/* should take the value for last path encountered */
	assert_true(val_present);
	assert_non_null(val);
	assert_string_equal(val, "blah");
	free(val);
}

static void
cm_xml_two_str(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val1 = NULL;
	char *val2 = NULL;
	bool val2_present = false;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/outer/inner1/str",
			   true,
			   &val1,
			   NULL);
	assert_int_equal(ret, 0);

	/* not required */
	ret = exml_str_want(xdoc,
			   "/outer/str",
			   false,
			   &val2,
			   &val2_present);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	exml_free(xdoc);

	assert_non_null(val1);
	assert_string_equal(val1, "val");
	assert_true(val2_present);
	assert_non_null(val2);
	assert_string_equal(val2, "blah");
	free(val1);
	free(val2);
}

static void
cm_xml_num_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	int32_t val1;
	int64_t val2;
	uint64_t val3;

	ret = exml_slurp(cm_xml_data_num_basic,
			strlen(cm_xml_data_num_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_int32_want(xdoc,
			   "/outer/num",
			   true,
			   &val1,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_int64_want(xdoc,
			   "/outer/inner1/neg",
			   true,
			   &val2,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_uint64_want(xdoc,
			   "/outer/huge",
			   true,
			   &val3,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_true(val1 == 100);
	assert_true(val2 == -100);
	assert_true(val3 == 18446744073709551615ULL);
}

static void
cm_xml_bool_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	bool val1;
	bool val2;

	ret = exml_slurp(cm_xml_data_bool_basic,
			strlen(cm_xml_data_bool_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_bool_want(xdoc,
			   "/outer/inner1/bool",
			   true,
			   &val1,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_bool_want(xdoc,
			   "/outer/next",
			   true,
			   &val2,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	exml_free(xdoc);

	assert_true(val1);
	assert_false(val2);
}

static void
cm_xml_base64_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val1 = NULL;
	char *val2 = NULL;

	ret = exml_slurp(cm_xml_data_b64_basic,
			strlen(cm_xml_data_b64_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_base64_want(xdoc,
			   "/outer/Label1",
			   true,
			   &val1,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_base64_want(xdoc,
			   "/outer/Label2",
			   true,
			   &val2,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	exml_free(xdoc);

	assert_non_null(val1);
	assert_string_equal(val1, "this is a label");
	assert_non_null(val2);
	assert_string_equal(val2, "istgt");
	free(val1);
	free(val2);
}

int cm_xml_basic_want_cb(struct xml_doc *xdoc,
		   const char *path,
		   const char *val,
		   void *cb_data)
{
	char **str = cb_data;

	asprintf(str, "got: %s", val);
	return 0;
}

static void
cm_xml_cb_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_cb_want(xdoc,
			   "/outer/inner1/str",
			   true,
			   cm_xml_basic_want_cb,
			   &val,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	assert_non_null(val);
	assert_string_equal(val, "got: val");

	exml_free(xdoc);
	free(val);
}

static int
cm_xml_path_want_cb(struct xml_doc *xdoc,
		   const char *path,
		   const char *val,
		   void *cb_data)
{
	int *cb_i = cb_data;

	assert_null(val);
	/* note the trailing '/' */
	assert_string_equal(path, "/outer[0]/inner1[0]/");
	(*cb_i)++;

	return 0;
}

/* add new finder from callback */
static void
cm_xml_path_cb_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	int cb_i = 0;
	bool called = false;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_path_cb_want(xdoc,
			   "/outer/inner1",
			   false,
			   cm_xml_path_want_cb,
			   &cb_i,
			   &called);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_true(called);
	assert_int_equal(cb_i, 1);
}

struct cm_xml_path_multi_cb_data {
	int cb_i;
	char *val0;
	char *val1;
	char *val2;
};

static int
cm_xml_path_multi_cb(struct xml_doc *xdoc,
		     const char *path,
		     const char *val,
		     void *cb_data)
{
	struct cm_xml_path_multi_cb_data *d = cb_data;

	assert_null(val);
	switch (d->cb_i++) {
	case 0:
		assert_string_equal(path, "/out[0]/in[0]/");
		exml_str_want(xdoc, "./str", true, &d->val0, NULL);
		break;
	case 1:
		assert_string_equal(path, "/out[0]/in[1]/");
		exml_str_want(xdoc, "./str", true, &d->val1, NULL);
		break;
	case 2:
		assert_string_equal(path, "/out[0]/in[2]/");
		exml_str_want(xdoc, "./str", true, &d->val2, NULL);
		break;
	default:
		assert_true(false);
		break;
	}

	return 0;
}

/* add new finder from callback */
static void
cm_xml_path_cb_multi(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	bool called = false;
	struct cm_xml_path_multi_cb_data cb_data;

	memset(&cb_data, 0, sizeof(cb_data));
	ret = exml_slurp(cm_xml_data_str_multi,
			strlen(cm_xml_data_str_multi), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_path_cb_want(xdoc,
			   "/out/in",
			   false,
			   cm_xml_path_multi_cb,
			   &cb_data,
			   &called);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_true(called);
	assert_int_equal(cb_data.cb_i, 3);
	assert_string_equal(cb_data.val0, "val0");
	assert_string_equal(cb_data.val1, "val1");
	assert_string_equal(cb_data.val2, "val2");
}

static void
cm_xml_attr_basic(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;
	bool got_attr = false;

	ret = exml_slurp(cm_xml_data_attr_basic,
			strlen(cm_xml_data_attr_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/PublishData/PublishProfile[@PublishMethod]",
			   true,
			   &val,
			   &got_attr);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_true(got_attr);
	assert_non_null(val);
	assert_string_equal(val, "AzureServiceManagementAPI");

	free(val);
}

static void
cm_xml_attr_multi(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val1 = NULL;
	char *val2 = NULL;
	char *val3 = NULL;
	bool got_attr1 = false;
	bool got_attr2 = false;
	bool got_attr3 = false;

	ret = exml_slurp(cm_xml_data_attr_basic,
			strlen(cm_xml_data_attr_basic), &xdoc);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/PublishData[0]/PublishProfile[0][@PublishMethod]",
			   true,
			   &val1,
			   &got_attr1);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,
			   "/PublishData/PublishProfile[0]/Subscription[@Id]",
			   true,
			   &val2,
			   &got_attr2);
	assert_int_equal(ret, 0);

	ret = exml_str_want(xdoc,	/* no element at index [1] */
			   "/PublishData/PublishProfile/Subscription[1][@Name]",
			   false,
			   &val3,
			   &got_attr3);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_true(got_attr1);
	assert_non_null(val1);
	assert_string_equal(val1, "AzureServiceManagementAPI");

	assert_true(got_attr2);
	assert_non_null(val2);
	assert_string_equal(val2, "55555555-4444-3333-2222-111111111111");

	assert_false(got_attr3);

	free(val1);
	free(val2);
}

static void
cm_xml_xpath_relative(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);
	/* current path is / (root) after slurp */

	ret = exml_str_want(xdoc,
			   "./outer/inner1/str",
			   true,
			   &val,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);

	assert_non_null(val);
	assert_string_equal(val, "val");

	exml_free(xdoc);
	free(val);
}

static void
cm_xml_xpath_wildcard(void **state)
{
	int ret;
	struct xml_doc *xdoc;
	char *val = NULL;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	/* wildcard at end */
	ret = exml_str_want(xdoc,
			   "/outer/inner1/*",
			   true,
			   &val,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_non_null(val);
	assert_string_equal(val, "val");
	free(val);
	val = NULL;

	ret = exml_slurp(cm_xml_data_str_basic,
			strlen(cm_xml_data_str_basic), &xdoc);
	assert_int_equal(ret, 0);

	/* wildcard at start */
	ret = exml_str_want(xdoc,
			   "/*/inner1/str",
			   true,
			   &val,
			   NULL);
	assert_int_equal(ret, 0);

	ret = exml_parse(xdoc);
	assert_int_equal(ret, 0);
	exml_free(xdoc);

	assert_non_null(val);
	assert_string_equal(val, "val");

	free(val);
}

static const UnitTest cm_xml_tests[] = {
	unit_test(cm_xml_str_basic),
	unit_test(cm_xml_str_dup),
	unit_test(cm_xml_two_str),
	unit_test(cm_xml_num_basic),
	unit_test(cm_xml_bool_basic),
	unit_test(cm_xml_base64_basic),
	unit_test(cm_xml_cb_basic),
	unit_test(cm_xml_path_cb_basic),
	unit_test(cm_xml_path_cb_multi),
	unit_test(cm_xml_attr_basic),
	unit_test(cm_xml_attr_multi),
	unit_test(cm_xml_xpath_relative),
	unit_test(cm_xml_xpath_wildcard),
};

int
cm_xml_run(void)
{
	return run_tests(cm_xml_tests);
}
