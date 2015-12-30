# nginx_modules

## Build && Run

```
./configure --add-module=module_plugin --add-module=module_handle --add-module=module_upstream --add-module=module_subrequest --add-module=module_shm_dict --with-http_realip_module
make
make install
nginx -c conf/nginx.conf -p `pwd`
```

## module_plugin

Manage C++ plugin in Nginx, including dynamically open, intialization and choosing proper plugin to handle each http request according nginx.conf.

## module_handle

Simple module handle http request.

## module_upstream

Upstream module with protobuf protocol.
```
+--------------+--------------+
|  size(32bit) |   CRC(32bit) |
+--------------+--------------+
|                             |
|                             |
|          protobuf           |
|                             |
|                             |
+-----------------------------+
```

## module_subrequest

Subrequest module processing arbitrary number of subrequests to multiple backend servers.
```
                             +-----------+
                        +--->| backend 1 |
                        |    +-----------+
                        |
                        |
           +-------+    |    +-----------+
 request-->| Nginx |-------->| backend 2 |
           +-------+    |    +-----------+
                        |  
                        |
                        |    +-----------+
                        +--->| backend 3 |
                             +-----------+
```

## module_shm_dict

Simple key-value store based on shared memory. To initialize a shared zone, using 
*shm_dict_zone* command in nginx.conf:
```
shm_dict_zone zone=AAA max_size=16m;
shm_dict_zone zone=BBB max_size=64m;
```
You can create multiple zones with different name and size.
