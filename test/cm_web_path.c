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
#include "web_path.h"
#include "dbg.h"

static void
cm_web_path(void **state)
{
	int ret;
	struct web_path path = { 0 };

	/* no scheme */
	ret = web_path_parse("/", &path);
	assert_true(ret < 0);

	/* no host */
	ret = web_path_parse("https:///", &path);
	assert_true(ret < 0);

	ret = web_path_parse("http://myhost", &path);
	assert_true(ret >= 0);
	assert_true(path.insecure_http);
	assert_string_equal(path.host, "myhost");
	assert_string_equal(path.dl_path, "/");
	web_path_free(&path);

	ret = web_path_parse("HTTP://myhost///", &path);
	assert_true(ret >= 0);
	assert_true(path.insecure_http);
	assert_string_equal(path.host, "myhost");
	assert_string_equal(path.dl_path, "///");
	web_path_free(&path);

	ret = web_path_parse("http://myhost/this/is/a/path", &path);
	assert_true(ret >= 0);
	assert_true(path.insecure_http);
	assert_string_equal(path.host, "myhost");
	assert_string_equal(path.dl_path, "/this/is/a/path");
	web_path_free(&path);

	ret = web_path_parse("HTTPS://myhost/this/is/a/path?query=stuff", &path);
	assert_true(ret >= 0);
	assert_true(!path.insecure_http);
	assert_string_equal(path.host, "myhost");
	assert_string_equal(path.dl_path, "/this/is/a/path?query=stuff");
	web_path_free(&path);

	/* not http or https */
	ret = web_path_parse("elasto://myhost/this/is/a/path?query=stuff", &path);
	assert_true(ret < 0);

	/* http:/// is malformed */
	ret = web_path_parse("http:///myhost///this/is//a/path?query", &path);
	assert_true(ret < 0);

	/* explicit port specification not supported */
	ret = web_path_parse("http://myhost:90/this/is/a/path?query", &path);
	assert_true(ret < 0);
}

static void
cm_web_path_dup(void **state)
{
	int ret;
	struct web_path path = { 0 };
	struct web_path path_dup = { 0 };

	ret = web_path_parse("https://myhost/this/is/a/path?query=stuff",
			     &path);
	assert_true(ret >= 0);
	ret = web_path_dup(&path, &path_dup);
	assert_true(ret >= 0);

	assert_true(!path.insecure_http);
	assert_string_equal(path.host, "myhost");
	assert_string_equal(path.dl_path, "/this/is/a/path?query=stuff");
	web_path_free(&path);

	assert_true(!path_dup.insecure_http);
	assert_string_equal(path_dup.host, "myhost");
	assert_string_equal(path_dup.dl_path, "/this/is/a/path?query=stuff");
	web_path_free(&path_dup);
}

static const UnitTest cm_web_path_tests[] = {
	unit_test(cm_web_path),
	unit_test(cm_web_path_dup),
};

int
cm_web_path_run(void)
{
	return run_tests(cm_web_path_tests);
}
