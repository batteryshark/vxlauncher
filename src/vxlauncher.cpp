#include <stdio.h>
#include <cstring>
#include <string>
#include <filesystem>


#include "vxapp.h"
#include "utils.h"





void usage(const char* name){
    printf("Usage: %s path_to_vxapp [config=config_name] [no_launch]\n",name);
    exit(-1);
}

int main(int argc, const char*argv[]){  

    if(!is_elevated()){
        printf("[VXLauncher] Error: This Application Must be Run with Administrative Privileges.\n");
        return -1;
    }
    if(argc < 2){usage(argv[0]);}
    int no_launch = 0;
    std::string config_id = "default";
    // Pull optional params
    if(argc > 2){
        for(int i=2; i < argc; i++){
            if(!strcmp(argv[i],"no_launch")){
                no_launch = 1;
            }
            if(strstr(argv[i],"config=")){
                config_id = strstr(argv[i],"config=") + strlen("config=");
            }
        }
    }

    std::filesystem::path vxapp_path = argv[1];
    
        
    if(!load_vxapp(vxapp_path,config_id,no_launch)){
        return -1;
    }
    printf("Press Any Key to Continue\n");  
    getchar();    
    unload_vxapp(vxapp_path);
    return 0;
}