#include <netinet/in.h>
#include "ngx_protocol.h"

ngx_int_t ngx_http_protocol_encode(ngx_str_t * data, ngx_pool_t * pool, ngx_buf_t ** buf)
{
    if (data == NULL || pool == NULL || buf == NULL) {
        return NGX_ERROR;
    }

    ngx_int_t len = NGX_ADPROTOCOL_HEADER_LENGTH + data->len;
    *buf = ngx_create_temp_buf(pool, len);

    if (*buf == NULL) {
        return NGX_ERROR;
    }

    /* encode request header */
    ngx_http_protocol_head_t head;
    head.flag = NGX_ADPROTOCOL_FLAG;
    head.length = htonl(data->len);
    (*buf)->last = ngx_copy((*buf)->last, (u_char*)&head, NGX_ADPROTOCOL_HEADER_LENGTH);
    /* encode request body */
    (*buf)->last = ngx_copy((*buf)->last, data->data, data->len);

    return NGX_ADPROTOCOL_eOK;
}

ngx_int_t ngx_http_protocol_decode(ngx_buf_t * buf)
{
    if (buf == NULL) {
        return NGX_ERROR;
    }

    size_t dataLen = buf->last - buf->pos;

    if(dataLen < NGX_ADPROTOCOL_HEADER_LENGTH) {
        return NGX_ADPROTOCOL_eMOREDATA;
    } 

    ngx_http_protocol_head_t * pstHead = (ngx_http_protocol_head_t*)buf->pos;

    if((int)NGX_ADPROTOCOL_FLAG != pstHead->flag) {
        return NGX_ADPROTOCOL_eERROR;
    }

    size_t bodyLen = ntohl(pstHead->length);
    if(bodyLen > dataLen - NGX_ADPROTOCOL_HEADER_LENGTH) {
        return NGX_ADPROTOCOL_eMOREDATA;
    }

    buf->pos += NGX_ADPROTOCOL_HEADER_LENGTH;
    return bodyLen;
}
