/*
 * Copyright (C) SUSE LINUX GmbH 2015-2016, all rights reserved.
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

#include "lib/dbg.h"
#include "lib/s3_path.h"

int
s3_path_parse(const char *custom_host,
	      uint16_t port,
	      const char *path,
	      bool insecure_http,
	      struct s3_path *s3_path)
{
	int ret;
	const char *cs;
	char *s;
	char *host = NULL;
	char *comp1 = NULL;
	char *comp2 = NULL;
	/*
	 * use $bkt.s3.amazonaws.com by default, or $custom_host/$bkt/... if an
	 * explicit hostname has been provided.
	 */
	bool bkt_as_host_prefix = true;

	if ((path == NULL) || (s3_path == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	if (custom_host != NULL) {
		bkt_as_host_prefix = false;
		host = strdup(custom_host);
	} else {
		host = strdup(S3_PATH_HOST_DEFAULT);
	}
	if (host == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}
	if (port == 0) {
		port = (insecure_http ? 80 : 443);
		dbg(1, "default port %d in use\n", port);
	}

	cs = path;
	if (*cs != '/') {
		/* no leading slash */
		ret = -EINVAL;
		goto err_host_free;
	}

	while (*cs == '/')
		cs++;

	if (*cs == '\0') {
		/* empty or leading slashes only */
		s3_path->type = S3_PATH_ROOT;
		goto done;
	}

	comp1 = strdup(cs);
	if (comp1 == NULL) {
		ret = -ENOMEM;
		goto err_host_free;
	}

	s = strchr(comp1, '/');
	if (s == NULL) {
		/* bucket only */
		s3_path->type = S3_PATH_BKT;
		goto done;
	}

	*(s++) = '\0';	/* null term for acc */
	while (*s == '/')
		s++;

	if (*s == '\0') {
		/* bucket + slashes only */
		s3_path->type = S3_PATH_BKT;
		goto done;
	}

	comp2 = strdup(s);
	if (comp2 == NULL) {
		ret = -ENOMEM;
		goto err_1_free;
	}

	s = strchr(comp2, '/');
	if (s != NULL) {
		dbg(0, "Invalid remote path: S3 object has trailing garbage\n");
		ret = -EINVAL;
		goto err_2_free;
	}
	s3_path->type = S3_PATH_OBJ;
done:
	assert(s3_path->type != 0);
	s3_path->host = host;
	s3_path->port = port;
	s3_path->bkt = comp1;
	s3_path->obj = comp2;
	s3_path->bkt_as_host_prefix = bkt_as_host_prefix;
	dbg(2, "parsed %s as S3 path: host%s=%s, port=%d, bkt=%s, obj=%s\n",
	    path, (s3_path->bkt_as_host_prefix ? "(prior to bkt prefix)" : ""),
	    s3_path->host, s3_path->port, (s3_path->bkt ? s3_path->bkt : ""),
	    (s3_path->obj ? s3_path->obj : ""));

	return 0;

err_2_free:
	free(comp2);
err_1_free:
	free(comp1);
err_host_free:
	free(host);
err_out:
	return ret;
}

void
s3_path_free(struct s3_path *s3_path)
{
	free(s3_path->host);
	free(s3_path->bkt);
	free(s3_path->obj);
	memset(s3_path, 0, sizeof(*s3_path));
}

int
s3_path_dup(const struct s3_path *path_orig,
	    struct s3_path *path_dup)
{
	int ret;
	struct s3_path dup = { 0 };

	if ((path_orig == NULL) || (path_dup == NULL)) {
		ret = -EINVAL;
		goto err_out;
	}

	if (path_orig->host == NULL) {
		dbg(0, "host not set in orig path\n");
		ret = -EINVAL;
		goto err_out;
	}

	dup.host = strdup(path_orig->host);
	if (dup.host == NULL) {
		ret = -ENOMEM;
		goto err_out;
	}
	dup.bkt_as_host_prefix = path_orig->bkt_as_host_prefix;
	dup.port = path_orig->port;

	dup.type = path_orig->type;
	if (path_orig->bkt != NULL) {
		dup.bkt = strdup(path_orig->bkt);
		if (dup.bkt == NULL) {
			ret = -ENOMEM;
			goto err_path_free;
		}
	} else {
		/* obj must also be NULL */
		goto done;
	}

	if (path_orig->obj != NULL) {
		dup.obj = strdup(path_orig->obj);
		if (dup.obj == NULL) {
			ret = -ENOMEM;
			goto err_path_free;
		}
	}

done:
	*path_dup = dup;
	return 0;

err_path_free:
	s3_path_free(&dup);
err_out:
	return ret;
}
