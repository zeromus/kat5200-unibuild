# kat5200-unibuild
kat5200 emulator vs2010 one-click build

from http://kat5200.jillybunch.com/

Includes libraries stripped down to win32 essentials only:
guichan 0.8.1
sdl 1.2.15 + dll
sdl_image 1.2.12
zlib 1.2.8

It can no longer be built in any other environment readily (the makefiles have been whacked)

maybe SDL image dlls are missing. maybe theyre needed. I'm not sure.

kat5200 writes to the kat5200.db3 in the PWD for its configuration and stuff. Maybe savestates too?

drop a 5200.bin in the PWD too, I think it'll autodetect it there? It may be scanning the PWD by hash.