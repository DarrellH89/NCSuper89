/*  SPD.C -- CPU speed change utility for Rev 3 and up Z80
**	CPU board, as well as NC Super89. Can be compiled for
**  HDOS2, HDOS3, CP/M 2.2.0x or CP/M 3. Not supported on MP/M.
**
**  Usage: SPD { 2 | 4 | 8 | M }
**
**  Arguments correspond to CPU speed in megahertz. If 'M' is
**  specified the board is set to the max speed as determined
**  by the installed crystal oscillator (typically 10Mhz or
**  16Mhz). Arguments are not case sensitive.
**
**  The CPU speed is controlled via port 362 octal (0xF2),
**  however a shadow copy of the port settings is maintained
**  in the OP2.CTL Control Byte in RAM. The program must parse
**  the command line, set the appropriate speed bits, output to
**  the control port and update the shadow copy in RAM. If no
**  arguments are given the current speed setting (in RAM) is
**  simply reported.
**
**  9/17/25: added code to display the current CPU speed on
**  the NCSuper89's OLED display. Code can be built with or
**  without this feature. Code will work fine even if executed
**  on a machine without the OLED.
**
**  Compiled with Software Toolworks C/80 V. 3.1
**
**  NOTE: Add -qHDOS=1 when compiling for HDOS. Also, HDOS files
**  must be saved in UNIX [nl-terminated] form but CP/M
**  in MSDOS [cr-lf terminated) form.
**
**  Build sequence (non OLED version):
**    cc spd
**    m80 = spd
**    l80 spd,pio,printf,clibrary,spd/n/m/e
**
**    OLED version:
**    cc -qOLED=1 spd
**    m80 = spd
**    l80 spd,i2clib,oledlib,pio,printf,stdlib/s,clibrary,spd/n/m/e
**
**	uses outp() from PIO.ASM
**
**	Glenn Roberts		16 September 2021
**	with input from Ken Owen and Douglas Miller.
** 
**  /gfr/ - Updated for NC Super89, CP/M 3 and other fixes
**  September 2025
**
**  Modified to update OLED display with current speed
**  /gfr/ - 15 September 2025
*/
#include "printf.h"

/* control port for speed and other functions */
#define CTLPRT	0xF2

/* mask 11101011to clear old speed bits */
#define	SMASK	0xEB

/* speed masks */
#define	MASK2		0x00
#define MASK4		0x10
#define	MASK8		0x04
#define	MASKMAX	0x14
#define MASKUNK 0x7F

/* control byte location */
char *ctl2fl;

#ifdef OLED
#include "oledlib.h"

#define NROWS 8
#define NCOLS 128
static char buf[NROWS][NCOLS];
static char heading[] = "CPU Speed (Mhz)";

#include "segments.c"
#include "font5x7.c"
#endif

#ifndef HDOS
/* offset from CP/M 3 BIOS warm boot for ctlflg */
#define CTLFLG  97

/* address of BIOS warm boot in low RAM */
char **bptr = 0x0001;

/********************************************************
**
**  *** Valid for use only in CP/M ***
**
** bdoshl - Call CP/M BDOS function with given values
**    for registers C and DE.  Return the value
**    from the HL register.
**
********************************************************/
int bdoshl(c,de)
  int c, de;
{
#asm
        POP     H
        POP     D
        POP     B
        PUSH    B
        PUSH    D
        PUSH    H
        CALL    5
#endasm
}
#endif

#ifdef OLED
/* doseg
**
** display a 7-segment digit defined by the
** array s starting at location c
*/
doseg(s, c)
char s[][32];
char c[][NCOLS];
{
  int i, j;

  for (i=0; i<6; i++)
    for (j=0; j<32; j++)
      c[i][j] = s[i][j];
}


/*
** addtxt - add text at specified location
**
** caution: no error checking
**
*/
int addtxt(dsp, txt)
char *dsp;
char *txt;
{
  char *c, *f;
  int i;

  for (c=txt; *c; c++) {
    /* point to character's font definition */
		f = font5x7 + ((*c - ' ') * 5);
		for (i=0; i<5; i++)
      *dsp++ = *f++;

    /* add space between characters */
    *dsp++ = 0;
	}
}
#endif

main(argc,argv)
int	argc;
char *argv[];
{
	int spd;
	char newspd, spdmask;
#ifdef OLED
	int i, j, offset;
#endif

#ifdef  HDOS
  ctl2fl = 0x2036;
#else
  if (bdoshl(0x0C, 0) < 0x31)
    /* cpm 2.2 */
    ctl2fl = 0x000D;
  else {
    /* assume CP/M 3 */
    ctl2fl = *bptr + CTLFLG;
  }
#endif

  if (argc > 1) {
    /* set mask according to argument. MASKUNK indicates
    ** missing or invalid argument.
    */
    switch (*argv[1]) {
      case '2':
        spdmask = MASK2;
        break;
      case '4':
        spdmask = MASK4;
        break;
      case '8':
        spdmask = MASK8;
        break;
      case 'm':
      case 'M':
        spdmask = MASKMAX;
        break;
      default:
        spdmask = MASKUNK;
        break;
    }
    if (spdmask == MASKUNK)
      printf("Usage: SPD { 2 | 4 | 8 | M )\n");
    else {
      /* Ready to set the speed. First we need to
      ** update the shadow RAM value. Disable interrupts
      ** so that the 2 ms clock update doesn't clobber
      ** the value we're modifying
      */
#asm
  di
#endasm
      /* clear old speed bits and OR in the new ones */
      *ctl2fl = *ctl2fl & SMASK | spdmask;
#asm
  ei
#endasm
      /* Now output the new speed setting to the control port */
      outp(CTLPRT, *ctl2fl);

#ifdef OLED
      /* clear the buffer */
      for (i=0; i<NROWS; i++)
        for (j=0; j<NCOLS; j++)
          buf[i][j] = 0;

      offset = (NCOLS-strlen(heading)*5)/2;
	    addtxt(&buf[0][offset], heading);

      switch (spdmask) {
        case MASK2:
          doseg(seg2, &buf[2][64]);
          break;
        case MASK4:
          doseg(seg4, &buf[2][64]);
          break;
        case MASK8:
          doseg(seg8, &buf[2][64]);
          break;
        case MASKMAX:
          /* note: assumes max=16 */
          doseg(seg1, &buf[2][32]);
          doseg(seg6, &buf[2][64]);
          break;
      }
      
      /* oledinit() and oledbuf() return error codes
      ** but here we ignore them. this lets users who
      ** don't have the OLED interface still use this program
      */
    	oledinit();
    	oledbuf(buf, sizeof(buf), OLED_DAT);
#endif

    }
  }
	
	/* Verify current CPU speed and report it, then exit */
	spd = 2 + ((*ctl2fl & MASK4) / 8) + ((*ctl2fl & MASK8) /2 * 3);
	printf("You are running at ");
  if (spd == 10)
    printf("maximum speed.\n");
  else
    printf("%d MHz.\n", spd);
}
