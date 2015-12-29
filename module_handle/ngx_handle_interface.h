#ifndef __NGX_HANDLER_INTERFACE_H__
#define __NGX_HANDLER_INTERFACE_H__

#if __cplusplus
extern "C" {
#endif

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_handle_module.h"

ngx_int_t ngx_http_handler_process(ngx_http_request_t *r);


#if __cplusplus
}
#endif

#endif  
