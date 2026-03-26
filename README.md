# vga_music_v2 — Run Instructions

IMPORTANT: These steps must be followed EXACTLY each time.
Do NOT skip the GDB_SERVER restart when switching programs.

## FIRST TIME (or after switching from another program):
------------------------------------------------------
Tab 1:
  cd path\to\vga_music_v2
  ./gmake DE1-SoC
  ./gmake DETECT_DEVICES
  ./gmake GDB_SERVER        <-- LEAVE THIS TAB OPEN, DO NOT CLOSE

Tab 2 (new PowerShell):
  cd path\to\vga_music_v2
  ./gmake
  ./gmake GDB_CLIENT

## SWITCHING FROM ANOTHER PROGRAM:
--------------------------------
1. Close the GDB_CLIENT tab (Ctrl+C or close window)
2. Close the GDB_SERVER tab (Ctrl+C or close window)  <-- MUST DO THIS
3. cd into vga_music_v2 in Tab 1
4. ./gmake GDB_SERVER        <-- restart server pointing at new folder
5. In Tab 2: cd into vga_music_v2
6. ./gmake                   <-- recompile
7. ./gmake GDB_CLIENT        <-- load new elf

If mouse does not respond: the GDB_SERVER was NOT restarted.
The server caches the old .elf in memory. Always restart it.
