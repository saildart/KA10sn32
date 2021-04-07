// dkb.c
// 64 line display keyboard scanner
// nee: ka10_dkb.c:Stanford Microswitch scanner.

#include "ka10_defs.h"
#ifndef NUM_DEVS_DKB
#define NUM_DEVS_DKB 0
#endif

#if NUM_DEVS_DKB > 0 

#define SIM_KEY_EVENT int

#define DKB_DEVNUM        0310

#define DONE              010     /* Device has character */
#define SPW               020     /* Scanner in SPW mode */

#define VALID             010000
#define SPW_FLG           020000
#define CHAR              001777
#define SHFT              000100
#define TOP               000200
#define CTRL              000400
#define META              001000

#define STATUS            u3
#define DATA              u4
#define PIA               u5
#define LINE              u6

// forwards
t_stat dkb_devio(uint32 dev, uint64 *data);
int dkb_keyboard (int keyline, int keycode);
t_stat dkb_svc(UNIT *uptr);
t_stat dkb_reset(DEVICE *dptr);
t_stat dkb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *dkb_description (DEVICE *dptr);

// SIM structs
int dkb_kmod = 0;
DIB dkb_dib = { DKB_DEVNUM, 1, dkb_devio, NULL};
UNIT dkb_unit[] = {{UDATA (&dkb_svc, UNIT_IDLE, 0) },  { 0 }};
MTAB dkb_mod[] = {{0}};
DEVICE dkb_dev = {
  "DKB", dkb_unit, NULL, dkb_mod, 2, 10, 31, 1, 8, 8,
  NULL, NULL, dkb_reset,
  NULL, NULL, NULL, &dkb_dib, DEV_DEBUG | DEV_DISABLE, 0, dev_debug,
  NULL, NULL, &dkb_help, NULL, NULL, &dkb_description
};

// KA10 iots
t_stat dkb_devio(uint32 dev, uint64 *data) {
  UNIT    *uptr = &dkb_unit[0];
  switch(dev & 3) {
  case CONI:
    *data = (uint64)(uptr->STATUS|uptr->PIA);
    sim_debug(DEBUG_CONI, &dkb_dev, "DKB %03o CONI %06o\n", dev, (uint32)*data);
    break;
  case CONO:
    uptr->PIA = (int)(*data&7);
    if (*data & DONE)
      uptr->STATUS = 0;
    clr_interrupt(DKB_DEVNUM);
    sim_debug(DEBUG_CONO, &dkb_dev, "DKB %03o CONO %06o\n", dev, (uint32)*data);
    break;
  case DATAI:
    *data = (uint64)((uptr->LINE << 18) | (uptr->DATA));
    uptr->STATUS = 0;
    clr_interrupt(DKB_DEVNUM);
    sim_debug(DEBUG_DATAIO, &dkb_dev, "DKB %03o DATAI %06o\n", dev, (uint32)*data);
    break;
  case DATAO:
    if (*data & 010000) {
      uptr->STATUS |= SPW;
      uptr->LINE = (int)(*data & 077);
    }
    sim_debug(DEBUG_DATAIO, &dkb_dev, "DKB %03o DATAO %06o\n", dev, (uint32)*data);
    break;
  }
  return SCPE_OK;
}

// Event ( arrival of message from SAIL gui terminal server - recv kludge is in the iii.c code )
int dkb_keyboard (int keyline, int keycode)
{
  sim_debug(DEBUG_DETAIL, &dkb_dev, "DKB key %04o line %02o\n", keycode, keyline);
  dkb_unit[0].LINE = keyline;
  dkb_unit[0].DATA = keycode | VALID;
  dkb_unit[0].STATUS |= DONE;
  set_interrupt(DKB_DEVNUM, dkb_unit[0].PIA);
  return 0;
}
t_stat dkb_svc( UNIT *uptr)
{
  return SCPE_OK;
}
t_stat dkb_reset( DEVICE *dptr)
{
  return SCPE_OK;
}
t_stat dkb_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
  fprintf (stderr, "Stanford keyboard scanner for III and Data Disc display consoles\n");
  return SCPE_OK;
}
const char *dkb_description (DEVICE *dptr)
{
  return "Stanford 64 line keyboard scanner for display consoles";
}
#endif
