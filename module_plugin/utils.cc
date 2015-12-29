#include <string>
#include <algorithm>

#include "utils.h"
#include "plugin.h"
#include "uri_codec.h"
#include "plugin_manager.h"
#include "ngx_plugin_module.h"

#define ahAssert(_e) if(!(_e)) {                                \
                        r->headers_out.content_length_n = 0;    \
                        r->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR; \
                        return NGX_ERROR;                       \
                     }

using namespace std;
using namespace plugin;

static void split_string(map<string, string> &kv, const char *s, size_t len, char delimiter);
static int get_post_body(ngx_http_request_t *r, string &post_body);
static int crequest_to_ngxr_impl(void *creq, void *ngxr, int nosend);

static plugin::CPluginManager *plugin_manager = NULL;

int plugin_create_manager(void *conf) {
    ngx_http_plugin_main_conf_t *pmcf = (ngx_http_plugin_main_conf_t *)conf;
    plugin_conf_t *pcf = (plugin_conf_t *)pmcf->plugin_conf.elts;

    plugin_manager = new plugin::CPluginManager();

    for(size_t i = 0; i < pmcf->plugin_conf.nelts; ++i) {
        string name((char *)pcf[i].name.data, pcf[i].name.len);
        string so_path((char *)pcf[i].so_path.data, pcf[i].so_path.len);
        string so_conf((char *)pcf[i].so_conf.data, pcf[i].so_conf.len);

        int rc = plugin_manager->LoadPlugin(name, so_path, so_conf);
        if(rc != 0) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

int plugin_init_master() {
    return plugin_manager->PluginInitMaster();  
}

int plugin_init_process() {
    return plugin_manager->PluginInitProcess();  
}

void plugin_exit_process() {
    plugin_manager->PluginExitProcess(); 
}


void plugin_exit_master() {
    plugin_manager->PluginExitMaster(); 

    if(plugin_manager != NULL) {
        delete plugin_manager;
        plugin_manager = NULL;
    }
}


void* plugin_getbyname(const char *name, size_t len) {
    return plugin_manager->GetPlugin(string(name, len));
}

int ngxr_to_crequest(void *ngxr, void *creq) {
    ngx_http_request_t *r = (ngx_http_request_t *)ngxr; 
    CRequest *request = (CRequest *)creq;

    gettimeofday(&request->start_time, NULL); 

    //get uri
    request->uri.append((char*)r->uri.data,r->uri.len);
	
    //get param
    if(r->args.len > 0) {
        //string uri_dec = UriDecode(string((char *)r->args.data, r->args.len));
        replace((char *)r->args.data, (char *)r->args.data + r->args.len, '\t', ' ');
        split_string(request->param, (char *)r->args.data, r->args.len, '&');
    }

    //get headers
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *elts = (ngx_table_elt_t *)part->elts;
    for(size_t i = 0; ; ++i) {
        if(i >= part->nelts) {
            if(part->next == NULL) {
                break;
            } 

            part = part->next;
            elts = (ngx_table_elt_t *)part->elts;
            i = 0;
        }

        string key = string((char *)elts[i].key.data, elts[i].key.len);
        string val = string((char *)elts[i].value.data, elts[i].value.len);
        request->headers_in.headers.push_back(make_pair(key, val));
    }

    //get host
    ngx_table_elt_t *host = r->headers_in.host;
    if(host && host->value.len) {
        request->headers_in.host = string((char *)host->value.data, 
                host->value.len);;
    }

    //get user_agent
    ngx_table_elt_t *user_agent = r->headers_in.user_agent;
    if(user_agent && user_agent->value.len) {
        request->headers_in.user_agent = string((char *)user_agent->value.data,
            user_agent->value.len);
        replace(request->headers_in.user_agent.begin(), request->headers_in.user_agent.end(), '\t', ' ');
    }

    //get referer
    ngx_table_elt_t *referer = r->headers_in.referer;
    if(referer && referer->value.len) {
        request->headers_in.referer = string((char *)referer->value.data,
                referer->value.len);
    }

    //get x_real_ip
    ngx_table_elt_t *x_real_ip = r->headers_in.x_real_ip;
    if(x_real_ip && x_real_ip->value.len) {
        request->headers_in.x_real_ip = string((char *)x_real_ip->value.data,
                x_real_ip->value.len);
    } else {
        request->headers_in.x_real_ip = string((char *)r->connection->addr_text.data,
                r->connection->addr_text.len);
    }
    
    //get x_forwarded_for
    ngx_table_elt_t **forward = (ngx_table_elt_t **)r->headers_in.x_forwarded_for.elts;
    for(size_t i = 0; i < r->headers_in.x_forwarded_for.nelts; ++i) {
        ngx_log_error(NGX_LOG_DEBUG,  r->connection->log, 0,
            "x_forwarded_for line %d: %V", i, &forward[i]->value);

        request->headers_in.x_forwarded_for.push_back(
                string((char *)forward[i]->value.data, forward[i]->value.len));
    }

    //get cookie
    ngx_table_elt_t **cookies = (ngx_table_elt_t**)r->headers_in.cookies.elts;
    for(size_t i = 0; i < r->headers_in.cookies.nelts; ++i) {
        ngx_log_error(NGX_LOG_DEBUG,  r->connection->log, 0,
            "Cookie line %d: %V", i, &cookies[i]->value);

        string cookie_str = string((char *)cookies[i]->value.data, cookies[i]->value.len);
        split_string(request->headers_in.cookies, (char *)cookies[i]->value.data, cookies[i]->value.len, ';'); 
    }

    //get request_body
    int rc = get_post_body(r, request->request_body);
    if(rc != 0) { 
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Get post body error");

        return NGX_ERROR;
    }
 
    return 0;
}


int crequest_to_ngxr(void *creq, void *ngxr) {
    return crequest_to_ngxr_impl(creq, ngxr, 0);
}


int crequest_to_ngxr_nosend(void *creq, void *ngxr) {
    return crequest_to_ngxr_impl(creq, ngxr, 1);
}


static int crequest_to_ngxr_impl(void *creq, void *ngxr, int nosend) {
    ngx_str_t str;

    ngx_http_request_t *r = (ngx_http_request_t *)ngxr; 
    CRequest *request = (CRequest *)creq;
    
    //set headers
    list<pair<string, string> >::iterator beg = request->headers_out.headers.begin();
    list<pair<string, string> >::iterator end = request->headers_out.headers.end();
    for(; beg != end; ++beg) {
        ngx_table_elt_t *field = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
        ahAssert(field);

        field->hash = 1;

        field->key.len = beg->first.size();
        field->key.data = (u_char *)ngx_palloc(r->pool, field->key.len); 
        ahAssert(field->key.data);
        ngx_memcpy(field->key.data, beg->first.c_str(), field->key.len);

        field->value.len = beg->second.size();
        field->value.data = (u_char *)ngx_palloc(r->pool, field->value.len); 
        ahAssert(field->value.data);
        ngx_memcpy(field->value.data, beg->second.c_str(), field->value.len);
    }

    //set status_line
    str.len = request->headers_out.status_line.size();
    str.data = (u_char *)ngx_palloc(r->pool, str.len);
    ahAssert(str.data);
    ngx_memcpy(str.data, request->headers_out.status_line.c_str(), str.len);
    r->headers_out.status_line = str;

    //set location
    ngx_table_elt_t *field = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
    ahAssert(field);

    field->hash = 1;

    field->key.len = sizeof("Location") - 1;
    field->key.data = (u_char *)ngx_palloc(r->pool, field->key.len); 
    ahAssert(field->key.data);
    ngx_memcpy(field->key.data, "Location", field->key.len);


    field->value.len = request->headers_out.location.size();
    field->value.data = (u_char *)ngx_palloc(r->pool, field->value.len); 
    ahAssert(field->value.data);
    ngx_memcpy(field->value.data, request->headers_out.location.c_str(), 
            request->headers_out.location.size());

    //set content_type
    str.len = request->headers_out.content_type.size();
    str.data = (u_char *)ngx_palloc(r->pool, str.len);
    ahAssert(str.data);
    ngx_memcpy(str.data, request->headers_out.content_type.c_str(), str.len);
    r->headers_out.content_type = str;
    r->headers_out.content_type_len = str.len;

    //set charset
    str.len = request->headers_out.charset.size();
    str.data = (u_char *)ngx_palloc(r->pool, str.len);
    ahAssert(str.data);
    ngx_memcpy(str.data, request->headers_out.charset.c_str(), str.len);
    r->headers_out.charset= str;

    //set content_length_n
    uint32_t body_size = request->response_body.size();
    r->headers_out.content_length_n = body_size;
    
    //set status
    uint32_t status = 0, set_status = request->headers_out.status;
    if(set_status == 0) {
        if(body_size == 0) {
            status = NGX_HTTP_INTERNAL_SERVER_ERROR;     
        } else {
            status = NGX_HTTP_OK;
        }
    } else {
        status = set_status;
    }
    r->headers_out.status = status;
    
    if(nosend) {
        return NGX_OK;
    }
  
    ngx_int_t rc;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    ngx_buf_t *b;
    ngx_chain_t out;
   

    if(body_size == 0) {
	    b = (ngx_buf_t *)ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	    ahAssert(b);
    }else {
 	    b = ngx_create_temp_buf(r->pool, request->response_body.size());
	    ahAssert(b);

 	    ngx_memcpy(b->pos, request->response_body.c_str(), 
        	    request->response_body.size());
    	b->last = b->pos + request->response_body.size();
    }

    b->last_buf = 1;
    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}


static void split_string(map<string, string> &kv, 
        const char *s, size_t len, char delimiter) {
    char *beg = (char *)s, *end = beg + len;
    
    while(beg < end) {
        while(beg < end && *beg == ' ') ++beg;  /* escape space */
        if(beg == end)
            break;

        char *pdel = beg, *equal = beg;

        while(pdel < end && *pdel != delimiter) pdel++;
        while(equal < pdel && *equal != '=') equal++;
        
        if(equal == beg) {          /* key can't be empty */
            beg = pdel + 1;
            continue;
        }
        string key = string(beg, equal - beg);

        string val;
        if((pdel - equal - 1) > 0) {
            val = string(equal + 1, pdel - equal - 1);
        } else {
            val = string("");
        }

        kv.insert(make_pair(key, UriDecode(val)));

        beg = pdel + 1;
    }   
}


static int get_post_body(ngx_http_request_t *r, string &post_body) {
    if (!(r->method & (NGX_HTTP_POST))) {
        return 0;
    }

    if(r->request_body == NULL || r->request_body->bufs == NULL) {
        return 0; 
    } 

    char *buf_temp = NULL;
    int buf_size = 0;

    for(ngx_chain_t *cl = r->request_body->bufs; cl; cl = cl->next) {
        if(ngx_buf_in_memory(cl->buf)) {
            post_body.append((char *)cl->buf->pos, cl->buf->last - cl->buf->pos);
            continue;
        } 

        /* buf in file */
        int nbytes = cl->buf->file_last - cl->buf->file_pos;
        if(nbytes <= 0) {
            continue;
        } 
        
        /* allocate lager temp buf */
        if(nbytes > buf_size) {
            if(buf_size > 0) delete [] buf_temp;

            buf_temp = new char[nbytes];
            buf_size = nbytes;
        }
        
        int buf_pos = 0;
        while(nbytes > 0) {
            int nread = ngx_read_file(cl->buf->file, (u_char *)buf_temp + buf_pos, 
                    cl->buf->file_last - cl->buf->file_pos, cl->buf->file_pos);

            if(nread < 0) { /* read temp file error */
                if(buf_size > 0) delete [] buf_temp;      

                return -1;
            }

            buf_pos += nread;
            nbytes -= nread; 
        }

        post_body.append(buf_temp, buf_size);
    } 

    if(buf_size > 0) delete [] buf_temp;

    return 0;
}
