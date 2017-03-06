---
layout: post
title:  "A New Loader"
date:   2017-03-01 22:04:00 +0000
categories: psx homebrew
---
## A New Loader ##

**TL;DR** “Give someone a program, frustrate them for a day. Teach someone to program, frustrate them for a lifetime.”

I'd so far I've managed to run some example code written by someone else on my makeshift PlayStation DevKit. The next step for me was to build my own homebrew and run it. For this I had two options; use the official PsyQ SDK[^1] provided by Sony many years ago to professional developers (most PlayStation games were written using this) or use an open source SDK written from scratch by other hobbyists. The trade off for me was to either use a Black Box solution with possible legal issues[^2] but that would have very few bugs and issues (i.e. PsyQ)  or an open solution which is incomplete in some areas and will probably have some bugs but allows me to learn more about the hardware by not hiding anything from me. In the end I went for the second option and downloaded a copy of [PSXSDK][1]. It's the harder option of the two I suspect but all the easy stuff is a bit boring ;)

Choosing PSXSDK means I need a unix environment. [Cygwin][5] is an option here but I'm unsure of it's support for serial I/O on windows. Serial I/O is my only way to communicate with the PlayStation at this point so the other option is a linux distro. I've wanted to try Manjaro for a little while so this seemed like a good excuse to install it. 

With Manjaro installed I grabbed the source code to a recent version of [GCC][2] and [BINUTILS][3] so I could build the toolchain from source. Instructions for building [PSXSDK are here.][4] Unusually for linux this process was pretty smooth. I'm pretty sure I lucked out here because the next steps were ~~far less smooth~~ an utter ball ache!

I now appeared to have a working toolchain to compile my own programs for the PlayStation. To check all was working correctly I compiled a sample program from PSXSDK which built fine. Next I just needed to upload it to the PlayStation and make sure it ran OK. Here I hit a snag; The PSXSERIAL program I used to upload programs to the PlayStation was written for windows and so wasn't going to work out of the box on linux. I tried a number a options here, first I tried running the program under [wine][1000]. Under wine PSXSERIAL ran but was unable to actually connect to the PlayStation. My guess here is that wine's serial port support is a lesser used feature and as such it's a little bit flaky. Next try was to use a virtual matchine running Windows 10. Setting up the virtual machine took a good few hours and worked first time! And then never worked again. So I gave up and booted up another machine with Windows installed and uploaded my test program from there.

Using another machine to upload was a little impractical. Each time I had to compile, copy to a USB, move the USB to the other machine, run PSXSERIAL on that machine and upload to the PlayStation. One option I thought of later is possibly to have SSH'd into the Windows machine and copy across the network but SSH is a real pain to set up on Windows if I remember correctly. My other alternative was to port PSXSERIAL to linux. I had the [source code][7] from the first version so it shouldn't be too difficult, I'd just have to learn a little about working with serial ports on linux.

#### Porting PSXSERIAL to linux ####
After a couple of evening studying [a few guides][6] and the original source I was in a position to write some code. After getting some test code working (e.g. opening and closing the serial port) and reading the PlayStation client source I was pretty sure what I needed to do and I ended up with these two code snippets

{% highlight C linenos %}
void PSXPort_init(char const* device, long bps) {
  /*
   Open 'device' for read & write, O_NOCTTY means that no terminal will control the process opening the serial port.
   O_NDELAY is ignore the DCD line.
  */
  termios_t port_settings;

  PSXPort = open(device, O_RDWR | O_NOCTTY /*| O_NDELAY*/);

  if (PSXPort == 1) 
    printf("Error opening %s\n", device);
  else
    printf("Opened %s\n", device);

  // Get settings to read in the debugger
  tcgetattr(PSXPort, &port_settings);
  // Set the port read/write speeds (hard coded and ignores bps)
  cfsetispeed(&port_settings, B115200);
  cfsetospeed(&port_settings, B115200);

  // Sets the port into 'raw' mode. There is a call to cfmakeraw() but do it this way to be explicit but 
  // the flags differ slightly.
  // Enable the receiver and set local mode
  port_settings.c_cflag |= (CLOCAL /*| CREAD*/);
  // enable No parity 8 bit mode, 1 stop bit (not 2)
  port_settings.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  port_settings.c_cflag |= CS8;
  // enable RAW input mode
  port_settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);
  // enable RAW ouptut mode
  port_settings.c_oflag &= ~OPOST;
  // disable software flow control
  port_settings.c_iflag &= ~(IXON /*| IXOFF | IXANY*/);
  // no SIGINT and don't convert \r->\n
  port_settings.c_iflag &= ~(BRKINT | ICRNL);
  // disable parity check, and strip the parity bits
  port_settings.c_iflag &= ~(INPCK | ISTRIP);
  // Apply the settings to the port. TCSAFLUSH is empty I/O buffer first
  tcsetattr(PSXPort, TCSAFLUSH, &port_settings);

  // Turn off RTS & DTR, ON: ST
  int set;
  set = TIOCM_RTS | TIOCM_DTR; //clear
  ioctl(PSXPort, TIOCMBIC, &set);
  set = TIOCM_ST; //set
  ioctl(PSXPort, TIOCMBIS, &set);

}
{% endhighlight %}

{% highlight C linenos %}
char PSXPort_putch(char c)
{
  int         n, set;
  time_t      start = time(NULL);

  // busy wait on CTS==true & SR==false
  do {
    ioctl(PSXPort, TIOCMGET, &set);
    time_t cur = time(NULL);
    float diff = (float)difftime(cur, start);
    if (diff > 1/*seconds*/) {
      printf("timeout %#X!\n", set);
      return -1;
    }
  } while ((set & (TIOCM_CTS | TIOCM_SR)) != TIOCM_CTS);

  // write
  if ((n = write(PSXPort, &c, 1)) < 0)
    printf("write failed\n");
  return n;
{% endhighlight %}

These two routines are the main workers in my port. PSXPort_init is just opening the port for talking to the PlayStation. This is pretty standard stuff for opening a serial port in RAW mode. I'm not sure what the other modes are that you can use with the serial port. I knew I needed RAW mode and that was a far as I read about. PSXPort_putc actually write data to the PlayStation one byte at a time. From reading the PlayStation client code for PSXSERIAL is works like so: 

First its raises the CTS (Clear To Send) signal and waits forever. This is a que for the PC side to begin sending data to the PlayStation. Once the PC side see the raised CTS it can send a byte of data to the PlayStation. When the PlayStation receives the byte is clears the CTS signal and does whatever processing this needs to with the byte (normally it just copies it into memory somewhere). When the PlayStation is ready for another byte it starts over again. There is some extra logic on the PC side which checks the TIOM_SR bit is also clear, the SR bit is the status bit for RXD (receive) buffer and we are checking to ensure the receive buffer is empty (i.e. nothing is waiting to be read by the PlayStation). To be honest, I think the CTS logic is enough to ensure is all works but it's something the original code did so I kept it in my port.

With those functions I was able to send data to the PlayStation and the basic flow of what I needed to do was this:
 * Send an initial sync byte (0x63)
 * Send the header of the EXE I wanted to copy to the PlayStation. This was a fixed size of 2048 bytes and contains stuff like the start stack pointer address, the initial program counter address, etc
 * Send the .text data size, .text data start address and the global pointer (this is something MIPS related). One odd this is these things already exists in the header of the EXE just sent so I find it odd that they are needed again.
 * Send the .text section (i.e. the actual code). Once sent PSXSERIAL will boot the downloaded EXE

Excluding usual programming crap of things never working first time, I got something starting the download to the PlayStation pretty quick. There was a problem, however, as the download would hang midway though. After some extra research I discovered (as far as I could tell) that the protocol for PSXSERIAL had changed over versions. The source code I had was from one of the earlist versions of PSXSERIAL and looked to be incompatible with the newest versions. I started looking into using some earlier version of PSXSERIAL but after two failed attempts I said hell to it, I'll write my own version. I had to learn some details of how the PlayStation BIOS worked but I'd probably end up doing it sooner or later.

#### The PlayStation side of things ####

Following the process defined above I laid out something pretty quick but lacking any code to read from the serial port. Writing to or reading from the serial port is completely different to the process on linux. Linux treats the port as a file that can be read from or written to. The PlayStation, like many other embedded devices I suspect, uses [memory mapped I/O][8] to work with the port. Given the choice of the two, memory mapped I/O is my preferred method. On the PlayStation the serial port is mapped the memory address 0x1F801050, 0x1F801054, 0x1F80105A. 0x1F801054 and 0x1F80105A map to the status register and control register respectfully, 0x1F801050 is where any data for the serial port is written to or read from depending on if the PlayStation is sending or receiving data. 

The PlayStation had to match the Linux code which means setting the **C**lear **T**o **S**end flag in the control register. Once set the Linux machine should send down a new byte of data which I check the status register to confirm the byte has arrived (this is the case when the RXRDY flag in the status register is true). Once the byte has arrived the code reads the data from 0x1F801050 and unsets the CTS bit in the control register. Rinse and repeat until all data is sent. Some example code below:

{% highlight C linenos %}
/** SIO FIFO Buffer (TX/RX) Register [Read/Write] */
#define SIO_TX_RX       *((volatile unsigned char*)0x1F801050)
/** SIO Status Register [Read Only] */
#define SIO_STAT        *((volatile unsigned short*)0x1F801054)
/** SIO Mode Register [Read/Write] */
#define SIO_MODE        *((volatile unsigned short*)0x1F801058)
/** SIO Control Register [Read/Write] */
#define SIO_CTRL        *((volatile unsigned short*)0x1F80105A)
/** SIO Baud Rate Register [Read/Write] */
#define SIO_BPSV        *((volatile unsigned short*)0x1F80105E)

unsigned char SIOReadByte() {
    return (unsigned char)SIO_TX_RX;
}
int SIOCheckOutBuffer() {
    /*Return status of TX Ready flag*/
    return (SIO_STAT & 0x4)>0;
}

uint8_t sio_read_uint8() {
    uint8_t r;
    SIO_CTRL |= CR_RTS; // RTS on
    while (!SIOCheckInBuffer()); // Checks RXRDY
    r = SIOReadByte(); // Reads the value from 0x1F801050
    SIO_CTRL &= ~CR_RTS; // RTS off
    return r;
}
{% endhighlight %}

After a few practice runs with this code the PlayStation was is successfully downloading an exe from the Linux machine. The last step was to add code to boot the downloaded exe and I was finished. Sadly, it was that simple as it turns out there was another problem waiting...

#### Address Conflicts ####

At this point I was able to download an PlayStation exe from my linux machine but my test PlayStation loader simply discarded any data given to it as I was only testing the serial IO protocol at this point. Instead of discarding any data downloaded I'd need to store it in the correct place in the PlayStation's memory. Once finished my loader would just run the newly copied exe. The first naive version that I wrote would just hang midway through the download. "Odd" I thought "it downloaded fine before?" but it didn't take long to deduce the cause of this new issue. My new loader exe is located at 0x80010000 in memory and the exe I was downloading was located at 0x80010000. At some point the loader copied over it's own code for downloading from the linux machine and crashed (probably on an invalid opcode or memory read/write). The first fix I tried for this was to create some relocatable code. With relocatable code I could move the downloading and copying functions to someplace safe in memory while getting the new exe. Once done, the PlayStation would boot the new exe. The easiest way to create a section of code I could relocate at runtime was to write some in assembly. I've not worked with [MIPS][12] assembly at all but I am somewhat use to RISC (from working on Xbox 360 and PlayStation 3 mostly) so the jump wasn't that hard ([I got by mostly with just this reference][13]). After an hour or two I had the following

{% highlight asm linenos %}
.global ldr
.global ldr_END

.global memldr
.global memldr_END

#constants
.set SIO_BASE_ADDR,     0x1F801050
.set SIO_TX_RX_OSET,    0x0
.set SIO_STAT_OSET,     0x4
.set SIO_CTRL_OSET,     0xA

.set SIO_TX_RX_ADDR,    0x1F801050
.set SIO_STAT_ADDR,     0x1F801054
.set SIO_CTRL_ADDR,     0x1F80105A
.set SIO_CTRL_CTS,      0x00000020
.set SIO_STATR_RXRDY,   0x00000002

ldr: # a0 = address of PSX-EXE buffer
    lw $t0, 0x0($a0) # Load the write addres (pc0)
    lw $t1, 0xc($a0) # Load text section size (t_size)
    li $a1, SIO_BASE_ADDR # Load serial io read/write address
    li $a2, SIO_STAT_ADDR # Load serial io status reg address
    li $a3, SIO_CTRL_ADDR # Load serial io ctrl reg address
    li $t4, -1
    li $t5, SIO_CTRL_CTS 
    xor $t5, $t4, $t5 # Store ~CTS

ldr_readSIO:
    lh $t3, ($a3)   # read sio ctrl reg
    ori $t3, $t3, SIO_CTRL_CTS  # CTS: on
    sh $t3, ($a3)   # write CTS: on
ldr_sioWait:
    lw $t3, ($a2)       # read sio status reg
    andi $t3, $t3, SIO_STATR_RXRDY  # check RX has data in
    beq $t3, $zero, ldr_sioWait     # wait on RX flag to be on
    nop

    lb $t3, ($a1)       # read the sio data
    sb $t3, 0($t0)              # write to write address

    lh $t3, ($a3)   # read sio ctrl reg
    and $t3, $t5, $t3           # CTS: off
    sh $t3, ($a3)   # write CTS: off

    addiu $t0, 1            # pc0 + 1
    addiu $t1, -1           # t_size - 1
    bne $t1, $zero, ldr_readSIO
    nop

    # Custom version
    # Using syscalls. Can't use syscall.s version, may have been destroyed
    # call EnterCriticalSection
    or $t5, $a0, $a0 # $t5=$a0 
    li $a0, 1
    syscall
    nop
    # call Exec
    or $a0, $t5, $t5 # $a0=$t5 
    li $a1, 1 # argc
    li $a2, 0 # argv (null)
    li $9, 0x43 
    j 0xa0 # call Exec. 
    nop

ldr_END:
{% endhighlight %}

Note, I didn't end up using this in the end so it will have bugs I'm sure. It's also my first attempt at MIPS assembly so it's probably a bit crappy. The one issue that really caught me out was reading the control register, it turns out that I had to use a lh/sh instruction (load/store halfword) not a lw/sw (load/store word) otherwise I'd get a bus/address error. Anyway, this small amount of code reads the serial IO port for data and writes that data into the address in $t0. Once all data has been copied (so $t1 equals zero) it turns off CPU interrupts and calls a BIOS function to reboot the PlayStation. Said BIOS function needs the header for the PlayStation exe to boot but it's passed to this code in $a0 so I simply pass it along but note that it's being saved across the call to EnterCriticalSection(). I'm not 100% sure CPU interrupts need to be disabled or about the save to the $t5 register but other code I've seen did this so I followed their example. A new coding trick for me was the use of the ldr: and ldr_END: labels. This labels enabled my C code to find this code in memory and know how large is was. Knowing these two things enabled me to move this function anywhere in memory before starting the download process. The (easy) question is where? To answer that question below is the memory map of the PlayStation.

 | Address Range         | Use                  |
 |-----------------------|----------------------|
 | 0x00000000-0x0000FFFF | Kernel (64K) |
 | 0x00010000-0x001FFFFF | User Memory (1.9 Meg) | 
 | 0x1F000000-0x1F00FFFF | Parallel Port (64K) |
 | 0x1F800000-0x1F8003FF | Scratch Pad (1024 bytes) |
 | 0x1F801000-0x1F802FFF | Hardware Registers (8K) |
 | 0x80000000-0x801FFFFF | Kernel and User Memory Mirror (2 Meg) Cached |
 | 0xA0000000-0xA01FFFFF | Kernel and User Memory Mirror (2 Meg) Uncached |
 | 0xBFC00000-0xBFC7FFFF | BIOS (512K) |

Note that the address ranges 0x00000000-0x001FFFFF, 0x80000000-0x801FFFFF and 0xA0000000-0xA01FFFFF are mirrored and therefore contain the same data. Normally, PlayStations exes are located at the start of memory (i.e. 0x80010000 not 0x8000000 because the first 0x10000 is used by the PlayStation Kernel) and the stack is located at the end of memory at 0x801FFFFF so the easiest and probably safest place to move the assembly code too is on the stack. This give as much space as possible for the exe. A quick reshuffle of the code and this was working **most, but not all of the time**. I tried a few hours of debugging this in an emulator will no luck (it always worked in the emulator) so I started looking for another way around the problem.

If I couldn't move a single function, why not just move the entire program to the end of memory? It must be possible. Turns out it is and it's actually pretty easy. Had I maybe been more experienced with the GCC toolchain I'd have done this first. When linking my loader it GCC/BINUTILS uses a linker script which look like this...

{%highlight C %}
TARGET("elf32-littlemips")
OUTPUT_ARCH("mips")

ENTRY("_start")

SEARCH_DIR("/usr/local/psxsdk/lib")
STARTUP(start.o)
INPUT(-lpsx -lgcc)

SECTIONS
{
  . = 0x80010000;

  __text_start = .;
  .text ALIGN(4) : { *(.text*) }
  __text_end = .;

  ...

{%endhighlight%}

It's pretty easy to spot that 0x80010000 start address. I changed that value to 0x80C00000 (to leave some space for my loader exe to live in) and rebuilt everything. Worked first time! This means I have a working loader I can use from Linux.

#### Final issues ####

I had one final issue to deal with. My loader doesn't work if the exe it attempts to boot uses the CD-ROM driver in the BIOS. **sigh** At this point I doubt I'll solve this easily so my plan is to avoid this until I've got a real debugger in-place. Then I can really dig into the issue but until then it'll just be an exercise in frustration. I can at least work with what I've got now so I'll stop at this point.

And that's it for this post, I suspect the next post will be on starting to write the GDB debugger stub for the PlayStation. Fun...

For those interested, full source code for my loader is here [pc_loader][9], [psx_loader][10], [psx_asm][11]. Be warned, it's messy.

[^1]: **S**oftware **D**evelopment **K**it
[^2]: Sony officially still own PsyQ and it's use is probably not legal (**probably** because I'm not a lawyer but read that as **definitely**). It's unlikely Sony would do anything about it these days but who knows? They technically have the right

[1]: http://unhaut.x10host.com/psxsdk/
[2]: https://gcc.gnu.org/
[3]: https://www.gnu.org/software/binutils/
[4]: /data/new_loader/psxsdk_toolchain.txt
[5]: https://www.cygwin.com/
[6]: https://www.cmrr.umn.edu/~strupp/serial.html
[7]: http://hitmen.c02.at/files/releases/psx/hitserial-source.zip
[8]: https://en.wikipedia.org/wiki/Memory-mapped_I/O
[9]: /data/new_loader/pc_serial.c
[10]: /data/new_loader/psxldr.c
[11]: /data/new_loader/ldr.s
[12]: /data/new_loader/mips_arch.pdf
[13]: http://vhouten.home.xs4all.nl/mipsel/r3000-isa.html
 
