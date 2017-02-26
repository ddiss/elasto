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
#ifndef _WEB_REQ_H_
#define _WEB_REQ_H_

/* not required, but start at 300 to avoid az_opcode overlap */
enum web_opcode {
	WEBOP_OBJ_GET = 300,
	WEBOP_OBJ_HEAD,
};

struct web_req_dl_get {
	uint64_t off;
	uint64_t len;
};

struct web_rsp_dl_head {
	uint64_t len;
	char *content_type;
};

struct web_req {
	/* no struct web_path, as op has all fields needed to duplicate */
	union {
		struct web_req_dl_get dl_get;
		/* No other request specific data aside from @path. */
	};
};

struct web_rsp {
	union {
		struct web_rsp_dl_head dl_head;
		/* No other response specific data handled yet */
	};
};

int
web_req_dl_get(const struct web_path *path,
	       uint64_t src_off,
	       uint64_t src_len,
	       struct elasto_data *dest_data,
	       struct op **_op);

int
web_req_dl_head(const struct web_path *path,
		struct op **_op);

#endif /* ifdef _WEB_REQ_H_ */
