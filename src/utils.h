#pragma once
#include <string>
#include <vector>

int determine_target_arch(const char* path_to_executable);
std::string generate_app_id(std::string& vxapp_name);
int is_elevated();

