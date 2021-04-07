/* This file is named simka/video_switch.c -*- mode:C;coding:utf-8; -*-

        Implement the KA10 I/O device named VDS,
        which is the Stanford Video-Display-(crossbar)-Switch
        designed and build by Lester Earnest.

B.g.Baumgart `2020-06-28 bgbaumgart@mac.com'
 */

#include "ka10_defs.h"
#include <unistd.h>

// mark or omit scratch-work printf
#define bamboo
#define bamboff 0&&
#define bambon  1&&

#define VDS_DEVNUM 0340
#define VDS_OFF (1 << DEV_V_UF)
#define PIA_CH  u3
#define PIA_FLG 07
#define TVLINE  u4

t_stat vds_devio(uint32 dev, uint64 *data);
t_stat vds_srv(UNIT *uptr);
t_stat vds_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat vds_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat vds_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat vds_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);

const char *vds_description (DEVICE *dptr);

extern int nlzero(uint64 w);
extern int sockfd_ddd_terminal;
void error(const char *msg);

uint64 vds_crossfoo[64]; // updates sent to terminal server via YAML message stream
uint64 vds_crossbar[64]; // mask of 36-bits selecting video sources to output to terminal TV# monitor.
uint32 tvline[40]; // destination TV line for DD channel -- starts with DD#11 octal going to all TV lines.
uint32 ddchan[64]; // source DD channel assigned to TV line#

UNIT vds_unit[] = {
  {UDATA(&vds_srv, UNIT_IDLE|UNIT_DISABLE, 0)},  /* 0 */
};
DIB vds_dib = {VDS_DEVNUM, 1, &vds_devio, NULL};
MTAB vds_mod[] = {
  { MTAB_VDV, 0, "ON", "ON", vds_set_on, vds_show_on },
  { MTAB_VDV, VDS_OFF, NULL, "OFF", vds_set_off },
  { 0 }
};
DEVICE vds_dev = {
  "VDS", vds_unit, NULL, vds_mod, 1, 8, 0, 1, 8, 36,
  NULL, NULL, NULL, NULL, NULL, NULL,
  &vds_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG, 0, dev_debug,
  NULL, NULL, &vds_help, NULL, NULL, &vds_description
};

// KA10 iots for the Lester Earnest Video Display 40x64 crossbar Switch
t_stat vds_devio(uint32 dev, uint64 *data)
{
  DEVICE *dptr = &vds_dev;
  UNIT *uptr = &vds_unit[0];
  int tv = uptr->TVLINE;
  int dd;
  int nlz;
  int dstvid,srcvid;
  switch(dev & 07) {
    
  case CONI:
    *data = 0;
    sim_debug(DEBUG_CONI, &vds_dev, "%06o / CONI VDS,%12llo\n", PC, *data);
    break;
    
  case CONO:
    vds_unit[0].PIA_CH &= ~(PIA_FLG);
    vds_unit[0].PIA_CH |= (int32)(*data & PIA_FLG);
    sim_debug(DEBUG_CONO, &vds_dev, "%06o / CONO VDS,%12llo\n", PC, *data);
    uptr->TVLINE = *data & 077; // 64 tvline destinations come out of the crossbar
    bamboff fprintf(stderr,"VDS %06o /  CONO VDS,%12llo TV#%d\r\n", PC, *data, uptr->TVLINE);
    break;
    
  case DATAI:
    *data = 0;
    sim_debug(DEBUG_DATAIO, &vds_dev, "%06o / DATAI VDS,%12llo\n", PC, *data);
    break;
    
  case DATAO:
    sim_debug(DEBUG_DATAIO, &vds_dev, "%06o / DATAO VDS,%12llo\n", PC, *data);
    nlz = nlzero( *data ); // note result 0 to 36.
    srcvid = (nlz<32) ? nlz : 040 + (*data & 07LL); // addressing for the 8. analog video sources
    dstvid = uptr->TVLINE;
    bamboff fprintf(stderr, "VDS %06o / DATAO VDS,%12llo first DD#%02o nlz=%02o, output to ttyLine#%o dkb#%o\r\n", PC, *data, srcvid, nlz, 020+tv, tv );
    vds_crossbar[tv] = *data; // DD inputs for TVLINE output
    ddchan[tv] = nlz;
    tvline[nlz]= tv;
    if(sockfd_ddd_terminal){ // send VDS update message to the DD server
      int n;char buf[64];
      sprintf(buf,"---\nVDS_%02o: 0o%012llo\n",tv,*data); // YAML key: value
      n = write(sockfd_ddd_terminal,buf,strlen(buf));
      if (n < 0) error("ERROR writing to ddd socket");
    }
    break;
  default:
    break;
  }
  return SCPE_OK;
}
t_stat
vds_srv(UNIT * uptr)
{
  return SCPE_OK;
}
t_stat vds_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &vds_dev;
  dptr->flags &= ~VDS_OFF;
  return SCPE_OK;
}
t_stat vds_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &vds_dev;
  dptr->flags |= VDS_OFF;
  return SCPE_OK;
}
t_stat vds_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
  DEVICE *dptr = &vds_dev;
  fprintf (st, "%s", (dptr->flags & VDS_OFF) ? "off" : "on");
  return SCPE_OK;
}
t_stat vds_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
  fprintf(st,"Data Disc VDS help message\n\n");
  fprint_set_help (st, dptr);
  fprint_show_help (st, dptr);
  fprint_reg_help (st, dptr);
  return SCPE_OK;
}
const char *vds_description (DEVICE *dptr)
{
  char *D="Stanford VDS video display 40x64 crossbar switch";
  return D;
}
/* VDS section from FACIL.TED[H,DOC]7 which is the Panofsky hardware manual 1974-11-15
 * ------------------------------------------------------------------------------------------
 see documentation at
 https://www.saildart.org/[H,DOC]
 https://www.saildart.org/FACIL.TED[H,DOC]7

 hardware and construction notes at
 https://www.saildart.org/VDS.LES[H,DOC]
 * ------------------------------------------------------------------------------------------

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
 A 1 in bit position N enables DD channel
 N in the output; a 0 disables it.

 32	DD sync inhibit (for use with nonsynchronous analog sources)

 33-35	Analog channel selection, 1-7; 0=> none.


 ANALOG CHANNEL ASSIGNMENTS:                    Suggested animated GIF for reënactment
src#
 0 nothing
 1	  Cohu TV camera (hand-eye table)       blocks water pump
 2	Sierra TV camera (hand-eye table)       doll or plastic horse on turntable
 3	Kintel TV camera (movable)              Zoe flick or people portrait gallery
 4	  Cart receiver                         cartroom cartchairs cartroadway
 5	Lounge TV set                           Star Trek or Watergate
 6	Secondary video synthesizer ( Quam Heath Kit Color TV set in the DPY room )
 7	  Primary video synthesizer ( black and white studio monitor )
+040
 * ------------------------------------------------------------------------------------------
For the 1974 Data Disc control key commands, see

https://www.saildart.org/DDKEY.BH[UP,DOC]7

part of which says
______________________________________________________________________________
|    TO GET	     TYPE  THEN  THEN |	    TO GET	  TYPE	 THEN  THEN  |
| DD white on black [ESC]    C	      |	DD black on white [BREAK] C	     |
| line ed refresh   [ESC]    R	      |	select Lounge TV  [BREAK] S	     |
| select channel    [ESC] <chan>   S  | select line       [BREAK]<line> S    |
| add channel	    [ESC] <chan>   A  | add line	  [BREAK]<line> A    |
| delete channel    [ESC] <chan>   D  |	delete line	  [BREAK]<line> D    |
| temp select chan  [ESC] <chan>   T  |	temp select line  [BREAK]<line> T    |
| hide channel	    [ESC]    H	      |	unhide channel	  [BREAK] H	     |
| sel audio w/page  [ESC] <chan>   U  |	audio w/o page	  [BREAK]<chan> U    |
| allow beeps	    [ESC]    B	      |	don't allow beeps [BREAK] B	     |
|									     |
| A null video <chan> means yours.  Thus, [ESC] S normalizes the switch.     |
| Channels:  0:31 DD users      42 Sierra camera   45  Lounge TV	     |
|	     32:37 QVS bits     43 Kintel camera   46  color synthesizer     |
|	     41 Cohu camera     44 cart receiver   47  video synthesizer     |
| Audio: 0 quiet + paging; 1 TV; 2 Helliwell tuner; 3 music tuner; 4 D-to-A; |
|	 5 beep; 6 AM tuner; 7 BH tuner; 10 voder; 16 HPM TV; 17 KSAN;	     |
|____________________________________________________________________________|

 * ------------------------------------------------------------------------------------------
 */                                       
