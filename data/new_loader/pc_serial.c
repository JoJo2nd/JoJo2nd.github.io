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
	RSinit(argv[2], 115200);
	printf("Going into listen mode...\n");

	for (;;) {
		char c = RSgetch();
		printf("%c", c);
	}


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
char RSgetch() {
	int n;
	char dat;

    /* read */
    if ((n = read(RSport,&dat,1)) < 0) {
        printf("read failed\n");
    }
	return dat;
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
