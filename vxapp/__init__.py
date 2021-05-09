# Parent Class to Represent VXApps
import os
import platform
import shutil
from pathlib import Path
import hashlib
import atexit
from .vxappconfig import VXAppConfig
from smoothie import create_smoothie, destroy_smoothie
from smoothie.mapmanager import resolve_map_path

TMP_ROOT = Path("/vxtmp").resolve()
SAVE_ROOT = Path("/vxsave").resolve()
# TODO - Replace with just ./bin
BIN_ROOT = Path("/repos/pdx/bin").resolve()

CODE_POOL = [
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F',
    'G','H','J','K','L','M','N','P','Q','R','S','T','U','V','W','X',
    'Y'
]
if platform.system() == "Windows":
    EXT_EXE = "exe"
    EXT_LIB = "dll"
else:
    EXT_EXE = "elf"
    EXT_LIB = "so"

def determine_target_arch(path_to_executable):
    with open(path_to_executable,"rb") as f:
        data = f.read()
        if b"\x50\x45\x00\x00\x64" in data:
            return "64"
        elif b"\x50\x45\x00\x00\x4C" in data:
            return "32"
        elif data[0x12] == b"\x03":
            return "32"
        elif data[0x12] == b"\x3E":
            return "64"
    return "64"


class VXApp:
    def __init__(self, path_to_vxapp,selected_config="default", no_launch=False):
        self.valid = False
        self.config = VXAppConfig(os.path.join(path_to_vxapp,"vxapp.info"),selected_config)
        self.vxapp_name = os.path.splitext(os.path.basename(path_to_vxapp))[0]
        self.id = self.generate_app_id(self.vxapp_name)
        self.tmp_path = TMP_ROOT.joinpath(self.id)
        self.save_path = SAVE_ROOT.joinpath(self.id)
        if not self.save_path.exists():
            self.save_path.mkdir(parents=True,exist_ok=True)
        if not self.tmp_path.exists():
            self.tmp_path.mkdir(parents=True,exist_ok=True)
        self.content_path = os.path.join(path_to_vxapp,"content")
        self.local_plugin_path = os.path.join(path_to_vxapp,"plugins")
        path_to_mapfile = os.path.join(self.content_path,self.config.selected_configuration['map'])
        res = create_smoothie(self.tmp_path, path_to_mapfile,path_to_persistence=str(self.save_path.resolve()))
        if res == -1:
            print("Smoothie Environment Creation Failed")
            return    
        atexit.register(self.cleanup)
        if not no_launch:
            self.valid = self.launch_process()
        else:
            self.valid = True

        
    def generate_app_id(self,app_name):
        o_code = ""
        s = hashlib.sha1()
        s.update(app_name.encode('ascii'))
        h_code = []
        for cv in bytearray(s.digest()):
            h_code.append(CODE_POOL[cv % len(CODE_POOL)])
        o_code = "".join(h_code)
        return f"{o_code[0:3]}-{o_code[3:6]}-{o_code[6:9]}"

    
    def cleanup(self):
        destroy_smoothie(self.tmp_path)
        shutil.rmtree(self.tmp_path)

    def resolve_preload_path(self, preload_name):
        local_path = os.path.join(self.local_plugin_path,preload_name)
        if os.path.exists(local_path):
            return str(local_path.resolve())
        global_path = BIN_ROOT.joinpath(preload_name)
        if global_path.exists():
            return str(global_path.resolve())
        return None

    def launch_process(self):
        print(f"Launching App as ID: {self.id}")
        map_root = os.path.join(self.tmp_path,"map")        
        # Get EXE Path and Architecture First
        exe_vpath = self.config.selected_configuration['executable']
        resolved_exe_path = resolve_map_path(map_root,exe_vpath)
        if not os.path.exists(resolved_exe_path):
            print(f"Executable Path Cannot be Found: {exe_vpath}")
            return False
        resolved_exe_path = str(resolved_exe_path)
        # Determine Architecture
        exe_arch = determine_target_arch(resolved_exe_path)

        # Set up Any Envars Needed
        envars = self.config.selected_configuration.get("envar",{})
        if envars:
            for ek in envars.keys():
                os.environ[ek] = envars[ek]
        # Set up Required Envars
        os.environ['PDXPROC'] = str(Path(BIN_ROOT).resolve())

        # Base List of Plugins - Always load pdxfs.
        preload_names = [f"pdxfs{exe_arch}.{EXT_LIB}"]
        preload_names.extend(self.config.selected_configuration.get("preload",[]))
        preload_paths = []
        for preload_name in preload_names:
            preload_path = self.resolve_preload_path(preload_name)
            if not preload_path:
                print(f"Missing Preload: {preload_name}")
                return False
            preload_paths.append(preload_path)

        print(preload_paths)
        os.environ['PDXPL'] = ";".join(preload_paths)
        os.environ['PDXFS_ROOT'] = map_root
        os.environ['PDXFS_MODE'] = "1"
        os.environ['PDXFS_IGNORE'] = ";".join([
            str(TMP_ROOT),
            str(SAVE_ROOT),
            str(BIN_ROOT)
        ])



        exe_args = self.config.selected_configuration.get('args',"")
        exe_vcwd = self.config.selected_configuration.get('cwd',"")
        
        resolved_cwd = None
        if exe_vcwd:
            resolved_cwd = resolve_map_path(map_root,exe_vcwd)
            if not os.path.exists(resolved_cwd):
                print(f"Executable CWD Cannot be Found: {exe_vcwd}")
                return False


        dropkick_path = str(BIN_ROOT.joinpath(f"dropkick{exe_arch}.{EXT_EXE}").resolve())
        pdxproc_path = str(BIN_ROOT.joinpath(f"pdxproc{exe_arch}.{EXT_LIB}").resolve())
        if not os.path.exists(dropkick_path):
            print(f"Could Not Find Path to Dropkick: {dropkick_path}")
            return False
        
        if not os.path.exists(pdxproc_path):
            print(f"Could Not Find Path to PDXProc: {pdxproc_path}")
            return False

        if platform.system() == "Windows":
            if not resolved_cwd:
                cmd = f"{dropkick_path} start {pdxproc_path} {resolved_exe_path} {exe_args}"
            else:
                cmd = f"{dropkick_path} start_in {pdxproc_path} {resolved_exe_path} {resolved_cwd} {exe_args}"
        else:
            cmd = f"LD_PRELOAD={pdxproc_path} {resolved_exe_path} {exe_args}"
        
        print(f"Executing: {cmd}")
        os.system(cmd)
        return True
