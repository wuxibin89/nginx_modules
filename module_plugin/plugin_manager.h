#ifndef __PLUGIN_MANAGER_H__
#define __PLUGIN_MANAGER_H__

#include "plugin.h"

#include <dlfcn.h>
#include <map>
#include <string>
#include <tr1/memory>

namespace plugin {

struct CPluginInfo;
class CPluginManager;

typedef std::tr1::shared_ptr<CPluginInfo> PluginInfoPtr;
typedef std::map<std::string, PluginInfoPtr> PluginInfoPtrMap;

struct CPluginInfo {
    std::string plugin_name_;
    std::string so_path_;
    std::string so_conf_;

    CPlugin*     plugin_ptr;
    void*       so_handler;

    CPluginInfo() {
        so_handler = NULL;
        plugin_ptr = NULL;
    }

    ~CPluginInfo() {
        if(plugin_ptr != NULL) {
              delete plugin_ptr;
              plugin_ptr = NULL;
        }

        if(so_handler != NULL) {
              dlclose(so_handler);
              so_handler = NULL;
        }
    }
};

class CPluginManager {
public:
    CPluginManager() {}

    virtual ~CPluginManager() {}

    CPlugin* GetPlugin(const std::string& plugin_name);

    int LoadPlugin(const std::string& plugin_name, 
            const std::string& so_path, const std::string& so_conf);

    int PluginInitMaster();         
    
    int PluginInitProcess();        
    
    void PluginExitProcess();

    void PluginExitMaster();
private:
    PluginInfoPtrMap plugins_info_map_;
};

}

#endif 
