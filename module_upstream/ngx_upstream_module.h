#ifndef _NGX_ADUPSTREAM_STRUCT_H_INCLUDED_
#define _NGX_ADUPSTREAM_STRUCT_H_INCLUDED_

#if __cplusplus
extern "C" {
#endif

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/time.h>

typedef struct {
    ngx_http_upstream_conf_t     upstream;
    void                         *plugin;
} ngx_http_proto_upstream_loc_conf_t;

typedef struct {
    int create_req;
    int do_upstream;
    int process_header;
    int do_filter;
    int until_final;
} ngx_http_up_time_data_t;

typedef struct {
    ngx_http_request_t      *request;
    ngx_str_t               message;
    int                     has_processed;
    unsigned int            status_code;
    void                    *crequest;
    struct timeval          tv_begin;
    struct timeval          tv_end;
    ngx_http_up_time_data_t time_data;
} ngx_http_proto_upstream_ctx_t;

#if __cplusplus
}
#endif

#endif //_NGX_ADUPSTREAM_STRUCT_H_INCLUDED_
