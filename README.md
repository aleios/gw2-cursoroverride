 = GW2 Cursor Override =

 This is a basic dynamic library to be loaded by arcdps that overrides the default mouse cursors with a custom image.

 Works as an alternative to things like YoloMouse (limited in comparison). Primarily made it for use under WINE on Linux.

 Currently just looks for a file named cursor.png next to GW2-64.exe to use as the cursor.

 == Compile Instructions ==

 Build generated by Cmake. Suggest compilation via the MSVC toolchain.


 == Hooked WIN32 Events ==

 * SetCursor
 * SetClassLongPtrA
 * SetClassLongPtrW

 * WM_SETCURSOR