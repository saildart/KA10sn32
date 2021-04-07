/* 440.xgp.c: Xerox Graphics Printer.

   Copyright (c) 2018, Lars Brinkhoff
   Copyright (c) 2020, Bruce Baumgart

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   RICHARD CORNWELL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "ka10_defs.h"

#ifndef NUM_DEVS_XGP
#define NUM_DEVS_XGP 0
#endif

#if (NUM_DEVS_XGP > 0)

#define XGP_DEVNUM 0440
#define XGP_OFF (1 << DEV_V_UF)

#define PIA_CH          u3
#define PIA_FLG         07
#define CLK_IRQ         010

t_stat         xgp_devio(uint32 dev, uint64 *data);
const char     *xgp_description (DEVICE *dptr);
t_stat         xgp_srv(UNIT *uptr);
t_stat         xgp_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         xgp_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         xgp_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc);

UNIT xgp_unit[] = {
    {UDATA(xgp_srv, UNIT_IDLE|UNIT_DISABLE, 0)},  /* 0 */
};
DIB xgp_dib = {XGP_DEVNUM, 1, &xgp_devio, NULL};

MTAB xgp_mod[] = {
    { MTAB_VDV, 0, "ON", "ON", xgp_set_on, xgp_show_on },
    { MTAB_VDV, XGP_OFF, NULL, "OFF", xgp_set_off },
    { 0 }
    };

DEVICE xgp_dev = {
    "XGP", xgp_unit, NULL, xgp_mod, 1, 8, 0, 1, 8, 36,
    NULL, NULL, NULL, NULL, NULL, NULL,
    &xgp_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG, 0, NULL,
    NULL, NULL, NULL, NULL, NULL, &xgp_description
};

t_stat xgp_devio(uint32 dev, uint64 *data)
{
    DEVICE *dptr = &xgp_dev;
    switch(dev & 3) {
    case DATAI:
      *data = 0;
      break;
    case CONI:
      *data = 0;
      break;
    case CONO:
        xgp_unit[0].PIA_CH &= ~(PIA_FLG);
        xgp_unit[0].PIA_CH |= (int32)(*data & PIA_FLG);
        break;
    default:
        break;
    }
    return SCPE_OK;
}

t_stat
xgp_srv(UNIT * uptr)
{
    return SCPE_OK;
}

const char *xgp_description (DEVICE *dptr)
{
    return "Stanford Xerox Graphics Printer on the KA10 bus for the reënactment";
}

t_stat xgp_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    DEVICE *dptr = &xgp_dev;
    dptr->flags &= ~XGP_OFF;
    return SCPE_OK;
}

t_stat xgp_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    DEVICE *dptr = &xgp_dev;
    dptr->flags |= XGP_OFF;
    return SCPE_OK;
}

t_stat xgp_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
    DEVICE *dptr = &xgp_dev;
    fprintf (st, "%s", (dptr->flags & XGP_OFF) ? "off" : "on");
    return SCPE_OK;
}

#endif
