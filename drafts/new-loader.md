
* Switch to using PSXSDK. Get it built.
* rewrite loader as non work on linux. Tried running some under wine & vm box with no luck
* Write one and run in debug mode (allow redownload, don't boot exe, do CRC check)
* Try load exe - doesn't work.
 * Try running in emulator, EXE works but no$psx doesn't because only bin/cue supported. Turns out to config file references exe name larger than 8.3 DOS format

 #### The PSX Memory Map ####
 * 0x0000_0000-0xx0000_FFFF Kernel (64K)
 * 0x0001_0000-0x001F_FFFF User Memory (1.9 Meg) 
 * 0x1F00_0000-0x1F00_FFFF Parallel Port (64K)
 * 0x1F80_0000-0x1F80_03FF Scratch Pad (1024 bytes)
 * 0x1F80_1000-0x1F80_2FFF Hardware Registers (8K)
 * 0x8000_0000-0x801F_FFFF Kernel and User Memory Mirror (2 Meg) Cached
 * 0xA000_0000-0xA01F_FFFF Kernel and User Memory Mirror (2 Meg) Uncached
 * 0xBFC0_0000-0xBFC7_FFFF BIOS (512K)
 * 0x801F_FFF0-0x801F_FFFF Stack (?)

 * Got loader runing with Exec() sys call and EnterCriticalSection(). PSXSDK_RunExe() function worked on emulators, didn't on hardware. Exec worked but rebooted the same exe and so needed the exe loaded in to RAM before running. Loading into RAM via SIO locks up the loader (crashes? can't tell). Assuming that this is due to stamping over running process

 * Need to write PosIndepCode in asm to read SIO, write exe in place then jump to Exec sys call.

 * mipsel-unknown-elf-nm psxldr.elf (a.k.a. nm psxldr.elf) to dump symbol locations. This lists ldr & ldr_END