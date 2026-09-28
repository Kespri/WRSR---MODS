# Vehicle Materials 0.4.0

Choose ONE installation method. Subscribing alone does not activate a native DLL.

1. Classic TesmioLoader: with the game closed, copy the DLL and INI from hooks
   into tesmioloader\build\plugins. Preserve an existing personal INI. Enable the
   plugin in TesmioLauncher and provide Resources with all configured materials.
2. SML: the root soviet.mod.ini declares hooks\vehicle_materials.dll. Keep its
   INI beside it. Disable/remove any duplicate local vehicle_materials.dll first.
   Configured materials must exist in SML's resource catalog. SML 0.6.0 provides
   its own Resources service; this package does not bundle SML core plugins.
   Package compatibility is prepared; an actual SML game test is still needed.
3. Tesmio Settings / Autoload: select this package, review personal settings, then
   deploy before starting the unchanged TesmioLauncher. DLLs are staged locally;
   personal overrides remain in build\user_config. Workshop originals remain
   untouched. Deploy again before launching to pick up Workshop updates.

Autoload and SML are alternatives, not simultaneous loaders. Autoload overrides
do not automatically affect the Workshop INI loaded directly by SML.

Requires Windows x64, WRSR 1.1.1.9, TesmioLoader API 4 and a Resources service
registering all configured materials (glass and cable by default). Native DLLs
execute code: use only packages from authors you trust.

Version 0.4.0 changes only INI discovery to use the plugin DLL directory.
Vehicle material calculations remain unchanged from 1.1.0.
