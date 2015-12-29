#ifndef __PLUGIN_MANAGER_WRAPPER_H__
#define __PLUGIN_MANAGER_WRAPPER_H__

#if __cplusplus
extern "C" {
#endif

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

int plugin_create_manager(void *conf);
int plugin_init_master();
int plugin_init_process();
void plugin_exit_process();
void plugin_exit_master();

/**
 * @brief Get a Plugin instance by plugin_name.
 * @param name plugin_name specified in nginx.conf.
 * @param len Length of plugin_name.
 * @return If success, return Plugin instance; otherwise return NULL.
 */
void* plugin_getbyname(const char *name, size_t len);

/**
 * @brief Convert ngx_http_request_t to CRequest.
 * @param ngxr Original ngx_http_request from nginx.
 * @param creq CReuqst to be passed to Handle.
 * @return  If success, return 0; otherwise return -1.
 */
int ngxr_to_crequest(void *ngxr, void *creq);

/**
 * @brief Convert CRequest to ngx_http_request_t, send_header and output_filter.
 * @param creq CReuqst passed to Handle.
 * @param ngxr Original ngx_http_request from nginx.
 * @return If success, return 0; otherwise return -1.
 */
int crequest_to_ngxr(void *creq, void *ngxr);

/**
 * @brief Convert CRequest to ngx_http_request_t, not send_header and output_filter.
 * @param creq CReuqst passed to Handle.
 * @param ngxr Original ngx_http_request from nginx.
 * @return If success, return 0; otherwise return -1.
 */
int crequest_to_ngxr_nosend(void *creq, void *ngxr);

#if __cplusplus
}
#endif

#endif
