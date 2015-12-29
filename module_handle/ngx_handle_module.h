#ifndef __NGX_HTTP_ADHANDLER_MODULE_H__
#define __NGX_HTTP_ADHANDLER_MODULE_H__

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/time.h>

typedef struct {
    u_char*   request_body;
} ngx_http_request_body_plugin_ctx_t;

extern ngx_module_t ngx_http_handle_module;

#endif
