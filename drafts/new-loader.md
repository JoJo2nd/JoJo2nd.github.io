
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
{code snippt asm}
.global ldr
.global ldr_END

.global memldr
.global memldr_END

#constants
.set SIO_BASE_ADDR, 	0x1F801050
.set SIO_TX_RX_OSET,	0x0
.set SIO_STAT_OSET, 	0x4
.set SIO_CTRL_OSET, 	0xA

.set SIO_TX_RX_ADDR, 	0x1F801050
.set SIO_STAT_ADDR,  	0x1F801054
.set SIO_CTRL_ADDR, 	0x1F80105A
.set SIO_CTRL_CTS,   	0x00000020
.set SIO_STATR_RXRDY, 	0x00000002

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
	lh $t3, ($a3)	# read sio ctrl reg
	ori $t3, $t3, SIO_CTRL_CTS  # CTS: on
	sh $t3, ($a3)	# write CTS: on
ldr_sioWait:
	lw $t3, ($a2)		# read sio status reg
	andi $t3, $t3, SIO_STATR_RXRDY 	# check RX has data in
	beq $t3, $zero, ldr_sioWait 	# wait on RX flag to be on
	nop

	lb $t3, ($a1)		# read the sio data
	sb $t3, 0($t0)				# write to write address

	lh $t3, ($a3)	# read sio ctrl reg
	and $t3, $t5, $t3 			# CTS: off
	sh $t3, ($a3)	# write CTS: off

	addiu $t0, 1			# pc0 + 1
	addiu $t1, -1			# t_size - 1
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

memldr:
	lw $t0, 0x0($a0) # Load the write addres (pc0)
	lw $t1, 0xc($a0) # Load text section size (t_size)
	lw $t2, ($a1) # source address

memldr_copyloop:
	lb $t3, ($t2)
	sb $t3, ($t0)

	addiu $t0, 1
	addiu $t2, 1
	addiu $t1, -1
	bne $t1, $zero, memldr_copyloop
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
memldr_END:
======
psxldr.c

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

#if !CVAR_USE_SIO
# include "data.inl"
#else
uint8_t data_array[16] = {0};
#endif

/* status bits */
#define SR_IRQ		0x200
#define SR_CTS		0x100
#define SR_DSR		0x80
#define SR_FE		0x20
#define SR_OE		0x10
#define SR_PERROR	0x8
#define SR_TXU		0x4
#define SR_RXRDY	0x2
#define SR_TXRDY	0x1

#define SIO_CTS		0x100
#define SIO_DSR		0x80
#define SIO_FE		0x20
#define SIO_OE		0x10
#define SIO_PERROR	0x8
#define SIO_TXU		0x4
#define SIO_RXRDY	0x2
#define SIO_TXRDY	0x1

/* control bits */
#define CR_DSRIEN	0x1000
#define CR_RXIEN	0x800
#define CR_TXIEN	0x400
#define CR_BUFSZ_1	0x0
#define CR_BUFSZ_2	0x100
#define CR_BUFSZ_4	0x200
#define CR_BUFSZ_8	0x300
#define CR_INTRST	0x40
#define CR_RTS		0x20
#define CR_ERRRST	0x10
#define CR_BRK		0x8
#define CR_RXEN		0x4
#define CR_DTR		0x2
#define CR_TXEN		0x1

#define SIO_BIT_DTR	CR_DTR
#define SIO_BIT_RTS	CR_RTS

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
	unsigned long s_addr;	//0x20
	unsigned long s_size;	//0x24
	unsigned long sp,fp,gp,ret,base; //0x28
} exec_t;


typedef struct XF_HDR {
	char key[8]; //0x0
	unsigned long text; //0x08
	unsigned long data; //0x0C
	exec_t exec;
	char title[60];		/* "PlayStation(tm) Executable A1" */
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

extern void memldr(void* exec, void* src);
extern void memldr_END();

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
	if (CVAR_USE_SIO) {
		SIO_CTRL |= CR_RTS; // RTS on
		while (!SIOCheckInBuffer());
		r = SIOReadByte();
		SIO_CTRL &= ~CR_RTS; // RTS off
	} else {
		static uint32_t data_i = sizeof(data_array);
		if (data_i == sizeof(data_array)) {
			r = 99;
			data_i = 0;
		} else {
			r = data_array[data_i++];
		}
	}
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
 This currently seems to work BUT requires two syscalls (e.g. EnterCriticalSection() & Exec())
 and currently stamps over itself.
 TODO: 
 	* create a .asm file with position independent code for copying exe
 	* get location of code, copy to the top(?) of the stack (I think we can run code from there)
*/
typedef void (*ldr_func_t)(void*);
typedef void (*memldr_func_t)(void*, void*);

int main() {
	uint8_t hdr_data[PSXEXE_HEADER_SIZE];
	uint8_t ldr_data[PSXEXE_LDR_SIZE+4];
	psxexeheader_t* hdr = (psxexeheader_t*)hdr_data;

	PSX_InitEx(0);

	memset(ldr_data, 0, PSXEXE_LDR_SIZE+4);
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

		render_state(RecivedSync, hdr, NULL, NULL);		

		render_state(ReadingHeader, hdr, NULL, NULL);	
		for (uint32_t i = 0; i < PSXEXE_HEADER_SIZE; ++i) {
			hdr_data[i] = sio_read_uint8();
		}
		printf("Loaded header\n");
		if (strcmp("PS-X EXE", hdr->key))
			continue;

		printf("Header OK!\n"
		"pc0 = %x\n gp0 = %x\n t_addr = %x\n t_size = %x\n d_addr = %x\n d_size = %x\n b_addr = %x\n b_size = %x\n s_addr = %x\n s_size = %x\n",
		hdr->exec.pc0,hdr->exec.gp0,hdr->exec.t_addr,hdr->exec.t_size,hdr->exec.d_addr,hdr->exec.d_size,hdr->exec.b_addr,hdr->exec.b_size,hdr->exec.s_addr,hdr->exec.s_size);
		render_state(ReadingEXE, hdr, &hdr->exec, ldr_data);
#if 1
		hdr->exec.s_addr = /*STACKP*/0x801ffff0;
		hdr->exec.s_size = 0;
		//EnterCriticalSection(); 

		if (CVAR_USE_SIO) {
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
		} else {
			if (CVAR_USE_CVERSION) {
				uint8_t* exe = (uint8_t*)hdr->exec.pc0;
				for (uint32_t i = 0, n = hdr->exec.t_size; i < n; ++i) {
					*exe = sio_read_uint8(); 
					exe++;
				}

				EnterCriticalSection();
				Exec(&hdr->exec, 1, 0);
			} else {
				printf("Copying Loader\n");
				// use space on the stack to move our loader to
				uintptr_t pic_size = (uintptr_t)memldr_END - (uintptr_t)memldr;
				uint8_t* pic_loc = (uint8_t*)(((uintptr_t)ldr_data + 3) & ~3);
				// Copy to the stack (which is high mem)
				memcpy(pic_loc, memldr, pic_size);
				printf("Jumping to Loader @ %p\n", pic_loc);
				((memldr_func_t)pic_loc)(&hdr->exec, data_array+2048);
			}
		}
#endif		
#if 0
		uint8_t cb;
		for (uint32_t i = 0, n = hdr->exec.t_size; i < n; ++i) {
			cb = sio_read_uint8(); 
		}
		(void)cb;
#endif
#if 0
		uint8_t* exe = (uint8_t*)write_address;
		//memcpy(exe, header.headerData, 2048);

		render_state(ReadingEXE, 0);
		for (uint32_t i = 0, n = f_len; i < n; ++i) {
			*exe = sio_read_uint8(); 
			exe++;
			//render_state(ReadingEXE, i);
		}

		// ensure we do pull ldr.s in to exe
		ldr();

		struct XF_HDR* head = (struct XF_HDR*)&header;
		head->exec.s_addr = /*STACKP*/0x801ffff0;
		head->exec.s_size = 0;
		EnterCriticalSection();
		Exec(&head->exec, 1, 0);
#endif
	}
}

//AAAAAAAA 00 11 22 33    44 55 66 77    88 99 AA BB    CC DD EE FF        abcd

======
hitserial.c


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

FILE	*fp;

int RSport = 0;
int    	RDB	;/* Read Register */
int     TDB	;/* Write Register */
int    	IER	;    /* Interrupt Enable Register */
int    	LCR	;	/* Line Control Register */
int    	MCR	;	/* Modem Control Register */
int    	LSR ;	/* Line Status Register */
int     MSR	;	/* Modem Status Register */
int     DLL	;	/* */
int    	DLM	;	/* */

int main(int argc, char *argv[]);
void RSinit(char const* device, long bps);
void RSclose();
char RSputch(char c);
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

RDB	 =  RSport+0;/* Read Register */
TDB	 =	RSport+0;/* Write Register */
IER	 =	RSport+1;/* Interrupt Enable Register */
LCR	 =	RSport+3;/* Line Control Register */
MCR	 =	RSport+4;/* Modem Control Register */
LSR  =  RSport+5;/* Line Status Register */
MSR	 =	RSport+6;/* Modem Status Register */
DLL	 =	RSport+0;/* */
DLM	 =	RSport+1;/* */




	
	RSinit(argv[2], 115200);



	fp = fopen(argv[1], "rb");

	if (fp==NULL){printf("File not found.\n");




						   exit(0);}


	RSputch(99);



	fseek(fp,0,SEEK_SET);

	for (x=0;x<2048;x++){
		RSputch(fgetc(fp));
	}                  /* header schicen */


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
	if (CVAR_SEND_INFO) {
		RSputch(foo.b[0]/*fgetc(fp)*/);
		RSputch(foo.b[1]/*fgetc(fp)*/);
		RSputch(foo.b[2]/*fgetc(fp)*/);
		RSputch(foo.b[3]/*fgetc(fp)*/);
	}
	fseek(fp,24,SEEK_SET);

	foo.b[0] = fgetc(fp);
	foo.b[1] = fgetc(fp);
	foo.b[2] = fgetc(fp);
	foo.b[3] = fgetc(fp);
	printf("write addr: 0x%x\n", foo.d);

	if (CVAR_SEND_INFO) {
		RSputch(foo.b[0]/*fgetc(fp)*/);
		RSputch(foo.b[1]/*fgetc(fp)*/);
		RSputch(foo.b[2]/*fgetc(fp)*/);
		RSputch(foo.b[3]/*fgetc(fp)*/);
	}

	foo.b[0] = fgetc(fp);
	foo.b[1] = fgetc(fp);
	foo.b[2] = fgetc(fp);
	foo.b[3] = fgetc(fp);
	printf("f_len: %x\n", foo.d);

	if (CVAR_SEND_INFO) {
		RSputch(foo.b[0]/*fgetc(fp)*/);
		RSputch(foo.b[1]/*fgetc(fp)*/);
		RSputch(foo.b[2]/*fgetc(fp)*/);
		RSputch(foo.b[3]/*fgetc(fp)*/);
	}

	fseek(fp,2048,SEEK_SET);

	//sleep(1);

	(void)file_len;
	time_t start = time(NULL);

	for (int x=0, n=foo.d; x<n;) {
		if (RSputch(fgetc(fp)) >= 0) {
			iPercent = (100* (x)) / n;
			if (x > 0) printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
			printf("%3u%% (%u/%u)", iPercent, x+1, n);
			//if (!x % 512) usleep(1000*16);
			++x;
		}
	}

	printf("\n");

	float diff = (float)difftime(time(NULL), start);
	printf("Uploaded %uKB in %.2f seconds\n", foo.d/1024, diff);

	//sleep(3);

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

//	int	r;
//
//	/* set RS232C mode */
//	r = 384 / (bps / 300);		/* calculate baud rate */
//
//	out_port(LCR, 0x80);		/* board rate setting mode */
//	out_port(DLL, r&0x00ff);	/* set lower byte */
//	out_port(DLM, r>>8);		/* set higher byte */
//
//	out_port(LCR, N81MODE);		/* N81, 1/16 mode */
//
///*	out_port(IER, 0x01);		/* RxRDY interrupt enable */
//
//	out_port(MCR, 0x08);		/* OUT2:on RTS:off DTR:off */

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
	int 			dtr = TIOCM_DTR;
	int 			cts = TIOCM_CTS;
	time_t			start = time(NULL);
	timeval_t  		t = {0, 100};
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
			//	printf("select");
			//	return -1;
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
	// flush output
	//ioctl(RSport, TCFLSH, TCOFLUSH);
	//do {
	//	n = write(RSport, &c, 1);
	//} while (n < 0);
	return n;
//	while(!((in_port(MSR) & 0x10)&&(in_port(LSR) & 0x20)))
//	{
//		/* CTS:on & Transferd Data Register is empty? */
//		if(kbhit())
//		{
//
//			fclose(fp);
//			exit(0);
//		}
//	}
//	out_port(TDB, c);		/* output data */
//	return c;
}

/*============================================================
	Receive character
============================================================*/
char RSgetch(void)
{
#if 0	
	int n, set;
	char dat;

    /* handshake */
    set = TIOCM_RTS;
    ioctl(RSport,TIOCMBIS,&set);

    /* read */
    if ((n = read(RSport,dat,1)) < 0)
        printf("read failed\n");
	return dat;
#endif
	return 0;
//	char	c;
//
//	out_port(MCR, 0x0a);		/* OUT2:on RTS:on DTR:off */
//	while (!(in_port(LSR) & 0x01))
//	{	/* Received Data? */
//		if(kbhit())
//		{
//		
//			fclose(fp);
//			exit(0);
//		}
//	}
//	c = in_port(RDB);		/* input data */
//	out_port(MCR, 0x08);		/* OUT2:on RTS:off DTR:off */
//	return c;
}

/*============================================================
	String to Hex function (fixed)
============================================================*/
unsigned long atoh(char *s)
{
	int	i;
	char	*c;
	int	clen = 0;
	unsigned long	hex = 0;

	c = s;
	while(*c++)
		clen++;

	if(clen>=8) clen = 8;
	for(i=0; i<clen; i++)
	{
		if (s[i] < 58)
			hex |= (unsigned long)(s[i]&0x0f)<<(4*(clen-1-i));
		else
			hex |= (unsigned long)((s[i]+9)&0x0f)<<(4*(clen-1-i));
	}
	return hex;
}
====