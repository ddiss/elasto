/*
 * Copyright (C) SUSE LINUX Products GmbH 2012, all rights reserved.
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
 *
 * Author: David Disseldorp <ddiss@suse.de>
 */
#ifndef _AZURE_REQ_H_
#define _AZURE_REQ_H_

enum azure_opcode {
	AOP_MGMT_GET_SA_KEYS = 1,
	AOP_CONTAINER_LIST,
	AOP_CONTAINER_CREATE,
	AOP_BLOB_LIST,
	AOP_BLOB_PUT,
	AOP_BLOB_GET,
	AOP_PAGE_PUT,
	AOP_BLOB_DEL,
};

struct azure_req_mgmt_get_sa_keys {
	char *sub_id;
	char *service_name;
};
struct azure_rsp_mgmt_get_sa_keys {
	char *primary;
	char *secondary;
};

struct azure_ctnr {
	struct list_node list;
	char *name;
};

struct azure_req_ctnr_list {
	char *account;
};
/* @ctnrs: struct azure_ctnr list */
struct azure_rsp_ctnr_list {
	int num_ctnrs;
	struct list_head ctnrs;
};

struct azure_req_ctnr_create {
	char *account;
	char *ctnr;
};

struct azure_blob {
	struct list_node list;
	char *name;
	bool is_page;
	uint64_t len;
};

struct azure_req_blob_list {
	char *account;
	char *ctnr;
};
/* @blobs: struct azure_blob list */
struct azure_rsp_blob_list {
	int num_blobs;
	struct list_head blobs;
};

/*
 * The Content-Length header and body data are derived from op.iov.
 * @content_len_bytes corresponds to the x-ms-blob-content-length header, and
 * is needed for page blobs only.
 */
#define BLOB_TYPE_BLOCK	"BlockBlob"
#define BLOB_TYPE_PAGE	"PageBlob"
#define PBLOB_SECTOR_SZ 512
struct azure_req_blob_put {
	char *account;
	char *container;
	char *bname;
	char *type;
	uint64_t pg_len;
};
struct azure_rsp_blob_put {
	time_t last_mod;
	char *content_md5;
};

struct azure_req_blob_get {
	char *account;
	char *container;
	char *bname;
	char *type;
	uint64_t off;
	uint64_t len;
};
struct azure_rsp_blob_get {
	time_t last_mod;
	char *content_md5;
};

struct azure_req_page_put {
	char *account;
	char *container;
	char *bname;
	uint64_t off;
	uint64_t len;
	bool clear_data;
};
struct azure_rsp_page_put {
	time_t last_mod;
	char *content_md5;
	uint64_t seq_num;
};

struct azure_req_blob_del {
	char *account;
	char *container;
	char *bname;
};

/* error response buffer is separate to request/response data */
struct azure_rsp_error {
	char *msg;
	uint8_t *buf;
	uint64_t len;
	uint64_t off;
};

enum azure_op_data_type {
	AOP_DATA_NONE = 0,
	AOP_DATA_IOV,
	AOP_DATA_FILE,
};
struct azure_op_data {
	enum azure_op_data_type type;
	uint8_t *buf;
	uint64_t len;
	union {
		struct {
			/* @buf is allocated io buffer of size @len */
			uint64_t off;
		} iov;
		struct {
			/* @buf is io file path, file is @len bytes in size */
			uint64_t off;
			int fd;
		} file;
	};
};

#define REQ_METHOD_GET		"GET"
#define REQ_METHOD_PUT		"PUT"
#define REQ_METHOD_DELETE	"DELETE"
struct azure_op {
	struct azure_conn *aconn;
	enum azure_opcode opcode;
	struct curl_slist *http_hdr;
	bool sign;
	char *sig_src;	/* debug, compare with signing error response */
	const char *method;
	char *url;

	struct {
		union {
			struct azure_req_mgmt_get_sa_keys mgmt_get_sa_keys;
			struct azure_req_ctnr_list ctnr_list;
			struct azure_req_ctnr_create ctnr_create;
			struct azure_req_blob_list blob_list;
			struct azure_req_blob_put blob_put;
			struct azure_req_blob_get blob_get;
			struct azure_req_page_put page_put;
			struct azure_req_blob_del blob_del;
		};
		struct azure_op_data data;
	} req;

	struct {
		bool is_error;
		int32_t err_code;
		union {
			struct azure_rsp_error err;
			struct azure_rsp_mgmt_get_sa_keys mgmt_get_sa_keys;
			struct azure_rsp_ctnr_list ctnr_list;
			struct azure_rsp_blob_list blob_list;
			/*
			 * No response specific data handled yet:
			 * struct azure_rsp_ctnr_create ctnr_create;
			 * struct azure_rsp_blob_put blob_put;
			 * struct azure_rsp_blob_get blob_get;
			 * struct azure_rsp_page_put page_put;
			 * struct azure_rsp_blob_del blob_del;
			 */
		};
		uint64_t clen;
		uint64_t write_cbs;
		struct azure_op_data data;
	} rsp;
};

int
azure_op_mgmt_get_sa_keys(const char *sub_id,
			  const char *service_name,
			  struct azure_op *op);

int
azure_op_ctnr_list(const char *account,
		   struct azure_op *op);

int
azure_op_ctnr_create(const char *account,
		     const char *ctnr,
		     struct azure_op *op);

int
azure_op_blob_list(const char *account,
		   const char *ctnr,
		   struct azure_op *op);

int
azure_op_blob_put(const char *account,
		  const char *container,
		  const char *bname,
		  enum azure_op_data_type data_type,
		  uint8_t *buf,
		  uint64_t len,
		  struct azure_op *op);

int
azure_op_blob_get(const char *account,
		  const char *container,
		  const char *bname,
		  bool is_page,
		  uint64_t off,
		  uint64_t len,
		  struct azure_op *op);

int
azure_op_page_put(const char *account,
		  const char *container,
		  const char *bname,
		  uint8_t *buf,
		  uint64_t off,
		  uint64_t len,
		  struct azure_op *op);

int
azure_op_blob_del(const char *account,
		  const char *ctnr,
		  const char *bname,
		  struct azure_op *op);

bool
azure_rsp_is_error(enum azure_opcode opcode, int err_code);

void
azure_op_free(struct azure_op *op);

int
azure_rsp_process(struct azure_op *op);

#endif /* ifdef _AZURE_REQ_H_ */
