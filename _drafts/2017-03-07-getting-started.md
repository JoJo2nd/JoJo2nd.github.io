---
layout: post
title:  "Getting Started"
date:   2017-03-07 21:31:00 +0000
categories: psx homebrew
---

Professionally, I've worked on PlayStation 2, 3 & 4; PlayStation Portable, Wii and Xbox 360. I've always found a sense of enjoyment from working on consoles[^1]. I put this down to being a console player as a youngster, it's neat to make games on the type of computers I used to use. I've found in recent years a yearning to develop for the game consoles I played on as a kid and not just the ones around now. Initially this started with an interest with the Dreamcast. I'm very fond of my 3 Dreamcasts[^2] and actually getting Homebrew running on a Dreamcast is pretty trivial due to them having pretty much zero copy protection measures. You just have to look at all the unofficial games released after the Dreamcast's death to see that (e.g [Pier Solar][1], [Gunlord][2], [Last Hope][3]). After briefly looking into Dreamcast Homebrew and getting distracted with some other coding projects I stumbled onto [this custom MegaDrive dev kit][7] (which the videos are worth a watch [here][4], [here][5] & [here][6]). This project is pretty awesome and peaked my interest but I never looked into making my own. I spent a little bit of time looking into the MegaDrive hardware but never went beyond that. 

Then about 3~4 months back I found [PSXDEV][8] and had a hunt around the forums. This led me to find a version of the official PlayStation SDK "psyQ" developed by Psygnosis. The SDK has lots of interesting info in it and a good number of code examples. After a week skim reading it all I thought I'd give it a go as it looked "do-able" on a normal PlayStation.

I should clarify, when I say "do-able" it still means I'd have to get out a soldering iron and build some custom hardware. It's not as easy as the Dreamcast but I did some electronics at school! I thought to myself "It'll be fine :)""

From reading the [PSXDEV][8] forums, my understanding of the steps involved in getting some Homebrew code running on a PlayStation are:
* [Getting a PlayStation](#getting-a-playstation) (the model of the playstation matters here)
* [Modchipping the PlayStation](#modchip-the-playstation)
* [Building a PC to PlayStation cable](#build-a-pc-to-playstation-cable)
* [Burning a loader CD and uploading some Homebrew](#burn-a-loader-cd-and-upload-some-homebrew)

#### Getting a Playstation ####

As I said the model of playstation matters here. I needed a version with the a serial I/O port which is all of the original PlayStation models had. Serial I/O was used for linking two PlayStations together for multiplayer but not many games use this feature.

<img src="/images/getting_started/models_v1.jpg" width="200" />

Additionally, I wanted a parallel port I/O for future plans[^3]. [This limited me to models up to the SCPH-75XX][10]. The redesigned PSone model *can* work but requires soldering a PC connection directly to the main PCB board[^4]. In the end I managed to get a hold of a SCPH-1002 (the bottom one in the picture above). These models are prone to CD read problems but I could in future work around that by replacing the CD unit from a PSone unit.


#### Modchip the PlayStation ####

In order to run any Homebrew I create on the PlayStation I needed to be able to play copied CDs. This required installing a mod chip. I won't go into any real detail here [^5] but I used some [PIC12F629][15]s I had left over from a previous project, a hex dump of the mod chip code and my PIC programmer. With a correctly programmed PIC12F629 I soldered it onto the PlayStation's PCB. The one extra snag was that the chip and hex dump I used was designed for later PlayStation models, not the SCPH-1002, so I had to do some extra research to find the correct solder points on the main board. However, after some googling and cross-referencing pictures of the newer PlayStation PCB boards I ended up with this:

![installed mod chip][9]

The chip is under Harley Quinn's arse. This was my crude method to prevent the chip shorting on the internal shielding in the PlayStation as there's not a lot of space on the underside of the PlayStation's case.

#### Build a PC to PlayStation cable ####

My PC to PlayStation cable was built with the help of [this awesome guide][16]. I'll elaborate on some of the details here as I think I used slightly different components to what is suggested in the guide. One thing to note is that there seems to be two types of cable detailed on the internet. A 'PSXSerial' type cable and a SIOCON type cable (I built the latter type). The difference between the two is that the SIOCON cable will work in all situations but the PSXSerial cable only works in some cases (because it lacks hand shaking lines). It required extra effort but I wanted my options to be open later so I built the fully SIOCONs compatible cable.

First thing I did was grab a [PlayStation Link Cable from eBay][17][^6] and chop it in half. I needed to figure out which of the 8 wires in the cable are used for which lines/signals. As a side note, the PlayStation serial I/O port looks to use a bastardized RS-232 protocol[^7] running at Half Duplex with some of the signals inverted (I could be wrong here, don't take my word for it). The RS-232 protocol defines 8 signals and the link cable has 8 wires, so it looked like I had a 1-to-1 match. I just needed match the the following signals to each wire.

* GND - Ground
* TXD - Transmitted Data
* RXD - Received Data
* DCD - Data Carrier Detect
* DTR - Data Terminal Ready
* CTS - Clear to Send
* RTS - Request to Send
* DSR - Data Set Ready

Grabbing a multi-meter, I started testing connections and based on [info in the original guide][16] I matched up the following wire colours to corresponding signals

* Brown - RXD
* Red - Not Connected!
* Orange - DSR
* Yellow - TXD
* Green - CTS
* Blue - DTR
* Purple - GND
* Gray - RTS

Note that DCD isn't connected here (hence my use of the term bastardized RS-232). To make matters more confusing was the signal inversion, which played a part later on in the build...

So at this point I could have soldered up the correct wires to a [DE-9 plug][20] and be done! Except, I haven't seen a PC with a serial port in a long time. As I don't have access to an old machine nor did I want to get an old machine just to connect to a PlayStation, I wanted my cable to use USB instead. This required extra hardware in the form of a FTDI232 chip to convert between USB and UART. I got mine[^8] from [RS components][21] which might not have been the cheapest but avoids any Chinese fakes from eBay (It sounds like there are a number of fake floating around).

Once I had my FTDI232 I then needed to solder up my wires. However, the bastardized RS-232 came in to play. Although both the PlayStation and the FDTI chip use the same signals some of the PlayStation signals are swapped so I had to remap them. It a bit of a guess but I think swapping the signals was an attempt by Sony to slow down any attempts to reverse engineer the link cable. Anyway, the remapping for the signals is below ([sourced from here, again][16])

* RTS -> CTS
* GND -> GND
* DTR -> DSR
* CTS -> RTS
* TXD -> RXD
* DSR -> DTR
* RXD -> TXD

With the remapping done I built myself a prototype. Note, the wire colours in the picture below match the PlayStation serial cable's wire colours

![PC2PSX prototype 01][23]

![PC2PSX prototype 02][24]

With prototype built, the last step was to program the FDTI chip. Remember before that I said some of the signals were inverted? I needed to make the chip I used aware of this. Luckily, the FDTI chip comes with tools to enable this so it was actually pretty trivial to do. On Windows, [I used this program][25] to set the RTS, CTS, DSR & DTR signals to be inverted.

![fdti_prog][26]

And with that, it was time to test it out.

#### Burn a loader CD and upload some homebrew ####

This was the final step! At this point I needed an EXE to run on the PlayStation. I built one using some example code from the PsyQ SDK. It was a 'Hello World' like program as it seemed quick and simple to build ([see here][27] and [most importantly, here][28] for info on how to build some samples). Next I needed a loader to download the EXE I'd built to the PlayStation. For the loader I burnt [PSXSERIAL][29] to a CD[^9]. With the test EXE ready and the boot loader burnt to a CD I was ready to test it all out :) Connecting up the PlayStation, turning it on and trying it out I got this...

<blockquote class="twitter-tweet" data-lang="en"><p lang="en" dir="ltr">It works!!!! <a href="https://t.co/CPPwFciL2g">pic.twitter.com/CPPwFciL2g</a></p>&mdash; James Moran (@JoJo_2nd) <a href="https://twitter.com/JoJo_2nd/status/817518666073968641">January 6, 2017</a></blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>

I'd call that a successful test.

#### Next Steps ####

Next step should be to build my own little test program. However, I can't see myself getting far without some sort of debugging. For that I think I'll need to look into using PSXSDK instead of PsyQ for development. While the official PsyQ PlayStation SDK is easy to find on the internet it uses a custom compiler by SN Systems (that I'm guessing is a fork of GCC, maybe). The file formats for the SN compiler & linker aren't documented so I don't think I'll have much luck debugging with anything other than an official [PlayStation DevKit (a.k.a. DTL-H2000)][30]. I've not got the space for PS1 DevKit sadly so PSXSDK looks like my best option here. So that'll probably be a topic of the next post...

#### End ####

[^1]: Except the Wii, that can die in a fire.
[^2]: *REALLY* fond of them it seems but even I don't know how I end up with 3 of the sodding things!
[^3]: [This][11]
[^4]: Details of PSone use [here][12] & [here][13]
[^5]: [Google it][14]
[^6]: [Link Cable in action][18]
[^7]: [RS-232][19]
[^8]: I got a [UM232R][22], which is cheap and good enough.
[^9]: Just using ImgBurn. Anything will do.

[1]: https://www.youtube.com/watch?v=NFDHl-c9sac
[2]: https://www.youtube.com/watch?v=HygVagZtSO4
[3]: https://www.youtube.com/watch?v=FUbFYsyjAV4
[4]: https://www.youtube.com/watch?v=mEH7a-a8dvQ
[5]: https://www.youtube.com/watch?v=JxBzxhMhANI
[6]: https://www.youtube.com/watch?v=dLoudQc8L08
[7]: https://hackaday.io/project/1507-usb-megadrive-devkit
[8]: http://www.psxdev.net/
[9]: /images/getting_started/1002_modchip_installed.jpg
[10]: https://en.wikipedia.org/wiki/PlayStation_models#Comparison_of_models
[11]: http://ps-io.com/
[12]: http://www.psxdev.net/forum/viewtopic.php?f=62&t=365
[13]: http://assemblergames.com/l/threads/psone-usb-serial-mini-yaroze.25853/
[14]: https://www.google.co.uk/webhp?hl=en&sa=X&ved=0ahUKEwiGqu-j6_7RAhWEWBQKHTVaCfgQPAgD#hl=en&q=mm3+modchip+install
[15]: https://uk.rs-online.com/web/p/microcontrollers/5441670/
[16]: http://www.psxdev.net/forum/viewtopic.php?f=62&t=1071&p=10295&hilit=serial+cable#p10295
[17]: /images/getting_started/psx_link_cable.jpg
[18]: https://www.youtube.com/watch?v=kbM_Wt7YZl0
[19]: https://en.wikipedia.org/wiki/RS-232
[20]: https://en.wikipedia.org/wiki/D-subminiature
[21]: http://uk.rs-online.com/web/
[22]: https://uk.rs-online.com/web/p/interface-development-kits/0406568/
[23]: /images/getting_started/breadboard_pt.jpg
[24]: /images/getting_started/breadboard_pt2.jpg
[25]: http://www.ftdichip.com/Support/Utilities.htm#FT_PROG
[26]: /images/getting_started/ft_prog.png
[27]: http://www.psxdev.net/help/psyq_install.html
[28]: http://www.psxdev.net/help/psyq_hello_world.html
[29]: http://www.psxdev.net/forum/viewtopic.php?f=69&t=378
[30]: http://www.psxdev.net/images/store/DTL-H2000.JPG
