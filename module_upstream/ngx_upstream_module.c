#include "ngx_upstream_module.h"

#include "ngx_upstream_interface.h"
#include "ngx_protocol.h"
#include "utils.h"

static ngx_int_t ngx_http_proto_upstream_init_plugin(ngx_http_request_t *r, ngx_http_proto_upstream_loc_conf_t  *mlcf);
static ngx_int_t ngx_http_proto_upstream_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_proto_upstream_reinit_request(ngx_http_request_t *r);
static ngx_int_t ngx_http_proto_upstream_process_header(ngx_http_request_t *r);
static ngx_int_t ngx_http_proto_upstream_filter_init(void *data);
static ngx_int_t ngx_http_proto_upstream_filter(void *data, ssize_t bytes);
static void ngx_http_proto_upstream_abort_request(ngx_http_request_t *r);
static void ngx_http_proto_upstream_finalize_request(ngx_http_request_t *r,
    ngx_int_t rc);

static void *ngx_http_proto_upstream_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_proto_upstream_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static ngx_int_t ngx_http_proto_upstream_handler(ngx_http_request_t *r);
static char *ngx_http_proto_upstream_pass(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_conf_bitmask_t  ngx_http_proto_upstream_next_upstream_masks[] = {
    { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
    { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
    { ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
    { ngx_string("not_found"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
    { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
    { ngx_null_string, 0 }
};


static ngx_command_t  ngx_http_proto_upstream_commands[] = {
    
    { ngx_string("upstream_pass"),
      NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
      ngx_http_proto_upstream_pass,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("upstream_timeout"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proto_upstream_loc_conf_t, upstream.connect_timeout),
      NULL },

    { ngx_string("upstream_buffer_size"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proto_upstream_loc_conf_t, upstream.buffer_size),
      NULL },

    { ngx_string("next_upstream"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_proto_upstream_loc_conf_t, upstream.next_upstream),
      &ngx_http_proto_upstream_next_upstream_masks },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_proto_upstream_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_proto_upstream_create_loc_conf,    /* create location configuration */
    ngx_http_proto_upstream_merge_loc_conf      /* merge location configuration */
};


ngx_module_t  ngx_http_proto_upstream_module = {
    NGX_MODULE_V1,
    &ngx_http_proto_upstream_module_ctx,        /* module context */
    ngx_http_proto_upstream_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_http_proto_upstream_init_plugin(ngx_http_request_t *r, ngx_http_proto_upstream_loc_conf_t  *mlcf)
{
    ngx_str_t  var;
    ngx_uint_t  hash;
    ngx_http_variable_value_t   *vv;
    
    var.len  = sizeof ("plugin_name") - 1;
    var.data = (u_char *) "plugin_name";
    
    hash = ngx_hash_key ((u_char*)"plugin_name", sizeof ("plugin_name")-1);
    vv = ngx_http_get_variable(r, &var, hash);
    if (vv == NULL || vv->not_found || vv->len==0) {
    	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                    "[proto_upstream] plugin_name variable is not found!");
    
    	return NGX_ERROR;
    }

    mlcf->plugin = plugin_getbyname((char *)vv->data, vv->len);
	
    if(mlcf->plugin == NULL) {
	    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, 
                "[proto_upstream] plugin handler is null_ptr!");

	    return NGX_ERROR;
    }
    
    return NGX_OK;
}

static void *
ngx_http_proto_upstream_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_proto_upstream_loc_conf_t  *conf = NULL;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_proto_upstream_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

    /* the hardcoded values */
    conf->upstream.store_access = 0600;
    conf->upstream.buffering = 0;
    conf->upstream.bufs.num = 8;
    conf->upstream.bufs.size = ngx_pagesize;
    conf->upstream.busy_buffers_size = 2 * ngx_pagesize;
    conf->upstream.temp_file_write_size = 2 * ngx_pagesize;
    conf->upstream.max_temp_file_size = 1024 * 1024 * 1024;
    conf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    conf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

/*    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;
*/
    conf->plugin = NULL;

    return conf;
}


static char *
ngx_http_proto_upstream_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_proto_upstream_loc_conf_t *prev = parent;
    ngx_http_proto_upstream_loc_conf_t *conf = child;

    ngx_conf_merge_ptr_value(conf->upstream.local,
                              prev->upstream.local, NULL);

    ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
                              prev->upstream.connect_timeout, 60000);
    
    conf->upstream.send_timeout = conf->upstream.connect_timeout;

    conf->upstream.read_timeout = conf->upstream.connect_timeout;

    ngx_conf_merge_size_value(conf->upstream.buffer_size,
                              prev->upstream.buffer_size,
                              8096);

    ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
                              prev->upstream.next_upstream,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_UPSTREAM_FT_ERROR
                               |NGX_HTTP_UPSTREAM_FT_TIMEOUT));

    if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
        conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
                                       |NGX_HTTP_UPSTREAM_FT_OFF;
    }

    if (conf->upstream.upstream == NULL) {
        conf->upstream.upstream = prev->upstream.upstream;
    }
   /* 
    char temp[128];
    memcpy(temp, conf->plugin_name.data, conf->plugin_name.len);
    temp[conf->plugin_name.len] = '\0';
    printf("after merge plugin_name: %s\n\
                time_out: %d\n\
                buffer_size: %d\n", 
                temp, 
                (int)conf->upstream.connect_timeout,
                (int)conf->upstream.buffer_size);
*/
    return NGX_CONF_OK;
}


static char *
ngx_http_proto_upstream_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_proto_upstream_loc_conf_t *mlcf = conf;

    ngx_str_t                 *value = NULL;
    ngx_url_t                  u;
    ngx_http_core_loc_conf_t  *clcf = NULL;

    if (mlcf->upstream.upstream) {
        return "is duplicate";
    }

    value = cf->args->elts;

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.no_resolve = 1;

    /* parse upstream server configuration, it may be server address or server group */
    mlcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
    if (mlcf->upstream.upstream == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_proto_upstream_handler;

    if (clcf->name.data[clcf->name.len - 1] == '/') {
        clcf->auto_redirect = 1;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_proto_upstream_handler(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_http_upstream_t            *u;
    ngx_http_proto_upstream_ctx_t       *ctx;
    ngx_http_proto_upstream_loc_conf_t  *mlcf;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    u = r->upstream;

    ngx_str_set(&u->schema, "proto_upstream://");
    u->output.tag = (ngx_buf_tag_t) &ngx_http_proto_upstream_module;

    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_proto_upstream_module);

//    if (mlcf->plugin == NULL) {
        if (ngx_http_proto_upstream_init_plugin(r, mlcf)) {
            return NGX_ERROR;
        }
  //  }
    

    ctx = ngx_palloc(r->pool, sizeof(ngx_http_proto_upstream_ctx_t));

    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    gettimeofday(&ctx->tv_begin, NULL);
    ctx->has_processed = 0;
    ctx->status_code = 0;
    ctx->crequest = ngx_upstream_create_crequest();
    if (ngxr_to_crequest(r, ctx->crequest) ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngxr to crequest error, handler stop.");
        ngx_upstream_destroy_crequest(mlcf->plugin, ctx->crequest);
        return NGX_ERROR;
    }
    ctx->request = r;

    ngx_http_set_ctx(r, ctx, ngx_http_proto_upstream_module);
    
    int ret = ngx_upstream_create_request(mlcf->plugin, ctx);
        
    if (ret) {
        if (ret == NGX_UPSTREAM_NOSEND) {
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                        "[upstream_handler] don't need use upstream.");
        } else {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "[upstream_handler] create a request failure.");
        }
        if (crequest_to_ngxr(ctx->crequest, r)) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"[upstream_handler] crequest to ngxr error.");
        }
        ngx_upstream_destroy_crequest(mlcf->plugin, ctx->crequest);
        return NGX_DONE;
    }

    u->conf = &mlcf->upstream;

    u->create_request = ngx_http_proto_upstream_create_request;
    u->reinit_request = ngx_http_proto_upstream_reinit_request;
    u->process_header = ngx_http_proto_upstream_process_header;
    u->abort_request = ngx_http_proto_upstream_abort_request;
    u->finalize_request = ngx_http_proto_upstream_finalize_request;
    
    u->input_filter_init = ngx_http_proto_upstream_filter_init;
    u->input_filter = ngx_http_proto_upstream_filter;
    u->input_filter_ctx = ctx;

    r->main->count++;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "upstream handler process OK, start a upstream request.");

    gettimeofday(&ctx->tv_end, NULL);
    int interval = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);
    ctx->time_data.create_req = interval;
 
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "[upstream_handler] plugin create a request used %d us.", interval);


    ngx_http_upstream_init(r);
    return NGX_DONE;
}


static ngx_int_t
ngx_http_proto_upstream_create_request(ngx_http_request_t *r)
{
    ngx_buf_t                      *b = NULL;
    ngx_chain_t                    *cl = NULL;
    ngx_http_proto_upstream_ctx_t       *ctx = NULL;
    ngx_http_proto_upstream_loc_conf_t  *mlcf = NULL;

    if (r->request_line.data && r->uri.data) {
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "[upstream_create_request] start, request_line: \n\"%V\"\nuri: \"%V\"\n", &r->request_line, &r->uri);
    }
   
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_proto_upstream_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_proto_upstream_module);

    if (mlcf == NULL || ctx == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[upstream_create_request] %s is NULL.", ((mlcf == NULL)?"mlcf":"ctx"));
        return NGX_ERROR;
    }

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream_create_request] plugin create a upstream request: \"%V\"", &ctx->message);

    ngx_http_protocol_encode(&(ctx->message), r->pool, &b);
    cl = ngx_alloc_chain_link(r->pool);

    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;
    cl->next = NULL;
    r->upstream->request_bufs = cl;
    
/*    ngx_http_upstream_t            * u = r->upstream;
    printf("merge upstream param: %dms,%dms,%dms,%dbytes\n", (int)u->conf->connect_timeout,
                (int)u->conf->send_timeout, (int)u->conf->read_timeout, (int)u->conf->buffer_size);
*/
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream_create_request] plugin request has been ready, will send to backend \"%V\".",
                   &r->connection->addr_text);

    gettimeofday(&ctx->tv_begin, NULL);
    return NGX_OK;
}


static ngx_int_t
ngx_http_proto_upstream_reinit_request(ngx_http_request_t *r)
{
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream_reinit_request]");
    return NGX_OK;
}


static ngx_int_t
ngx_http_proto_upstream_process_header(ngx_http_request_t *r)
{
    ngx_http_proto_upstream_ctx_t       *ctx = 
        ngx_http_get_module_ctx(r, ngx_http_proto_upstream_module);
    
    ngx_http_upstream_t     *u;
    u = r->upstream;

    int length = ngx_http_protocol_decode(&(u->buffer));

    if (length < 0) {
        if (length == NGX_ADPROTOCOL_eERROR) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "head flag error.");
        } else if (length == NGX_ADPROTOCOL_eMOREDATA) {
            ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                        "recieve response NOT complete, recieve again. length now: %d", u->buffer.last - u->buffer.pos);
            return NGX_AGAIN;
        }
        return NGX_ERROR;
    }
    
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream_process_header] recieve a response from backend \"%V\", start process header."
                   , &r->connection->addr_text);
    
    gettimeofday(&ctx->tv_end, NULL);
    int interval = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);
    ctx->time_data.do_upstream = interval;

    gettimeofday(&ctx->tv_begin, NULL);
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream_process_header] decode response success, raw_body_message length: %d", length);
    
    ngx_http_proto_upstream_loc_conf_t  *mlcf = NULL;
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_proto_upstream_module);

    int ret = ngx_upstream_process_body(mlcf->plugin, ctx, &u->buffer);
    gettimeofday(&ctx->tv_end, NULL);
    ctx->has_processed = 1;
   
    if (ret) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"[upstream_process_header] process body error.");
    }
    
    interval = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);
    ctx->time_data.process_header = interval;

    //u->headers_in.content_length_n = u->buffer.last - u->buffer.pos;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "[upstream_process_header] process body used %d us, processed_body_message length: %d", interval, u->headers_in.content_length_n);
    gettimeofday(&ctx->tv_begin, NULL);
    return NGX_OK;
}

static ngx_int_t
ngx_http_proto_upstream_filter_init(void *data)
{
    ngx_http_proto_upstream_ctx_t  *ctx = data;
    ngx_http_upstream_t  *u = ctx->request->upstream;
    
    u->length = u->headers_in.content_length_n;

  //  ctx->request->subrequest_in_memory = 0;
    ngx_log_error(NGX_LOG_DEBUG, ctx->request->connection->log, 0,
                   "[upstream_filter_init]");
    return NGX_OK;
}


static ngx_int_t
ngx_http_proto_upstream_filter(void *data, ssize_t bytes)
{
    ngx_http_proto_upstream_ctx_t  *ctx = data;
    ngx_log_error(NGX_LOG_DEBUG, ctx->request->connection->log, 0,
                "[upstream_filter] start, bytes:%z", bytes);

    u_char               *last = NULL;
    ngx_buf_t            *b = NULL;
    ngx_chain_t          *cl, **ll;
    ngx_http_upstream_t  *u = NULL;

    u = ctx->request->upstream;
    b = &u->buffer;

    if(bytes > u->length) {
        ngx_log_error(NGX_LOG_ERR, ctx->request->connection->log, 0, 
                "[upstream_filter] backend \"%V\" response too long, discard this response.",
                &ctx->request->connection->addr_text);
        u->length = 0;
        return NGX_OK;
    }

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    cl = ngx_chain_get_free_buf(ctx->request->pool, &u->free_bufs);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf->flush = 1;
    cl->buf->memory = 1;

    *ll = cl;

    last = b->last;
    cl->buf->pos = last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    ngx_log_error(NGX_LOG_DEBUG, ctx->request->connection->log, 0,
                   "[upstream_filter] proto_upstream filter bytes:%z size:%z length:%z",
                   bytes, b->last - b->pos, u->length);

    u->length -= bytes;

    if(u->length == 0) {
        u->keepalive = 1;
    }
   
    gettimeofday(&ctx->tv_end, NULL);
    int interval = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);
    ctx->time_data.do_filter = interval;
    
    gettimeofday(&ctx->tv_begin, NULL);
    return NGX_OK;
}


static void
ngx_http_proto_upstream_abort_request(ngx_http_request_t *r)
{
    ngx_http_proto_upstream_ctx_t       *ctx = NULL;
    ngx_http_proto_upstream_loc_conf_t  *mlcf = NULL;
    ctx = ngx_http_get_module_ctx(r, ngx_http_proto_upstream_module);
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_proto_upstream_module);

    if (!ctx->has_processed) {
	struct timeval tv_begin, tv_end;
        gettimeofday(&tv_begin, NULL);
        int ret = ngx_upstream_process_body(mlcf->plugin, ctx, &r->upstream->buffer);
        gettimeofday(&tv_end, NULL);
        ctx->has_processed = 1;

        if (ret) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"[abort_request] process body error.");
        }

        int interval = 1000000*(tv_end.tv_sec-tv_begin.tv_sec)+(tv_end.tv_usec-tv_begin.tv_usec);
        
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "[abort_request] may be timeout, in abort_request process body used %d us.", interval);
    }

    ngx_upstream_destroy_crequest(mlcf->plugin, ctx->crequest);
    ctx->crequest = NULL;
    
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[abort_request] upstream request abort, may be timeout or NOT need send to backend.");
    return;
}


static void
ngx_http_proto_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,"[finalize_request] finalize_request start..");
    ngx_http_proto_upstream_ctx_t       *ctx = NULL;
    ngx_http_proto_upstream_loc_conf_t  *mlcf = NULL;
    
    mlcf = ngx_http_get_module_loc_conf(r, ngx_http_proto_upstream_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_proto_upstream_module);
    gettimeofday(&ctx->tv_end, NULL);
    int interval_final_start = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);

    if (!ctx->has_processed) {
        if (rc) {
            ctx->status_code = rc;
        }
        struct timeval tv_begin, tv_end;
        gettimeofday(&tv_begin, NULL);
        int ret = ngx_upstream_process_body(mlcf->plugin, ctx, &r->upstream->buffer);

        gettimeofday(&tv_end, NULL);
        ctx->has_processed = 1;

        if (ret) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,"[finalize_request] process body error.");
        }

        int interval = 1000000*(tv_end.tv_sec-tv_begin.tv_sec)+(tv_end.tv_usec-tv_begin.tv_usec);
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                    "[finalize_request] may be connect failure, in finalize_request process body used %d us.", interval);
    }

    ngx_upstream_destroy_crequest(mlcf->plugin, ctx->crequest);
    ctx->crequest = NULL;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[finalize_request] finalize http proto_upstream request");
    gettimeofday(&ctx->tv_end, NULL);
    int interval = 1000000*(ctx->tv_end.tv_sec-ctx->tv_begin.tv_sec)+(ctx->tv_end.tv_usec-ctx->tv_begin.tv_usec);
    ctx->time_data.until_final= interval;

    ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                   "[upstream time data: create_request:%d , do_upstream: %d, process_header: %d, do_filter: %d, final_start: %d, until_final: %d]",
		     ctx->time_data.create_req,
		     ctx->time_data.do_upstream,
		     ctx->time_data.process_header,
		     ctx->time_data.do_filter,
 			interval_final_start,
		     ctx->time_data.until_final
			);

    return;
}
