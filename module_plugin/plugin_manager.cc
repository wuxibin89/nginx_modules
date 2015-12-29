#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include "plugin_manager.h"

using namespace std;

namespace plugin {

const static string kCreatePluginFunc = "create_instance";

typedef CPlugin *(*CreatePluginFunc)();


int CPluginManager::LoadPlugin(const std::string& plugin_name, 
            const std::string& so_path, const std::string& so_conf) {  
    cerr << "LoadPlugin: " << plugin_name << " " << so_path << " " << so_conf << endl;

    void *so_handler = dlopen(so_path.c_str(), RTLD_LAZY);
    if (so_handler == NULL) {
        cerr << "plugin_manager dlopen path=" << so_path 
            << ", error=" << dlerror() << endl; 

        return -1;
    }

    CreatePluginFunc handler = NULL;
    handler = (CreatePluginFunc)dlsym(so_handler, kCreatePluginFunc.c_str());
    if (handler == NULL) {
        dlclose(so_handler);
        cerr << "plugin_manager dlsym path=" << so_path
            << ", error=" << dlerror() << endl;

        return -1;
    }

    CPlugin* plugin = (*handler)();
    if (plugin == NULL) {
        dlclose(so_handler);
        cerr << "plugin_manager create_instance path=" << so_path << endl;

        return -1;
    }

    PluginInfoPtr plugin_info(new CPluginInfo());
    plugin_info->plugin_name_ = plugin_name;
    plugin_info->so_path_ = so_path;
    plugin_info->so_conf_ = so_conf;
    plugin_info->plugin_ptr = plugin;
    plugin_info->so_handler = so_handler;

    plugins_info_map_.insert(make_pair(plugin_name, plugin_info));

    return 0;
}


int CPluginManager::PluginInitMaster() {
    PluginInfoPtrMap::iterator it = plugins_info_map_.begin();

    for(; it != plugins_info_map_.end(); ++it) {
        CPluginInfo *plugin_info = it->second.get();
        int rc = plugin_info->plugin_ptr->InitMaster(plugin_info->so_conf_); 
        if(rc) {
            cerr << plugin_info->so_path_ << " init module error, so_conf=" 
                << plugin_info->so_conf_ << endl;
        
            return -1;
        }
    }

    return 0;
}


int CPluginManager::PluginInitProcess() {
    PluginInfoPtrMap::iterator it = plugins_info_map_.begin();

    for(; it != plugins_info_map_.end(); ++it) {
        CPluginInfo *plugin_info = it->second.get();
        int rc = plugin_info->plugin_ptr->InitProcess(plugin_info->so_conf_); 
        if(rc) {
            cerr << plugin_info->so_path_ << " init process error, so_conf=" 
                << plugin_info->so_conf_ << endl;
        
            return -1;
        }
    }

    return 0;
}

void CPluginManager::PluginExitProcess() {
    PluginInfoPtrMap::iterator it = plugins_info_map_.begin();

    for(; it != plugins_info_map_.end(); ++it) {
        CPluginInfo *plugin_info = it->second.get();
        plugin_info->plugin_ptr->ExitProcess(); 
    }
}


void CPluginManager::PluginExitMaster() {
    PluginInfoPtrMap::iterator it = plugins_info_map_.begin();

    for(; it != plugins_info_map_.end(); ++it) {
        CPluginInfo *plugin_info = it->second.get();
        plugin_info->plugin_ptr->ExitMaster(); 
    }
}


CPlugin *CPluginManager::GetPlugin(const string &queryName) {
    PluginInfoPtrMap::iterator it = plugins_info_map_.find(queryName);
    if (it == plugins_info_map_.end()) {
        return NULL;
    }

    return it->second->plugin_ptr;
}

}
