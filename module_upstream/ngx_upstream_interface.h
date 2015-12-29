#ifndef __NGX_UPSTREAM_INTERACE_H_INCLUDED_
#define __NGX_UPSTREAM_INTERACE_H_INCLUDED_

#if __cplusplus
extern "C" {
#endif

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_UPSTREAM_NOSEND -10000

/* request api */
void * ngx_upstream_create_crequest();

void ngx_upstream_destroy_crequest(void *request_handler, void * crequest);

ngx_int_t ngx_upstream_create_request(void *request_handler, void *ctx);

ngx_int_t ngx_upstream_process_body(void *request_handler, void *ctx, ngx_buf_t * upstream_buf);


#if __cplusplus
}
#endif

#endif //__NGX_UPSTREAM_INTERACE_H_INCLUDED_
