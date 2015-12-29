#include "shmdict.h"

#define SHM_DICT_KEY_LEN_ZERO -10
#define SHM_DICT_VALUE_LEN_ZERO -9
#define SHM_DICT_ZONE_NULL  -2
#define SHM_DICT_RESPOSE_SUCCESS 0
#define SHM_DICT_RESPOSE_FAIL -1

extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_shm_dict_handler.h"
}


CNgxDictInterface::CNgxDictInterface() {	
    GetShmZone("");
}

void CNgxDictInterface::SetShmStr(void *str,const std::string& key) {
    ((ngx_str_t*)str)->data = (u_char *)key.c_str();
    ((ngx_str_t*)str)->len = key.length();
}


void* CNgxDictInterface::GetShmZone(const std::string& zoneName) {
    ngx_str_t ngx_zone_name = ngx_null_string;
    ngx_shm_zone_t *zone = NULL;

    if(zoneName.empty() && m_zone != NULL) {
        return m_zone;
    }

    SetShmStr(&ngx_zone_name,zoneName);

    zone = ngx_http_get_shm_zone((ngx_str_t *)&ngx_zone_name);
    if (zone == NULL) {
        return NULL;
    }

    m_zoneName = zoneName;
    m_zone = zone;

    return zone;
}

std::string CNgxDictInterface::GetZoneName() {
    return m_zoneName;
}


int CNgxDictInterface::SetZoneName(const std::string& zoneName) {
    ngx_str_t ngx_zone_name = ngx_null_string;
    ngx_shm_zone_t *zone = NULL;

    if ( zoneName == "" ) {
        return SHM_DICT_ZONE_NULL;
    }

    SetShmStr(&ngx_zone_name,zoneName);

    zone = ngx_http_get_shm_zone((ngx_str_t *)&ngx_zone_name);
    if (zone == NULL) {
        return SHM_DICT_RESPOSE_FAIL;
    }

    m_zoneName = zoneName;
    m_zone = zone;

    return SHM_DICT_RESPOSE_SUCCESS;
}


int CNgxDictInterface::Get(const std::string &key,std::string &value,
        uint32_t *exptime,const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;
    ngx_str_t ngx_key = ngx_null_string;
    ngx_str_t ngx_value = ngx_null_string;

    if( key == "" ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    SetShmStr(&ngx_key,key);
    SetShmStr(&ngx_value,value);

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_get(zone,&ngx_key,&ngx_value,exptime);
    value = std::string((char*)ngx_value.data, ngx_value.len);

    return rc;
}


int CNgxDictInterface::Set(const std::string &key,const std::string &value,
        uint32_t exptime,const std::string &zoneName) {

    int 				rc;
    ngx_shm_zone_t		*zone = NULL;
    ngx_str_t 			ngx_key = ngx_null_string;
    ngx_str_t 			ngx_value = ngx_null_string;

    if( key.empty() ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    if( value.empty() ) {
        return SHM_DICT_VALUE_LEN_ZERO;
    }

    SetShmStr(&ngx_key,key);
    SetShmStr(&ngx_value,value);

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_set(zone,&ngx_key,&ngx_value,exptime);
    return rc;
}


int CNgxDictInterface::Set(shm_str_t *key, shm_str_t *value, uint32_t exptime,
        const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;

    if( key->len == 0 ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }
    if( value->len == 0 ) {
        return SHM_DICT_VALUE_LEN_ZERO;
    }

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_set(zone,(ngx_str_t *)key,(ngx_str_t *)value,exptime);
    return rc;
}

int CNgxDictInterface::SetExptime(const std::string &key, uint32_t exptime,
        const std::string& zoneName) {

    int 				rc;
    ngx_shm_zone_t		*zone = NULL;
    ngx_str_t 			ngx_key = ngx_null_string;

    if( key.empty() ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    SetShmStr(&ngx_key,key);

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_set_exptime(zone,&ngx_key,exptime);
    return rc;
}

int CNgxDictInterface::SetExptime(shm_str_t* key, uint32_t exptime,
        const std::string& zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;

    if( key->len == 0 ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_set_exptime(zone,(ngx_str_t *)key,exptime);
    return rc;
}


int CNgxDictInterface::Del(const std::string &key,const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;
    ngx_str_t ngx_key = ngx_null_string;

    if( key.empty() ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    SetShmStr(&ngx_key,key);

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_delete(zone,&ngx_key);
    return rc;
}


int CNgxDictInterface::Del(shm_str_t *key,const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;

    if( key->len == 0 ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_delete(zone,(ngx_str_t *)key);
    return rc;
}

int CNgxDictInterface::Incr(const std::string &key,int count,int64_t *res,
        uint32_t exptime,const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t *zone = NULL;
    ngx_str_t ngx_key = ngx_null_string;

    if( key == "" ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    SetShmStr(&ngx_key,key);

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_incr_int(zone,&ngx_key, count,exptime,res);
    return rc;
}

int CNgxDictInterface::Incr(shm_str_t *ngx_key,int count,int64_t *res,
        uint32_t exptime,const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t  *zone = NULL;

    if( ngx_key->len == 0 ) {
        return SHM_DICT_KEY_LEN_ZERO;
    }

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_incr_int(zone,(ngx_str_t *)ngx_key,count,exptime,res);
    return rc;
}

int CNgxDictInterface::FlushAll(const std::string &zoneName) {

    int rc;
    ngx_shm_zone_t  *zone = NULL;

    zone = (ngx_shm_zone_t *)GetShmZone(zoneName);
    if( zone == NULL ) {
        return SHM_DICT_ZONE_NULL;
    }

    rc = ngx_shm_dict_handler_flush_all(zone);
    return rc;
}

std::string CNgxDictInterface::GetError(int ret) {
    switch(ret)
    {
        case SHM_DICT_RESPOSE_SUCCESS:
            return "shm dict response success";
        case SHM_DICT_RESPOSE_FAIL:
            return "shm dict response fail";
        case SHM_DICT_ZONE_NULL:
            return "shm dict zone null";
        case SHM_DICT_VALUE_LEN_ZERO:
            return "shm dict value len zero";
        case SHM_DICT_KEY_LEN_ZERO:
            return "shm dict key len zero";
        default:
            return "not found";
    }
}
