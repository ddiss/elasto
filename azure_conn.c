/*
 * Copyright (C) SUSE LINUX 2012, all rights reserved
 *
 * Author: ddiss@suse.de
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <curl/curl.h>
#include <libxml/tree.h>

#include "azure_req.h"
#include "azure_conn.h"

static size_t
curl_read_cb(char *ptr,
	     size_t size,
	     size_t nmemb,
	     void *userdata)
{
	struct azure_req *req = (struct azure_req *)userdata;
	uint64_t num_bytes = (size * nmemb);

	if (req->iov.off + num_bytes > req->iov.buf_len) {
		printf("fatal: curl_read_cb buffer exceeded, "
		       "len %lu off %lu io_sz %lu\n",
		       req->iov.buf_len, req->iov.off, num_bytes);
		return -1;
	}

	memcpy(ptr, (void *)(req->iov.buf + req->iov.off), num_bytes);
	req->iov.off += num_bytes;
	return num_bytes;
}

static size_t
curl_write_cb(char *ptr,
	      size_t size,
	      size_t nmemb,
	      void *userdata)
{
	struct azure_req *req = (struct azure_req *)userdata;
	uint64_t num_bytes = (size * nmemb);

	if (req->iov.off + num_bytes > req->iov.buf_len) {
		printf("fatal: curl_write_cb buffer exceeded, "
		       "len %lu off %lu io_sz %lu\n",
		       req->iov.buf_len, req->iov.off, num_bytes);
		return -1;
	}

	memcpy((void *)(req->iov.buf + req->iov.off), ptr, num_bytes);
	req->iov.off += num_bytes;
	return num_bytes;
}

static size_t
curl_fail_cb(char *ptr,
	     size_t size,
	     size_t nmemb,
	     void *userdata)
{
	printf("Failure: server body data when not expected!\n");
	return -1;
}

static int
azure_conn_send_prepare(struct azure_conn *aconn, struct azure_req *req)
{
	/* XXX we need to clear preset opts when reusing */
	req->http_hdr = curl_slist_append(req->http_hdr,
					  "x-ms-version: 2012-03-01");
	if (req->http_hdr == NULL) {
		return -ENOMEM;
	}
	curl_easy_setopt(aconn->curl, CURLOPT_HTTPHEADER, req->http_hdr);
	curl_easy_setopt(aconn->curl, CURLOPT_CUSTOMREQUEST, req->method);
	curl_easy_setopt(aconn->curl, CURLOPT_URL, req->url);
	/* one-way xfers only so far */
	if (strcmp(req->method, REQ_METHOD_GET) == 0) {
		curl_easy_setopt(aconn->curl, CURLOPT_WRITEDATA, req);
		curl_easy_setopt(aconn->curl, CURLOPT_WRITEFUNCTION,
				 curl_write_cb);
		curl_easy_setopt(aconn->curl, CURLOPT_READFUNCTION,
				 curl_fail_cb);
	} else if (strcmp(req->method, REQ_METHOD_PUT) == 0) {
		curl_easy_setopt(aconn->curl, CURLOPT_READDATA, req);
		curl_easy_setopt(aconn->curl, CURLOPT_READFUNCTION,
				 curl_read_cb);
		curl_easy_setopt(aconn->curl, CURLOPT_WRITEFUNCTION,
				 curl_fail_cb);
	}

	return 0;	/* FIXME detect curl_easy_setopt errors */
}

int
azure_conn_send_req(struct azure_conn *aconn,
		    struct azure_req *req)
{
	int ret;
	CURLcode res;

	ret = azure_conn_send_prepare(aconn, req);
	if (ret < 0) {
		return ret;
	}

	/* dispatch */
	res = curl_easy_perform(aconn->curl);
	if (res != CURLE_OK) {
		printf("curl_easy_perform() failed: %s\n",
		       curl_easy_strerror(res));
		curl_easy_setopt(aconn->curl, CURLOPT_HTTPHEADER, NULL);
		return -EBADF;
	}

	/* reset headers, so that req->http_hdr can be freed */
	curl_easy_setopt(aconn->curl, CURLOPT_HTTPHEADER, NULL);

	return 0;
}

int
azure_conn_init(const char *pem_file,
		const char *pem_pw,
		struct azure_conn *aconn)
{
	aconn->curl = curl_easy_init();
	if (aconn->curl == NULL) {
		return -ENOMEM;
	}

	curl_easy_setopt(aconn->curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(aconn->curl, CURLOPT_SSLCERT, pem_file);
	curl_easy_setopt(aconn->curl, CURLOPT_SSLKEYTYPE, "PEM");
	curl_easy_setopt(aconn->curl, CURLOPT_SSLKEY, pem_file);
	if (pem_pw) {
		curl_easy_setopt(aconn->curl, CURLOPT_KEYPASSWD, pem_pw);
	}

	return 0;
}

void
azure_conn_free(struct azure_conn *aconn)
{
	curl_easy_cleanup(aconn->curl);
}

int
azure_conn_subsys_init(void)
{
	CURLcode res;

	res = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (res != CURLE_OK)
		return -ENOMEM;

	return 0;
}

void
azure_conn_subsys_deinit(void)
{
	curl_global_cleanup();
}
