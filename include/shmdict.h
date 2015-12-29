#ifndef __NGX_SHM_DICT_INTERFACE_H__
#define __NGX_SHM_DICT_INTERFACE_H__

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <stdint.h>


typedef struct {
    size_t  len;
    u_char* data;
}shm_str_t;


class CNgxDictInterface {
    public:
        CNgxDictInterface();
        virtual ~CNgxDictInterface(){};

        int Get(const std::string &key, std::string &value, uint32_t *exptime=0,
                const std::string &zoneName="");

        int Set(const std::string& key, const std::string& value, uint32_t exptime=0,
                const std::string& zoneName="");


        int Set(shm_str_t* key, uint32_t exptime=0, const std::string& zoneName="");

        int Set(shm_str_t* key, shm_str_t* value, uint32_t exptime=0,
                const std::string& zoneName="");

        int SetExptime(shm_str_t *key, uint32_t exptime,
                const std::string& zoneName="");

        int SetExptime(const std::string& key, uint32_t exptime,
                const std::string& zoneName="");

        int Del(const std::string& key, const std::string& zoneName="");

        int Del(shm_str_t* key,const std::string& zoneName="");

        int Incr(const std::string& key, int count,int64_t *res,uint32_t exptime=0,
                const std::string& zoneName="");

        int Incr(shm_str_t* shm_key, int count, int64_t *res,uint32_t exptime=0,
                const std::string& zoneName="");

        int FlushAll(const std::string& zoneName="");

        std::string GetZoneName();
        int SetZoneName(const std::string& zoneName);

        std::string GetError(int ret);

    protected:
        void* GetShmZone(const std::string& zoneName);
        void  SetShmStr(void *str,const std::string& key);
    private:
        std::string m_zoneName;
        void* m_zone;
};


#endif
