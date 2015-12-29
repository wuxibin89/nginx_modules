#include "ngx_subrequest_interface.h"

#include "ngx_subrequest_module.h"
#include "plugin.h"
#include "utils.h"

#include <ctype.h>
#include <stdint.h>
#include <string>
#include <iostream>

using namespace std;
using namespace plugin;

static ngx_int_t start_subrequest(ngx_http_request_t *r);
static ngx_int_t init_plugin(ngx_http_request_t *r);

static void post_body(ngx_http_request_t *r);

static ngx_int_t subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);

/*--------------------------------public function-----------------------------*/
/*
 * @return
 *      NGX_OK      GET request;
 *                  POST request with body read completely. 
 *      NGX_AGAIN   POST request and body read incompletely.
 *      NGX_ERROR   POST request read client request body error.
 */
ngx_int_t plugin_init_request(ngx_http_request_t *r) {
    if(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD)) {
        return NGX_OK;
    } 

    if(r->method & NGX_HTTP_POST) {
        return ngx_http_read_client_request_body(r, post_body);
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
            "[subrequest] only GET/HEAD/POST request accepted");

    return NGX_ERROR;
}

/*
 * @return 
 *      NGX_OK      plugin process request sucess   
 *      NGX_AGAIN   plugin has subrequest to be processed
 *      NGX_ERROR   plugin process reuquest fail
 */
ngx_int_t plugin_process_request(ngx_http_request_t *r) {
    ngx_int_t rc;
    ngx_http_subrequest_ctx_t *ctx;

    /* create context for each http request exactly once */
    rc = init_plugin(r);
    if(rc != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "[subrequest] plugin create CRequest error");

        return NGX_ERROR;
    }

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest*)ctx->request;
    CPlugin *plugin = (CPlugin *)ctx->plugin;

    rc = plugin->Handle(*request);

    if(rc == CPlugin::PLUGIN_AGAIN) {
        rc = start_subrequest(r);
        if(rc != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[subrequest] plugin start subrequest error");
            
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    }

    /* assert(rc = PLUGIN_OK/PLUGIN_ERROR) */
    return NGX_OK;
}

/*
 * @return 
 *      NGX_OK          all subrequests have been done
 *      NGX_AGAIN       some subrequests haven't been done
 */
ngx_int_t plugin_check_subrequest(ngx_http_request_t *r) {
    size_t                  n;
    subrequest_t            *st;
    ngx_http_upstream_t     *up;
    ngx_http_subrequest_ctx_t  *ctx;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest *)ctx->request;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "[subrequest] plugin check subrequest, count = %d", r->main->count);

    st = (subrequest_t *)ctx->subrequests->elts; 
    n = ctx->subrequests->nelts;
    for(size_t i = 0; i < n; i++, st++) {
        if(st->subr->done != 1) {
            return NGX_AGAIN;
        } 
    }

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "[subrequest] all subrequest done");

    st = (subrequest_t *)ctx->subrequests->elts; 
    vector<CSubrequest>::iterator it = request->subrequest.begin();
    for(size_t i = 0; i < n; i++, st++, it++) {
        up = st->subr->upstream;
        if(up == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                    "[subrequest] plugin subrequest upstream null, location not found ?");

            return NGX_ERROR;
        }

        it->status = up->state->status;
        it->sec = up->state->response_sec;
        it->usec = up->state->response_msec;
        it->response = string((char *)up->buffer.pos, up->buffer.last - up->buffer.pos);  
    }

    return NGX_OK;
}


/*
 * @return 
 *      NGX_OK      plugin process request sucess   
 *      NGX_AGAIN   plugin has subrequest to be processed
 *      NGX_ERROR   plugin process reuquest fail
 */
ngx_int_t plugin_post_subrequest(ngx_http_request_t *r) {
    ngx_int_t rc;
    ngx_http_subrequest_ctx_t *ctx;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest *)ctx->request;
    CPlugin *plugin = (CPlugin *)ctx->plugin;

    rc = plugin->ProcessBody(*request);

    if(rc == CPlugin::PLUGIN_AGAIN) {
        rc = start_subrequest(r);
        if(rc != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[subrequest] plugin start subrequest error");
            
            return NGX_ERROR;
        }

        return NGX_AGAIN;
    } 

    /* assert(rc = PLUGIN_OK/PLUGIN_ERROR) */
    return NGX_OK;
}


/*
 * @return 
 *      NGX_OK          output filter body complete
 *      NGX_AGAIN       output filter body incomplete
 *      NGX_ERROR       plugin finalize request fail
 */
ngx_int_t plugin_final_request(ngx_http_request_t *r) {
    ngx_http_subrequest_ctx_t  *ctx;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest *)ctx->request;
    CPlugin *plugin = (CPlugin *)ctx->plugin;
    
    ngx_int_t rc = crequest_to_ngxr(request, r);

    if(request) {
        plugin->Destroy(*request);
        delete request;
    }
    request = NULL;

    return rc;
}

ngx_int_t plugin_done_request(ngx_http_request_t *r) {
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
            "[subrequest] done request, count = %d", r->main->count);

    return ngx_http_output_filter(r, NULL);
}

void plugin_destroy_request(ngx_http_request_t *r) {
    ngx_http_subrequest_ctx_t *ctx;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest *)ctx->request;
    CPlugin *plugin = (CPlugin *)ctx->plugin;

    if(request) {
        plugin->Destroy(*request);
        delete request;
    }
    request = NULL;
}


/*---------------------------- local function --------------------------------*/
static ngx_int_t start_subrequest(ngx_http_request_t *r) {
    size_t n;
    subrequest_t *st;
    ngx_http_subrequest_ctx_t *ctx;
    ngx_http_post_subrequest_t *psr;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);
    CRequest *request = (CRequest *)ctx->request;

    /* destroy subrequests created before */
    ngx_int_t rc = NGX_OK;
    if(ctx->subrequests) {
        rc = ngx_array_init(ctx->subrequests, r->pool, 1, sizeof(subrequest_t));
    } else {
        ctx->subrequests = ngx_array_create(r->pool, 1, sizeof(subrequest_t)); 
    }

    if(rc != NGX_OK) {
        return NGX_ERROR;
    }

    n = request->subrequest.size();
    for(size_t i = 0; i < n; i++) {
        CSubrequest& ups = request->subrequest[i];
        
        st = (subrequest_t *)ngx_array_push(ctx->subrequests); 
        if(st == NULL) {
            return NGX_ERROR;
        }

        st->uri.data = (u_char *)ngx_pcalloc(r->pool, ups.uri.length());
        if(st->uri.data == NULL) {
            return NGX_ERROR;
        }
        ngx_memcpy(st->uri.data, ups.uri.c_str(), ups.uri.length()); 
        st->uri.len = ups.uri.length();

        st->args.data = (u_char *)ngx_pcalloc(r->pool, ups.args.length());
        if(st->args.data == NULL) {
            return NGX_ERROR;
        }
        ngx_memcpy(st->args.data, ups.args.c_str(), ups.args.length()); 
        st->args.len = ups.args.length();
    }

    n = ctx->subrequests->nelts;
    st = (subrequest_t *)ctx->subrequests->elts;
    for(size_t i = 0; i < n; i++, st++) {
        int flags = NGX_HTTP_SUBREQUEST_IN_MEMORY | NGX_HTTP_SUBREQUEST_WAITED;

        psr = (ngx_http_post_subrequest_t *)ngx_palloc(r->pool, 
                sizeof(ngx_http_post_subrequest_t));
        if(psr == NULL) {
            return NGX_ERROR;
        }

        psr->handler = subrequest_post_handler;
        psr->data = NULL;

        ngx_int_t rc = ngx_http_subrequest(r, &st->uri, &st->args, 
                &st->subr, psr, flags);

        if(rc != NGX_OK) 
            return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc) {
    (void)data;
    (void)rc;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
            "[subrequest] subrequest finish %V?%V", &r->uri, &r->args);

    r->parent->write_event_handler = ngx_http_core_run_phases;

    return NGX_OK;
}


static void post_body(ngx_http_request_t *r) {
    ngx_http_subrequest_ctx_t *ctx;

    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);

    /* read whole request body at first time */
    if(ctx->state == MODULE_STATE_INIT) {
        ngx_http_finalize_request(r, NGX_DONE);

        return;
    }

    /* read request body in multiple times */
    ctx->state = MODULE_STATE_PROCESS;

    ngx_http_finalize_request(r, r->content_handler(r));
    ngx_http_run_posted_requests(r->connection);
}


static ngx_int_t init_plugin(ngx_http_request_t *r) {
    ngx_str_t  var;
    ngx_uint_t  hash;
    ngx_http_variable_value_t   *vv;
    ngx_http_subrequest_ctx_t   *ctx;
    
    ctx = (ngx_http_subrequest_ctx_t *)ngx_http_get_module_ctx(r, ngx_http_subrequest_module);    

    /* get plugin */
    var.len  = sizeof ("plugin_name") - 1;
    var.data = (u_char *) "plugin_name";
    
    hash = ngx_hash_key ((u_char*)"plugin_name", sizeof ("plugin_name")-1);
    vv = ngx_http_get_variable(r, &var, hash);
    if (vv == NULL || vv->not_found || vv->len==0) {
    	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                    "[subrequest] plugin_name variable is not found!");
    
    	return NGX_ERROR;
    }

    CPlugin *plugin = (CPlugin*)plugin_getbyname((char *)vv->data, vv->len);
    if(plugin == NULL) {
	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "[subrequest] plugin handler init failed!");

	return NGX_ERROR;
    }

    ctx->plugin = plugin;


    /* convert request */
    CRequest *request = new CRequest(); 
    if(request == NULL) {
        return NGX_ERROR;
    }

    gettimeofday(&request->start_time, NULL);
    ngxr_to_crequest(r, request);

    ctx->request = request;

    return NGX_OK;
}
