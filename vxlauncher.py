import os
import sys
import ctypes

from vxapp import VXApp

# Adminsitrative Guard
is_admin = False
try:
	is_admin = os.getuid() == 0
except AttributeError:
	import ctypes
	is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
if not is_admin:
	print("[VX] Error: This Application Must be Run with Administrative Privileges")
	sys.exit(-1)


def usage():
    print(f"Usage: {sys.argv[0]} path_to_vxapp [no_launch] [config=config_name]")
    sys.exit(-1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        usage()
    
    app_path = sys.argv[1]
    no_launch = False
    config_name = "default"
    for i in range(0,len(sys.argv)):
        if "config=" in sys.argv[i]:
            config_name = sys.argv[i].replace("config=","")
        if sys.argv[i] == "no_launch":
            no_launch = True
    app = VXApp(app_path,selected_config=config_name,no_launch=no_launch)
    input("Press Any Key to Clean Up\n")

