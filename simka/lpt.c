/* lpt.c Stanfo KA10 LPT: Data Products chain printer with custom character set 1966
nee ka10_lp.c: PDP-10 line printer simulator -*- mode:c; coding:utf-8 -*-

   Copyright (c) 2011-2020, Richard Cornwell

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

   Except as contained in this notice, the name of Richard Cornwell shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Richard Cornwell.
--
further editing `2020-05-17 bgbaumgart@mac.com'
*/

#include "ka10_defs.h"
#include <ctype.h>

// BgBaumgart KA10 tool kit
#include "../library/kayten.h"

#ifndef NUM_DEVS_LP
#define NUM_DEVS_LP 0
#endif

#if (NUM_DEVS_LP > 0)

#define LP_DEVNUM 0124
#define STATUS   u3
#define COL      u4
#define POS      u5
#define LINE     u6
#define MARGIN   6
#define  COLS_PER_LINE recsize
#define LINES_PER_PAGE capac

#define UNIT_V_CT    (UNIT_V_UF + 0)
#define UNIT_UC      (1 << UNIT_V_CT)
#define UNIT_UTF8    (2 << UNIT_V_CT)
#define UNIT_CT      (3 << UNIT_V_CT)

#define PI_DONE  000007
#define PI_ERROR 000070
#define DONE_FLG 000100
#define BUSY_FLG 000200
#define ERR_FLG  000400
#define CLR_LPT  002000
#define C96      002000
#define C128     004000
#define DEL_FLG  0100000

t_stat          lpt_devio(uint32 dev, uint64 *data);
t_stat          lpt_svc (UNIT *uptr);
t_stat          lpt_reset (DEVICE *dptr);
t_stat          lpt_attach (UNIT *uptr, CONST char *cptr);
t_stat          lpt_detach (UNIT *uptr);
t_stat          lpt_setlpp(UNIT *, int32, CONST char *, void *);
t_stat          lpt_getlpp(FILE *, UNIT *, int32, CONST void *);
t_stat          lpt_setcpl(UNIT *, int32, CONST char *, void *);
t_stat          lpt_getcpl(FILE *, UNIT *, int32, CONST void *);
t_stat          lpt_setdev(UNIT *, int32, CONST char *, void *);
t_stat          lpt_getdev(FILE *, UNIT *, int32, CONST void *);
t_stat          lpt_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char     *lpt_description (DEVICE *dptr);

char            lpt_buffer[134 * 3];
uint8           lpt_chbuf[5];             /* Read in Character buffers */

/* LPT data structures

   lpt_dev      LPT device descriptor
   lpt_unit     LPT unit descriptor
   lpt_reg      LPT register list
*/

DIB lpt_dib = { LP_DEVNUM, 1, &lpt_devio, NULL };

// see struct declaration in sim_defs.h circa line# 544, count carefully.
UNIT lpt_unit = {
  UDATA (&lpt_svc, UNIT_SEQ+UNIT_ATTABLE+UNIT_TEXT+UNIT_UTF8, 66), 100,
  0,0,0,0,NULL,NULL,0,0,NULL,120
    };

REG lpt_reg[] = {
    { DRDATA (STATUS, lpt_unit.STATUS, 18), PV_LEFT | REG_UNIT },
    { DRDATA (TIME, lpt_unit.wait, 24), PV_LEFT | REG_UNIT },
    { BRDATA(BUFF, lpt_buffer, 16, 8, sizeof(lpt_buffer)), REG_HRO},
    { BRDATA(CBUFF, lpt_chbuf, 16, 8, sizeof(lpt_chbuf)), REG_HRO},
    { NULL }
};

MTAB lpt_mod[] = {
    {UNIT_CT, 0, "Lower case", "LC", NULL},
    {UNIT_CT, UNIT_UC, "Upper case", "UC", NULL},
    {UNIT_CT, UNIT_UTF8, "UTF8 output", "UTF8", NULL},
    
    {MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "LINESPERPAGE", "LPP",
     &lpt_setlpp, &lpt_getlpp, NULL, "Number of lines per page, default 66 lines"},

    {MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "COLUMNSPERLINE", "CPL",
     &lpt_setcpl, &lpt_getcpl, NULL, "Number of columns per line, SAIL was 120 cols"},
    
    {MTAB_XTD|MTAB_VDV|MTAB_VALR, 0, "DEV", "DEV",
     &lpt_setdev, &lpt_getdev, NULL, "Device address of printer, default 124"},
    
    { 0 }
};

DEVICE lpt_dev = {
    "LPT", &lpt_unit, lpt_reg, lpt_mod,
    1, 10, 31, 1, 8, 8,
    NULL, NULL, &lpt_reset,
    NULL, &lpt_attach, &lpt_detach,
    &lpt_dib, DEV_DISABLE | DEV_DEBUG, 0, dev_debug,
    NULL, NULL, &lpt_help, NULL, NULL, &lpt_description
};

t_stat lpt_devio(uint32 dev, uint64 *data) {
    UNIT *uptr = &lpt_unit;
    switch(dev & 3) {
    case CONI:
         *data = uptr->STATUS & (PI_DONE|PI_ERROR|DONE_FLG|BUSY_FLG|ERR_FLG);
         if ((uptr->flags & UNIT_UC) == 0)
             *data |= C96;
         if ((uptr->flags & UNIT_UTF8) != 0)
             *data |= C128;
         if ((uptr->flags & UNIT_ATT) == 0)
             *data |= ERR_FLG;
         sim_debug(DEBUG_CONI, &lpt_dev, "LP CONI %012llo PC=%06o\n", *data, PC);
         break;
    case CONO:
         clr_interrupt(dev);
         sim_debug(DEBUG_CONO, &lpt_dev, "LP CONO %012llo PC=%06o\n", *data, PC);
         uptr->STATUS &= ~0777;
         uptr->STATUS |= ((PI_DONE|PI_ERROR|DONE_FLG|BUSY_FLG|CLR_LPT) & *data);
         if (*data & CLR_LPT) {
             uptr->STATUS &= ~DONE_FLG;
             uptr->STATUS |= BUSY_FLG;
             sim_activate (&lpt_unit, lpt_unit.wait);
         }
         if ((uptr->flags & UNIT_ATT) == 0) {
             uptr->STATUS |= ERR_FLG;
             set_interrupt(dev, (uptr->STATUS >> 3));
         }
         if ((uptr->STATUS & DONE_FLG) != 0)
             set_interrupt(dev, uptr->STATUS);
         break;
    case DATAO:
         if ((uptr->STATUS & DONE_FLG) != 0) {
             int i, j;
             for (j = 0, i = 29; i > 0; i-=7)
                 lpt_chbuf[j++] = ((uint8)(*data >> i)) & 0x7f;
             uptr->STATUS &= ~DONE_FLG;
             uptr->STATUS |= BUSY_FLG;
             clr_interrupt(dev);
             sim_activate (&lpt_unit, lpt_unit.wait);
             sim_debug(DEBUG_DATAIO, &lpt_dev, "LP DATO %012llo PC=%06o\n", *data, PC);
         }
         break;
    case DATAI:
         *data = 0;
         break;
    }
    return SCPE_OK;
}

void
lpt_printline(UNIT *uptr, int nl) {
    int   trim = 0;
    /* Trim off trailing blanks */
    while (uptr->COL >= 0 && lpt_buffer[uptr->POS - 1] == ' ') {
         uptr->COL--;
         uptr->POS--;
         trim = 1;
    }
    lpt_buffer[uptr->POS] = '\0';
    sim_debug(DEBUG_DETAIL, &lpt_dev, "LP output %d %d [%s]\n", uptr->COL, nl, lpt_buffer);
    /* Stick a carraige return and linefeed as needed */
    if (uptr->COL != 0 || trim)
        lpt_buffer[uptr->POS++] = '\r';
    if (nl != 0) {
        lpt_buffer[uptr->POS++] = '\n';
        uptr->LINE++;
    }
    if (nl > 0 && uptr->LINE >= ((int32)uptr->LINES_PER_PAGE - MARGIN)) {
        lpt_buffer[uptr->POS++] = '\f';
        uptr->LINE = 0;
    } else if (nl < 0 && uptr->LINE >= (int32)uptr->LINES_PER_PAGE) {
        uptr->LINE = 0;
    }       
    sim_fwrite(&lpt_buffer, 1, uptr->POS, uptr->fileref);
    uptr->pos += uptr->POS;
    uptr->COL = 0;
    uptr->POS = 0;
    if (ferror (uptr->fileref)) {                           /* error? */
        perror ("LPT I/O error");
        clearerr (uptr->fileref);
        uptr->STATUS |= ERR_FLG;
        set_interrupt(LP_DEVNUM, (uptr->STATUS >> 3));
        return;
    }
    return;
}
/*
  The Stanford LPT Data Products chain printer hardware
  has a customized special character set with glyph codes
  that are NOT at the SAIL7 code positions ! 

  So we must undo eight character code conversions
  which WAITS does in the LPTSER device driver.

  See the LPTCTB: table in WAITS 1974 source code file 
  https://www.saildart.org/LPTSER[J18,SYS] at line #362

   0030 into 0137       _ on LPT is ←
   0032 into 0136       ~ on LPT is ↑
   0100 into 0140       @ on LPT is `
   0134 into 0032       \ on LPT is ~
   0137 into 0030       ← on LPT is _
   0140 into 0100       ` on LPT is @
   0175 into 0176       § on LPT is }
   0176 into 0174       } on LPT is |
*/
uint16 Stanford_utf8_code[128] = {
      0x00b7,           /* 000 ·        middle dot      */
      0x2193,           /* 001 ↓        down arrow      */
      0x03b1,           /* 002 α        alpha           */
      0x03b2,           /* 003 β        beta            */
      0x2227,           /* 004 ∧        boolean AND     */
      0x00ac,           /* 005 ¬        boolean NOT     */
      0x03b5,           /* 006 ε        epsilon         */
      0x03c0,           /* 007 π        pi              */
      //      
      0x03bb,           /* 010 λ        lambda */
      0x03b3,           /* 011 γ        small gamma */
      0x03b4,           /* 012 δ        small delta */
      0x222b,           /* 013 ∫        integral */
      0x00b1,           /* 014 ±        plus-minus */
      0x2295,           /* 015 ⊕        circle-plus */
      0x221e,           /* 016 ∞        infinity */
      0x2202,           /* 017 ∂        partial derivative */
      //      
      0x2282,           /* 020 ⊂        left horse shoe, contains,   Subset of */
      0x2283,           /* 021 ⊃        right horse shoe, implies, Superset of */
      0x2229,           /* 022 ∩        Intersection */
      0x222a,           /* 023 ∪        union */
      0x2200,           /* 024 ∀        For-all */
      0x2203,           /* 025 ∃        Exists */
      0x2297,           /* 026 ⊗        circle-times */
      0x2194,           /* 027 ↔        horizontal double arrow */
      //      
      0x2190, // 0x005f,/* 030 _        underscore                              on LPT is ← */
      0x2192,           /* 031 →        right arrow */
      0x2191, // 0x007e,/* 032 ~        tilde                                   on LPT is ↑ */
      0x2260,           /* 033 ≠        not equal */
      0x2264,           /* 034 ≤        Less than or equal */
      0x2265,           /* 035 ≥        Greater than or equal */
      0x2261,           /* 036 ≡        equivalence */
      0x2228,           /* 037 ∨        boolean OR */
      //
      040,041,042,043,044,045,046,047, // ␣ ! \ # $ % & ' 
      050,051,052,053,054,055,056,057, // ( ) * + , - . /
      060,061,062,063,064,065,066,067, // 0 1 2 3 4 5 6 7
      070,071,072,073,074,075,076,077, // 8 9 : ; < = > ?
      // @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]↑←
      0140, // 0100,                                                            on LPT is `
      0101,0102,0103,0104,0105,0106,0107,
      0110,0111,0112,0113,0114,0115,0116,0117,
      0120,0121,0122,0123,0124,0125,0126,0127,
      0130,0131,0132,0133,
      0x007e, // 0134,     // back slash                                        on LPT is ~
      0135,
      0x2191,              // ↑ SAIL7 at 0136 up arrow    ASCII ^ caret
      0137,   // 0x2190,   // ← SAIL7 at 0137 left arrow  ASCII _ underscore    on LPT is _
      //  abcdefghijklmnopqrstuvwxyz{|§}
      0100, //  0140,                                                           on LPT is @
      0141,0142,0143,0144,0145,0146,0147,
      0150,0151,0152,0153,0154,0155,0156,0157,
      0160,0161,0162,0163,0164,0165,0166,0167,
      0170, // 0170
      0171, // 0171
      0172, // 0172
      '{',  // 0173
      '|',  // 0174
      0176, // 0175     0xA7,                                             on LPT is }
      0174, // 0176     '}',                                              on LPT is |
      '\\'  // 0177 The physical LPT hardware eats 0177 as an escape, Two 0177 print a back slash.
 };

#ifndef _KAYTEN_H
// BgBaumgart LPT table from kayten library.
// Array of 128 unicode characters as old fashion 'C' strings.
// ( NOT used yet here inside SIMKA code )
char *SAIL7CODE_lpt[]={
  "·", // LPT_000 center dot U00B7
  "↓",  "α",  "β",  "∧",  "¬",  "ε",  "π", "λ",         // 001 to 010
  "γ", // LPT_011 TB prints U03B3 small gamma
  "δ", // LPT_012 LF prints U03B4 small delta
  "∫", // LPT_013 VT prints U222B integral sign
  "±", // LPT_014 FF prints U00B1 plus-minus sign
  "⊕", // LPT_015 CR prints U2295 circle-plus sign
  "∞",   "∂", // 016, 017 
  "⊂",   "⊃",   "∩",   "∪",   "∀",   "∃",   "⊗",   "↔", //     027 
  "_",   "→",   "~",   "≠",   "≤",   "≥",   "≡",   "∨", //     037 
  " ","!","\"","#","$","%","&","'",     // 040 to 047
  "(",")","*","+",",","-",".","/",      // 050 to 057
  "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?", // 060 to 077
  "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", // 0100 to 0132
  "[",   "\\",  "]",   "↑",   "←",   "`", // 0140
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", // 0141 to 0172
  "{",   "|",   "§",  "}",
  "\\", // The Stanford LPT prints backslash at code position 0177
};
#endif

/* Unit service */
void
lpt_output(UNIT *uptr, char c) {
  uint16 u;
  if (c == 0)
    return;
  if (uptr->COL == uptr->COLS_PER_LINE){
    lpt_printline(uptr, 1);
    return;
  }
  // convert SAIL7 7-bit into UTF8 up to three bytes append to LPT buffer
  u = Stanford_utf8_code[c & 0177];
  if (u > 0x7ff) {
    lpt_buffer[uptr->POS++] = 0xe0 + ((u >> 12) & 0xf);
    lpt_buffer[uptr->POS++] = 0x80 + ((u >> 6) & 0x3f);
    lpt_buffer[uptr->POS++] = 0x80 + (u & 0x3f);
  } else if (u > 0x7f) {
    lpt_buffer[uptr->POS++] = 0xc0 + ((u >> 6) & 0x3f);
    lpt_buffer[uptr->POS++] = 0x80 + (u & 0x3f);
  } else {
    lpt_buffer[uptr->POS++] = u & 0x7f;
  }
  uptr->COL++;
}

// Process one PDP10 word into the LPT buffer
t_stat lpt_svc (UNIT *uptr)
{
  char    c;
  int     pos;
  int     cpos;
  //
  if ((uptr->flags & DONE_FLG) != 0) {
    set_interrupt(LP_DEVNUM, uptr->STATUS);
    return SCPE_OK;
  }
  if ((uptr->flags & UNIT_ATT) == 0) {
    uptr->STATUS |= ERR_FLG;
    set_interrupt(LP_DEVNUM, (uptr->STATUS >> 3));
    return SCPE_OK;
  }
  if (uptr->STATUS & CLR_LPT) {
    for (pos = 0; pos < uptr->COL; lpt_buffer[pos++] = ' ');
    uptr->POS = uptr->COL;
    lpt_printline(uptr, 0);
    uptr->STATUS &= ~(DEL_FLG|ERR_FLG|BUSY_FLG|CLR_LPT);
    uptr->STATUS |= DONE_FLG;
    set_interrupt(LP_DEVNUM, uptr->STATUS);
    return SCPE_OK;
  }
  for (cpos = 0; cpos < 5; cpos++) {
    c = lpt_chbuf[cpos];
    if (uptr->STATUS & DEL_FLG) {
      lpt_output(uptr, c);
      uptr->STATUS &= ~DEL_FLG;
    } else if (c == 0177) {  /* Check for DEL Character */
      uptr->STATUS |= DEL_FLG;
    } else if (c < 040) { /* Control character */
      switch(c) {
      case 011:     /* Horizontal tab, space to 8'th column */
        lpt_output(uptr, ' ');
        while ((uptr->COL & 07) != 0)
          lpt_output(uptr, ' ');
        break;
      case 015:     /* Carriage return, print line */
        lpt_printline(uptr, 0);
        break;
      case 012:     /* Line feed, print line, space one line */
        lpt_printline(uptr, 1);
        break;
      case 014:     /* Form feed, skip to top of page */
        lpt_printline(uptr, 0);
        sim_fwrite("\014", 1, 1, uptr->fileref);
        uptr->pos++;
        uptr->LINE = 0;
        break;
      case 013:     /* Vertical tab, Skip mod 20 */
        lpt_printline(uptr, 1);
        while((uptr->LINE % 20) != 0) {
          sim_fwrite("\r\n", 1, 2, uptr->fileref);
          uptr->pos+=2;
          uptr->LINE++;
        }
        break;
#if 0
        /* These cases might exist at WAITS User level IO mode 100 but NOT at hardware device level */
      case 020:     /* Skip half page */
        lpt_printline(uptr, 1);
        while((uptr->LINE % 30) != 0) {
          sim_fwrite("\r\n", 1, 2, uptr->fileref);
          uptr->pos+=2;
          uptr->LINE++;
        }
        break;
      case 021:     /* Skip even lines */
        lpt_printline(uptr, 1);
        while((uptr->LINE % 2) != 0) {
          sim_fwrite("\r\n", 1, 2, uptr->fileref);
          uptr->pos+=2;
          uptr->LINE++;
        }
        break;
      case 022:     /* Skip triple lines */
        lpt_printline(uptr, 1);
        while((uptr->LINE % 3) != 0) {
          sim_fwrite("\r\n", 1, 2, uptr->fileref);
          uptr->pos+=2;
          uptr->LINE++;
        }
        break;
      case 023:     /* Skip one line */
        lpt_printline(uptr, -1);
        break;
#endif
      default: /* Stanford special SAIL7 character */
        lpt_output(uptr, c);
        break;
      }
    } else {
      lpt_output(uptr, c);
    }
  }
  uptr->STATUS &= ~BUSY_FLG;
  uptr->STATUS |= DONE_FLG;
  set_interrupt(LP_DEVNUM, uptr->STATUS);
  return SCPE_OK;
}

/* Reset routine */

t_stat lpt_reset (DEVICE *dptr)
{
    UNIT *uptr = &lpt_unit;
    uptr->POS = 0;
    uptr->COL = 0;
    uptr->LINE = 1;
    uptr->STATUS = DONE_FLG;

    clr_interrupt(LP_DEVNUM);
    sim_cancel (&lpt_unit);      /* deactivate unit */
    return SCPE_OK;
}

/* Attach routine */

t_stat lpt_attach (UNIT *uptr, CONST char *cptr)
{
    t_stat reason;

    reason = attach_unit (uptr, cptr);
    if (sim_switches & SIM_SW_REST)
        return reason;
    uptr->STATUS &= ~ERR_FLG;
    clr_interrupt(LP_DEVNUM);
    return reason;
}

/* Detach routine */

t_stat lpt_detach (UNIT *uptr)
{
    uptr->STATUS |= ERR_FLG;
    set_interrupt(LP_DEVNUM, uptr->STATUS >> 3);
    return detach_unit (uptr);
}

/*
 * Line printer routines
 */

// Lines per Page
t_stat
lpt_setlpp(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    t_value   i;
    t_stat    r;
    if (cptr == NULL)
        return SCPE_ARG;
    if (uptr == NULL)
        return SCPE_IERR;
    i = get_uint (cptr, 10, 100, &r);
    if (r != SCPE_OK)
        return SCPE_ARG;
    uptr->LINES_PER_PAGE = (t_addr)i;
    uptr->LINE = 0;
    return SCPE_OK;
}

t_stat
lpt_getlpp(FILE *st, UNIT *uptr, int32 v, CONST void *desc)
{
    if (uptr == NULL)
        return SCPE_IERR;
    fprintf(st, "linesperpage=%d", uptr->LINES_PER_PAGE);
    return SCPE_OK;
}

// Columns per Line
t_stat
lpt_setcpl(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    t_value   i;
    t_stat    r;
    if (cptr == NULL)
        return SCPE_ARG;
    if (uptr == NULL)
        return SCPE_IERR;
    i = get_uint (cptr, 10, 1024, &r); // max 1024. columns per line
    if (r != SCPE_OK)
        return SCPE_ARG;
    uptr->COLS_PER_LINE = (t_addr)i;
    uptr->COL = 0;
    return SCPE_OK;
}

t_stat
lpt_getcpl(FILE *st, UNIT *uptr, int32 v, CONST void *desc)
{
    if (uptr == NULL)
        return SCPE_IERR;
    fprintf(st, "columnsperline=%d", uptr->COLS_PER_LINE);
    return SCPE_OK;
}

t_stat
lpt_setdev(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
    t_value   i;
    t_stat    r;
    if (cptr == NULL)
        return SCPE_ARG;
    if (uptr == NULL)
        return SCPE_IERR;
    i = get_uint (cptr, 8, 01000, &r);
    if (r != SCPE_OK)
        return SCPE_ARG;
    if ((i & 03) != 0)
        return SCPE_ARG;
    lpt_dib.dev_num = (int)i;
    return SCPE_OK;
}

t_stat
lpt_getdev(FILE *st, UNIT *uptr, int32 v, CONST void *desc)
{
    if (uptr == NULL)
        return SCPE_IERR;
    fprintf(st, "dev=%03o", lpt_dib.dev_num);
    return SCPE_OK;
}

t_stat lpt_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
fprintf (st, "Stanford Data Products Line Printer (LPT)\n\n");
fprintf (st, "The line printer (LPT) writes data to a disk file.  The POS register specifies\n");
fprintf (st, "the number of the next data item to be written.  Thus, by changing POS, the\n");
fprintf (st, "user can backspace or advance the printer.\n");
fprintf (st, "The Line printer can be configured to any number of lines per page with the:\n");
fprintf (st, "        sim> SET %s0 LINESPERPAGE=n\n\n", dptr->name);
fprintf (st, "The default is 66 lines per page.\n\n");
fprintf (st, "The device address of the Line printer can be changed\n");
fprintf (st, "        sim> SET %s0 DEV=n\n\n", dptr->name);
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
return SCPE_OK;
}

const char *lpt_description (DEVICE *dptr)
{
    return "Stanford LPT Dataproducts chain line printer" ;
}

#endif
