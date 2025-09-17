/*
 * Low level I2C/OLED bit-banging
**
**  Originally by Douglas Miller
**
**  Modifications to support C/80 assembler
**  by Glenn Roberts, Sept. 2025
**
 */
#define PORT       81H

#define I2C_CLK    04H

#define DS_CE      10H

#define SS_WEN     20H

#define SS_WD      80H
#define SS_WDN     7FH

#asm
@CTL:	  DB	0
@DLY2:	CALL	@DLY1
@DLY1:	RET
#endasm

/* state: 0= not initialized; 0xFF = running */
static char std = 0;

int i2cinit()
{
#asm
    MVI    A,SS_WEN+I2C_CLK
    STA    @CTL
    OUT    PORT
#endasm
}

int i2cstart()
{
  /* if already running just return */
	if (std)
    return 0;
#asm
    LDA    @CTL
    ANI    NOT SS_WEN
    ORI    SS_WD
    OUT    PORT
    CALL   @DLY2
    ANI    SS_WDN
    OUT    PORT
    ANI    NOT I2C_CLK
    OUT    PORT
    STA    @CTL
#endasm

  /* flag as initialized */
	std = 0xFF;
}

int i2cstop()
{
  /* If not running just return */
	if (!std)
    return;
#asm
    LDA    @CTL
    ANI    NOT SS_WEN
    ANI    SS_WDN
    ORI    I2C_CLK
    OUT    PORT
    ORI    SS_WD
    OUT    PORT
    ORI    SS_WEN
    OUT    PORT
    STA    @CTL
#endasm

  /* flag as shut down */
	std = 0;
}

/*
 * caller has already done START and issued I2C adr and ctl.
 * caller is responsible for STOP.
 */
int i2cput(dat)
char dat;
{
#asm
    LXI    H,2                ; point past return address
    DAD    SP
    MOV    D,M                ; fetch data
    
    LDA    @CTL
    ANI    NOT SS_WEN
    OUT    PORT
    
    MVI    B,8                ; bit-bang 8 bits

I2CP1:
    RAL
    MOV    C,A
    
    MOV    A,D                ; fetch data byte
    RAL                       ; rotate left
    MOV    D,A                ; save it
    
    MOV    A,C
    RAR
    OUT    PORT

    ORI    I2C_CLK            ; clock on
    OUT    PORT               ; send it...
    ANI    NOT I2C_CLK          ; clock off
    OUT    PORT               ; send it
    
    DCR    B                  ; count down
    JNZ    I2CP1              ; loop over bits

    ORI    SS_WEN             ; write enable
    OUT    PORT
    ORI    I2C_CLK            ; clock on
    OUT    PORT
    
    PUSH   PSW
    IN     PORT
    ANI    1
    MOV    L,A
    POP    PSW
    
    ANI    NOT I2C_CLK            ; clock off
    OUT    PORT
    
    STA    @CTL
    MVI    H,0
#endasm
}
