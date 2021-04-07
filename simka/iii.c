// iii.c
// nee: ka10_iii.c: Triple III display processor. Copyright (c) 2019-2020, Richard Cornwell
/*
  The Stanford display terminals in 1974 consisted of
  six vector graphic CRT displays and
  32 video raster TV display (monochrome) channels
  which were switched among 64 monitors

 6 vector terminals, device name DPY,
32 raster terminals, device name DDD,
with 64 keyboard lines, device name DKB,
and the video crossbar switch, device name VDS,
32 digital TV inputs
 7  analog TV inputs going to 64 outputs,
58 of which were monochrome TV screens and a further 6 into a video synthesizer.

  Displays send message (stream)  to  independent III terminal 'Server', like an X-windows-display 'Server'.
  Keyboard recv message (stream) from independent III terminal 'Server'.
*/

#include "ka10_defs.h"

#ifndef NUM_DEVS_III
#define NUM_DEVS_III 0
#endif
#if NUM_DEVS_III > 0

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>
#include <fcntl.h>

// mark and/or omit scratch-work de jour
#define bamboo
#define Bamboo  0&&// skip

#if 0
float scale[] = { 1.0F, // size=0 no change, size=1 to 7
                  1.0F,  /* 128 chars per line */
                  1.3F,  /* 96 chars per line */
                  2.0F,  /* 64 chars per line */
                  2.5F,  /* 48 chars per line */
                  4.0F,  /* 32 chars per line */
                  5.3F,  /* 24 chars per line */
                  8.0F   /* 16 chars per line */
};
#endif
int I_char_dx[]= { 0,  8, 12,    14,    16, 24,    32,  48   };
int I_char_dy[]= { 0, 16, 25,    28,    32, 48,    64, 102   };

// globals
uint64 instruction; // III display instruction
int    iii_maddr;   // III memory address of instruction
int    iii_select=07700; // Select terminal mask for the six lines named TTY20 to TTY25.
//int  iii_mask  =07700; // enable translation into iii buffer for transmission
int    iii_mask  =04000; // enable translation into iii buffer for transmission

// forwards
t_stat iii_devio(uint32 dev, uint64 *data);
t_stat iii_svc(UNIT *uptr);
t_stat iii_reset(DEVICE *dptr);
t_stat iii_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *iii_description (DEVICE *dptr);

// forwards
int dkb_keyboard (int keyline, int keycode);

void error(const char *msg)
{
  perror(msg);
  exit(0);
}

// globals variables for sending messages to a remote III terminal processor
int sockfd_iii_terminal;
extern int sockfd_ddd_terminal;
char iiiold[40000];
char iiinew[40000];
char *iiibuf=iiinew;
extern int tv_spacewar_buttons;
void hark(){
  int n=0,keyline=0,keycode=0;
  char *fromwhom;
  char msg[128],*p,*q;
  //
  if(sockfd_iii_terminal){ // III link exists
    n = recv(sockfd_iii_terminal,msg,128,MSG_DONTWAIT);// no blocking on sockfd_iii_terminal
    fromwhom = "III";
  }
  if(sockfd_ddd_terminal && !(n>0)){ // DDD link exists and III not needed.
    n = recv(sockfd_ddd_terminal,msg,128,MSG_DONTWAIT);// no blocking on sockfd_ddd_terminal
    fromwhom = "DDD";
  }
  if(n>0){
    msg[n]=0;
    Bamboo fprintf(stderr,"\r\nhark recv n=%d msg='%s' from %s\r\n",n,msg,fromwhom);
    p = msg;
    while(q=index(p,';')){
      if (2==sscanf(p,"DKBevent=%02o%04o;",&keyline,&keycode)){
        //fprintf(stderr,"sscanf OK keyline %02o keycode %04o\r\n",keyline,keycode);
        //keycode = strtol(msg,NULL,8);
        dkb_keyboard(keyline,keycode);
        //fprintf(stderr,"sscanf OK keycode %04o\r\n",keycode);
      }
      if (1==sscanf(p,"swrbut=%04X;",&tv_spacewar_buttons)){
        //fprintf(stderr,"sscanf OK swrbut %04X\r\n",tv_spacewar_buttons);
      }
      if (1==sscanf(p,"iii_mask=%05o;",&iii_mask)){ // example "iii_mask=07700;" from iiisexplex.py
        fprintf(stderr,"sscanf OK iii_mask %05o\r\n",iii_mask);
      }
      p = q + 1;
    }
  }
}
void send_to_iii_terminal(){
  int n;
  // hark();
  bamboo if(!sockfd_iii_terminal)return; // No III server.
  if(!strcmp(iiiold,iiinew)){
    // Nothing new ... send nothing.
    Bamboo fprintf(stderr,"Nothing new for III ... send nothing to III\r\n");
    iiibuf = iiinew; // next refill
    return;
  }
  if(0){
    printf("Content of the III buffer. NOT sent to display server.");
    printf("\r\n============== III ==================\r\n");
    n = write(0,iiinew,strlen(iiinew));    
    printf("\r\n============== EOM ==================\r\n");
  }
  if(sockfd_iii_terminal){
    // fprintf(stderr,"strlen(iiinew)=%ld\r\n",strlen(iiinew));
    n = write(sockfd_iii_terminal,iiinew,strlen(iiinew));
    if (n < 0) error("ERROR writing to socket");
  }
  strcpy(iiiold,iiinew);
  iiibuf = iiinew; // next refill
}
void init_iii_server()
{
  int portno=1974;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  static int once=0;
  //
  if(once++)return; // pass once only
  sockfd_iii_terminal = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_iii_terminal < 0) 
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
  if (connect(sockfd_iii_terminal,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
    fprintf(stderr,"No III terminal server\r\n");
    close(sockfd_iii_terminal);
    sockfd_iii_terminal=0;
    // error("ERROR connecting");
  }
  //  if(fcntl(sockfd_iii_terminal,F_SETFL,O_NONBLOCK)) error("fcntl SET sockfd_iii_terminal NONBLOCK");
}

// see sim_defs.h struct UNIT
#define III_DEVNUM 0430
#define STATUS u3
#define MAR    u4
#define PIA    u5
#define POS    u6

/* CONO Bits */
#define SET_PIA    000000010    /* Set if this bit is zero */
#define STOP       000000020    /* Stop processor after instruction */
#define CONT       000000040    /* Start execution at address */
#define F          000000100    /* Clear flags */
#define SET_MSK    000360000    /* Set mask */
#define RST_MSK    007400000    /* Reset mask */

/* CONI Bits */
#define PIA_MSK    000000007
#define INST_HLT   000000010    /* 32 - Halt instruction */
#define WRAP_ENB   000000020    /* 31 - Wrap around mask */
#define EDGE_ENB   000000040    /* 30 - Edge interrupt mask */
#define LIGH_ENB   000000100    /* 29 - Light pen enable mask */
#define CLK_STOP   000000200    /* 28 - Clock stop */
                                /* 27 - Not used */
#define CLK_BIT    000001000    /* 26 - Clock */
#define NXM_BIT    000002000    /* 25 - Non-existent memory */
#define IRQ_BIT    000004000    /* 24 - Interrupt pending */
#define DATAO_LK   000010000    /* 23 - PDP10 gave DATAO when running */
#define CONT_BIT   000020000    /* 22 - Control bit */
#define LIGHT_FLG  000040000    /* 21 - Light pen flag */
#define WRAP_FLG   000100000    /* 20 - Wrap around flag */
#define EDGE_FLG   000200000    /* 19 - Edge overflow */
#define HLT_FLG    000400000    /* 18 - Not running */

#define WRAP_MSK   00001
#define EDGE_MSK   00002
#define LIGH_MSK   00004
#define HLT_MSK    00010
#define WRP_FBIT   00020
#define EDG_FBIT   00040
#define LIT_FBIT   00100
#define CTL_FBIT   00200
#define HLT_FBIT   00400
#define NXM_FLG    01000
#define DATA_FLG   02000
#define RUN_FLG    04000

#define TSS_INST   012          /* Test-Set-Skip */
#define LVW_INST   006          /* Long vector */
#define SVW_INST   002          /* Short vector */
#define JMP_INST   000          /* Jump or Halt */
#define JSR_INST   004          /* JSR(1) or JMS(0), SAVE(3) */
#define RES_INST   014          /* Restore */
#define SEL_INST   010          /* Select line */

#define POS_X      01777400000
#define POS_Y      00000377700
#define CBRT       00000000070 /* beam brightness */
#define CSIZE      00000000007 /* character  size */
#define POS_X_V    17
#define POS_Y_V    6
#define CBRT_V     3
#define CSIZE_V    0


// SIM data structures for this device
DIB    iii_dib = { III_DEVNUM, 1, iii_devio, NULL};
UNIT   iii_unit[] = {
  {UDATA(&iii_svc,UNIT_IDLE,0)},
};
MTAB   iii_mod[]  = {{0}};
DEVICE iii_dev = {
  "III", iii_unit, NULL, iii_mod, 1, 10, 31, 1, 8, 8,
  NULL, NULL, iii_reset,
  NULL, NULL, NULL, &iii_dib, DEV_DEBUG | DEV_DISABLE | DEV_DIS, 0, dev_debug,
  NULL, NULL, &iii_help, NULL, NULL, &iii_description
};

// KA10 iots for the central Triple III vector display unit
t_stat iii_devio(uint32 dev, uint64 *data) {
  UNIT *uptr = &iii_unit[0];
  switch(dev & 3) {
    
  case CONI:
    *data = (((uint64)iii_select) << 24) | (uint64)(uptr->PIA);
    if ((instruction & 037) == 0)             *data |= INST_HLT;
    *data |= (uptr->STATUS & 07) << 4;
    if (uptr->STATUS & NXM_FLG)               *data |= NXM_BIT;
    if (uptr->STATUS & DATA_FLG)              *data |= DATAO_LK;
    if ((uptr->STATUS & RUN_FLG) == 0)        *data |= HLT_FLG;
    if (uptr->STATUS & CTL_FBIT)              *data |= CONT_BIT;
    if (uptr->STATUS & WRP_FBIT)              *data |= WRAP_FLG;
    if (uptr->STATUS & EDG_FBIT)              *data |= EDGE_FLG;
    if (uptr->STATUS & LIT_FBIT)              *data |= LIGHT_FLG;
    sim_debug(DEBUG_CONI, &iii_dev, "%06o / CONI III,%12llo\n", PC, *data);
    break;
    
  case CONO:
    clr_interrupt(III_DEVNUM);
    if (*data & SET_PIA)     uptr->PIA = (int)(*data&PIA_MSK);
    if (*data & F)           uptr->STATUS &= ~(WRP_FBIT|EDG_FBIT|LIT_FBIT|DATA_FLG|NXM_FLG);
    uptr->STATUS &= ~(017 & ((*data >> 14) ^ (*data >> 10)));
    uptr->STATUS ^=  (017 &  (*data >> 10));
    if (*data & STOP)        uptr->STATUS &= ~RUN_FLG;
    if (*data & CONT) {
      uptr->STATUS |= RUN_FLG;
      instruction = M[uptr->MAR];
      sim_debug(DEBUG_CONO, &iii_dev,
                "%06o / CONO III,%12llo select %04o fetch %06o / %012llo run\n",
                PC, *data, iii_select, uptr->MAR, instruction);
      uptr->MAR++;
      sim_activate(uptr, 10);
    } else {
      sim_debug(DEBUG_CONO, &iii_dev, "%06o / CONO III,%12llo\n", PC, *data);
    }
    if (((uptr->STATUS >> 3) & (uptr->STATUS & (WRAP_MSK|EDGE_MSK|LIGH_MSK))) != 0)     set_interrupt(III_DEVNUM, uptr->PIA);
    if (uptr->STATUS & HLT_MSK)                                                         set_interrupt(III_DEVNUM, uptr->PIA);
    break;
    
  case DATAI:
    sim_debug(DEBUG_DATAIO, &iii_dev, "%06o / DATAI III,%12llo\n", PC, *data);
    break;
    
  case DATAO:
    if(!sockfd_iii_terminal)break; // No III terminal server - so quit now.
    if (uptr->STATUS & RUN_FLG){
      uptr->STATUS |= DATA_FLG;
      sim_debug(DEBUG_DATAIO, &iii_dev, "%06o / DATAO III,%12llo interlocked\n", PC, *data);
    } else {
      instruction = *data;
      if( PC == 0127176 ){ // DPYAVL displayed at DPYSER#673
        send_to_iii_terminal();
      }
      sim_debug(DEBUG_DATAIO, &iii_dev, "%06o / DATAO III,%12llo single-step %04o\n", PC, *data, iii_select);
      sim_activate(uptr, 10);
    }
    break;
  }
  return SCPE_OK;
}

// Note: variable length display instruction opcodes,
// The unused bits were likely ignored by the real hardware, so several codes execute the same instruction.
#define DOP_HALT      0 // 5-bit opcode  .00000       0     040?
#define DOP_TEXT      1 // 1-bit opcode       1       1                 ok
#define DOP_SVW      02 // 4-bit opcodes ..0010     002 022 042 062     ok
#define DOP_JMS      04 //               ..0100     004     044         ok
#define DOP_LVW      06 //               ..0110     006 026 046 066     ok
#define DOP_SELECT  010 //               ..1000     010 030 050 070     ok
#define DOP_TSS     012 //               ..1010     012 032 052 072     ok
#define DOP_RESTORE 014 //               ..1100     014 034 054 074     ok
#define DOP_NOP     016 //               ..1110     016 036 056 076
#define DOP_JUMP    020 // 5-bit opcodes .10000     020     060?
#define DOP_JSR     024 // 5-bit opcodes .10100     024
#define DOP_SAVE    064 // 6-bit opcode  110100     064

// familiar user level definitions
#define DOP_AV 0106 // Absolute   Visible
#define DOP_AI 0146 // Absolute Invisbile
#define DOP_RV 0006 // Relative   Visible
#define DOP_RI 0046 // Relative Invisbile

/* CUT from ALLDAT[J17,SYS]
 * ================================================================================
524: BEGIN DPYDAT
525: SUBTTL III AND DATA DISK DATA STORAGE
526: 
527: ;; NOW SOME DISPLAY OPCODES AND MACROS FOR GENERATING DP INSTRS.
528: 
529: DISJMP←←20      ;DP JMP INSTR. OPCODE.
530: DISJMS←←4       ; JMS (STORES TWO WORDS)
531: DISJSR←←24      ; STORES ONLY RETURN ADDR.
532: DISRST←←14      ;RESTORE.
533: DISSEL←←10      ;SELECT.
534: DISNOP←←14      ;ACTUALLY RST, BUT A GOOD NOP WITH ALL BITS OFF.
535: DISSKP←←12      ; TEST AND SKIP
536: DISKPN←←32      ; TEST AND NOT SKIP
537: 
538: DEFINE LVW(X,Y,TYPE,MODE,BRT,SIZ)       ;ASSEMBLES A LONG VECTOR.
539:   { MVW1 (MODE,TYPE,BRT,SIZ)    ;TWIDDLE PARAMS.
540:         BYTE (11)<X>,<Y>(3)B,S(2)MD,TT(3)3 }    ;ASSEMBLE INSTR.
541: DEFINE MVW1 (M,T,BRT,SIZ)
542:  {IFIDN {M}{A}{MD←1;}MD←0       ;MODE = `A' FOR ABSOLUTE.
543:   IFIDN {T}{I}{TT←2;}TT←0       ;TYPE = `I' FOR INVISIBLE.
544:   IFIDN {BRT}{}{B←0;}B←BRT      ;BRT = 0 IF OMITTED.
545:   IFIDN {SIZ}{}{S←0;}S←SIZ      ;SAME FOR SIZ.
546: }
547: DEFINE CW (C1,B1,C2,B2,C3,B3) {<BYTE (8)<B1>,<B2>,<B3>(3)<C1>,<C2>,<C3>>!4}
 * ================================================================================
 */
t_stat
iii_svc (UNIT *uptr)
{
  uint64    temp;
  int       A;
  int       ox, oy, nx, ny, br, sz, dop;
  int       i, j, ch;
  
  // initialize message buffer for 1 line
  if(iiibuf==iiinew){
    iiibuf += sprintf(iiibuf,"---\nIII_code: 0"); // YAML document Start-of-III-buffer with 0.
    // Sim needs to Fake the initial beam position (a side effect of the who-hold-avail Mickey Mouse)
    temp = (((uint64)uptr->POS) << 8) | 0146;
    iiibuf += sprintf(iiibuf," %012llo",temp);
  }
  
  // Beam position X, Y, Bright and Size from unit
  sz = (uptr->POS & CSIZE) >> CSIZE_V;
  br = (uptr->POS & CBRT ) >> CBRT_V;
  ox = (uptr->POS & POS_X) >> POS_X_V;
  oy = (uptr->POS & POS_Y) >> POS_Y_V;
  nx = ox = (ox ^ 02000) - 02000;
  ny = oy = (oy ^ 02000) - 02000;

  // The first display instruction is fetched by an IOT
  dop = (instruction &  1) ? 1 : (instruction & 076);
  
  switch(dop) {
    
  case 020: /* JMP */
    uptr->MAR = (instruction >> 18) & RMASK;
    break;
    
  case 000: /* HLT */
    iiibuf += sprintf(iiibuf," 00 "); // End-of-III-buffer with 00.
    uptr->STATUS &= ~RUN_FLG;
    if (uptr->STATUS & HLT_MSK){
      set_interrupt(III_DEVNUM, uptr->PIA);
    }
    // No further sim activation
    return SCPE_OK;

  case 001: // Text word 5 characters
    for (i = 29; i >= 1; i -= 7) {
      ch = (instruction >> i) & 0177;
      if (ch == '\t' || ch == 0) continue;
      if (ch == '\r') { ox = -512; continue; }
      if (ch == '\n') { oy -= I_char_dy[sz]; continue; }
      ox += I_char_dx[sz];
    }
    nx = ox;
    ny = oy;
    if(iii_select & iii_mask) iiibuf += sprintf(iiibuf," %012llo",instruction);
    uptr->POS= (POS_X & ((nx&03777)<<POS_X_V)) | (POS_Y & ((ny&03777)<<POS_Y_V)) | (CBRT & (br<<CBRT_V)) | (CSIZE & (sz<<CSIZE_V));
    //sim_debug( DEBUG_DETAIL, &iii_dev, "        text update beam %012llo\n", ((uint64)uptr->POS) <<8 );
    break;

  case 002: // Short Vectors packed two per instruction
  case 022:
  case 042:
  case 062:
    nx = (instruction >> 26) & 077;       // first vector
    ny = (instruction >> 20) & 077;
    nx = (nx ^ 040) - 040;              // sign extend
    ny = (ny ^ 040) - 040;
    nx += ox;                           // relative
    ny += oy;
    if (nx < -512 || nx > 512 || ny < -512 || ny > 512)      uptr->STATUS |= EDG_FBIT;
    ox = nx;
    oy = ny;
    nx = (instruction >> 12) & 0177;      // second vector
    ny = (instruction >>  6) & 0177;
    nx = (nx ^ 040) - 040;              // sign extend
    ny = (ny ^ 040) - 040;
    nx += ox;                           // relative
    ny += oy;
    if (nx < -512 || nx > 512 || ny < -512 || ny > 512)      uptr->STATUS |= EDG_FBIT;
    if(iii_select & iii_mask) iiibuf += sprintf(iiibuf," %012llo",instruction);
    uptr->POS= (POS_X & ((nx&03777)<<POS_X_V)) | (POS_Y & ((ny&03777)<<POS_Y_V)) | (CBRT & (br<<CBRT_V)) | (CSIZE & (sz<<CSIZE_V));
    //sim_debug( DEBUG_DETAIL, &iii_dev, "short vector update beam %012llo\n", ((uint64)uptr->POS) <<8 );
    break;

  case 006: // Long Vector Invisible    Move
  case 026: // end-point dot
  case 046: // Long Vector Visible      Line
  case 066: // end-point dot
    if (((instruction >> 8 ) & CSIZE) != 0)  sz = (instruction >> 8) & CSIZE;
    if (((instruction >> 11) & 7)     != 0)  br = (instruction > 11) & 7;               
    nx = (instruction >> 25) & 03777;
    ny = (instruction >> 14) & 03777;
    nx = (nx ^ 02000) - 02000;
    ny = (ny ^ 02000) - 02000;
    if ((instruction & 0100) == 0) { /* Relative mode */
      nx += ox;
      ny += oy;
      if (nx < -512 || nx > 512 || ny < -512 || ny > 512) uptr->STATUS |= EDG_FBIT;
    }
    if(iii_select & iii_mask) iiibuf += sprintf(iiibuf," %012llo",instruction);
    uptr->POS= (POS_X & ((nx&03777)<<POS_X_V)) | (POS_Y & ((ny&03777)<<POS_Y_V)) | (CBRT & (br<<CBRT_V)) | (CSIZE & (sz<<CSIZE_V));
    //sim_debug( DEBUG_DETAIL, &iii_dev, " long vector update beam %012llo\n", ((uint64)uptr->POS) <<8 );
    break;
    
  case 004: // JMS
    A = (instruction >> 18) & RMASK;
    M[A] =  (((uint64)uptr->MAR) << 18) | 020 /* | CPC */;
    M[A+1] = (((uint64)uptr->POS) << 8) | (uptr->STATUS & 0377);
    uptr->MAR = A+2;
    //sim_debug( DEBUG_DETAIL, &iii_dev, "004        JMS save beam %012llo\n", ((uint64)uptr->POS) <<8 );
    break;
    
  case 024: // JSR
    A = (instruction >> 18) & RMASK;
    M[A] =  (((uint64)uptr->MAR) << 18) | 020 /* | CPC */;
    uptr->MAR = A+1;
    break;
    
  case 064: // SAVE
    A = (instruction >> 18) & RMASK;
    M[A] = (((uint64)uptr->POS) << 8) | (uptr->STATUS & 0377);
    //sim_debug( DEBUG_DETAIL, &iii_dev, "064            save beam %012llo\n", ((uint64)uptr->POS) <<8 );
    break;
    
  case 010: // Select
  case 030: // Select, old-hardware tolerated unused bits on. Alternatively, garbage instructions should abort.
  case 050:
  case 070:
    iii_select |=  ((instruction >> 24) & 07777); /*  Set  mask : turn these lines  ON */
    iii_select &= ~((instruction >> 12) & 07777); /* Reset mask : turn those lines OFF */
    sim_debug(DEBUG_DETAIL, &iii_dev, "select %04o\n", iii_select);
    // tell client beam position
    if(iii_select & iii_mask){
        temp = (((uint64)uptr->POS) << 8) | 0146;
        iiibuf += sprintf(iiibuf," %012llo",temp);
    }
    // Omit the ugly hardware implementation details concerning both set and reset at the same time.
    // That mechanism was NOT used by the WAITS display code.
    if(iii_select & iii_mask) iiibuf += sprintf(iiibuf," %012llo",instruction);
    break;
    
  case 012: // TEST SET
  case 032: // TEST SET Skip
  case 052: // TEST SET
  case 072: // TEST SET Skip
    A = (uptr->STATUS & (int32)(instruction >> 12) & 0377) != 0;
    j = (int)((instruction >> 20) & 0377);    /* set mask */
    i = (int)((instruction >> 28) & 0377);    /* Reset */
    uptr->STATUS &= ~(i ^ j);
    uptr->STATUS ^= j;
    if (A ^ ((instruction & 020) != 0)) // skip
      uptr->MAR++;
    break;

  case 014: // Restore no operation
    break;
  case 034: // Restore Flags    
  case 054: // Restore Position
  case 074: // Restore Position and Flags
    A = (instruction >> 18) & RMASK;
    temp = M[A];
    if(instruction & 020){ // Flags
      uptr->STATUS &= ~0377;
      uptr->STATUS |= (temp & 0377);
    }
    if(instruction & 040){ // Position
      uptr->POS = (temp >> 8) & (POS_X|POS_Y|CBRT|CSIZE);
      temp &= ~0177LL; // mask
      temp |=    0146; // Fake Absolute Invisible Vector to set-remote-Terminal server POSITION and FLAGS
      if(iii_select & iii_mask) iiibuf += sprintf(iiibuf," %012llo",temp);
      // sim_debug( DEBUG_DETAIL, &iii_dev, "074         RESTORE beam %012llo\n", ((uint64)uptr->POS) <<8 );
    }
    break;
    
  case 016: // NOP instruction
  case 036:
  case 056:
  case 040: // ignore illegal DOP codes
  case 044:
  case 060:
  case 076:
    break;
  }
  // When III is running fetch next display instruction
  if (uptr->STATUS & RUN_FLG) {
    instruction = M[uptr->MAR];
    iii_maddr = uptr->MAR;
    sim_debug(DEBUG_DETAIL, &iii_dev, "select %04o fetch %06o / %012llo\n", iii_select, uptr->MAR, instruction);
    uptr->MAR++;
    uptr->MAR &= RMASK;
    sim_activate(uptr, 50);
  }
  if (((uptr->STATUS >> 3) & (uptr->STATUS & (WRAP_MSK|EDGE_MSK|LIGH_MSK))) != 0){
    set_interrupt(III_DEVNUM, uptr->PIA);
  }
  return SCPE_OK;
}
void init_ddd_terminal();
t_stat iii_reset (DEVICE *dptr)
{
  if (dptr->flags & DEV_DIS) {
    //        display_close(dptr);
  } else {
    //        display_reset();
    dptr->units[0].POS = 0;
    // Initialize the connections to ports 1974 and 2026 for III and DD.
    init_iii_server();
    init_ddd_terminal();
    if(!sockfd_iii_terminal){ // No III terminal server - so mark disabled.
      dptr->flags |= DEV_DIS;
    }
  }
  return SCPE_OK;
}
t_stat iii_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
  fprintf(st,"III help message\r\n");
  return SCPE_OK;
}
const char *iii_description (DEVICE *dptr)
{
  return "Stanford Triple III vector display for up to six terminal lines";
}
#endif
