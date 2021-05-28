#pragma once
#include <string>
#include <vector>




int load_vxapp(std::filesystem::path& vxapp_path, std::string& config_name, int no_launch);
int unload_vxapp(std::filesystem::path& vxapp_path);