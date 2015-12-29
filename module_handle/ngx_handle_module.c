#include "ngx_handle_module.h"
#include "ngx_handle_interface.h"

static char *ngx_http_handle(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_handle_handler(ngx_http_request_t *r);
static void ngx_http_handle_post_handler(ngx_http_request_t *r);

static ngx_command_t  ngx_http_handle_commands[] = {

    { ngx_string("nginx_handle"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
      ngx_http_handle,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_handle_module_ctx = {
    NULL,                                   /* preconfiguration */
    NULL,                                   /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    NULL,                                   /* create location configuration */
    NULL                                    /* merge location configuration */
};


ngx_module_t  ngx_http_handle_module = {
    NGX_MODULE_V1,
    &ngx_http_handle_module_ctx,         /* module context */
    ngx_http_handle_commands,            /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *ngx_http_handle(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module); 
    clcf->handler = ngx_http_handle_handler;

    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_handle_handler(ngx_http_request_t *r) {
	/* we response to 'GET' and 'HEAD' requests only */
	if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD | NGX_HTTP_POST))) {
			return NGX_HTTP_NOT_ALLOWED;
	}

	//post request_body
	if (r->method & NGX_HTTP_POST) {
	    return ngx_http_read_client_request_body(r, ngx_http_handle_post_handler);
	}
	
	return ngx_http_handler_process(r);	
}


static void ngx_http_handle_post_handler(ngx_http_request_t *r) {
	ngx_int_t	rc;
	
	rc = ngx_http_handler_process(r);
	ngx_http_finalize_request(r,rc);
}
