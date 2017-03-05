/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
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

		hdr->exec.s_addr = /*STACKP*/0x801fff00;
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
	}
}

//AAAAAAAA 00 11 22 33    44 55 66 77    88 99 AA BB    CC DD EE FF        abcd
