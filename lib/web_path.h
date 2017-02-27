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
#ifndef _WEB_PATH_H_
#define _WEB_PATH_H_

struct web_path {
	bool insecure_http;
	char *host;
	char *dl_path;
};

int
web_path_parse(const char *path,
	       struct web_path *web_path);

void
web_path_free(struct web_path *web_path);

int
web_path_dup(const struct web_path *path_orig,
	     struct web_path *path_dup);

#endif /* _WEB_PATH_H_ */
