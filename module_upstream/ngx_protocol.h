#ifndef _NGX_AHADPROTOCOL_H_INCLUDED_
#define _NGX_AHADPROTOCOL_H_INCLUDED_


#if __cplusplus
extern "C" {
#endif

#include <ngx_config.h>
#include <ngx_core.h>

#define NGX_ADPROTOCOL_FLAG		        0xe8
#define NGX_ADPROTOCOL_eOK              0
#define NGX_ADPROTOCOL_eERROR           -101
#define NGX_ADPROTOCOL_eMOREDATA        -102
#define NGX_ADPROTOCOL_HEADER_LENGTH    sizeof(ngx_http_protocol_head_t)

typedef struct {
    int flag;
    int length;
    char body[];
} ngx_http_protocol_head_t;

	//0:success     -1:error
ngx_int_t ngx_http_protocol_encode(ngx_str_t * data, ngx_pool_t * pool, ngx_buf_t ** buf);

	//>=0:bodylenth     -1:error    -2:moreData
ngx_int_t ngx_http_protocol_decode(ngx_buf_t * buf);

#if __cplusplus
}
#endif

#endif //_NGX_AHADPROTOCOL_H_INCLUDED_
