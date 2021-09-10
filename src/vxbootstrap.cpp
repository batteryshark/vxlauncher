#include <Windows.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include "strutils.hpp"

#define MAX_NUM_LIBS 99
#define UNC_MAX_PATH 0x7FFF
#define PAGE_SIZE 0x1000

// Structures from ntdll
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef NTSTATUS __stdcall tLdrLoadDll(PWCHAR PathToFile, ULONG Flags, PUNICODE_STRING ModuleFileName, HMODULE* ModuleHandle);


typedef struct _load_library_t {
	tLdrLoadDll* ldr_load_dll;
	DWORD num_libs_to_load;
	UNICODE_STRING filepath[MAX_NUM_LIBS];
} load_library_t;



/* -- Original HOST Function Call
__declspec(noinline) static void __stdcall load_library_worker(load_library_t *s){
	HMODULE module_handle; unsigned int ret = 0;
	for(int i = 0; i < s->num_libs_to_load; i++){
		s->ldr_load_dll(NULL, 0, &s->filepath[i], &module_handle);
	}
	return;
}
*/

#if __x86_64__
unsigned char load_library_worker[106] = {
	0x55, 0x48, 0x89, 0xE5, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x89, 0x4D, 0x10, 0xC7, 0x45, 0xF8, 0x00, 
	0x00, 0x00, 0x00, 0xC7, 0x45, 0xFC, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x39, 0x48, 0x8B, 0x45, 0x10, 
	0x48, 0x8B, 0x00, 0x8B, 0x55, 0xFC, 0x48, 0x63, 0xD2, 0x48, 0x83, 0xC2, 0x01, 0x48, 0x89, 0xD1, 
	0x48, 0xC1, 0xE1, 0x04, 0x48, 0x8B, 0x55, 0x10, 0x48, 0x01, 0xD1, 0x48, 0x8D, 0x55, 0xF0, 0x49, 
	0x89, 0xD1, 0x49, 0x89, 0xC8, 0xBA, 0x00, 0x00, 0x00, 0x00, 0xB9, 0x00, 0x00, 0x00, 0x00, 0xFF, 
	0xD0, 0x83, 0x45, 0xFC, 0x01, 0x48, 0x8B, 0x45, 0x10, 0x8B, 0x50, 0x08, 0x8B, 0x45, 0xFC, 0x39, 
	0xC2, 0x77, 0xB9, 0x90, 0x48, 0x83, 0xC4, 0x30, 0x5D, 0xC3
};
#else
unsigned char load_library_worker[98] = {
	0x55, 0x89, 0xE5, 0x83, 0xEC, 0x28, 0xC7, 0x45, 0xF0, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xF4, 
	0x00, 0x00, 0x00, 0x00, 0xEB, 0x3A, 0x8B, 0x45, 0x08, 0x8B, 0x00, 0x8B, 0x55, 0xF4, 0x8D, 0x0C, 
	0xD5, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x55, 0x08, 0x01, 0xCA, 0x8D, 0x4A, 0x08, 0x8D, 0x55, 0xEC, 
	0x89, 0x54, 0x24, 0x0C, 0x89, 0x4C, 0x24, 0x08, 0xC7, 0x44, 0x24, 0x04, 0x00, 0x00, 0x00, 0x00, 
	0xC7, 0x04, 0x24, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xD0, 0x83, 0xEC, 0x10, 0x83, 0x45, 0xF4, 0x01, 
	0x8B, 0x45, 0x08, 0x8B, 0x50, 0x04, 0x8B, 0x45, 0xF4, 0x39, 0xC2, 0x77, 0xB9, 0x90, 0xC9, 0xC2, 
	0x04, 0x00
};
#endif

void usage() {
	printf("Usage: vxcmd=start vxexe=PATH_TO_EXE vxlibs=PATH_TO_LIBS [vxwd=PATH_TO_WD] [ARGS...]\n");
    printf("Usage: vxcmd=inject vxlibs=PATH_TO_LIBS vxpid=PID vxtid=TID vxls=LEAVE_SUSPENDED\n");
	exit(-1);
}

BOOL GetDirectoryPath(LPSTR lpFilename, LPSTR in_path) {
	strcpy(lpFilename, in_path);
    char* last_delimiter = strrchr(lpFilename, 0x5C);
    if (!last_delimiter) { return FALSE; }
    memset(last_delimiter + 1, 0x00, 1);
    return TRUE;
}

// Process Manipulation Stuff
void* write_to_process(DWORD target_pid, const void* data, unsigned int length){

	HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE , FALSE, target_pid);

	if (!hProcess || hProcess == INVALID_HANDLE_VALUE) { printf("Error Opening Process Handle\n"); return NULL; }
	
	void* addr = VirtualAllocEx(hProcess, NULL, length,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (addr == NULL) {
		printf("[-] Error allocating memory in process: %ld!\n",
			GetLastError());
			CloseHandle(hProcess);
			return NULL;
	}

	DWORD_PTR bytes_written;
	if (WriteProcessMemory(hProcess, addr, data, length,
		&bytes_written) == FALSE || bytes_written != length) {
		printf("[-] Error writing data to process: %ld\n", GetLastError());
		CloseHandle(hProcess);
		return NULL;
	}	
	CloseHandle(hProcess);
	return addr;
}

PWSTR write_path_to_process(wchar_t* wc_path,int max_length, DWORD target_pid){
	return (PWSTR)write_to_process(target_pid, (const void*)wc_path, max_length);	
}




void FormatArchSpecificPreload(std::string& path, BOOL is_wow64){
	if(ends_with(path,std::string(".dlldynamic"))){
		if(is_wow64){
			replace(path,".dlldynamic","32.dll");
    	}else{
			replace(path,".dlldynamic","64.dll");
    	}
	}
}

HANDLE open_thread(unsigned int tid){
	HANDLE thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
	if (thread_handle == NULL) {
		printf("[-] Error getting access to thread: %ld!\n", GetLastError());
	}

	return thread_handle;
}

void resume_thread(unsigned int tid){
	HANDLE thread_handle = open_thread(tid);
	ResumeThread(thread_handle);
	CloseHandle(thread_handle);
}

void suspend_thread(unsigned int tid){
	HANDLE thread_handle = open_thread(tid);
	SuspendThread(thread_handle);
	CloseHandle(thread_handle);
}

void kill_process_by_pid(DWORD pid){
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	TerminateProcess(hProcess,0);
}

void inject_remote_thread(DWORD target_pid, void* addr, void* arg){
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid);
	if(!hProcess){return;}
	HANDLE threadID = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)addr, arg,0, NULL);
	CloseHandle(hProcess);
}

void register_with_handler(DWORD target_pid){
	char msg[64] = {0x00};
	sprintf(msg,"REGISTER %d",target_pid);
	DWORD bytesWritten = 0;
	HANDLE fileHandle = INVALID_HANDLE_VALUE;
	char response[4];
	char paddr[64] = {0x00};
	sprintf(paddr,"\\\\.\\pipe\\VX_%s",getenv("VXAPP_ID"));
	
	fileHandle = CreateFileA(paddr, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	WriteFile(fileHandle,msg,strlen(msg)+1,&bytesWritten,NULL);
	CloseHandle(fileHandle);
}

int inject_process(DWORD target_pid, DWORD target_tid, std::vector<std::string>& libs_to_sideload, BOOL leave_suspended){
	printf("Inject Process: %d %d ls=%d\n",target_pid,target_tid,leave_suspended);
	BOOL is_wow64 = FALSE;
	IsWow64Process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid), &is_wow64);
	load_library_t s;
	ZeroMemory(&s,sizeof(s));
	s.num_libs_to_load = libs_to_sideload.size();
	s.ldr_load_dll = (tLdrLoadDll*)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrLoadDll");
    // Register Modules
	for(int i=0;i<libs_to_sideload.size();i++){
        FormatArchSpecificPreload(libs_to_sideload[i],is_wow64);
		wchar_t wc_path[2048] = {0x00};
		MultiByteToWideChar(CP_ACP, 0, libs_to_sideload[i].c_str(), -1, wc_path, sizeof(wc_path));
		s.filepath[i].Length = wcslen(wc_path) * sizeof(wchar_t);
		s.filepath[i].MaximumLength = s.filepath[i].Length+2;
		s.filepath[i].Buffer = write_path_to_process(wc_path,s.filepath[i].MaximumLength,target_pid);
	}
	
	void* settings_addr = write_to_process(target_pid, &s, sizeof(s));	
	void* shellcode_addr = write_to_process(target_pid, load_library_worker, sizeof(load_library_worker));
	for(int i=0;i<s.num_libs_to_load;i++){
		if(s.filepath[i].Buffer == NULL){
			printf("Error - Buffer is Null for FilePath: %d\n",i);
			kill_process_by_pid(target_pid); 	
			return -1;	
		}
	}
	if(!shellcode_addr || !settings_addr){
			kill_process_by_pid(target_pid); 	
			printf("[-] Error: Shellcode or settings addr is Null\n");		
			return -1;		
	}
	if(!leave_suspended){
		HANDLE thread_handle = open_thread(target_tid);
		if(!thread_handle){
			kill_process_by_pid(target_pid); 	
			printf("[-] Error: Could not Open Thread Handle\n");		
			return -1;
		}
		// Add LdrLoadDll(..., dll_path, ...) to the APC queue.
		if (QueueUserAPC((PAPCFUNC)shellcode_addr, thread_handle,(ULONG_PTR)settings_addr) == 0) {
			printf("[-] Error adding task to APC queue: %ld\n", GetLastError());
			kill_process_by_pid(target_pid); 
			return -1;		
		}

		CloseHandle(thread_handle);
		resume_thread(target_tid);
	}else{
		inject_remote_thread(target_pid,shellcode_addr,settings_addr);
	}
	//Registration Message to Handler with PID/PPID
	register_with_handler(target_pid);
	return 0;
}

// Entrypoint to Spawn a New Process
// StartProcess(target_exe, libs_to_sideload, target_working_directory, target_args);
int start_process(std::string& target_exe, std::vector<std::string>& libs_to_sideload, std::string& target_working_directory, std::vector<std::string>& target_args){
	// Set our APP ID and Channel Name
	std::string cmd = "\"" + target_exe + "\" ";
	
	int param_offset = 4;
	char* exe_base_path = NULL;

	if(target_working_directory == ""){
		// Get Full Path of Executable as our starting directory.
		exe_base_path = (char*)malloc(UNC_MAX_PATH);
		GetDirectoryPath(exe_base_path,(LPSTR)target_exe.c_str());
	}else{
		exe_base_path = (char*)target_working_directory.c_str();
	}

	// Set Parameters
	std::string cmd_args = implode(target_args,' ');
	cmd += cmd_args;
	// Start the Process in a Suspended State
	PROCESS_INFORMATION pi;
	STARTUPINFOA Startup;
	ZeroMemory(&Startup, sizeof(Startup));
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, exe_base_path, &Startup, &pi)) {
		printf("CreateProcess Failed: %d!\n", GetLastError());
		free(exe_base_path);
		return -1;
	}
	free(exe_base_path);
	// Determine if this is a 32 or 64bit target
	BOOL is_wow64 = FALSE;
	IsWow64Process(pi.hProcess, &is_wow64);
	return inject_process(pi.dwProcessId,pi.dwThreadId,libs_to_sideload,FALSE);
}



int main(int argc, char* argv[]) {
	if(getenv("PDXDBG")){
		AllocConsole();
	}
	
	if (argc < 2) { usage(); }
	std::string cmd = "start";
	std::string target_exe = "";
	std::string target_working_directory = "";

	int target_pid = 0;
	int target_tid = 0;
	BOOL leave_suspended = FALSE;
	std::vector<std::string> target_args;
	std::vector<std::string> libs_to_sideload;
	for(int i = 1; i < argc; i++){
		std::string cs = argv[i];
		if(cs.rfind("vxcmd=", 0) != std::string::npos){
			cmd = cs;
			replace(cmd,std::string("vxcmd="),std::string(""));
		}else if(cs.rfind("vxexe=", 0) != std::string::npos){
			target_exe = cs;
			replace(target_exe,std::string("vxexe="),std::string(""));
		}else if(cs.rfind("vxwd=", 0) != std::string::npos){
			target_working_directory = cs;
			replace(target_working_directory,std::string("vxwd="),std::string(""));
		}else if(cs.rfind("vxpid=", 0) != std::string::npos){
			std::string pid_str = cs;
			replace(pid_str,std::string("vxpid="),std::string(""));
			target_pid = atoi(pid_str.c_str());
		}else if(cs.rfind("vxtid=", 0) != std::string::npos){
			std::string tid_str = cs;
			replace(tid_str,std::string("vxtid="),std::string(""));
			target_tid = atoi(tid_str.c_str());
		}else if(cs.rfind("vxls=", 0) != std::string::npos){
			std::string ls_str = cs;
			replace(ls_str,std::string("vxls="),std::string(""));
			leave_suspended = atoi(ls_str.c_str());
		}else if(cs.rfind("vxlibs=", 0) != std::string::npos){
			std::string libs_str = cs;
			replace(libs_str,std::string("vxlibs="),std::string(""));
			libs_to_sideload = split(libs_str,';');
		}else{
			target_args.push_back(cs);
		}
	}

	/*
	printf("Info: \n");
	printf("cmd = %s\n",cmd.c_str());
	printf("exe = %s\n",target_exe.c_str());
	printf("wd = %s\n",target_working_directory.c_str());
	printf("pid = %d\n",target_pid);
	printf("tid = %d\n",target_tid);
	printf("ls = %d\n",leave_suspended);
	for(int i=0;i<libs_to_sideload.size();i++){
		printf("preload = %s\n",libs_to_sideload[i].c_str());		
	}
	for(int i=0;i<target_args.size();i++){
		printf("arg = %s\n",target_args[i].c_str());		
	}
	*/

	if(cmd == "start"){
		return start_process(target_exe,libs_to_sideload,target_working_directory,target_args);
	}else if(cmd == "inject"){
		return inject_process(target_pid,target_tid,libs_to_sideload,leave_suspended);
	}else{
		usage();
	}

/*
    if(!stricmp(cmd,"start")){
        if(argc < 4){usage(argv[0]);}
	    return start_process(argc,argv,0);
    }else if(!stricmp(cmd,"start_in")){
        if(argc < 5){usage(argv[0]);}
		return start_process(argc,argv,1);
    }else if(!stricmp(cmd,"inject")){
        if(argc != 7){usage(argv[0]);}
		char* path_to_libs = argv[2];
        DWORD target_pid = atoi(argv[3]);
		DWORD target_tid = atoi(argv[4]);	
		DWORD target_ppid = atoi(argv[5]);
		int leave_suspended = atoi(argv[6]);
		return inject_process(target_pid,target_tid,target_ppid,path_to_libs,leave_suspended);
    }else{
        usage(argv[0]);
    }
	*/
    return 0;
}
