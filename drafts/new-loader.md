
* Switch to using PSXSDK. Get it built.
* rewrite loader as non work on linux. Tried running some under wine & vm box with no luck
* Write one and run in debug mode (allow redownload, don't boot exe, do CRC check)
* Try load exe - doesn't work.
 * Try running in emulator, EXE works but no$psx doesn't because only bin/cue supported. Turns out to config file references exe name larger than 8.3 DOS format