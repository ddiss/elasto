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
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "ccan/list/list.h"
#include "azure_xml.h"
#include "azure_req.h"
#include "azure_conn.h"

int main(void)
{
	struct azure_conn aconn;
	struct azure_req req;
	const char *pem_file = "/home/ddiss/azure/privateKey.pem";
	const char *pem_pword = "disso";
	const char *subscriber_id = "9baf7f32-66ae-42ca-9ad7-220050765863";
	const char *blob_acc = "istgt";
	const char *blob_container = "target1";
	const char *blob_name = "test";
	struct azure_ctnr *ctnr;
	bool ctnr_exists;
	int ret;
	uint8_t *buf;

	azure_conn_subsys_init();
	azure_xml_subsys_init();

	memset(&req, 0, sizeof(req));

	ret = azure_conn_init(pem_file, pem_pword, &aconn);
	if (ret < 0) {
		goto err_global_clean;
	}

	ret = azure_req_mgmt_get_sa_keys(subscriber_id, blob_acc, &req);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = azure_conn_send_req(&aconn, &req);
	if (ret < 0) {
		goto err_req_free;
	}

	ret = azure_req_mgmt_get_sa_keys_rsp(&req);
	if (ret < 0) {
		goto err_req_free;
	}

	printf("primary key: %s\n"
	       "secondary key: %s\n",
	       req.mgmt_get_sa_keys.out.primary,
	       req.mgmt_get_sa_keys.out.secondary);

	ret = azure_conn_sign_setkey(&aconn, blob_acc,
				     req.mgmt_get_sa_keys.out.primary);
	if (ret < 0) {
		goto err_req_free;
	}

	azure_req_free(&req);

	ret = azure_req_ctnr_list(blob_acc, &req);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = azure_conn_send_req(&aconn, &req);
	if (ret < 0) {
		goto err_req_free;
	}

	ret = azure_req_ctnr_list_rsp(&req);
	if (ret < 0) {
		goto err_req_free;
	}

	ctnr_exists = false;
	list_for_each(&req.ctnr_list.out.ctnrs, ctnr, list) {
		if (strcmp(ctnr->name, blob_container) == 0) {
			ctnr_exists = true;
			break;
		}
	}

	azure_req_free(&req);

	if (ctnr_exists == false) {
		ret = azure_req_ctnr_create(blob_acc, blob_container, &req);
		if (ret < 0) {
			goto err_conn_free;
		}
		/*
		 * returns:
		 * < HTTP/1.1 201 Created
		 * < HTTP/1.1 409 The specified container already exists.
		 */

		ret = azure_conn_send_req(&aconn, &req);
		if (ret < 0) {
			goto err_req_free;
		}

		azure_req_free(&req);
	}

	ret = azure_req_blob_put(blob_acc, blob_container, blob_name,
				 false, 0,
				 (uint8_t *)strdup("hello world"),
				 sizeof("hello world"),
				 &req);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = azure_conn_send_req(&aconn, &req);
	if (ret < 0) {
		goto err_req_free;
	}

	azure_req_free(&req);

	buf = malloc(sizeof("hello world"));
	ret = azure_req_blob_get(blob_acc, blob_container, blob_name,
				 buf,
				 sizeof("hello world"),
				 &req);
	if (ret < 0) {
		goto err_conn_free;
	}

	ret = azure_conn_send_req(&aconn, &req);
	if (ret < 0) {
		goto err_req_free;
	}

	printf("data consistency test: %s\n",
	       strcmp((char *)buf, "hello world") ? "failed" : "passed");

	ret = 0;
err_req_free:
	azure_req_free(&req);
err_conn_free:
	azure_conn_free(&aconn);
err_global_clean:
	azure_xml_subsys_deinit();
	azure_conn_subsys_deinit();

	return ret;
}
