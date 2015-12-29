#include "ngx_handle_interface.h"

#include "plugin.h"
#include "utils.h"

static ngx_int_t ngx_http_handler_init(ngx_http_request_t *r);
ngx_int_t ngx_http_handler_process(ngx_http_request_t *r);

/* plugin handler*/
void *handle_plugin = NULL;

static ngx_int_t ngx_http_handler_init(ngx_http_request_t *r) {
    ngx_str_t  var;
    ngx_uint_t  hash;
    ngx_http_variable_value_t  *vv;

    var.len  = sizeof ("plugin_name") - 1;
    var.data = (u_char *) "plugin_name";

    hash = ngx_hash_key ((u_char*)"plugin_name", sizeof ("plugin_name")-1);
    vv = ngx_http_get_variable(r, &var, hash);
    if (vv == NULL || vv->not_found || vv->len==0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "[handle] plugin_name variable is not found!");
        return NGX_ERROR;
    }

    handle_plugin= (plugin::CPlugin*)plugin_getbyname((char *)vv->data,vv->len);
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "[handle] plugin  handler  init!");

    //exist hande_plugin
    if(handle_plugin == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "[handle] plugin name %s handler   init failed!",vv->data);
        return NGX_ERROR;
    }

    return NGX_OK;
}


ngx_int_t ngx_http_handler_process(ngx_http_request_t *r) {
    plugin::CRequest request;
    size_t	zero = 0;
    ngx_int_t               rc;

    //init 
    rc = ngx_http_handler_init(r);
    if(rc != NGX_OK) {
        r->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        r->headers_out.content_length_n = zero;

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //get request headler
    rc = ngxr_to_crequest(r,&request);
    if(rc != NGX_OK) {
        r->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        r->headers_out.content_length_n = zero;
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "[handle] ngx_request_t to crequest  is error!");

        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //process buffer
    rc = ((plugin::CPlugin*)handle_plugin)->Handle(request);
    if(rc != NGX_OK) {
        r->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
        r->headers_out.content_length_n = zero;

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "[handle] plugin Handler is error!");
    }


    //set response header body
    return crequest_to_ngxr(&request,r);
}
