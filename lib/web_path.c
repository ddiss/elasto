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
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <event2/http.h>

#include "lib/dbg.h"
#include "lib/web_path.h"

int
web_path_parse(const char *path,
	       struct web_path *web_path)
{
	int ret;
	struct evhttp_uri *http_uri = NULL;
	const char *url_scheme;
	const char *url_host;
	int port;
	const char *dl_path;
	const char *query;

	if ((path == NULL) || (web_path == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	http_uri = evhttp_uri_parse(path);
	if (http_uri == NULL) {
		dbg(0, "malformed web url");
		ret = -EINVAL;
		goto err_out;
	}

	url_scheme = evhttp_uri_get_scheme(http_uri);
	if (url_scheme == NULL || (strcasecmp(url_scheme, "https") != 0 &&
					strcasecmp(url_scheme, "http") != 0)) {
		dbg(0, "web url must be http or https: %s\n", path);
		ret = -EINVAL;
		goto err_uri_free;
	}

	url_host = evhttp_uri_get_host(http_uri);
	if ((url_host == NULL) || (strlen(url_host) == 0)) {
		dbg(0, "missing host in web URL: %s\n", path);
		ret = -EINVAL;
		goto err_uri_free;
	}
	web_path->host = strdup(url_host);
	if (web_path->host == NULL) {
		ret = -ENOMEM;
		goto err_uri_free;
	}

	port = evhttp_uri_get_port(http_uri);
	if (port != -1) {
		dbg(0, "port specification not supported in URL: %s\n", path);
		ret = -ENOTSUP;
		goto err_host_free;
	}

	dl_path = evhttp_uri_get_path(http_uri);
	if (strlen(dl_path) == 0) {
		dl_path = "/";
	}

	query = evhttp_uri_get_query(http_uri);
	if (query == NULL) {
		ret = asprintf(&web_path->dl_path, "%s", dl_path);
	} else {
		ret = asprintf(&web_path->dl_path, "%s?%s", dl_path, query);
	}
	if (ret < 0) {
		ret = -ENOMEM;
		goto err_host_free;
	}

	evhttp_uri_free(http_uri);
	dbg(2, "parsed %s as web path: host=%s, dl_path=%s\n",
	    path, web_path->host, web_path->dl_path);

	return 0;

err_host_free:
	free(web_path->host);
	web_path->host = NULL;
err_uri_free:
	evhttp_uri_free(http_uri);
err_out:
	return ret;
}

void
web_path_free(struct web_path *web_path)
{
	free(web_path->host);
	web_path->host = NULL;
	free(web_path->dl_path);
	web_path->dl_path = NULL;
}

int
web_path_dup(const struct web_path *path_orig,
	     struct web_path *path_dup)
{
	int ret;

	if ((path_orig == NULL) || (path_dup == NULL)
	 || (path_orig->host == NULL) || (path_orig->dl_path == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	path_dup->host = strdup(path_orig->host);
	if (path_dup->host == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}

	path_dup->dl_path = strdup(path_orig->dl_path);
	if (path_dup->dl_path == NULL) {
		ret = -ENOMEM;
		goto err_host_free;
	}

	return 0;

err_host_free:
	free(path_dup->host);
	path_dup->host = NULL;
err_out:
	return ret;
}
