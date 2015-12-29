#include "ngx_upstream_interface.h"

#include "ngx_upstream_module.h"
#include "plugin.h"
#include "utils.h"

/*
 * @return
 *      NGX_OK      GET request;
 *                  POST request with body read completely. 
 *      NGX_AGAIN   POST request and body read incompletely.
 *      NGX_ERROR   POST request read client request body error.
 */
void * ngx_upstream_create_crequest()
{
    return new plugin::CRequest;
}

void ngx_upstream_destroy_crequest(void *request_handler, void * crequest)
{
    plugin::CRequest * c = (plugin::CRequest *)crequest;

    if (request_handler) {
        plugin::CPlugin * plugin = (plugin::CPlugin *)request_handler;
        plugin->Destroy(*c);
    }

    if (c) {
        delete c;
        c = NULL;
    }
}

ngx_int_t ngx_upstream_create_request(void *request_handler, void *ctx)
{
    if (request_handler == NULL) {
        return NGX_ERROR;
    }

    ngx_http_proto_upstream_ctx_t * c = (ngx_http_proto_upstream_ctx_t*)ctx;
    plugin::CRequest * crequest = (plugin::CRequest *)c->crequest;
    
    plugin::CPlugin * plugin = (plugin::CPlugin *)request_handler; 
    int ret = plugin->Handle(*crequest);

    if (ret) {
        if (ret == NGX_UPSTREAM_NOSEND) {
            return NGX_UPSTREAM_NOSEND;
        }
        return NGX_ERROR;
    }

    c->message.data = (u_char*)(const_cast<char*>(crequest->response_body.c_str()));
    c->message.len = crequest->response_body.length();

    return NGX_OK;
}
/*
 * @return 
 *      NGX_OK      plugin process request sucess   
 *      NGX_AGAIN   plugin has subrequest to be processed
 *      NGX_ERROR   plugin process reuquest fail
 */

ngx_int_t ngx_upstream_process_body(void *request_handler, void *ctx, ngx_buf_t * upstream_buf)
{
    if (request_handler == NULL || ctx == NULL || upstream_buf == NULL) {
        return NGX_ERROR;
    }

    ngx_http_proto_upstream_ctx_t * c = (ngx_http_proto_upstream_ctx_t*)ctx;
    plugin::CRequest * crequest = (plugin::CRequest *)c->crequest;
    plugin::CPlugin * plugin = (plugin::CPlugin *)request_handler;

    size_t len = c->request->upstream->buffer.last - c->request->upstream->buffer.pos;
	
    if (len ==0) {
		ngx_log_error(NGX_LOG_WARN, c->request->connection->log, 0, 
                    "[upstream_process_body] buffer length is 0!");
    }
	
    crequest->response_body = std::string((char*)c->request->upstream->buffer.pos, len);
    if (c->status_code) {
        crequest->headers_out.status = c->status_code;
    }
    
    int res = plugin->ProcessBody(*crequest);

    upstream_buf->pos = (u_char*)crequest->response_body.c_str();
    upstream_buf->last = upstream_buf->pos + crequest->response_body.length();

    if(res) {
        return NGX_ERROR;
    }

	ngx_http_request_t      *r = c->request;
    ngx_http_upstream_t     *u = r->upstream;
    u->headers_in.status_n = 200;
    u->state->status = 200;
    u->headers_in.content_length_n = u->buffer.last - u->buffer.pos;
	
    if (crequest->headers_out.content_type.length() == 0) {
		ngx_str_set(&r->headers_out.content_type, "text/html"); 
		r->headers_out.content_type_len = sizeof("text/html") - 1;
    } else {
        r->headers_out.content_type.data = (u_char *)crequest->headers_out.content_type.c_str();
        r->headers_out.content_type.len = crequest->headers_out.content_type.length();
        r->headers_out.content_type_len = crequest->headers_out.content_type.length();
	}

    return NGX_OK;
}
