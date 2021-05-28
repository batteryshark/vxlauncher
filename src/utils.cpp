#include <vector>
#include <string>
#include <fstream>
#include "sha1.hpp"

#if _WIN32
#include <Shlobj.h>
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
