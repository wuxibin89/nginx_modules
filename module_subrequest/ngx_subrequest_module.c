#include "ngx_subrequest_module.h"

#include "ngx_subrequest_interface.h"
#include "utils.h"

static char *ngx_http_subrequest_cb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_subrequest_handler(ngx_http_request_t *r);


static ngx_command_t  ngx_http_subrequest_commands[] = {

    { ngx_string("nginx_subrequest"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
      ngx_http_subrequest_cb,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_subrequest_module_ctx = {
    NULL,                                   /* preconfiguration */
    NULL,                                   /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    NULL,                                   /* create location configuration */
    NULL                                    /* merge location configuration */
};


ngx_module_t  ngx_http_subrequest_module = {
    NGX_MODULE_V1,
    &ngx_http_subrequest_module_ctx,        /* module context */
    ngx_http_subrequest_commands,           /* module directives */
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


static char *ngx_http_subrequest_cb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module); 
    clcf->handler = ngx_http_subrequest_handler; 

    return NGX_OK;
}


static ngx_int_t ngx_http_subrequest_handler(ngx_http_request_t *r) {
    ngx_int_t rc;
    ngx_http_subrequest_ctx_t *ctx;
    
    ctx = ngx_http_get_module_ctx(r, ngx_http_subrequest_module);    
    if(ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_subrequest_ctx_t));
        if(ctx == NULL) {
            return NGX_ERROR;
        }

        /*
         * set by ngx_pcalloc():
         *
         *  ctx->subrequests = NULL;
         *  ctx->plugin_ctx = NULL;
         */

        ngx_http_set_ctx(r, ctx, ngx_http_subrequest_module);

        ctx->state= MODULE_STATE_INIT;
        gettimeofday(&ctx->time_start, NULL);
    }

    if(ctx->state == MODULE_STATE_INIT) {
        rc = plugin_init_request(r);

        if(rc == NGX_OK) {              
            /* GET or POST body read completely */
            ctx->state = MODULE_STATE_PROCESS;
        } else if(rc == NGX_AGAIN) {    
            /* 
             * POST body read incompletely
             * Don't need r->main->count++ because ngx_http_read_client_body 
             * has already increased main request count.
             */
            return NGX_DONE;
        } else {
            ctx->state = MODULE_STATE_ERROR;
        }
    }

    if(ctx->state == MODULE_STATE_PROCESS) {
        rc = plugin_process_request(r);

        if(rc == NGX_OK) {
            ctx->state = MODULE_STATE_FINAL;
        } else if(rc == NGX_AGAIN) {
            ctx->state = MODULE_STATE_WAIT_SUBREQUEST;

            r->main->count++;
            return NGX_DONE;
        } else {
            ctx->state = MODULE_STATE_ERROR;
        }
    }

    if(ctx->state == MODULE_STATE_WAIT_SUBREQUEST) {
        rc = plugin_check_subrequest(r);

        if(rc == NGX_OK) {
            ctx->state = MODULE_STATE_POST_SUBREQUEST;
        } else if(rc == NGX_AGAIN) {
            /* ctx->state = MODULE_STATE_WAIT_SUBREQUEST; */
            r->main->count++;

            return NGX_DONE;
        } else {
            ctx->state = MODULE_STATE_ERROR;
        }
    }

    if(ctx->state == MODULE_STATE_POST_SUBREQUEST) {
        rc = plugin_post_subrequest(r); 

        if(rc == NGX_OK) {
            ctx->state = MODULE_STATE_FINAL;
        } else if(rc == NGX_AGAIN) {
            ctx->state = MODULE_STATE_WAIT_SUBREQUEST;
            r->main->count++;

            return NGX_DONE;
        } else {
            ctx->state = MODULE_STATE_ERROR;
        }
    }   

    if(ctx->state == MODULE_STATE_FINAL) {
        ctx->state = MODULE_STATE_DONE; 

        gettimeofday(&ctx->time_end, NULL);
        int ts = ctx->time_end.tv_sec- ctx->time_start.tv_sec;
        int tms = (ctx->time_end.tv_usec - ctx->time_start.tv_usec) / 1000;
        if(tms < 0) {
            ts--;
            tms += 1000;
        }


        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, 
                "[subrequest] final request, time consume: %ds.%dms", ts, tms);

        return plugin_final_request(r);
    }

    if(ctx->state == MODULE_STATE_DONE) {
        return plugin_done_request(r); 
    }

    if(ctx->state == MODULE_STATE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                "[subrequest] plugin process error, destory request context");

        /* destroy request context exactly once */
        plugin_destroy_request(r);

        return NGX_ERROR;
    }

    return NGX_OK;
}
