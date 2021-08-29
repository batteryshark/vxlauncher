#include <stdio.h>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#include "sha1.hpp"
#include "json.hpp"



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
#else
#include <dlfcn.h>
#define LIBSMOOTHIE "libsmoothie.so"
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




#if _WIN32
#include <shlobj.h>
int is_elevated(){
    return IsUserAnAdmin();
}
#else
#include <unistd.h>
int is_elevated(){
    return geteuid() == 0;
}
#endif


void * memmem (const void *haystack, size_t haystack_len, const void *needle, size_t needle_len){
  const char *begin;
  const char *const last_possible
    = (const char *) haystack + haystack_len - needle_len;

  if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
       the beginning of the string.  */
    return (void *) haystack;

  /* Sanity check, otherwise the loop might search through the whole
     memory.  */
  if (haystack_len < needle_len)
    return NULL;

  for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    if (begin[0] == ((const char *) needle)[0] &&
	!memcmp ((const void *) &begin[1],
		 (const void *) ((const char *) needle + 1),
		 needle_len - 1))
      return (void *) begin;

  return NULL;
}

std::vector<unsigned char> HexToBytes(const std::string& hex) {
  std::vector<unsigned char> bytes;

  for (unsigned int i = 0; i < hex.length(); i += 2) {
    std::string byteString = hex.substr(i, 2);
    unsigned char byte = (char) strtoul(byteString.c_str(), NULL, 16);
    bytes.push_back(byte);
  }

  return bytes;
}

static char code_pool[] = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F',
    'G','H','J','K','L','M','N','P','Q','R','S','T','U','V','W','X',
    'Y'
};


std::string generate_app_id(std::string& vxapp_name){
    std::string cv = "";
    SHA1 checksum;
    checksum.update(vxapp_name.c_str());
    const std::string hash = checksum.final();
    std::vector<unsigned char> bytes = HexToBytes(hash);
    for (auto & val: bytes) {
        cv += code_pool[val % sizeof(code_pool)];
    }
    std::string code = cv.substr(0,3) + std::string("-") + cv.substr(3,3) + std::string("-") + cv.substr(6,3);
    return code;
}


int determine_target_arch(const char* path_to_executable){
    FILE* fp = fopen(path_to_executable,"rb");
    if(!fp){return 64;}    
    unsigned char file_data[0x1000] = {0x00};
    fread(file_data,0x1000,1,fp);
    fclose(fp);
    unsigned char pe_new[5] = {0x50, 0x45, 0x00, 0x00, 0x64};
    unsigned char pe_old[5] = {0x50, 0x45, 0x00, 0x00, 0x4C};
    if(memmem(file_data,sizeof(file_data),pe_new,sizeof(pe_new))){return 64;}
    // This could be 32 or 64bit still via 32bit Word Machine...
    if(memmem(file_data,sizeof(file_data),pe_old,sizeof(pe_old))){
        unsigned short characteristics = 0;
        unsigned char* hdr = (unsigned char*)memmem(file_data,sizeof(file_data),pe_old,sizeof(pe_old)) + 0x16;
        characteristics = *(unsigned short*)hdr;
        if(characteristics & 0x0100){return 32;}
        return 64;
    }
    // Linux Handling
    if(file_data[0x12] == 0x03){return 32;}
    if(file_data[0x12] == 0x3E){return 64;}
    return 64;
}


void init_paths(){
    #if _WIN32
    dropkick_32 = "dropkick32.exe";
    dropkick_64 = "dropkick64.exe";
    pdxfs64 = "pdxfs64.dll";
    pdxfs32 = "pdxfs32.dll";
    pdxproc64 = "pdxproc64.dll";
    pdxproc32 = "pdxproc32.dll";
    if(getenv("VXPATH_BIN")){
        BIN_PATH = getenv("VXPATH_BIN");
    }else{
        BIN_PATH = "C:\\repos\\pdx\\bin";
    }
    if(getenv("VXPATH_TMP")){
        TMP_ROOT = getenv("VXPATH_TMP");
    }else{
        TMP_ROOT = "C:\\vxtmp";
    }
    if(getenv("VXPATH_SAVE")){
        SAVE_ROOT = getenv("VXPATH_SAVE");
    }else{
        SAVE_ROOT = "C:\\vxsave";
    }
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
    void* hSmoothie = dlopen(LIBSMOOTHIE,RTLD_NOW);
    if(!hSmoothie){return 0;}
    smoothie_create = (tsmoothie_create*)dlsym(hSmoothie,"smoothie_create");
    smoothie_destroy = (tsmoothie_destroy*)dlsym(hSmoothie,"smoothie_destroy");
    smoothie_resolve = (tsmoothie_resolve*)dlsym(hSmoothie,"smoothie_resolve");
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

int load_configuration(std::string path_to_config, ConfigInfo* selected_config){
    std::ifstream i(path_to_config);
    json j;
    i >> j;


    for(auto config: j){
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

int load_vxapp(std::filesystem::path& vxapp_path, std::string& config_name, int no_launch, int frontend){
    // Initalize Configuration Paths    
    init_paths();
    std::filesystem::path config_path = vxapp_path / std::string("vxapp.config");
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
    if(!load_configuration(config_path.string(),selected_config)){return 0;}


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
        putenv((char*)envar.c_str());
    }   

    // Add PDXProc Envars
    std::string pdxproc_env = std::string("PDXPROC=") + BIN_PATH.string();
    putenv((char*)pdxproc_env.c_str());
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
    putenv((char*)pdxpl.c_str());


    // Add PDXFS Envars
    std::filesystem::path map_root = tmp_path / std::string("map");
    std::string pdxfs_root = std::string("PDXFS_ROOT=") + map_root.string();
    putenv((char*)pdxfs_root.c_str());    
    putenv((char*)"PDXFS_MODE=1"); 
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
    putenv((char*)pdxfs_ignore.c_str()); 

    // Resolve Dropkick + PDXProc Path
    std::string dropkick_name = (arch == 64) ? dropkick_64:dropkick_32;
    std::string pdxproc_name =  (arch == 64) ? pdxproc64:pdxproc32;
    std::filesystem::path dropkick_path = resolve_preload_path(local_plugins_path,dropkick_name);
    std::filesystem::path pdxproc_path = resolve_preload_path(local_plugins_path,pdxproc_name);
    
    std::string cmd;
    #if _WIN32
    if(selected_config->cwd.empty()){
        if(frontend){
    cmd = dropkick_path.string() + std::string(" start \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" ") + selected_config->args;
        }else{
    cmd = dropkick_path.string() + std::string(" start \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" ") + selected_config->args;   
        }
    
    }else{
            char* cwd_cpath = (char*)calloc(1,32768);
            if(!smoothie_resolve(tmp_path.string().c_str(), selected_config->cwd.c_str(), exe_cpath)){
            printf("[VXLauncher] Error: Could not Resolve Target CWD\n");
            std::filesystem::remove_all(tmp_path);        
            free(cwd_cpath);
            return 0;
        }
        std::filesystem::path resolved_cwd = cwd_cpath;
        free(cwd_cpath);
        if(frontend){
cmd = dropkick_path.string() + std::string(" start_in \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" \"") + resolved_cwd.string() + std::string("\" ") + selected_config->args;
   
        }else{
 cmd = dropkick_path.string() + std::string(" start_in \"") + pdxproc_path.string() + std::string("\" \"") + exe_path.string() + std::string("\" \"") + resolved_cwd.string() + std::string("\" ") + selected_config->args;
        }
     
    }
    #else
        cmd = std::string("LD_PRELOAD=") + pdxproc_path.string() + std::string(" \"") + exe_path.string() + std::string("\" ") + selected_config->args;
    #endif
    printf("Launching %s [%s]\n", app_name.c_str(),app_id.c_str());
    printf("CMD: %s\n",cmd.c_str());
    //system(cmd.c_str());
    WinExec(cmd.c_str(), SW_HIDE);
    return 1;
}

void usage(const char* name){
    printf("Usage: %s path_to_vxapp [config=config_name] [no_launch]\n",name);
    exit(-1);
}



int main(int argc, const char*argv[]){
    char backsplash_cmd[1024] = {0x00};
    if(!is_elevated()){
        printf("[VXLauncher] Error: This Application Must be Run with Administrative Privileges.\n");
        return -1;
    }
    if(argc < 2){usage(argv[0]);}
    int no_launch = 0;
    int cleanup = 0;
    int frontend = 0;
    std::string config_id = "default";
    std::filesystem::path vxapp_path = argv[1];
    // Pull optional params
    if(argc > 2){
        for(int i=2; i < argc; i++){
            if(!strcmp(argv[i],"no_launch")){
                no_launch = 1;
            }
            if(!strcmp(argv[i],"frontend")){
                frontend = 1;
            }
            if(!strcmp(argv[i],"cleanup")){
                unload_vxapp(vxapp_path);
                return 0;
            }
            if(strstr(argv[i],"config=")){
                config_id = strstr(argv[i],"config=") + strlen("config=");
            }
        }
    }

   // sprintf(backsplash_cmd,"backsplash.exe \"%s\"",argv[1]);
  //  WinExec(backsplash_cmd,1);
    
        
    if(!load_vxapp(vxapp_path,config_id,no_launch,frontend)){
        return -1;
    }
    if(!frontend){
        printf("Press Any Key to Continue\n");  
        getchar();    
        unload_vxapp(vxapp_path);
    }
    return 0;
}