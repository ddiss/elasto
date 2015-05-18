/*
 * Copyright (C) SUSE LINUX GmbH 2015, all rights reserved.
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
#ifndef _S3_STAT_H_
#define _S3_STAT_H_

int
s3_fstat(void *mod_priv,
	 struct elasto_conn *conn,
	 struct elasto_fstat *fstat);

int
s3_fstatvfs(void *mod_priv,
	    struct elasto_conn *conn,
	    struct elasto_fstatfs *fstatfs);

#endif /* _S3_STAT_H_ */