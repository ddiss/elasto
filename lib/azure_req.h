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
 */
#ifndef _AZURE_REQ_H_
#define _AZURE_REQ_H_

enum azure_opcode {
	AOP_ACC_KEYS_GET = 1,
	AOP_ACC_LIST,
	AOP_ACC_CREATE,
	AOP_ACC_DEL,
	AOP_CONTAINER_LIST,
	AOP_CONTAINER_CREATE,
	AOP_CONTAINER_DEL,
	AOP_BLOB_LIST,
	AOP_BLOB_PUT,
	AOP_BLOB_GET,
	AOP_PAGE_PUT,
	AOP_BLOCK_PUT,
	AOP_BLOCK_LIST_PUT,
	AOP_BLOCK_LIST_GET,
	AOP_BLOB_DEL,
	/* Amazon S3 ops below this point */
	S3OP_SVC_LIST,
	S3OP_BKT_LIST,
	S3OP_BKT_CREATE,
	S3OP_BKT_DEL,
	S3OP_OBJ_PUT,
};

enum azure_op_data_type {
	AOP_DATA_NONE = 0,
	AOP_DATA_IOV,
	AOP_DATA_FILE,
};

struct azure_req_acc_keys_get {
	char *sub_id;
	char *service_name;
};
struct azure_rsp_acc_keys_get {
	char *primary;
	char *secondary;
};

/* azure storage account descriptor */
struct azure_account {
	struct list_node list;
	char *svc_name;
	char *label;
	char *url;
	char *desc;
	char *affin_grp;
	char *location;
};

struct azure_req_acc_list {
	char *sub_id;
};

/* @accs is a list of struct azure_account */
struct azure_rsp_acc_list {
	int num_accs;
	struct list_head accs;
};

struct azure_req_acc_create {
	char *sub_id;
	struct azure_account *acc;
};

struct azure_req_acc_del {
	char *sub_id;
	char *account;
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

struct azure_req_ctnr_del {
	char *account;
	char *container;
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

/* The block must be less than or equal to 4 MB in size. */
#define BLOB_BLOCK_MAX (4 * 1024 * 1024)
struct azure_req_block_put {
	char *account;
	char *container;
	char *bname;
	char *blk_id;
};
struct azure_rsp_block_put {
	char *content_md5;
};

enum azure_block_state {
	BLOCK_STATE_UNSENT = 0,
	BLOCK_STATE_COMMITED,
	BLOCK_STATE_UNCOMMITED,
	BLOCK_STATE_LATEST,
};

struct azure_block {
	struct list_node list;
	enum azure_block_state state;
	char *id;
	uint64_t len;
};
struct azure_req_block_list_put {
	char *account;
	char *container;
	char *bname;
	struct list_head *blks;
};

struct azure_req_block_list_get {
	char *account;
	char *container;
	char *bname;
};
struct azure_rsp_block_list_get {
	int num_blks;
	struct list_head blks;
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
	char *redir_endpoint;
};

struct s3_req_svc_list {
	/* no arguments */
};

struct s3_bucket {
	struct list_node list;
	char *name;
	char *create_date;
};

struct s3_rsp_svc_list {
	char *id;
	char *disp_name;
	int num_bkts;
	struct list_head bkts;
};

struct s3_req_bkt_list {
	char *bkt_name;
};

struct s3_object {
	struct list_node list;
	char *key;
	char *last_mod;
	uint64_t size;
	char *store_class;
};

struct s3_rsp_bkt_list {
	bool truncated;
	int num_objs;
	struct list_head objs;
};

struct s3_req_bkt_create {
	char *bkt_name;
	char *location;
};

struct s3_req_bkt_del {
	char *bkt_name;
};

struct s3_req_obj_put {
	char *bkt_name;
	char *obj_name;
};

/*
 * @base_off is the base offset into the input/output
 * buffer. i.e. @iov.base_off + @off = read/write offset
 */
struct azure_op_data {
	enum azure_op_data_type type;
	uint8_t *buf;
	uint64_t len;
	uint64_t off;
	uint64_t base_off;
	union {
		struct {
			/* @buf is allocated io buffer of size @len */
		} iov;
		struct {
			/* @buf is io file path, file is @len bytes in size */
			int fd;
		} file;
	};
};

#define REQ_METHOD_GET		"GET"
#define REQ_METHOD_PUT		"PUT"
#define REQ_METHOD_DELETE	"DELETE"
#define REQ_METHOD_POST		"POST"
struct azure_op {
	struct elasto_conn *econn;
	enum azure_opcode opcode;
	struct curl_slist *http_hdr;
	bool sign;
	char *sig_src;	/* debug, compare with signing error response */
	const char *method;
	char *url;

	struct {
		union {
			struct azure_req_acc_keys_get acc_keys_get;
			struct azure_req_acc_list acc_list;
			struct azure_req_acc_create acc_create;
			struct azure_req_acc_del acc_del;
			struct azure_req_ctnr_list ctnr_list;
			struct azure_req_ctnr_create ctnr_create;
			struct azure_req_ctnr_del ctnr_del;
			struct azure_req_blob_list blob_list;
			struct azure_req_blob_put blob_put;
			struct azure_req_blob_get blob_get;
			struct azure_req_page_put page_put;
			struct azure_req_block_put block_put;
			struct azure_req_block_list_put block_list_put;
			struct azure_req_block_list_get block_list_get;
			struct azure_req_blob_del blob_del;

			struct s3_req_svc_list svc_list;
			struct s3_req_bkt_list bkt_list;
			struct s3_req_bkt_create bkt_create;
			struct s3_req_bkt_del bkt_del;
			struct s3_req_obj_put obj_put;
		};
		uint64_t read_cbs;
		struct azure_op_data *data;
	} req;

	struct {
		bool is_error;
		int32_t err_code;
		union {
			struct azure_rsp_error err;
			struct azure_rsp_acc_keys_get acc_keys_get;
			struct azure_rsp_acc_list acc_list;
			struct azure_rsp_ctnr_list ctnr_list;
			struct azure_rsp_blob_list blob_list;
			struct azure_rsp_block_list_get block_list_get;

			struct s3_rsp_svc_list svc_list;
			struct s3_rsp_bkt_list bkt_list;
			/*
			 * No response specific data handled yet:
			 * struct azure_rsp_acc_del acc_del;
			 * struct azure_rsp_ctnr_create ctnr_create;
			 * struct azure_rsp_ctnr_del ctnr_del;
			 * struct azure_rsp_blob_put blob_put;
			 * struct azure_rsp_blob_get blob_get;
			 * struct azure_rsp_page_put page_put;
			 * struct azure_rsp_block_put block_put;
			 * struct azure_rsp_blob_del blob_del;
			 *
			 * struct s3_rsp_bkt_create bkt_create;
			 * struct s3_rsp_bkt_del bkt_del;
			 */
		};
		bool clen_recvd;
		uint64_t clen;
		uint64_t write_cbs;
		struct azure_op_data *data;
		bool recv_cb_alloced;	/* data buffer alloced by conn cb */
	} rsp;
};

void
azure_op_data_destroy(struct azure_op_data **data);

int
azure_op_data_file_new(char *path,
		       uint64_t file_len,
		       uint64_t base_off,
		       int open_flags,
		       mode_t create_mode,
		       struct azure_op_data **data);

int
azure_op_data_iov_new(uint8_t *buf,
		      uint64_t buf_len,
		      uint64_t base_off,
		      bool buf_alloc,
		      struct azure_op_data **data);

int
azure_op_acc_keys_get(const char *sub_id,
		      const char *service_name,
		      struct azure_op *op);

int
azure_op_acc_list(const char *sub_id,
		  struct azure_op *op);

int
azure_op_acc_create(const char *sub_id,
		    const char *svc_name,
		    const char *label,
		    const char *desc,
		    const char *affin_grp,
		    const char *location,
		    struct azure_op *op);

int
azure_op_acc_del(const char *sub_id,
		 const char *account,
		 struct azure_op *op);

int
azure_op_ctnr_list(const char *account,
		   struct azure_op *op);

int
azure_op_ctnr_create(const char *account,
		     const char *ctnr,
		     struct azure_op *op);

int
azure_op_ctnr_del(const char *account,
		   const char *container,
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
		  bool insecure_http,
		  struct azure_op *op);

int
azure_op_blob_get(const char *account,
		  const char *container,
		  const char *bname,
		  bool is_page,
		  enum azure_op_data_type data_type,
		  uint8_t *buf,
		  uint64_t off,
		  uint64_t len,
		  bool insecure_http,
		  struct azure_op *op);

int
azure_op_page_put(const char *account,
		  const char *container,
		  const char *bname,
		  uint8_t *buf,
		  uint64_t off,
		  uint64_t len,
		  bool insecure_http,
		  struct azure_op *op);

int
azure_op_block_put(const char *account,
		   const char *container,
		   const char *bname,
		   const char *blk_id,
		   struct azure_op_data *data,
		   bool insecure_http,
		   struct azure_op *op);

int
azure_op_block_list_put(const char *account,
			const char *container,
			const char *bname,
			struct list_head *blks,
			struct azure_op *op);

int
azure_op_block_list_get(const char *account,
			const char *container,
			const char *bname,
			struct azure_op *op);

int
azure_op_blob_del(const char *account,
		  const char *ctnr,
		  const char *bname,
		  struct azure_op *op);

int
s3_op_svc_list(bool insecure_http,
	       struct azure_op *op);

int
s3_op_bkt_list(const char *bkt_name,
	       bool insecure_http,
	       struct azure_op *op);

int
s3_op_bkt_create(const char *bkt_name,
		 const char *location,
		 bool insecure_http,
		 struct azure_op *op);

int
s3_op_bkt_del(const char *bkt_name,
	      bool insecure_http,
	      struct azure_op *op);

int
s3_op_obj_put(const char *bkt_name,
	      const char *obj_name,
	      struct azure_op_data *data,
	      bool insecure_http,
	      struct azure_op *op);

bool
azure_rsp_is_error(enum azure_opcode opcode, int err_code);

void
azure_op_free(struct azure_op *op);

int
azure_rsp_process(struct azure_op *op);

#endif /* ifdef _AZURE_REQ_H_ */
