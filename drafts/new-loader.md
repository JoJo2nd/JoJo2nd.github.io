## Outline ##

* [-] Explain move to linux & PSXSDK
* [-] Process to build PSXSDK
* [-] Explain problem with current loader (windows only) and the process to upload an EXE
  *  [-] note that Cygwin was an option but unsure of serial IO and need it for debugger.
  *  [-] also keen to work on linux
* [-] Source of the original loader - explain it's quite old now
  * [-] Port to linux serial port IO
  * [-] Attempt to use old PSXSerial loader exe - doesn't work. The protocol has changed between the original send
 * [-] As the protocol has changed the need to rewirte the PlayStation side of the loader
    *  [-] Explain the serial process
    *  [-] Write loader based on what PC side does and what PSXSDK_LoadExe() does
    *  [-] Built a debug versio to test serial protocol. 1st load failes, EXE boots on PSX but doesn't get any data, emulator doesn't boot this exposed ISO 8.3 filename limit.
    *  [-] So the first run source (with no reboot)
 * [-] Explain the what the loader needs to do (address, code to copy, ASM, etc)
    * [-] Show the original C loader code. This works but dies because code stamps over itself
    * [-] Explain options; C: move where code is located or ASM: copy the loader
    * [-] Go through ASM version, doesn't work because of notes below
    * [-] Switch back to C version, fix up the linker script and change the elf2exe to read the correct start address
 * [-] Works & still remaining issues

## Real Deal ##

## NOTES ##

* Switch to using PSXSDK. Get it built.
* rewrite loader as non work on linux. Tried running some under wine & vm box with no luck
* Write one and run in debug mode (allow redownload, don't boot exe, do CRC check)
* Try load exe - doesn't work.
 * Try running in emulator, EXE works but no$psx doesn't because only bin/cue supported. Turns out to config file references exe name larger than 8.3 DOS format

 #### The PSX Memory Map ####
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

 * 0x1F801050 - SIO TX RX
 * 0x1F801054 - SIO STAT (check )

 * Got loader runing with Exec() sys call and EnterCriticalSection(). PSXSDK_RunExe() function worked on emulators, didn't on hardware. Exec worked but rebooted the same exe and so needed the exe loaded in to RAM before running. Loading into RAM via SIO locks up the loader (crashes? can't tell). Assuming that this is due to stamping over running process

 * Need to write PosIndepCode in asm to read SIO, write exe in place then jump to Exec sys call.

 * mipsel-unknown-elf-nm psxldr.elf (a.k.a. nm psxldr.elf) to dump symbol locations. This lists ldr & ldr_END
#### Mips addressing modes ####
 * Register Addressing This is used in the jr (jump register) instruction.
 * PC-Relative Addressing This is used in the beq and bne (branch equal, branch not equal) instructions.
 * Pseudo-direct Addressing This is used in the j (jump) instruction.
 * Base Addressing This is used in the lw and sw (load word, store word) instructions.
 * __MIPS does not support indirect addressing__

 * stuck on reading ctrl register as only 2 btyes but reading as 4 byte was crashing
 * Added EnterCriticalSection() and returned before calling Exec
 * Moved to high memory using linker script
 * Some exe's don't work - thought is was size based - was linked to calling _96_init()/_96_close()


Loader.s
```asm
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
    #lw $sp, 0x20($a0) # Load initial sp value (s_addr) ~ Avoid doing this, don't think it's needed.
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
    #lw $t0, ($a0) # Load the write addres (pc0)
    #jr $t0 # jump to program start
    #nop
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
```
======
psxldr.c
```C
/**
 * PSXSDK loader
 *
 * released to the Public Domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <psx.h>
#include <psxbios.h>

#define CVAR_USE_SIO (1)
#define CVAR_USE_CVERSION (0)
#define CVAR_USE_MOVED_ASM (1)

/* status bits */
#define SR_IRQ      0x200
#define SR_CTS      0x100
#define SR_DSR      0x80
#define SR_FE       0x20
#define SR_OE       0x10
#define SR_PERROR   0x8
#define SR_TXU      0x4
#define SR_RXRDY    0x2
#define SR_TXRDY    0x1

#define SIO_CTS     0x100
#define SIO_DSR     0x80
#define SIO_FE      0x20
#define SIO_OE      0x10
#define SIO_PERROR  0x8
#define SIO_TXU     0x4
#define SIO_RXRDY   0x2
#define SIO_TXRDY   0x1

/* control bits */
#define CR_DSRIEN   0x1000
#define CR_RXIEN    0x800
#define CR_TXIEN    0x400
#define CR_BUFSZ_1  0x0
#define CR_BUFSZ_2  0x100
#define CR_BUFSZ_4  0x200
#define CR_BUFSZ_8  0x300
#define CR_INTRST   0x40
#define CR_RTS      0x20
#define CR_ERRRST   0x10
#define CR_BRK      0x8
#define CR_RXEN     0x4
#define CR_DTR      0x2
#define CR_TXEN     0x1

#define SIO_BIT_DTR CR_DTR
#define SIO_BIT_RTS CR_RTS

typedef enum ctrlcode {
    ctrlcode_sync = 99,

    ctrlcode_max = 255
} ctrlcode_t;


typedef struct EXEC {                   
    unsigned long pc0;      //0x00
    unsigned long gp0;      //0x04
    unsigned long t_addr;   //0x08
    unsigned long t_size;   //0x0c
    unsigned long d_addr;   //0x10
    unsigned long d_size;   //0x14
    unsigned long b_addr;   //0x18
    unsigned long b_size;   //0x1c
    unsigned long s_addr;   //0x20
    unsigned long s_size;   //0x24
    unsigned long sp,fp,gp,ret,base; //0x28
} exec_t;


typedef struct EXE_HDR {
    char key[8]; //0x0
    unsigned long text; //0x08
    unsigned long data; //0x0C
    exec_t exec;
    char title[60];     /* "PlayStation(tm) Executable A1" */
} psxexeheader_t;

#define PSXEXE_HEADER_SIZE (2048)
#define PSXEXE_LDR_SIZE (1024)

typedef enum LoaderState {
    WaitingForSync,
    RecivedSync,
    ReadingHeader,
    ReadingInfo,
    ReadingEXE,
    CRCCheck,
    BootingEXE,

    Max
} LoaderState_t;


char const* LoaderStateStr[] = {
    "WaitingForSync",
    "RecivedSync",
    "ReadingHeader",
    "ReadingInfo",
    "ReadingEXE",
    "Checking EXE...",
    "Booting EXE...",

    "Invalid"
};

unsigned int prim_list[4096]; // did we need 0x20000 (128KB!!) of prim list space? we draw very little. Infact we could just allocate this from the bottom of memory.
int display_is_old = 1;
volatile int frame_number;

extern void ldr(void* exec);
extern void ldr_END();

void loader_vblank_handler() {
    ++frame_number;
}

void render_state(LoaderState_t st, psxexeheader_t const* hdr, void* a1, void* a2) {
    GsSortCls(0, 0, 255);
    uintptr_t pic_size = (uintptr_t)ldr_END - (uintptr_t)ldr;

    GsPrintFont(24, 8, "%s-%s", __DATE__, __TIME__);
    GsPrintFont(24, 16, "%s - Ldr size: %u/%u @ %p", LoaderStateStr[st], pic_size, PSXEXE_LDR_SIZE, a2);
    GsPrintFont(24, 24, "M: %s title: %s", hdr->key, hdr->title);
    GsPrintFont(24, 32, "pc: %x, t_addr: %x, l: %u (%p, %p, %p)", hdr->exec.pc0, hdr->exec.t_addr, hdr->exec.t_size, hdr, a1, a2);

    GsDrawList();
    while(GsIsDrawing());
}

void render_state2(LoaderState_t st, psxexeheader_t const* hdr, void* a2, uint32_t bytes) {
    while(GsIsDrawing());
    GsSortCls(0, 0, 255);
    uintptr_t pic_size = (uintptr_t)ldr_END - (uintptr_t)ldr;

    GsPrintFont(24, 8, "%s-%s", __DATE__, __TIME__);
    GsPrintFont(24, 16, "%s - Ldr size: %u/%u @ %p", LoaderStateStr[st], pic_size, PSXEXE_LDR_SIZE, a2);
    GsPrintFont(24, 24, "M: %s title: %s", hdr->key, hdr->title);
    GsPrintFont(24, 32, "pc: %x, t_addr: %x, l: %u/%u (%p, %p)", hdr->exec.pc0, hdr->exec.t_addr, bytes, hdr->exec.t_size, hdr, a2);

    GsDrawList();
}

void render_state3(LoaderState_t st, psxexeheader_t const* hdr, void* a2, uint32_t bytes) {
    GsSortCls(0, 0, 255);
    uintptr_t pic_size = (uintptr_t)ldr_END - (uintptr_t)ldr;

    GsPrintFont(24, 8, "%s-%s", __DATE__, __TIME__);
    GsPrintFont(24, 16, "%s - Ldr size: %u/%u @ %p", LoaderStateStr[st], pic_size, PSXEXE_LDR_SIZE, a2);
    GsPrintFont(24, 24, "M: %s title: %s", hdr->key, hdr->title);
    GsPrintFont(24, 32, "pc: %x, t_addr: %x, l: %u/%u (%p, %p)", hdr->exec.pc0, hdr->exec.t_addr, bytes, hdr->exec.t_size, hdr, a2);

    GsDrawList();
    while(GsIsDrawing());
}

uint8_t sio_read_uint8() {
    uint8_t r;
    SIO_CTRL |= CR_RTS; // RTS on
    while (!SIOCheckInBuffer());
    r = SIOReadByte();
    SIO_CTRL &= ~CR_RTS; // RTS off
    return r;
}

uint32_t sio_read_uint32() {
    uint8_t b[4];
    for (uint32_t i = 0; i < 4; ++i) {
        b[i] = sio_read_uint8();
    }
    return b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;
}

/*
 This currently seems to work BUT Exec failes when loaded exec calls _96_init()
*/
typedef void (*ldr_func_t)(void*);

int main() {
    uint8_t hdr_data[PSXEXE_HEADER_SIZE];
    uint8_t ldr_data[PSXEXE_LDR_SIZE];
    psxexeheader_t* hdr = (psxexeheader_t*)hdr_data;

    PSX_InitEx(0);

    memset(ldr_data, 0, PSXEXE_LDR_SIZE);
    memset(hdr_data, 0, PSXEXE_HEADER_SIZE);

    GsInit();
    GsClearMem();

    GsSetVideoMode(640, 240, VMODE_PAL);
    GsSetList(prim_list);
    GsSetDispEnvSimple(0, 0);
    GsSetDrawEnvSimple(0, 0, 640, 240);
    GsLoadFont(768, 0, 768, 256);

    SetVBlankHandler(loader_vblank_handler);

    SIOStart(115200);

    while(1) {
        render_state(WaitingForSync, hdr, NULL, NULL);

        uint8_t b = sio_read_uint8();
        if (b != ctrlcode_sync)
            continue; // restart the process        

        render_state(ReadingHeader, hdr, NULL, NULL);   
        for (uint32_t i = 0; i < PSXEXE_HEADER_SIZE; ++i) {
            hdr_data[i] = sio_read_uint8();
        }

        if (strcmp("PS-X EXE", hdr->key))
            continue;

        render_state(ReadingEXE, hdr, &hdr->exec, ldr_data);
        hdr->exec.s_addr = /*STACKP*/0x801ffff0;
        hdr->exec.s_size = 0;

        if (CVAR_USE_CVERSION) {
            uint8_t*exe = (uint8_t*)hdr->exec.pc0;
            for (uint32_t i = 0, n = hdr->exec.t_size; i < n; ++i) {
                *exe = sio_read_uint8(); 
                exe++;
                if ((i%2048) == 0) {
                    render_state2(ReadingEXE, hdr, ldr, i);
                }
            }
            render_state3(ReadingEXE, hdr, ldr, hdr->exec.t_size);
            PSX_DeInit();
            EnterCriticalSection();
            Exec(&hdr->exec, 1, 0);
        } else {
            if (CVAR_USE_MOVED_ASM) {
                // use space on the stack to move our loader to
                uintptr_t pic_size = (uintptr_t)ldr_END - (uintptr_t)ldr;
                uint8_t* pic_loc = (uint8_t*)(((uintptr_t)ldr_data + 3) & ~3);
                // Copy to the stack (which is high mem)
                memcpy(pic_loc, ldr, pic_size);
                // Call our moved code
                ((ldr_func_t)pic_loc)(&hdr->exec);
            } else {
                ldr(&hdr->exec);
            }
        }
    }
}
```

======
hitserial.c
```C

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>  /* File Control Definitions          */
#include <termios.h>/* POSIX Terminal Control Definitions*/
#include <unistd.h> /* UNIX Standard Definitions         */
#include <errno.h>  /* ERROR Number Definitions          */
#include <sys/ioctl.h> // for TIOCM_RTS

#define CVAR_WAIT_ON_CTS_BIT (1)
#define CVAR_WAIT_ON_SR_BIT (0)
#define CVAR_SEND_INFO (0)

typedef struct termios termios_t;
typedef struct timeval timeval_t;

char buf[2500];
char *bufp;

FILE    *fp;

int main(int argc, char *argv[]);
void RSinit(char const* device, long bps);
void RSclose();
char RSgetch(void);



/*============================================================
    Program entry point
============================================================*/
int main(int argc, char *argv[]) {
    char tosend;
    char toget;
    char buffer[200];

    long x=0;
    long file_len;
    int iPercent=0;

    (void)tosend;
    (void)toget;
    (void)buffer;
    (void)iPercent;

    printf("\nSEND serial, Version 1.3. This is an absolute BETA, dont spread.");
    printf("\nUsage:SEND <FILENAME.EXT> <PORTADRESS>\n");
    printf("Jihad/HITMEN, September 98\n");

    if (argc == 1) {exit(0);}

    //sscanf(argv[2],"%lX",&RSport);

    printf("\nUsing port: %s \n", argv[2]);
    RSinit(argv[2], 115200);
    fp = fopen(argv[1], "rb");
    if (fp==NULL){
        printf("File not found.\n");
        exit(0);
    }

    RSputch(99);
    fseek(fp,0,SEEK_SET);
    for (x=0;x<2048;x++){
        RSputch(fgetc(fp));
    }


    fseek(fp,0,SEEK_END);
    file_len = ftell(fp);

    fseek(fp,16,SEEK_SET);
    union {
        uint32_t d;
        uint8_t b[4];
    } foo;
    foo.b[0] = fgetc(fp);
    foo.b[1] = fgetc(fp);
    foo.b[2] = fgetc(fp);
    foo.b[3] = fgetc(fp);
    printf("x_addr: 0x%x\n", foo.d);
    fseek(fp,24,SEEK_SET);
    foo.b[0] = fgetc(fp);
    foo.b[1] = fgetc(fp);
    foo.b[2] = fgetc(fp);
    foo.b[3] = fgetc(fp);
    printf("write addr: 0x%x\n", foo.d);
    foo.b[0] = fgetc(fp);
    foo.b[1] = fgetc(fp);
    foo.b[2] = fgetc(fp);
    foo.b[3] = fgetc(fp);
    printf("f_len: %x\n", foo.d);

    fseek(fp,2048,SEEK_SET);

    (void)file_len;
    time_t start = time(NULL);

    for (int x=0, n=foo.d; x<n;) {
        if (RSputch(fgetc(fp)) >= 0) {
            iPercent = (100* (x)) / n;
            if (x > 0) printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
            printf("%3u%% (%u/%u)", iPercent, x+1, n);
            ++x;
        }
    }

    printf("\n");

    float diff = (float)difftime(time(NULL), start);
    printf("Uploaded %uKB in %.2f seconds\n", foo.d/1024, diff);

    RSclose();
    return 0;
} /* end of main */
/*============================================================
    Init Port
============================================================*/
void RSinit(char const* device, long bps)
{
    /*
     Open 'device' for read & write, O_NOCTTY means that no terminal will control the process opening the serial port.
     O_NDELAY is ignore the DCD line.
    */
    termios_t port_settings;

    RSport = open(device, O_RDWR | O_NOCTTY /*| O_NDELAY*/);

    if (RSport == 1) 
        printf("Error opening %s\n", device);
    else
        printf("Opened %s\n", device);

    // Get settings to read in the debugger
    tcgetattr(RSport, &port_settings);
    // Set the port read/write speeds (hard coded atm)
    cfsetispeed(&port_settings, B115200);
    cfsetospeed(&port_settings, B115200);

    // Sets the port into 'raw' mode. There is a call to cfmakeraw() but do it this way to be explicit but 
    // the flags differ slightly. These flags are based on siocons but don't exactly match
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
    /*
    port_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    port_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    port_settings.c_oflag &= ~(OPOST);
    port_settings.c_cflag &= ~(CSIZE | PARENB);
    port_settings.c_cflag |= (CS8);
    port_settings.c_cc[VMIN] = 1;
    port_settings.c_cc[VTIME] = 0;
    */
    // Apply the settings to the port. TCSAFLUSH is empty I/O buffer first
    tcsetattr(RSport, TCSAFLUSH, &port_settings);

    // Turn off RTS & DTR, ON: ST
    int set;
    set = TIOCM_RTS | TIOCM_DTR; //clear
    ioctl(RSport, TIOCMBIC, &set);
    set = TIOCM_ST; //set
    ioctl(RSport, TIOCMBIS, &set);
}

void RSclose() {
    printf("Closing serial port.\n");
    close(RSport);
}

/*============================================================
    Send Character
============================================================*/
char RSputch(char c)
{
    int             n, set;
    int             dtr = TIOCM_DTR;
    int             cts = TIOCM_CTS;
    time_t          start = time(NULL);
    timeval_t       t = {0, 100};
    (void)dtr;(void)cts;(void)set;(void)t;

    // busy wait on CTS==true & SR==false
    if (CVAR_WAIT_ON_CTS_BIT) {
        do {
            ioctl(RSport, TIOCMGET, &set);
            time_t cur = time(NULL);
            float diff = (float)difftime(cur, start);
            if (diff > 1/*seconds*/) {
                printf("timeout %#X!\n", set);
                return -1;
            }

            //if ((n = select(0, NULL, NULL, NULL, &t)) < 0) {
            //  printf("select");
            //  return -1;
            //}
        } while ((set & (TIOCM_CTS | TIOCM_SR)) != TIOCM_CTS);
    } else if (CVAR_WAIT_ON_SR_BIT) {
        do {
            ioctl(RSport, TIOCMGET, &set);
            time_t cur = time(NULL);
            float diff = (float)difftime(cur, start);
            if (diff > 1/*seconds*/) {
                printf("timeout!\n");
                return -1;
            }
        } while (!(set & TIOCM_SR));
    }

    /* write */
    if ((n = write(RSport, &c, 1)) < 0)
        printf("write failed\n");
}

```
====
