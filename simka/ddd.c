// simka/ddd.c
/* nee simka/datadisc.c
// --------------------------------------------------------------------------------------------- 100
        Stanford Data Disc Display terminals
        for the SIMH / SIMS code package

Ignoring the planet wide plague of this year,
this device simulation was coded by BgBaumgart
in June and July 2020 based on 'C' code from SIMH and SIMS
by R.M.Supnick, R.Cornwell, Lars Brinkhof et al.

Further based on
a close reading of source and binary from the SAILDART [J17,SYS] directory
as well as the Ted Panofsky hardware manual SAILDART [H,DOC] directory.
Several pages of Panofsky documentation is pasted in at the end of this file.

For SYSTEM.DMP [ J17, SYS ] the Available Screen message was maintained on DD channel #9 octal 011.
after system initialization,
DD channel 011 has the "Take Me I'm Yours!" greeting, System load status, and Date/Time.
The y position is randomized to prevent screen-burn-in here move to column x=2 line y=348 from top.
The screen raster origin (0,0) is upper left corner ( North West Corner ) of the raster.
moveto(2,348)
dpystr("""
                ␣␣␣␣␣␣␣␣␣␣␣␣␣DD␣JBS,TCOR␣␣R,RCOR␣UCOR␣␣NL␣DSKQ\r\n
                ␣␣␣␣␣␣␣␣␣␣␣␣␣␣0␣␣␣0,0␣␣␣␣␣0,0␣␣␣178K␣␣␣␣%␣␣0D␣␣JUL␣26␣␣FRIDAY␣␣15:07\r\n
                ␣\r\n
                ␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣Take␣Me␣I'm␣Yours!\r\n
""")

                via the VDS video crossbar display switch goes out to 58. TV lines #6 to #63.

DATA DISC raster display

        Stanford Data Disc 480x512 bit raster display with 32 video channels

        The Data Disc was simply a 32x480x512 bit-frame-buffer, which is 960. Kilo bytes.
        The Disc was replaced with RAM in July 1982, by the "NewDD" built by EJS, Ted Selker.
        "NewDD" is pronounced "nudity".

The RASTER size was 480 x 512 composed of little,
wintergreen flavored, slow phosphor P31 glowing dots.
These pixels were close enough to being square.

The standard character glyphs fit within a grid of 6 pixels wide by 12. pixels high;
The standard text window is 40. rows of 85. characters per row.

The double-size font was 12. pixels wide and 24. pixels high.
The double-size text window is 20. rows with 42. characters per row.

In 1974, the PDP10 device named VDS, for Video (crossbar) Display Switch,
distributed the 32 digital Data Discs channels + 8 analog video channels.
So 40. inputs are crossbar switched into 58 output video TV lines, most of the lines
go to P31 green phosphor TV monitors located in almost every room comprising the D.C.Power Lab building.

At each monitor station there was a special DKB keyboard.
Six TV output lines were left unused because the Triple-III vector display "owned" keyboards #0 to #5,
which were numbered as TTY#20 to #TTY25 in the 1974 SAIL-WAITS system.

 */
#include "ka10_defs.h"

#ifndef NUM_DEVS_DDD
#define NUM_DEVS_DDD 0
#endif
#if NUM_DEVS_DDD > 0

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <fcntl.h>

#define bamboo // Markers for scratch-work de jour
#define bambon  1&& // switch on
#define bamboff 0&& // switch off

// See sim_defs.h struct UNIT
#define DDD_DEVNUM 0510
#define DDD_OFF (1 << DEV_V_UF)
#define STATUS  u3
#define MAR     u4
#define PIA     u5
#define DDchan  u6
#define TVline  us9

// CONO bits
#define SET_HALT_IE  00040
#define SET_DDGO     00100
#define SET_USERMODE 01000

// CONI bits
#define PIA_MASK  0007
#define HALT_FLAG 0010
#define INTERRUPT 0020
#define FIELD     0040
#define HALT_IE   0100

t_stat ddd_devio(uint32 dev, uint64 *data);
t_stat ddd_srv(UNIT *uptr);
t_stat ddd_reset(DEVICE *dptr);
t_stat ddd_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ddd_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat ddd_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat ddd_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);

const char *ddd_description (DEVICE *dptr);

DIB  ddd_dib    = {DDD_DEVNUM, 1, &ddd_devio, NULL};
UNIT ddd_unit[] = {
  {UDATA(&ddd_srv,UNIT_IDLE,0)},
};
MTAB ddd_mod[]  = {
  { MTAB_VDV, 0, "ON", "ON", ddd_set_on, ddd_show_on },
  { MTAB_VDV, DDD_OFF, NULL, "OFF", ddd_set_off },
  { 0 }
};
DEVICE ddd_dev = {
  "DDD", ddd_unit, NULL, ddd_mod, 1, 8, 0, 1, 8, 36,
  NULL, NULL, ddd_reset,
  NULL, NULL, NULL,
  &ddd_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG, 0, dev_debug,
  NULL, NULL, &ddd_help, NULL, NULL, &ddd_description
};

// forward
void error(const char *msg);
void hark();

// global
int ddd_maddr; // DDD memory address of instruction
int ddd_hwdg;  // Data Disc function code register : 040 ¬high, 010 wide, 4 dark, 1 gmode.
int sockfd_ddd_terminal;
// max buffer size for octal dump of one screen pure graphics with 4 extra words per line.
#define DDD_BUFSIZ 480*20*13
char dddold[DDD_BUFSIZ];
char dddnew[DDD_BUFSIZ];
char *dddbuf=dddnew;

void send_to_ddd_terminal(){
  int n;
  //  hark();
  if(!sockfd_ddd_terminal)return; // No DD server.
  if(!strcmp(dddold,dddnew)){
    bamboff fprintf(stderr,"Nothing new for DD ... send nothing to DD\r\n");
    dddbuf = dddnew; // next refill
    return;
  }
  if(!sockfd_ddd_terminal){
    printf("Content of the DDD buffer. NOT sent to display server.");
    printf("\r\n============== DDD ==================\r\n");
    n = write( 0, dddnew, strlen(dddnew));    
    printf("\r\n============== EOM ==================\r\n");
  }else{
    n = write(sockfd_ddd_terminal,dddnew,strlen(dddnew));
    bamboff fprintf(stderr,"ddd msg sent n=%d\r\n",n);
    if (n < 0) error("ERROR writing to socket");
  }
  strcpy(dddold,dddnew);
  dddbuf = dddnew; // next refill
}
extern uint64 vds_crossfoo[64]; // sent
extern uint64 vds_crossbar[64]; // current
void init_ddd_terminal()
{
  int portno=2026;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  static int once=0;
  //
  if(once++)return; // pass once only
  bzero(vds_crossfoo,sizeof(vds_crossfoo));
  bzero(vds_crossbar,sizeof(vds_crossbar));
  //
  sockfd_ddd_terminal = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ddd_terminal < 0) 
    error("ERROR opening socket");
  server = gethostbyname("localhost");
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd_ddd_terminal,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
    fprintf(stderr,"No DD terminal server\r\n");
    close(sockfd_ddd_terminal);
    sockfd_ddd_terminal=0;
    // error("ERROR connecting");
  }else{
    fprintf(stderr,"OK ! We are connected to a Data Disc terminal server\r\n");
  }
  //  if(fcntl(sockfd_ddd_terminal,F_SETFL,O_NONBLOCK)) error("fcntl SET sockfd NONBLOCK");
}
/*
        KA10 iots for the DDD central Data Disc raster display unit,
        supporting 32 of a potential 64 video terminal locations
        via the VDS, Video crossbar Display Switch
*/
t_stat ddd_devio(uint32 dev, uint64 *data)
{
  UNIT *uptr = &ddd_unit[0];
  DEVICE *dptr = &ddd_dev;
  static int field=0;
  switch(dev & 07) {
    /* CONI 510,
       _____________________________________________________________________
       |18	       24| 25 | 26 | 27 | 28 | 29 | 30  |31 | 32 |33	  35|
       |		 | non|user|late|    |halt|     |   |    |	    |
       |    unused	 | ex |    |int |late|int |field|int|halt|    PI    |
       |		 | mem|mode|enb |    |enb |     |   |    |	    |
       |_________________|____|____|____|____|____|_____|___|____|__________|
       |      |        |           |	          |              |	    |            

       PI field is the PI channel currently in the PI register.

       Halt bit means the interface has finished its current buffer and  has not been re-initialized.

       Int bit means the interface is now interrupting on the channel loaded into it's PI register.

       Field  bit  is  0 during the even field rotation of the disc, and a 1 during the odd field.

       Halt  int enb bit means the interface is enabled to interrupt when it halts.
    */
  case CONI:
    *data = (uint64)((uptr->STATUS & ( HALT_FLAG | INTERRUPT | HALT_IE )) | ddd_unit[0].PIA);
    //if (1&field++) *data |= (uint64)FIELD; // alternated the EVEN and ODD video refresh fields
    sim_debug(DEBUG_CONI, &ddd_dev, "%06o / CONI DDD,%12llo\n", PC, *data);
    //if(*data!=0110 && *data!=0113) bamboo fprintf(stderr, "%06o / CONI DDD, %6llo\r\n", PC, *data & RMASK);
    break;

    /* CONO 510,
       The USUAL is defined in DPYSER as 0563
       ___________________________________________________________________________________________
       |18 . 19 . 20 . 21 . 22 . 23 . 24 . 25 | 26 | 27 | 28 | 29 | 30 | 31  | 32 | 33 . 34 . 35 |
       |                                      |user|late|    |    |halt|force| re-|              |
       |     unused                           |    |int |    |DDGO|int |     | set|      PI      |
       |                                      |mode|enb |    |    |enb |field|    |              |
       |___.____.____.____.____.____.____.____|____|____|____|____|____|_____|____|____.____.____|
       |             |              |              |              |               |              |
    */
  case CONO:
    clr_interrupt(DDD_DEVNUM);
    ddd_unit[0].PIA &= ~(PIA_MASK);
    ddd_unit[0].PIA |= (int32)(*data & PIA_MASK);
    if(*data & SET_HALT_IE){
      uptr->STATUS |= HALT_IE;
    }
    if( ddd_unit[0].PIA )
      uptr->STATUS |= INTERRUPT; // set INTERRUPT bit when PIA non-zero
    else
      uptr->STATUS &= ~INTERRUPT; // clear INTERRUPT bit
    sim_debug(DEBUG_CONO, &ddd_dev, "%06o / CONO DDD,%12llo\n", PC, *data);
    bamboff fprintf(stderr,"%06o / CONO DDD,%6llo\r\n", PC, *data);
    break;
    
  case DATAI:
    *data = 0;
    sim_debug(DEBUG_DATAIO, &ddd_dev, "%06o / DATAI DDD,%12llo\n", PC, *data);
    bamboo fprintf(stderr,"%06o / DATAI DDD,%12llo\r\n", PC, *data);
    break;
    
  case DATAO: // start address
    {
      uint64 ddword = M[*data & RMASK];
      uint32 ddcode = ddword & 07777;
      if(!sockfd_ddd_terminal)break; // No DDD terminal server - so quit now.
      uptr->DDchan = (ddcode==01214||ddcode==01234) ? ((ddword>>20)&0377LL) : 040;
      sim_debug(DEBUG_DATAIO, &ddd_dev, "%06o / DATAO DDD,%12llo\n", PC, *data);
      bamboff fprintf(stderr,"%06o / DATAO DDD, start address %6llo select DD#%02o ddword=%12llo\r\n", PC, *data, uptr->DDchan, ddword );
      // start YAML message for DD_code: value
      dddbuf += sprintf(dddbuf,"---\nDD_code: %012o", 0); // pack ZERO word into message buffer
      ddd_maddr = *data & RMASK;
      uptr->MAR = *data & RMASK;
      //uptr->STATUS &= ~INTERRUPT; // dismiss
      sim_activate(uptr, 10);
    }
    break;
  default:
    break;
  }
  return SCPE_OK;
}
extern char *SAIL7CODE_lpt[];
t_stat
ddd_srv(UNIT * uptr)
{
  uint64 instruction; // DD instruction
  int ddd_select=0xFFFFffff; // 32 channels into VDS
  int ddd_mask  =04000;
  int dop;
  char remark[256]; // bamboo
  // fetch instruction
  ddd_maddr = uptr->MAR;
  instruction = M[uptr->MAR];
  dop =
    (instruction & 001)     ? 1 : // text word
    (instruction & 017)== 2 ? 2 : // graphics word
    (instruction & 007)== 4 ? 4 : // command word
    instruction & 076 ;          // codes for jump, halt and nop
  uptr->MAR++;
  uptr->MAR &= RMASK;
  *remark=0; // bamboo
  switch(dop){
  case 1: // text word
    {
      char c[6];
      c[1]=(instruction>>29) & 0177;
      c[2]=(instruction>>22) & 0177;
      c[3]=(instruction>>15) & 0177;
      c[4]=(instruction>> 8) & 0177;
      c[5]=(instruction>> 1) & 0177;
      bamboff sprintf(remark,"text/%s%s%s%s%s/",SAIL7CODE_lpt[c[1]],SAIL7CODE_lpt[c[2]],SAIL7CODE_lpt[c[3]],SAIL7CODE_lpt[c[4]],SAIL7CODE_lpt[c[5]]);
    }
    dddbuf += sprintf(dddbuf," %012llo",instruction); // pack word into message buffer
    break;
  case 2: // graphics word
    bamboff strcpy(remark,"graphics");
    dddbuf += sprintf(dddbuf," %012llo",instruction); // pack word into message buffer
    break;
  case 4: // command word
    {
      int op1,op2,op3,ops;
      int d1,d2,d3;
      op1= (instruction>>9)&7; d1=(instruction>>28)&0377;
      op2= (instruction>>6)&7; d2=(instruction>>20)&0377;
      op3= (instruction>>3)&7; d3=(instruction>>12)&0377;
      ops= (instruction>>3)&0777;
      // set data disc function code register : ¬high, wide, dark, gmode
      if(op1==1) ddd_hwdg=d1;
      if(op2==1) ddd_hwdg=d2;
      if(op3==1) ddd_hwdg=d3;
      bamboff sprintf(remark,"commands: %o/%03o   %o/%03o   %o/%03o  h_wd_g=%02o", op1,d1, op2,d2, op3,d3, ddd_hwdg );
      
      // consider the three ops as one-command
      switch(ops){
      case 0123: // select DDchan set-x set-graphics-mode
      case 0121: // select DDchan       set-text-mode
        uptr->DDchan = d2;
        bamboff sprintf(remark+strlen(remark)," %s DD#%o ", d1==017 ? "Erase" : d1==7 ? "continue" : "select", d2);
        break;
      case 0333: // set x
        bamboff sprintf(remark+strlen(remark)," set x=%d ",d1);
        break;
      case 0345: // move beam to x,y
        {
          int x=d1, y=((d2&037)<<4)|(d3&017);
          bamboff sprintf(remark+strlen(remark)," Move x=%d y=%d", x, y );
          // When TEXT mode, force y even to avoid the two-phase NTSC frame refresh.
          // See wikipedia, for the NTSC 20th century analog TV signal details.
          if(y&1 && !(ddd_hwdg&1)) instruction &= ~010000; // clear the bit sent to display server.
        }
        break;
      }
    }
    dddbuf += sprintf(dddbuf," %012llo",instruction); // pack word into message buffer
    break;    
  case 000: // halt
  case 040:
  case 060:
    dop=0;
    bamboff strcpy(remark,"HALT\n");
    // YAML end-of-message for the DD_code: buffer value
    dddbuf += sprintf(dddbuf," %012o",0); // zero word
    send_to_ddd_terminal();
    uptr->STATUS |= HALT_FLAG;       // Data Disc processor has just halted. bit#32 0010
    if ( uptr->STATUS & HALT_IE &&   // Halt Interrupt Enabled.        bit#29 0100
         uptr->STATUS & INTERRUPT ){ // PIA non-zero
         set_interrupt(DDD_DEVNUM, uptr->PIA);
    }
    break;
  case 020: // jump
    uptr->MAR = (instruction>>18);
    bamboff sprintf(remark,"jump %06o",uptr->MAR);
    break;
  case 006: // nop
  case 010:
  case 012:
  case 016:
  case 032: // Data Disc NOP, however 032 is SKIPA for the Triple-III. J17SYS exploits this fact.
    bamboff strcpy(remark,"nop");
    break;
  default:
    bamboff strcpy(remark,"WTF");
    break;
  }
  bamboff fprintf(stderr,"DD#%02o  %06o / %012llo dop=%02o %s\r\n", uptr->DDchan, ddd_maddr, instruction, dop, remark);
  if(dop)sim_activate(uptr,50);
  return SCPE_OK;
}
t_stat ddd_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &ddd_dev;
  dptr->flags &= ~DDD_OFF;
  return SCPE_OK;
}
t_stat ddd_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &ddd_dev;
  dptr->flags |= DDD_OFF;
  return SCPE_OK;
}
t_stat ddd_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
  DEVICE *dptr = &ddd_dev;
  bamboo fprintf (st, "%s", (dptr->flags & DDD_OFF) ? "off" : "on");
  return SCPE_OK;
}
t_stat ddd_reset (DEVICE *dptr)
{
  if (dptr->flags & DEV_DIS) {
    // display_close(dptr);
  } else {
    // display_reset();
    // dptr->units[0].POS = 0;
    init_ddd_terminal();
  }
  return SCPE_OK;
}
t_stat ddd_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
  bamboo fprintf(st,"Data Disc DDD help message\n\n");
  fprint_set_help (st, dptr);
  fprint_show_help (st, dptr);
  fprint_reg_help (st, dptr);
  return SCPE_OK;
}
const char *ddd_description (DEVICE *dptr)
{
  return "Stanford Data Disc 480x512 bit raster display with 32 video channels";
}

/* Data Disc section from FACIL.TED[H,DOC]7 which is the Panofsky hardware manual 1974-11-15
 * ------------------------------------------------------------------------------------------
 see documentation at
 https://www.saildart.org/[H,DOC]
 https://www.saildart.org/FACIL.TED[H,DOC]7
 * ------------------------------------------------------------------------------------------
simka scratch notes

; examine first command word sent from PDP10 to DD
sim> e -2 14320
14320:	000011110000000000100110001010001100
;so
;echo 00001111 00000000 00100110 001 010 001  100
;echo databyte databyte databyte  1   2   1    4
 * ------------------------------------------------------------------------------------------

 DATA DISC OPERATION MANUAL				    SECTION 6

 DEVICE SELECT NUMBER:	510

 This manual describes the Data Disc Television Display System
 and interface (DDTDS&I) at the Stanford A.I. project.  The DDTDS&I is
 a  display  system,  using  a  disc with 64 tracks and rotating at 60
 r.p.s  for  data  storage.   The  consoles  are  standard  television
 monitors  with P31 phosphor to reduce flicker.  Each monitor uses one
 Channel which is two tracks on the  storage  disc.   The  tracks  are
 switched alternately every 1/60th second--one track for each field of
 the standard television raster. Characters are  sent  to  a  one-line
 buffer and then commanded to be written.  They must be written twice;
 once for each field.

 The DDTDS&I interface is a data  channel using the memory-bus
 multiplexor  in  the 2314  interface.    To operate  it,    first the
 interface must  be  CONOed  some  set  up bits,    then  DATAOed  the
 (absolute)  address   of  a  display   program  in  core.     Display
 instructions look like this:

 Text word.

 ___________________________________________________________
 |0	  6|7	      13|14	 20|21	    27|28     35| 35|
 |   chr1   |    chr2    |   chr3   |   chr4   |   chr5	| 1 |
 |__________|____________|__________|__________|_________|___|
 |    |    |    |    |    |    |    |    |    |    |    |    |            

 Upon  receiving  a   text  word,  the  interface   sends  the
 characters  to  the disc's  line  buffer.   Tabs  and  backspaces are
 ignored unless  preceded  by  a  backspace (177)  in  which  case,  a
 special character is  printed (i.e.  a small tb  is printed for tab).
 Nulls  are  always  ignored.    Carriage  return  and  line feed  are
 specially processed to  do the right  thing: If characters have  been
 transmitted since the last  execute command (see command word below),
 an execute is  generated. Then carriage  return causes the  interface
 to send  column select  of 2; line  feed sends  a line address  (both
 parts)  14  greater  than  the  previous  line  address sent.    Both
 carriage return and line  feed, if preceded  by a 177, print  special
 characters instead of the above functions.

 DATA DISC OPERATION MANUAL				    SECTION 6

 Graphics word
  ___________________________________________________________
 |0 	     7|8          15|16        23|24        31|32  35|
 |   byte 1   |   byte 2    |   byte 3   |   byte 4   |  02  |
 |____________|_____________|____________|____________|______|
 |    |    |    |    |    |    |    |    |    |    |    |    |            

 The interface sends four 8-bit bytes directly to the disc's line buffer with no modification.

 Halt
  ___________________________________________________________
 |0						29|30	  35|
 |			unused			  |    X0   |
 |________________________________________________|_________|
 |   |    |    |    |    |    |    |    |    |    |    |    |            

 A halt is executed on op-codes 0,40, and 60. A halt  word  does  just
 that,  and sets the halt bit which can be read by a CONI.  The bit is
 reset by a CONO. If the halt interrupt enable bit is on,  the  device
 interrupts.

 Jump
  ___________________________________________________________
 |0			    17|18		29|30	  35|
 |	 jump address	      |			  |   20    |
 |____________________________|___________________|_________|
 |   |    |    |    |    |    |    |    |    |    |    |    |            

 A jump word causes the MA to be loaded with the Contents of the  left
 half of the word and execution continues from there.

 NO OPERATION
  ___________________________________________________________
 |0			    17|18	        29|30	  35|
 |	    unused	      |	     unused	  |   XX    |
 |____________________________|___________________|_________|
 |   |    |    |    |    |    |    |    |    |    |    |    |

 Op-codes 06,10,12, and 16 have no  function.   The  controller  skips
 over  them  and  proceeds.   Do  not use no-ops frequently especially
 during text output as each one takes about two microseconds.

 DATA DISC OPERATION MANUAL				    SECTION 6

 Command word
  ___________________________________________________________
 |0          7|8	 15|16	       23|2426|2729|3032|3335|
 |   data 1   |   data 2   |    data 3   |op 1|op 2|op 3| 4  |
 |____________|____________|_____________|____|____|____|____|
 |    |    |    |    |    |    |    |    |    |    |    |    |            

 A  command  word  causes the interface to send DD three command bytes
 with the op-codes specified.  The commands possible are as follows:

 OP-CODE	USE

 0		Execute:	Empties line buffer onto the disc  at
 .                              the  position  previously  specified.
 .                              Data is irrelevant.
 1		function code:	Loads  function  code register.  Bits
 .                              will be explained later.
 2		channel select:	Channel specified in data is selected
 .                              for  writing.  If erase bit is on and
 .                              graphics mode bit is set, the channel
 .                              selected  is erased to the background
 .                              selected by the dark/light bit.
 3		column select:	Data   is  loaded   into  the  column
 .                              register and the line buffer  address
 .                              register.   This  sets the X position
 .                              of your output. Column 0  is  illegal
 .                              and will hang the controller.  Column
 .                              85 is the last column to be displayed
 .                              with  characters; characters sent for
 .                              columns 86-128 are flushed, over 128,
 .                              you wrap around.    A  column  select
 .                              greater  than  85  will also hang the
 .                              controller.  The last graphics column
 .                              is  64  and columns greater than that
 .                              will hang the controller.
 4		high order	Data  is loaded  into the  high order
 .                              line address:	5 bits of the line address.
 5		low order	Data is loaded into the  low  order 4
 .                              line address:	bits of the line address.  Line range
 .                              is  from  0  to  737  (octal).   Line
 .                              addresses  between  740 and 777 cause
 .                              execute  commands  to   be   ignored.
 .                              Above 777 wraps around.
 6		write directly:	Data  is written directly on the disc
 .                              at the  location  previously  set  up
 .                              without  using  the  line buffer. The
 .                              column   address   is   automatically
 .                              incremented.     Executes   are   not
 .                              necessary.
 7		line buffer	Data  is  loaded into the line buffer
 .                              address:	address only.  This  allows  some  of
 .                              of  the  line  buffer  contents to be
 .                              changed and the  rest  retained.  The
 .                              first character displayed will be the
 .                              one specified by the column  address,
 .                              and  the  last  character will be the
 .                              last one sent after this command.

 DATA DISC
 The function code register has the following format:

 8	    7	    6	    5	    4	    3	    2	    1
 _______________________________________________________________
 |	|	|single |nospace|2 wide	| dark	| write	|graphic|
 |unused|unused	|height	| (add) |(erase)| back	|enable	| mode	| 1
 |_______|______|_______|_______|_______|_______|_______|_______|
 |	 |	|double	| space |	| light	|display| alpha	|
 |unused |unused|height	| (rep)	|1 wide	| back	|direct	|numeric| 0
 |_______|______|_______|_______|_______|_______|_______|_______|
 MSB		|			|		   LSB	|

 .      single height/double height:     Single height characters  are
 40     12  lines  tall;  10  lines  above the "base" line and 2 lines
 bit    below. Top line of character prints  on  the  line  addressed.
 .      This bit has no effect in graphics mode.

 .      space/nospace:   when  this  bit   is   on,   characters   are
 20     substituted  on  top of the line previously written; when off,
 bit    remainder of line is erased.  In graphics mode this  bit  does
 .      this:

 .      additive/replacement:    When  this bit is on, only 1 bits are
 20     written, ORed with the bits already written; when off, 1's and
 bit    0's  are  written  clobbering  previous  data.  CAUTION!: when
 .      (G)    replacing, the bits at the  beginning  and  end  of  the  line
 .      segment  you  are  writing  should be the same as the previous
 .      data or bit lossage may occur.

 .      double width/single width:       With this bit on,  characters
 10     are  5  bits  wide  with  a  0  bit on the end (total 6 bits);
 bit    characters are 10 bits wide with two 0 bits on  the  end  when
 .      the  bit is off. CAUTION!: When using double width characters,
 .      do not exceed 43 characters in a line or the  controller  will
 .      hang.  In graphics mode this bit does this:

 10     erase:   in  graphics mode, if this bit is on, the screen will
 bit    be erased to the background selected when a channel select  is
 .      (G)    done.

 .      dark/light:      When  this  bit is on, erase causes screen to
 4      go dark and characters and graphic 1 bits are light. When off,
 bit    erase  goes  to  light  and  characters and graphic 1 bits are
 .      dark.

 .      write/display directly:  When this bit is on, operations go to
 2      the  disc.  When  off,  data is displayed once on the selected
 bit    channel and then goes away and previous data remains.

 1      alphanumerics/graphics:  When this  bit  is  on,  you  are  in
 bit    graphics mode; you are in alphanumeric mode when it is off.

 DATA DISC OPERATION MANUAL				    SECTION 6

 IOT's to Data Disc Interface

 CONI 510,
 _____________________________________________________________________
 |18	       24| 25 | 26 | 27 | 28 | 29 | 30  |31 | 32 |33	  35|
 |		 | non|user|late|    |halt|     |   |    |	    |
 |    unused	 | ex |    |int |late|int |field|int|halt|    PI    |
 |		 | mem|mode|enb |    |enb |     |   |    |	    |
 |_______________|____|____|____|____|____|_____|___|____|__________|
 |      |        |              |	        |        |	    |            

 PI field is the PI channel currently in the PI register.

 Halt bit means the interface has finished its current buffer and  has
 not been re-initialized.

 Int bit means the interface is now interrupting on the channel loaded
 into it's PI register.

 Field  bit  is  0 during the even field rotation of the disc, and a 1
 during the odd field.

 Halt  int enb bit means the interface is enabled to interrupt when it
 halts.

 Late bit is set if, during a write directly transfer,  the  interface
 falls  behind  the  disc.   When  this  happens  that  line should be
 retransmitted.

 Late int enb bit means the interface is enabled to interrupt when the
 late flag comes on.

 User mode bit allows  the first channel select (from  a command word)
 to  have its  proper effect.  All other  channel selects  are ignored
 until the next CONO.

 Non ex mem bit means that the interface tried to access non existant
 memory.

 CONO 510,
  ____________________________________________________________
 |18		  25| 26 | 27 | 28 | 29 | 30 | 31  |32 |33      35|
 |		    |user|late|    |    |halt|force|re-|	  |
 |	unused	    |    |int |SPGO|DDGO|int |     |set|    PI    |
 |		    |mode|enb |    |    |enb |field|   |	  |
 |__________________|____|____|____|____|____|_____|___|__________|
 |       |       |	      |		     |	       |	  |

 PI field sets PI register to channel indicated.

 Reset bit causes Data Disc to be reset.  This should be done  if  the
 controller  is  hung  up  for some reason like a column number is too
 large.

 Force field bit, when set, causes Data Disc to look at the low  order
 bit  of  the line address.  This indicates which field the characters
 go out on.  Both fields must  be  written  for  complete  characters.
 When  this  bit is off, the characters are written on the first field
 that comes around.  This bit has no effect on graphics data.

 Halt int enb bit sets or  resets  the  halt  interrupt  enable  which
 enables interrupts when the interface halts.

 DDGO  sets  or resets the DDGO flip-flop in the interface.  This must
 be set if you want the interface to talk to Data Disc.

 SPGO bit sets or resets the SPGO flip-flop  in  the  interface.  This
 should  never  be  set.   It is to be used for some device in the sky
 that could go through the same interface.

 Late int enb sets or resets the late interrupt enable  which  enables
 interrupts when the late bit comes on.

 User mode bit allows  the first channel select (from  a command word)
 to  have its  proper effect.  All other  channel selects  are ignored
 until the next CONO.

 DATAO 510,
 ___________________________________________________________
 |0			    17|18			  35|
 |	   unused	      |	     starting address	    |
 |____________________________|_____________________________|
 |   |    |    |    |    |    |    |    |    |    |    |    |            
 A DATAO to the interface loads the memory address register  with  the
 right half of the data and starts the interface at that location.

 * ================================================================================ *
 hardware and construction notes at
 https://www.saildart.org/VDS.LES[H,DOC]

 VIDEO SWITCH						    SECTION 7

 The video  switch is a  logic matrix  connecting the 32  Data
 Disk display  channels and up to  7 analog video sources  to up to 64
 video monitors (normally consoles).   Each monitor may  independently
 be switched  to any combination  of Data  Disk channels, and  also to
 any  one of the 7 analog video  sources, or none.  Selecting multiple
 sources produces an  output which is the  MAX (i.e. most "white")  of
 the  selected inputs.   This  means that  Data  Disk "white-on-black"
 channels can be properly superimposed, but "black-on-white"  channels
 cannot.  Data  Disk sync signal  is normally supplied along  with the
 video,  but this can be  suppressed when necessary to  view an analog
 source which is  not synchronous  with Data Disk  video.   Naturally,
 combining such  a signal with any  Data Disk output will  not produce
 the desired effect.

 Control  of the  video switch  is  fairly simple.  Associated
 with  each  of the  64 output  channels  is a  36-bit  register which
 determines which  sources are  selected to  that output.   Loading  a
 given register is accomplished by doing  a CONO to select the channel
 and a DATAO of the desired map data.  More detail is given below.


 CONO 340,:

 18-29	Unused
 30-35	Channel to be affected by subsequent DATAOs


 DATAO 340,:

 0-31	DD channel selection mask.
 .      A 1 in bit position N enables DD channel
 .      N in the output; a 0 disables it.

 32	DD sync inhibit (for use with nonsynchronous analog sources)

 33-35	Analog channel selection, 1-7; 0=> none.


 ANALOG CHANNEL ASSIGNMENTS:

 1	Cohu TV camera (hand-eye table)
 2	Sierra TV camera (hand-eye table)
 3	Kintel TV camera (movable)
 4	Cart receiver
 5	Lounge TV set
 6	Secondary video synthesizer (or chroma of color synthesizer)
 7	Primary video synthesizer

-----------------------------------------------------------------------------------------------
some lines from DPYSER [ J17, SYS ]

2869: SUBTTL DATA DISK SERVICE ROUTINE - JAM, DEC. 1970
2870: 
2871: ; THESE ROUTINES ARE CALLED BY THE FOLLOWING IN APRINT
2872: ;       SOSGE DDCNT
2873: ;       JRST DDCLK
2874: ;       SKIPE DDSTART
2875: ;       JRST DDSTRT
2876: ;APRADD:
2877: ; THE DATA DISK ROUTINES KNOW ALL ABOUT THE PAGE PRINTER
2878: ; AND THE PIECE OF GLASS MECHANISM. TRANSFERS ARE STARTED
2879: ; BY PLACING AN ADDRESS IN THE QUEUE WITH THE LEFT HALF GIVING
2880: ; THE TYPE OF TRANSFER.
2881: 
2882: ; HERE ARE THE DD HARDWARE BITS
2883: ; CONO BITS
2884: 
2885: START←←100      ; STARTS TRANSFER
2886: RESET←←10       ; RESETS INTERRUPT BIT
2887: ENB←←40         ; INTERRUPT ENABLE
2888: LOSENB←←400     ; ENABLE LATE FLAG INTERRUP
2889: DAMMIT←←20      ; MAKES TRANSFER GO OUT ON SPECIFIC FIELD
2890: EXECM←←1000     ; EXEC MODE, ALLOWS ONLY ONE CHANNEL SELECT
2891: DDNXM←←2000     ; NON-EX MEM FLAG
2892: USUAL←←START!ENB!LOSENB!DAMMIT!APRCHN
2893: 
2894: ; CONI BITS
2895: FIELD←←40       ; FIELD BIT
2896: INT←←20         ; INTERRUPT BIT
2897: LOSE←←200       ; LATE FLAG

 * --------------------------------------------------------------------------------------------- 100
 EOF datadisc.c */

#endif
