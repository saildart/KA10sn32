#include "ka10_defs.h"

#define TV_DEVNUM 0404
#define TV_OFF (1 << DEV_V_UF)
#define PIA_CH          u3
#define PIA_FLG         07
#define CLK_IRQ         010

t_stat         tv_devio(uint32 dev, uint64 *data);
const char     *tv_description (DEVICE *dptr);
t_stat         tv_srv(UNIT *uptr);
t_stat         tv_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         tv_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         tv_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc);

/* 20-bits to fly five 2-D space ships : 
        Rotate ccw.LEFT
        Rotate cw.RIGHT
        Thrust rocket motoe on
        Fire Torpedo
 */
int tv_spacewar_buttons=0; 

UNIT tv_unit[] = {
  {UDATA(tv_srv, UNIT_IDLE|UNIT_DISABLE, 0)},  /* 0 */
};
DIB tv_dib = {TV_DEVNUM, 1, &tv_devio, NULL};
MTAB tv_mod[] = {
  { MTAB_VDV, 0, "ON", "ON", tv_set_on, tv_show_on },
  { MTAB_VDV, TV_OFF, NULL, "OFF", tv_set_off },
  { 0 }
};
DEVICE tv_dev = {
  "TV", tv_unit, NULL, tv_mod, 1, 8, 0, 1, 8, 36,
  NULL, NULL, NULL, NULL, NULL, NULL,
  &tv_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG, 0, NULL,
  NULL, NULL, NULL, NULL, NULL, &tv_description
};
t_stat tv_devio(uint32 dev, uint64 *data)
{
  DEVICE *dptr = &tv_dev;
  switch(dev & 3) {
  case DATAI:
    *data = 0;
    break;
  case CONI:
    *data = tv_spacewar_buttons;
    break;
  case CONO:
    tv_unit[0].PIA_CH &= ~(PIA_FLG);
    tv_unit[0].PIA_CH |= (int32)(*data & PIA_FLG);
    break;
  default:
    break;
  }
  return SCPE_OK;
}
t_stat
tv_srv(UNIT * uptr)
{
  return SCPE_OK;
}
t_stat tv_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &tv_dev;
  dptr->flags &= ~TV_OFF;
  return SCPE_OK;
}
t_stat tv_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &tv_dev;
  dptr->flags |= TV_OFF;
  return SCPE_OK;
}
t_stat tv_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
  DEVICE *dptr = &tv_dev;
  fprintf (st, "%s", (dptr->flags & TV_OFF) ? "off" : "on");
  return SCPE_OK;
}
const char *tv_description (DEVICE *dptr)
{
  return "Stanford A.I.Lab black-n-white Television input and spacewar buttons";
}
