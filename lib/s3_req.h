/*
 * Copyright (C) SUSE LINUX GmbH 2012-2015, all rights reserved.
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
#ifndef _S3_REQ_H_
#define _S3_REQ_H_

/* not required, but start at 200 to avoid az_opcode overlap */
enum s3_opcode {
	S3OP_SVC_LIST = 200,
	S3OP_BKT_LIST,
	S3OP_BKT_CREATE,
	S3OP_BKT_DEL,
	S3OP_BKT_LOCATION_GET,
	S3OP_OBJ_PUT,
	S3OP_OBJ_GET,
	S3OP_OBJ_DEL,
	S3OP_OBJ_CP,
	S3OP_OBJ_HEAD,
	S3OP_MULTIPART_START,
	S3OP_MULTIPART_DONE,
	S3OP_MULTIPART_ABORT,
	S3OP_PART_PUT,
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
	char *location;
};

struct s3_rsp_bkt_loc_get {
	char *location;
};

struct s3_req_obj_get {
	uint64_t off;
	uint64_t len;
};

struct s3_req_obj_cp {
	struct s3_path src_path;
};

struct s3_rsp_obj_head {
	uint64_t len;
	char *content_type;
};

struct s3_rsp_mp_start {
	char *upload_id;
};

struct s3_part {
	struct list_node list;
	uint32_t pnum;
	char *etag;
};

struct s3_req_mp_done {
	char *upload_id;
};

struct s3_req_mp_abort {
	char *upload_id;
};

struct s3_req_part_put {
	char *upload_id;
	uint32_t pnum;
	struct elasto_data *data;
};

struct s3_rsp_part_put {
	char *etag;
};

struct s3_req {
	struct s3_path path;
	union {
		struct s3_req_bkt_create bkt_create;
		struct s3_req_obj_get obj_get;
		struct s3_req_obj_cp obj_cp;
		struct s3_req_mp_done mp_done;
		struct s3_req_mp_abort mp_abort;
		struct s3_req_part_put part_put;
		/*
		 * No request specific data aside from @path:
		struct s3_req_svc_list svc_list;
		struct s3_req_bkt_list bkt_list;
		struct s3_req_bkt_del bkt_del;
		struct s3_req_bkt_loc_get bkt_loc_get;
		struct s3_req_obj_put obj_put;
		struct s3_req_obj_del obj_del;
		struct s3_req_obj_head obj_head;
		struct s3_req_mp_start mp_start;
		 */
	};
};

struct s3_rsp {
	union {
		struct s3_rsp_svc_list svc_list;
		struct s3_rsp_bkt_list bkt_list;
		struct s3_rsp_bkt_loc_get bkt_loc_get;
		struct s3_rsp_obj_head obj_head;
		struct s3_rsp_mp_start mp_start;
		struct s3_rsp_part_put part_put;
		/*
		 * No response specific data handled yet:
		 * struct s3_rsp_bkt_create bkt_create;
		 * struct s3_rsp_bkt_del bkt_del;
		 * struct s3_rsp_bkt_cp bkt_cp;
		 * struct s3_rsp_mp_done mp_done;
		 * struct s3_rsp_mp_done mp_abort;
		 */
	};
};

int
s3_req_hostname_get(char *bkt,
		    char **_hostname);

int
s3_req_svc_list(struct s3_path *s3_path,
		struct op **_op);

int
s3_req_bkt_list(const struct s3_path *path,
		struct op **_op);

int
s3_req_bkt_create(const struct s3_path *path,
		  const char *location,
		  struct op **_op);

int
s3_req_bkt_del(const struct s3_path *path,
	       struct op **_op);

int
s3_req_bkt_loc_get(const struct s3_path *path,
		   struct op **_op);

int
s3_req_obj_put(const struct s3_path *path,
	       struct elasto_data *data,
	       struct op **_op);

int
s3_req_obj_get(const struct s3_path *path,
	       uint64_t src_off,
	       uint64_t src_len,
	       struct elasto_data *dest_data,
	       struct op **_op);

int
s3_req_obj_del(const struct s3_path *path,
		  struct op **_op);

int
s3_req_obj_cp(const struct s3_path *src_path,
	      const struct s3_path *dst_path,
	      struct op **_op);

int
s3_req_obj_head(const struct s3_path *path,
		struct op **_op);

int
s3_req_mp_start(const struct s3_path *path,
		   struct op **_op);

int
s3_req_mp_done(const struct s3_path *path,
	       const char *upload_id,
	       uint64_t num_parts,
	       struct list_head *parts,
	       struct op **_op);

int
s3_req_mp_abort(const struct s3_path *path,
		const char *upload_id,
		struct op **_op);

int
s3_req_part_put(const struct s3_path *path,
		const char *upload_id,
		uint32_t pnum,
		struct elasto_data *data,
		struct op **_op);

struct s3_rsp_svc_list *
s3_rsp_svc_list(struct op *op);

struct s3_rsp_bkt_list *
s3_rsp_bkt_list(struct op *op);

struct s3_rsp_bkt_loc_get *
s3_rsp_bkt_loc_get(struct op *op);

struct s3_rsp_obj_head *
s3_rsp_obj_head(struct op *op);

struct s3_rsp_mp_start *
s3_rsp_mp_start(struct op *op);

struct s3_rsp_part_put *
s3_rsp_part_put(struct op *op);

#endif /* ifdef _S3_REQ_H_ */
