#ifndef __NGX_HTTP_SUBREQUEST_MODULE_H__
#define __NGX_HTTP_SUBREQUEST_MODULE_H__

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/time.h>

typedef enum {
    MODULE_STATE_INIT,
    MODULE_STATE_PROCESS,
    MODULE_STATE_WAIT_SUBREQUEST,
    MODULE_STATE_POST_SUBREQUEST,
    MODULE_STATE_FINAL,
    MODULE_STATE_DONE,
    MODULE_STATE_ERROR
} subrequest_state_t;


typedef struct {
    ngx_str_t           uri;
    ngx_str_t           args;
    ngx_http_request_t  *subr;   
} subrequest_t;


typedef struct {
    struct timeval      time_start;
    struct timeval      time_end;
    subrequest_state_t  state;
    ngx_array_t         *subrequests;

    void                *request;
    void                *plugin;
} ngx_http_subrequest_ctx_t;

extern ngx_module_t  ngx_http_subrequest_module;

#endif
