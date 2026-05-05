# fw/openocd/debug.gdb
target extended-remote :3333
set print pretty on
set pagination off
monitor reset halt
load
break main
continue
