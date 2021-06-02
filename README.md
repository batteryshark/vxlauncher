# vxlauncher
A Launcher for the "vxapp" format - uses the native "libsmoothie" library.

Usage:

vxlauncher path_to_vxapp_directory [config=config_name] [no_launch]

Optional Arguments:
- config: Load a specified configuration as named in the app's vxapp.info file.
- no_launch: Load a configuration, but do not execute any target. Note, this will also not set any envars.