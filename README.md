# vxlauncher
A Launcher for the "vxapp" format - uses the native "libsmoothie" library.

Usage:

vxlauncher path_to_vxapp_directory [cmd=STARTUP/CLEANUP] [config="OTHER CONFIG"] [opt=NOLAUNCH]

Optional Arguments:
- cmd=STARTUP: default, loads an app.
- cmd=CLEANUP: unmounts and cleans up an existing app.
- config: Load a specified configuration as named in the app's vxapp.info file.
- NOLAUNCH: Load a configuration, but do not execute any target. Note, this will also not set any envars.
