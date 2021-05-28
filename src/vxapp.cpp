// Handlers for VXApp Info Files
#include <string>
#include <fstream>
#include <filesystem>
#include "json.hpp"

#include "utils.h"
#include "vxapp.h"

using json = nlohmann::json;

typedef int tsmoothie_create(const char* path_to_mapfile, const char* path_to_root, const char* path_to_persistence);
typedef int tsmoothie_destroy(const char* path_to_root);
typedef int tsmoothie_resolve(const char* path_to_root, const char* virtual_path, char* out_path);
static tsmoothie_create* smoothie_create;
static tsmoothie_destroy* smoothie_destroy;
static tsmoothie_resolve* smoothie_resolve;

#if _WIN32
#include <libloaderapi.h>
#define LIBSMOOTHIE "libsmoothie.dll"
#define EXT_LIB "dll"
#define EXT_EXEC "exe"
#else
#include <dlfcn.h>
#define LIBSMOOTHIE "libsmoothie.so"
#define EXT_LIB "so"
#define EXT_EXEC "elf"
#endif

std::filesystem::path BIN_PATH;
std::filesystem::path TMP_ROOT;
std::filesystem::path SAVE_ROOT;
std::string pdxfs64;
std::string pdxfs32;
std::string pdxproc64;
std::string pdxproc32;
std::string dropkick_32;
std::string dropkick_64;
void init_paths(){
    #if _WIN32
    dropkick_32 = "dropkick32.exe";
    dropkick_64 = "dropkick64.exe";
    pdxfs64 = "pdxfs64.dll";
    pdxfs32 = "pdxfs32.dll";
    pdxproc64 = "pdxproc64.dll";
    pdxproc32 = "pdxproc32.dll";
    BIN_PATH = "C:\\repos\\pdx\\bin";
    TMP_ROOT = "C:\\vxtmp";
    SAVE_ROOT = "C:\\vxsave";
    #else
    dropkick_32 = "dropkick32.elf";
    dropkick_64 = "dropkick64.elf";    
    BIN_PATH = "/repos/pdx/bin";
    TMP_ROOT = "/vxtmp";
    SAVE_ROOT = "/vxsave";
    pdxfs64 = "pdxfs64.so";
    pdxfs32 = "pdxfs32.so";
    pdxproc64 = "pdxproc64.so";
    pdxproc32 = "pdxproc32.so";    
    #endif

}

int resolve_smoothie(){
    #if _WIN32
    HMODULE hSmoothie = LoadLibraryA(LIBSMOOTHIE);
    if(!hSmoothie){return 0;}
    smoothie_create = (tsmoothie_create*)GetProcAddress(hSmoothie,"smoothie_create");
    smoothie_destroy = (tsmoothie_destroy*)GetProcAddress(hSmoothie,"smoothie_destroy");
    smoothie_resolve = (tsmoothie_resolve*)GetProcAddress(hSmoothie,"smoothie_resolve");
    #else
    void* hSmoothie = dlopen(LIBSMOOTHIE,RLTD_NOW);
    if(!hSmoothie){return 0;}
    smoothie_create = dlsym(hSmoothie,"smoothie_create");
    smoothie_destroy = dlsym(hSmoothie,"smoothie_destroy");
    smoothie_resolve = dlsym(hSmoothie,"smoothie_resolve");
    #endif
    if(!smoothie_create || !smoothie_destroy || !smoothie_resolve){return 0;}
    return 1;
}


std::filesystem::path resolve_preload_path(std::filesystem::path& local_plugin_path, std::string& preload_name){
    std::filesystem::path local_path = local_plugin_path / preload_name;
    std::filesystem::path global_path = BIN_PATH / preload_name;
    if(std::filesystem::exists(local_path)){return local_path;}
    if(std::filesystem::exists(global_path)){return global_path;}
    std::filesystem::path no_path = "";
    return no_path;
}

class ConfigInfo{
    private:
    public:
    std::string name;
    std::string map;
    std::string executable;
    std::string args;
    std::string cwd; 
    std::vector<std::string> envar;
    std::vector<std::string> preload;
};

void print_configinfo(ConfigInfo* info){
    printf("--------\n");    
    printf("Config Info:\n");
    printf("Name: %s\n", info->name.c_str());
    printf("Map File: %s\n", info->map.c_str());
    printf("Executable %s\n", info->executable.c_str());
    printf("Args: %s\n", info->args.c_str());
    printf("Cwd: %s\n", info->cwd.c_str());
    for(auto envar: info->envar){
        printf("Envar: %s\n", envar.c_str());
    }
    for(auto preload: info->preload){
        printf("Preload: %s\n", preload.c_str());
    }    
    printf("--------\n");
}

int load_configuration(std::string path_to_info, ConfigInfo* selected_config){
    std::ifstream i(path_to_info);
    json j;
    i >> j;
    if(!j.contains("configuration")){
        printf("[VXLauncher] Error: Invalid appinfo format.\n");
        return 0;
    }

    for(auto config: j["configuration"]){
        std::string curname = config["name"];
        if(!curname.compare(selected_config->name)){
            selected_config->map = config["map"];
            selected_config->executable = config["executable"];
            selected_config->args = config["args"];
            selected_config->cwd = config["cwd"];
            // Add Envars
            for(auto envar: config["envar"]){
                selected_config->envar.push_back(envar);
            }
            // Add Preloads
            for(auto preload: config["preload"]){
                selected_config->preload.push_back(preload);
            }


        }
    }
    return 1;
}

int unload_vxapp(std::filesystem::path& vxapp_path){
    init_paths();
    // Load our smoothie library
    if(!resolve_smoothie()){
        printf("[VXLauncher] Error: Could not Resolve LibSmoothie\n");
        return 0;
    }
    std::string app_name = vxapp_path.stem().string();
    std::string app_id = generate_app_id(app_name);
    std::filesystem::path tmp_path = TMP_ROOT / app_id;
    int status = smoothie_destroy(tmp_path.string().c_str());
    if(status){std::filesystem::remove_all(tmp_path);}
    return status;
}

int load_vxapp(std::filesystem::path& vxapp_path, std::string& config_name, int no_launch){
    // Initalize Configuration Paths    
    init_paths();
    std::filesystem::path info_path = vxapp_path / std::string("vxapp.info");
    std::filesystem::path content_path =  vxapp_path / std::string("content");    
    std::filesystem::path local_plugins_path = vxapp_path / std::string("plugins");
        
    std::string app_name = vxapp_path.stem().string();
    std::string app_id = generate_app_id(app_name);
    std::filesystem::path tmp_path = TMP_ROOT / app_id;
    std::filesystem::path save_path = SAVE_ROOT / app_id;

    // Load our smoothie library
    if(!resolve_smoothie()){
        printf("[VXLauncher] Error: Could not Resolve LibSmoothie\n");
        return 0;
    }
    // Pull our config data with the selected configuration.
    ConfigInfo* selected_config = new ConfigInfo();
    selected_config->name = config_name;
    if(!load_configuration(info_path.string(),selected_config)){return 0;}


    // Set up Smoothie Instance
    std::filesystem::create_directories(tmp_path);
    std::filesystem::create_directories(save_path);
    std::filesystem::path mapinfo_path = content_path / selected_config->map;

    if(!smoothie_create(mapinfo_path.string().c_str(),tmp_path.string().c_str(),save_path.string().c_str())){
        printf("[VXLauncher] Smoothie Create Failed.\n");
        std::filesystem::remove_all(tmp_path);
        return 0;
    }



   // If we aren't launching the process, we're done!
   if(no_launch){return 1;}
    char* exe_cpath = (char*)calloc(1,32768);
    if(!smoothie_resolve(tmp_path.string().c_str(), selected_config->executable.c_str(), exe_cpath)){
        printf("[VXLauncher] Error: Could not Resolve Target Executable\n");
        std::filesystem::remove_all(tmp_path);        
        free(exe_cpath);
        return 0;
    }
    std::filesystem::path exe_path = exe_cpath;
    free(exe_cpath);


    if(!std::filesystem::exists(exe_path)){
        printf("[VXLauncher] Error: Executable Does not Exist!\n");
        std::filesystem::remove_all(tmp_path);        
        free(exe_cpath);        
        return 0;
    }

   int arch = determine_target_arch(exe_path.string().c_str());

    // Add App Envars 
    for(auto envar: selected_config->envar){
        putenv(envar.c_str());
    }   

    // Add PDXProc Envars
    std::string pdxproc_env = std::string("PDXPROC=") + BIN_PATH.string();
    putenv(pdxproc_env.c_str());
    std::string pdxproc_preloads;
    // Always add PDXFS
    std::filesystem::path pdxfs64_lib = resolve_preload_path(local_plugins_path,pdxfs64);
    std::filesystem::path pdxfs32_lib = resolve_preload_path(local_plugins_path,pdxfs32);
    pdxproc_preloads = pdxfs64_lib.string() + std::string(";") + pdxfs32_lib.string();
    
    // Add the rest from our config.
    for(auto preload: selected_config->preload){
        std::filesystem::path cpath = resolve_preload_path(local_plugins_path,preload);
        pdxproc_preloads.append(";");
        pdxproc_preloads.append(cpath.string());
    }  
    std::string pdxpl = "PDXPL=";
    pdxpl.append(pdxproc_preloads);
    putenv(pdxpl.c_str());


    // Add PDXFS Envars
    std::filesystem::path map_root = tmp_path / std::string("map");
    std::string pdxfs_root = std::string("PDXFS_ROOT=") + map_root.string();
    putenv(pdxfs_root.c_str());    
    putenv("PDXFS_MODE=1"); 
    std::string pdxfs_ignore = "PDXFS_IGNORE=";
    pdxfs_ignore.append(SAVE_ROOT.string().c_str());
    pdxfs_ignore.append(";");
    pdxfs_ignore.append(BIN_PATH.string().c_str());
    pdxfs_ignore.append(";");
    pdxfs_ignore.append(TMP_ROOT.string().c_str());
    pdxfs_ignore.append(";");
    // We're Gonna get rid of some shaders as well...
    // TODO: Make this Configurable
    pdxfs_ignore.append("C:\\ProgramData\\Intel;C:\\ProgramData\\Nvidia Corporation;C:\\Users\\USER\\AppData\\Local\\nvidia\\glcache;C:\\Users\\USER\\AppData\\Local\\d3dscache");
    putenv(pdxfs_ignore.c_str()); 

    // Resolve Dropkick + PDXProc Path
    std::string dropkick_name = (arch == 64) ? dropkick_64:dropkick_32;
    std::string pdxproc_name =  (arch == 64) ? pdxproc64:pdxproc32;
    std::filesystem::path dropkick_path = resolve_preload_path(local_plugins_path,dropkick_name);
    std::filesystem::path pdxproc_path = resolve_preload_path(local_plugins_path,pdxproc_name);
    
    std::string cmd;
    #if _WIN32
    if(selected_config->cwd.empty()){
        cmd = dropkick_path.string() + std::string(" start \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" ") + selected_config->args;
    }else{
            char* cwd_cpath = (char*)calloc(1,32768);
            if(!smoothie_resolve(tmp_path.string().c_str(), selected_config->executable.c_str(), exe_cpath)){
            printf("[VXLauncher] Error: Could not Resolve Target CWD\n");
            std::filesystem::remove_all(tmp_path);        
            free(cwd_cpath);
            return 0;
        }
        std::filesystem::path resolved_cwd = cwd_cpath;
        free(cwd_cpath);
        cmd = dropkick_path.string() + std::string(" start_in \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" \"") + resolved_cwd.string() + std::string("\" ") + selected_config->args;
    }
    #else
        cmd = std::string("LD_PRELOAD=") + pdxproc_path.string() + std::string(" \"") + exe_path.string() + std::string("\" ") + selected_config->args;
    #endif
    printf("Launching %s [%s]\n", app_name.c_str(),app_id.c_str());
    printf("CMD: %s\n",cmd.c_str());
    system(cmd.c_str());
    return 1;
}