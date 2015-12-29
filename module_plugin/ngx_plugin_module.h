#ifndef __NGX_HTTP_PLUGIN_MODULE_H__
#define __NGX_HTTP_PLUGIN_MODULE_H__

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t   name;
    ngx_str_t   so_path;
    ngx_str_t   so_conf;
} plugin_conf_t;

typedef struct {
    ngx_array_t plugin_conf;
} ngx_http_plugin_main_conf_t;


#endif
