/*
 * Copyright (C) SUSE LINUX GmbH 2013, all rights reserved.
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

#include "ccan/list/list.h"
#include "lib/exml.h"
#include "lib/op.h"
#include "lib/conn.h"
#include "lib/azure_ssl.h"
#include "lib/util.h"
#include "lib/dbg.h"
#include "file_api.h"
#include "handle.h"
#include "xmit.h"

int
elasto_fmkdir(const struct elasto_fauth *auth,
	      const char *path)
{
	int ret;
	struct elasto_fh *fh;

	if (auth->type != ELASTO_FILE_AZURE) {
		ret = -ENOTSUP;
		goto err_out;
	}

	ret = elasto_conn_subsys_init();
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_fh_init(auth, &fh);
	if (ret < 0) {
		/* don't deinit subsystem on error */
		goto err_out;
	}

	ret = fh->ops.mkdir(fh->mod_priv, fh->conn, path);

	elasto_fh_free(fh);
err_out:
	return ret;
}

int
elasto_frmdir(const struct elasto_fauth *auth,
	      const char *path)
{
	int ret;
	struct elasto_fh *fh;

	if (auth->type != ELASTO_FILE_AZURE) {
		ret = -ENOTSUP;
		goto err_out;
	}

	ret = elasto_conn_subsys_init();
	if (ret < 0) {
		goto err_out;
	}

	ret = elasto_fh_init(auth, &fh);
	if (ret < 0) {
		/* don't deinit subsystem on error */
		goto err_out;
	}

	ret = fh->ops.rmdir(fh->mod_priv, fh->conn, path);

	elasto_fh_free(fh);
err_out:
	return ret;
}
