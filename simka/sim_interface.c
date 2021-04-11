/* simka/sim_interface.c
-*- mode:C;coding:UTF8; -*-
 * ------------------------------------------------------------------------------------
 Interface between the SIMH (and SIMS) control program SCP
 and the Stanford A.I. Lab dual processor PDP-10 KA sn#32 and PDP-6 sn#16
 re-enacted here as a solo processor faux PDP-10 KA WAITS with two user memory segments

 hypothetical uname print out
 # machine: PDP10-KA-TWOSEG  SAIL-WAITS-1974
 uname -m
        ka10_36
 # operating system
 uname -o
 # kernel release
 * ------------------------------------------------------------------------------------
*/
#include "ka10_defs.h"
#include <ctype.h>
/* SCP data structures and interface routines

   sim_name             simulator name string
   sim_PC               pointer to saved PC register descriptor
   sim_emax             number of words for examine
   sim_devices          array of pointers to simulated devices
   sim_stop_messages    array of pointers to stop messages
   sim_load             binary loader
*/
char sim_name[]="KA-10";
extern REG cpu_reg[];
REG *sim_PC = &cpu_reg[0];
int32 sim_emax = 1;
/*
  ; J17 DEVICE NAMES and I/O bus device number
  XD APR,       000     ; Arithmetic Processor
  XD PI,        004     ; Priority Interrup
  XD IOP,       010	; I/O Processor (167)

  XD PTP,       100     ; paper tape punch
  XD PTR,	104     ; paper tape reader
  XD CTY,	120     ; console TTY
  XD LPT,	124     ; Line Printer

  XD DC,	200     ; data control (136) for MTC and DTC    ; DTCSER
  XD DCB,	204	; data control (136) for ADC            ; ADCSER and ADSER
  XD DTC,	210     ; Dec Tape control                      ; DTCSER
  XD DTS,	214     ; Dec Tape status
  XD MTC,	220     ; Mag Tape control   MTC                ; MTCSER
  XD MTS,	224     ; Mag Tape status

  XD ADC,	240	; new AD converter (PDP-6)
  XD DAC,	244	; new DA converter (PDP-6)
  XD DCSA,	300     ; 8-line TT scanner
  XD DCSB,	304     ; 8-line TT scanner

  XD DKB,	310	; Microswitch Keyboard Scanner          ; TTYSER
  XD VDS,	340	; Earnest Video Switch                  ; DPYSER
  XD IMP,	400	; ARPA Intergallactic Massage Processor ; IMPSER
  XD TV,	404	; TV Camera interface with the Spacewar Switches ; TVSER
  XD AD,	424	; A/D Converter

  XD DPY,	430	; III vector display, 6 consoles.
  XD XGP,	440	; Xerox Graphics Printer                ; XGPSER
  XD DSK,	444	; Librascope disk interface

  XD PMP,	500	; P.Petit's IBM Channel
  XD IBM,	504	; P.Petit'S IBM Channel
  XD DDD,	510	; Data Disk, raster display, 32 screens
  XD PCLK,	730	; Phil Petit'S calendar clock crock ; UUOCON
  XD AS,	774	; Audio Switch
*/

DEVICE *sim_devices[] = {
  &cpu_dev, // 000 004 010      APR PI IOP
  &ptp_dev, // 100     PTP      Paper Tape Punch
  &ptr_dev, // 104     PTR      Paper Tape Reader
  &cty_dev, // 120     CTY      Console TTY
  &lpt_dev, // 123     LPT      Line Printer
  &xgp_dev, // 440     XGP      Xerox Graphics Printer
  &dct_dev, // 200 204          Data Channels
  &dtc_dev, // 210 214          DEC Tape
  &mtc_dev, // 220 224          Mag Tape
  &dsk_dev, // 444     DSK      Librascope large-size small-capacity fixed head disk
  &pmp_dev, // 500 504 IBM+PMP  IBM-3330 "pizza oven" with 4 removable 200 MB disk drives in 1974
  &dkb_dev, // 310     DKB      Microswitch Keyboard Scanner
  &iii_dev, // 430     III      Triple-III vector display
  &ddd_dev, // 510     DDD      Data Disc bit raster display
  &vds_dev, // 340     VDS      Lester Earnest Video-Display-Switch src(32,7,null) → dst(64)
  &tv_dev,  // 404      TV      Television Camera Interface, 7 sources and the spacewar buttons
  &pclk_dev,// 730     PCLK     Phil Petit Calendar Clock Crock
  NULL
};

const char *sim_stop_messages[] = {
  "Unknown error",
  "HALT instruction",
  "Breakpoint",
  "Invalid access",
};

// Simulator debug controls
DEBTAB dev_debug[] = {
  {"CMD",       DEBUG_CMD,      "Show command execution to devices"},
  {"DATA",      DEBUG_DATA,     "Show data transfers"},
  {"DETAIL",    DEBUG_DETAIL,   "Show details about device"},
  {"EXP",       DEBUG_EXP,      "Show exception information"},
  {"CONI",      DEBUG_CONI,     "Show coni instructions"},
  {"CONO",      DEBUG_CONO,     "Show coni instructions"},
  {"DATAIO",    DEBUG_DATAIO,   "Show datai and datao instructions"},
  // BgB verbosity
  {"BULL", DEBUG_BULL,"Show BgBaumgart bulletins"},
  {"DIAG", DEBUG_DIAG,"Show BgBaumgart diagnostics"},
  {"BGB",  DEBUG_BGB, "Show BgBaumgart sentinels"},
  {"MS",   DEBUG_MS,  "Show BgBaumgart milestones"},
  {"WP",   DEBUG_WP,  "Show BgBaumgart watchpoints"},
  {0, 0}
};

#define EXE_DIR 01776
#define EXE_VEC 01775
#define EXE_PDV 01774
#define EXE_END 01777
/*
  SAIL-WAITS octal dump loader.
  Simple octal ASCII file, one word per line.
  Lines which don't start with 0-7 are ignored.
  Everything after the octal constant is also ignored.
*/
t_stat load_dmp (FILE *fileref)
{
  char    buffer[100];
  char    *p;
  uint32  addr = 074;
  uint64  data;
  int     high = 0;
  while (fgets((char *)buffer, 80, fileref) != 0) {
    p = (char *)buffer;
    while (*p >= '0' && *p <= '7') {
      data = 0;
      while (*p >= '0' && *p <= '7') {
        data = (data << 3) + *p - '0';
        p++;
      }
      if (addr == 0135 && data != 0)
        high = (uint32)(data & RMASK);
      if (high != 0 && high == addr) {
        addr = 0400000;
        high = 0;
      }
      M[addr++] = data;
      if (*p == ' ' || *p == '\t')
        p++;
    }
  }
  return SCPE_OK;
}
t_stat save_dmp (FILE *fileref)
{
  uint32  addr = 074;           // The SAIL-WAITS dmp file format offset is 60. words
  uint64  data;
  while( addr < 0230000 ){      // The top of SYSTEM.DMP[J17,SYS] is forever 76K words
      data = M[addr++];
      fprintf( fileref, "%012llo\n", data );
  }
  return SCPE_OK;
}
// KA10 SAIL-WAITS save or load octal DMP file
t_stat sim_load (FILE *fileref, CONST char *cptr, CONST char *fnam, int flag)
{
  return flag ? save_dmp (fileref) : load_dmp (fileref);
}

// symbol table
#define I_V_FL 39
#define I_M_FL 03
#define I_AC 000000000000000
#define I_OP 010000000000000
#define I_IO 020000000000000
#define I_V_AC 00
#define I_V_OP 01
#define I_V_IO 02
static const uint64 masks[] = {  0777000000000, 0777740000000,  0700340000000, 0777777777777 };
static const char *opcode[] = {
  // AC field defines opcode
  "PORTAL", "JRSTF", "HALT",
  "XJRSTF", "XJEN", "XPCW",
  "JEN", "SFM", "XJRST", "IBP",
  "JFOV", "JCRY1", "JCRY0", "JCRY", "JOV",  
 
 // 000 undefined opcodes
  "ZERO", "U001", "U002", "U003", "U004", "U005", "U006", "U007",
  "U010", "U011", "U012", "U013", "U014", "U015", "U016", "U017",
  "U020", "U021", "U022", "U023", "U024", "U025", "U026", "U027",
  "U030", "U031", "U032", "U033", "U034", "U035", "U036", "U037",
  //
  "CALL",   "INITI",  "UO42",   "UO43",   "UO44", "UO45",   "UO46",   "CALLI",
  "OPEN",   "TTCALL", "UO52",   "UO53",   "UO54", "RENAME", "IN",     "OUT",
  "SETSTS", "STATO",  "STATUS", "GETSTS", "INBUF","OUTBUF", "INPUT",  "OUTPUT",
  "CLOSE",  "RELEAS", "MTAPE",  "UGETF",  "USETI", "USETO", "LOOKUP", "ENTER",
  
  // 100
  "U100","U101","U102","U103", "U104","U105","U106","U107",
  "U110","U111","U112","U113", "U114","U115","U116","U117",
  "U120","U121","U122","U123", "U124","U125","U126","U127",
  "UFA", "DFN", "FSC", "IBP",  "ILDB","LDB", "IDPB","DPB",
  
  // 140 float
  "FAD", "FADL", "FADM", "FADB", "FADR", "FADRL", "FADRM", "FADRB",
  "FSB", "FSBL", "FSBM", "FSBB", "FSBR", "FSBRL", "FSBRM", "FSBRB",
  "FMP", "FMPL", "FMPM", "FMPB", "FMPR", "FMPRL", "FMPRM", "FMPRB",
  "FDV", "FDVL", "FDVM", "FDVB", "FDVR", "FDVRL", "FDVRM", "FDVRB",
  
  // 200 FWT
  "MOVE", "MOVEI", "MOVEM", "MOVES", "MOVS", "MOVSI", "MOVSM", "MOVSS",
  "MOVN", "MOVNI", "MOVNM", "MOVNS", "MOVM", "MOVMI", "MOVMM", "MOVMS",
  "IMUL", "IMULI", "IMULM", "IMULB", "MUL", "MULI", "MULM", "MULB",
  "IDIV", "IDIVI", "IDIVM", "IDIVB", "DIV", "DIVI", "DIVM", "DIVB",
  //
  "ASH", "ROT", "LSH", "JFFO", "ASHC", "ROTC", "LSHC", "KAFIX",
  "EXCH", "BLT", "AOBJP", "AOBJN", "JRST", "JFCL", "XCT", "CONS",
  "PUSHJ", "PUSH", "POP", "POPJ", "JSR", "JSP", "JSA", "JRA",
  "ADD", "ADDI", "ADDM", "ADDB", "SUB", "SUBI", "SUBM", "SUBB",
  
  // 300 CAI
  "CAI", "CAIL", "CAIE", "CAILE", "CAIA", "CAIGE", "CAIN", "CAIG",
  "CAM", "CAML", "CAME", "CAMLE", "CAMA", "CAMGE", "CAMN", "CAMG",
  "JUMP", "JUMPL", "JUMPE", "JUMPLE", "JUMPA", "JUMPGE", "JUMPN", "JUMPG",
  "SKIP", "SKIPL", "SKIPE", "SKIPLE", "SKIPA", "SKIPGE", "SKIPN", "SKIPG",
  "AOJ", "AOJL", "AOJE", "AOJLE", "AOJA", "AOJGE", "AOJN", "AOJG",
  "AOS", "AOSL", "AOSE", "AOSLE", "AOSA", "AOSGE", "AOSN", "AOSG",
  "SOJ", "SOJL", "SOJE", "SOJLE", "SOJA", "SOJGE", "SOJN", "SOJG",
  "SOS", "SOSL", "SOSE", "SOSLE", "SOSA", "SOSGE", "SOSN", "SOSG",
  
  // 400 BOOL
  "SETZ", "SETZI", "SETZM", "SETZB", "AND", "ANDI", "ANDM", "ANDB",
  "ANDCA", "ANDCAI", "ANDCAM", "ANDCAB", "SETM", "SETMI", "SETMM", "SETMB",
  "ANDCM", "ANDCMI", "ANDCMM", "ANDCMB", "SETA", "SETAI", "SETAM", "SETAB",
  "XOR", "XORI", "XORM", "XORB", "IOR", "IORI", "IORM", "IORB",
  "ANDCB", "ANDCBI", "ANDCBM", "ANDCBB", "EQV", "EQVI", "EQVM", "EQVB",
  "SETCA", "SETCAI", "SETCAM", "SETCAB", "ORCA", "ORCAI", "ORCAM", "ORCAB",
  "SETCM", "SETCMI", "SETCMM", "SETCMB", "ORCM", "ORCMI", "ORCMM", "ORCMB",
  "ORCB", "ORCBI", "ORCBM", "ORCBB", "SETO", "SETOI", "SETOM", "SETOB",
  
  // 500 HWT
  "HLL", "HLLI", "HLLM", "HLLS", "HRL", "HRLI", "HRLM", "HRLS",
  "HLLZ", "HLLZI", "HLLZM", "HLLZS", "HRLZ", "HRLZI", "HRLZM", "HRLZS",
  "HLLO", "HLLOI", "HLLOM", "HLLOS", "HRLO", "HRLOI", "HRLOM", "HRLOS",
  "HLLE", "HLLEI", "HLLEM", "HLLES", "HRLE", "HRLEI", "HRLEM", "HRLES",
  "HRR", "HRRI", "HRRM", "HRRS", "HLR", "HLRI", "HLRM", "HLRS",
  "HRRZ", "HRRZI", "HRRZM", "HRRZS", "HLRZ", "HLRZI", "HLRZM", "HLRZS",
  "HRRO", "HRROI", "HRROM", "HRROS", "HLRO", "HLROI", "HLROM", "HLROS",
  "HRRE", "HRREI", "HRREM", "HRRES", "HLRE", "HLREI", "HLREM", "HLRES",
  
  // 600 TAM
  "TRN", "TLN", "TRNE", "TLNE", "TRNA", "TLNA", "TRNN", "TLNN",
  "TDN", "TSN", "TDNE", "TSNE", "TDNA", "TSNA", "TDNN", "TSNN",
  "TRZ", "TLZ", "TRZE", "TLZE", "TRZA", "TLZA", "TRZN", "TLZN",
  "TDZ", "TSZ", "TDZE", "TSZE", "TDZA", "TSZA", "TDZN", "TSZN",
  "TRC", "TLC", "TRCE", "TLCE", "TRCA", "TLCA", "TRCN", "TLCN",
  "TDC", "TSC", "TDCE", "TSCE", "TDCA", "TSCA", "TDCN", "TSCN",
  "TRO", "TLO", "TROE", "TLOE", "TROA", "TLOA", "TRON", "TLON",
  "TDO", "TSO", "TDOE", "TSOE", "TDOA", "TSOA", "TDON", "TSON",
  
  // 700 IOT classic I/O
  "BLKI", "DATAI", "BLKO", "DATAO",
  "CONO",  "CONI", "CONSZ", "CONSO",
  NULL
};
static const t_int64 opc_val[] = {
  0254040000000+I_OP, 0254100000000+I_OP,
  0254200000000+I_OP, 0254240000000+I_OP, 0254300000000+I_OP, 0254340000000+I_OP,
  0254500000000+I_OP, 0254600000000+I_OP, 0254640000000+I_OP, 0133000000000+I_OP,
  0255040000000+I_OP, 0255100000000+I_OP, 0255200000000+I_OP, 0255300000000+I_OP,
  0255400000000+I_OP,
  //
  0000000000000+I_AC, 0001000000000+I_AC, 0002000000000+I_AC, 0003000000000+I_AC,
  0004000000000+I_AC, 0005000000000+I_AC, 0006000000000+I_AC, 0007000000000+I_AC,
  0010000000000+I_AC, 0011000000000+I_AC, 0012000000000+I_AC, 0013000000000+I_AC,
  0014000000000+I_AC, 0015000000000+I_AC, 0016000000000+I_AC, 0017000000000+I_AC,
  0020000000000+I_AC, 0021000000000+I_AC, 0022000000000+I_AC, 0023000000000+I_AC,
  0024000000000+I_AC, 0025000000000+I_AC, 0026000000000+I_AC, 0027000000000+I_AC,
  0030000000000+I_AC, 0031000000000+I_AC, 0032000000000+I_AC, 0033000000000+I_AC,
  0034000000000+I_AC, 0035000000000+I_AC, 0036000000000+I_AC, 0037000000000+I_AC,
  0040000000000+I_AC, 0041000000000+I_AC, 0042000000000+I_AC, 0043000000000+I_AC,
  0044000000000+I_AC, 0045000000000+I_AC, 0046000000000+I_AC, 0047000000000+I_AC,
  0050000000000+I_AC, 0051000000000+I_AC, 0052000000000+I_AC, 0053000000000+I_AC,
  0054000000000+I_AC, 0055000000000+I_AC, 0056000000000+I_AC, 0057000000000+I_AC,
  0060000000000+I_AC, 0061000000000+I_AC, 0062000000000+I_AC, 0063000000000+I_AC,
  0064000000000+I_AC, 0065000000000+I_AC, 0066000000000+I_AC, 0067000000000+I_AC,
  0070000000000+I_AC, 0071000000000+I_AC, 0072000000000+I_AC, 0073000000000+I_AC,
  0074000000000+I_AC, 0075000000000+I_AC, 0076000000000+I_AC, 0077000000000+I_AC,
  //
  0100000000000+I_AC, 0101000000000+I_AC, 0102000000000+I_AC, 0103000000000+I_AC,
  0104000000000+I_AC, 0105000000000+I_AC, 0106000000000+I_AC, 0107000000000+I_AC,
  0110000000000+I_AC, 0111000000000+I_AC, 0112000000000+I_AC, 0113000000000+I_AC,
  0114000000000+I_AC, 0115000000000+I_AC, 0116000000000+I_AC, 0117000000000+I_AC,
  0120000000000+I_AC, 0121000000000+I_AC, 0122000000000+I_AC, 0123000000000+I_AC,
  0124000000000+I_AC, 0125000000000+I_AC, 0126000000000+I_AC, 0127000000000+I_AC,
  0130000000000+I_AC, 0131000000000+I_AC, 0132000000000+I_AC, 0133000000000+I_AC,
  0134000000000+I_AC, 0135000000000+I_AC, 0136000000000+I_AC, 0137000000000+I_AC,
  0140000000000+I_AC, 0141000000000+I_AC, 0142000000000+I_AC, 0143000000000+I_AC,
  0144000000000+I_AC, 0145000000000+I_AC, 0146000000000+I_AC, 0147000000000+I_AC,
  0150000000000+I_AC, 0151000000000+I_AC, 0152000000000+I_AC, 0153000000000+I_AC,
  0154000000000+I_AC, 0155000000000+I_AC, 0156000000000+I_AC, 0157000000000+I_AC,
  0160000000000+I_AC, 0161000000000+I_AC, 0162000000000+I_AC, 0163000000000+I_AC,
  0164000000000+I_AC, 0165000000000+I_AC, 0166000000000+I_AC, 0167000000000+I_AC,
  0170000000000+I_AC, 0171000000000+I_AC, 0172000000000+I_AC, 0173000000000+I_AC,
  0174000000000+I_AC, 0175000000000+I_AC, 0176000000000+I_AC, 0177000000000+I_AC,
  //
  0200000000000+I_AC, 0201000000000+I_AC, 0202000000000+I_AC, 0203000000000+I_AC,
  0204000000000+I_AC, 0205000000000+I_AC, 0206000000000+I_AC, 0207000000000+I_AC,
  0210000000000+I_AC, 0211000000000+I_AC, 0212000000000+I_AC, 0213000000000+I_AC,
  0214000000000+I_AC, 0215000000000+I_AC, 0216000000000+I_AC, 0217000000000+I_AC,
  0220000000000+I_AC, 0221000000000+I_AC, 0222000000000+I_AC, 0223000000000+I_AC,
  0224000000000+I_AC, 0225000000000+I_AC, 0226000000000+I_AC, 0227000000000+I_AC,
  0230000000000+I_AC, 0231000000000+I_AC, 0232000000000+I_AC, 0233000000000+I_AC,
  0234000000000+I_AC, 0235000000000+I_AC, 0236000000000+I_AC, 0237000000000+I_AC,
  0240000000000+I_AC, 0241000000000+I_AC, 0242000000000+I_AC, 0243000000000+I_AC,
  0244000000000+I_AC, 0245000000000+I_AC, 0246000000000+I_AC, 0247000000000+I_AC,
  0250000000000+I_AC, 0251000000000+I_AC, 0252000000000+I_AC, 0253000000000+I_AC,
  0254000000000+I_AC, 0255000000000+I_AC, 0256000000000+I_AC, 0257000000000+I_AC,
  0260000000000+I_AC, 0261000000000+I_AC, 0262000000000+I_AC, 0263000000000+I_AC,
  0264000000000+I_AC, 0265000000000+I_AC, 0266000000000+I_AC, 0267000000000+I_AC,
  0270000000000+I_AC, 0271000000000+I_AC, 0272000000000+I_AC, 0273000000000+I_AC,
  0274000000000+I_AC, 0275000000000+I_AC, 0276000000000+I_AC, 0277000000000+I_AC,
  //
  0300000000000+I_AC, 0301000000000+I_AC, 0302000000000+I_AC, 0303000000000+I_AC,
  0304000000000+I_AC, 0305000000000+I_AC, 0306000000000+I_AC, 0307000000000+I_AC,
  0310000000000+I_AC, 0311000000000+I_AC, 0312000000000+I_AC, 0313000000000+I_AC,
  0314000000000+I_AC, 0315000000000+I_AC, 0316000000000+I_AC, 0317000000000+I_AC,
  0320000000000+I_AC, 0321000000000+I_AC, 0322000000000+I_AC, 0323000000000+I_AC,
  0324000000000+I_AC, 0325000000000+I_AC, 0326000000000+I_AC, 0327000000000+I_AC,
  0330000000000+I_AC, 0331000000000+I_AC, 0332000000000+I_AC, 0333000000000+I_AC,
  0334000000000+I_AC, 0335000000000+I_AC, 0336000000000+I_AC, 0337000000000+I_AC,
  0340000000000+I_AC, 0341000000000+I_AC, 0342000000000+I_AC, 0343000000000+I_AC,
  0344000000000+I_AC, 0345000000000+I_AC, 0346000000000+I_AC, 0347000000000+I_AC,
  0350000000000+I_AC, 0351000000000+I_AC, 0352000000000+I_AC, 0353000000000+I_AC,
  0354000000000+I_AC, 0355000000000+I_AC, 0356000000000+I_AC, 0357000000000+I_AC,
  0360000000000+I_AC, 0361000000000+I_AC, 0362000000000+I_AC, 0363000000000+I_AC,
  0364000000000+I_AC, 0365000000000+I_AC, 0366000000000+I_AC, 0367000000000+I_AC,
  0370000000000+I_AC, 0371000000000+I_AC, 0372000000000+I_AC, 0373000000000+I_AC,
  0374000000000+I_AC, 0375000000000+I_AC, 0376000000000+I_AC, 0377000000000+I_AC,
  //
  0400000000000+I_AC, 0401000000000+I_AC, 0402000000000+I_AC, 0403000000000+I_AC,
  0404000000000+I_AC, 0405000000000+I_AC, 0406000000000+I_AC, 0407000000000+I_AC,
  0410000000000+I_AC, 0411000000000+I_AC, 0412000000000+I_AC, 0413000000000+I_AC,
  0414000000000+I_AC, 0415000000000+I_AC, 0416000000000+I_AC, 0417000000000+I_AC,
  0420000000000+I_AC, 0421000000000+I_AC, 0422000000000+I_AC, 0423000000000+I_AC,
  0424000000000+I_AC, 0425000000000+I_AC, 0426000000000+I_AC, 0427000000000+I_AC,
  0430000000000+I_AC, 0431000000000+I_AC, 0432000000000+I_AC, 0433000000000+I_AC,
  0434000000000+I_AC, 0435000000000+I_AC, 0436000000000+I_AC, 0437000000000+I_AC,
  0440000000000+I_AC, 0441000000000+I_AC, 0442000000000+I_AC, 0443000000000+I_AC,
  0444000000000+I_AC, 0445000000000+I_AC, 0446000000000+I_AC, 0447000000000+I_AC,
  0450000000000+I_AC, 0451000000000+I_AC, 0452000000000+I_AC, 0453000000000+I_AC,
  0454000000000+I_AC, 0455000000000+I_AC, 0456000000000+I_AC, 0457000000000+I_AC,
  0460000000000+I_AC, 0461000000000+I_AC, 0462000000000+I_AC, 0463000000000+I_AC,
  0464000000000+I_AC, 0465000000000+I_AC, 0466000000000+I_AC, 0467000000000+I_AC,
  0470000000000+I_AC, 0471000000000+I_AC, 0472000000000+I_AC, 0473000000000+I_AC,
  0474000000000+I_AC, 0475000000000+I_AC, 0476000000000+I_AC, 0477000000000+I_AC,
  //
  0500000000000+I_AC, 0501000000000+I_AC, 0502000000000+I_AC, 0503000000000+I_AC,
  0504000000000+I_AC, 0505000000000+I_AC, 0506000000000+I_AC, 0507000000000+I_AC,
  0510000000000+I_AC, 0511000000000+I_AC, 0512000000000+I_AC, 0513000000000+I_AC,
  0514000000000+I_AC, 0515000000000+I_AC, 0516000000000+I_AC, 0517000000000+I_AC,
  0520000000000+I_AC, 0521000000000+I_AC, 0522000000000+I_AC, 0523000000000+I_AC,
  0524000000000+I_AC, 0525000000000+I_AC, 0526000000000+I_AC, 0527000000000+I_AC,
  0530000000000+I_AC, 0531000000000+I_AC, 0532000000000+I_AC, 0533000000000+I_AC,
  0534000000000+I_AC, 0535000000000+I_AC, 0536000000000+I_AC, 0537000000000+I_AC,
  0540000000000+I_AC, 0541000000000+I_AC, 0542000000000+I_AC, 0543000000000+I_AC,
  0544000000000+I_AC, 0545000000000+I_AC, 0546000000000+I_AC, 0547000000000+I_AC,
  0550000000000+I_AC, 0551000000000+I_AC, 0552000000000+I_AC, 0553000000000+I_AC,
  0554000000000+I_AC, 0555000000000+I_AC, 0556000000000+I_AC, 0557000000000+I_AC,
  0560000000000+I_AC, 0561000000000+I_AC, 0562000000000+I_AC, 0563000000000+I_AC,
  0564000000000+I_AC, 0565000000000+I_AC, 0566000000000+I_AC, 0567000000000+I_AC,
  0570000000000+I_AC, 0571000000000+I_AC, 0572000000000+I_AC, 0573000000000+I_AC,
  0574000000000+I_AC, 0575000000000+I_AC, 0576000000000+I_AC, 0577000000000+I_AC,
  //
  0600000000000+I_AC, 0601000000000+I_AC, 0602000000000+I_AC, 0603000000000+I_AC,
  0604000000000+I_AC, 0605000000000+I_AC, 0606000000000+I_AC, 0607000000000+I_AC,
  0610000000000+I_AC, 0611000000000+I_AC, 0612000000000+I_AC, 0613000000000+I_AC,
  0614000000000+I_AC, 0615000000000+I_AC, 0616000000000+I_AC, 0617000000000+I_AC,
  0620000000000+I_AC, 0621000000000+I_AC, 0622000000000+I_AC, 0623000000000+I_AC,
  0624000000000+I_AC, 0625000000000+I_AC, 0626000000000+I_AC, 0627000000000+I_AC,
  0630000000000+I_AC, 0631000000000+I_AC, 0632000000000+I_AC, 0633000000000+I_AC,
  0634000000000+I_AC, 0635000000000+I_AC, 0636000000000+I_AC, 0637000000000+I_AC,
  0640000000000+I_AC, 0641000000000+I_AC, 0642000000000+I_AC, 0643000000000+I_AC,
  0644000000000+I_AC, 0645000000000+I_AC, 0646000000000+I_AC, 0647000000000+I_AC,
  0650000000000+I_AC, 0651000000000+I_AC, 0652000000000+I_AC, 0653000000000+I_AC,
  0654000000000+I_AC, 0655000000000+I_AC, 0656000000000+I_AC, 0657000000000+I_AC,
  0660000000000+I_AC, 0661000000000+I_AC, 0662000000000+I_AC, 0663000000000+I_AC,
  0664000000000+I_AC, 0665000000000+I_AC, 0666000000000+I_AC, 0667000000000+I_AC,
  0670000000000+I_AC, 0671000000000+I_AC, 0672000000000+I_AC, 0673000000000+I_AC,
  0674000000000+I_AC, 0675000000000+I_AC, 0676000000000+I_AC, 0677000000000+I_AC,
  //
  0700000000000+I_IO, 0700040000000+I_IO, 0700100000000+I_IO, 0700140000000+I_IO,
  0700200000000+I_IO, 0700240000000+I_IO, 0700300000000+I_IO, 0700340000000+I_IO,
  -1
};
#define NUMDEV 6
static const char *devnam[NUMDEV] = {
  "APR", "PI", "PAG", "CCA", "TIM", "MTR"
};
/* Symbolic decode
   Inputs:
   *of     =       output stream
   addr    =       current PC
   *val    =       pointer to values
   *uptr   =       pointer to unit
   sw      =       switches
   Outputs:
   return  =       status code
*/
#define FMTASC(x) ((x) < 040)? "<%03o>": "%c", (x)
#define SIXTOASC(x) ((x) + 040)
t_stat fprint_sym (FILE *of, t_addr addr, t_value *val, UNIT *uptr, int32 sw)
{
  int32 i, j, c, cflag, ac, xr, y, dev;
  uint64 inst;
  char buffer[32],*buf;
  inst = val[0];
  cflag = (uptr == NULL) || (uptr == &cpu_unit[0]);
  if (sw & SWMASK ('A')) {                                /* ASCII?             8-bit */
    if (inst > 0377)
      return SCPE_ARG;
    fprintf (of, FMTASC ((int32) (inst & 0177)));
    return SCPE_OK;
  }
  if (sw & SWMASK ('C')) {                                /* character?         SIXBIT */
    for (i = 30; i >= 0; i = i - 6) {
      c = (int32) ((inst >> i) & 077);
      fprintf (of, "%c", SIXTOASC (c));
    }
    return SCPE_OK;
  }
  if (sw & SWMASK ('P')) {                                /* packed?            SAIL 7-bit */
    for (i = 29; i >= 0; i = i - 7) {
      c = (int32) ((inst >> i) & 0177);
      fprintf (of, FMTASC (c));
    }
    return SCPE_OK;
  }
  if (!(sw & SWMASK ('M')))
    return SCPE_ARG;
  
  /* Instruction decode */
  
  ac = GET_AC (inst);
  xr = GET_XR (inst);
  y = GET_ADDR (inst);
  dev = GET_DEV (inst);

  // exact width 22 wide ;
  bzero(buffer,sizeof(buffer));
  buf=buffer;
  
  for (i = 0; opc_val[i] >= 0; i++) { // loop thru ops
    j = (int32) ((opc_val[i] >> I_V_FL) & I_M_FL); //
    if (((opc_val[i] & FMASK) == (inst & masks[j]))) { // match ?
      
      buf+=sprintf(buf,"%s ", opcode[i]);
      
      switch (j) {// case on class
        
      case I_V_AC:
        buf+=sprintf(buf, "%-o,", ac); // print AC, fall thru

      case I_V_OP:
        if (inst & INST_IND)
          buf+=sprintf(buf, "@");
        if (xr)
          buf+=sprintf(buf, "%-o(%-o)", y, xr);
        else
          buf+=sprintf(buf, "%-o", y);
        break;

      case I_V_IO:                                    /* I/O */
        if (dev < NUMDEV)
          buf+=sprintf(buf, "%s,", devnam[dev]);
        else
          buf+=sprintf(buf, "%-o,", dev<<2);
        if (inst & INST_IND)
          buf+=sprintf(buf, "@");
        if (xr)
          buf+=sprintf(buf, "%-o(%-o)", y, xr);
        else
          buf+=sprintf(buf, "%-o", y);
        break;
      } // end case
      sprintf(buf,"%*s;",21-(int)strlen(buffer),""); // pad right spaces then semicolon ;
      fprintf(of,"%s",buffer);
      return SCPE_OK;
    } // end if
  } // end for
  return SCPE_ARG;
}
/* Get operand, including indirect and index
   Inputs:
   *cptr   =       pointer to input string
   *status =       pointer to error status
   Outputs:
   val     =       output value
*/
t_value get_opnd (const char *cptr, t_stat *status)
{
  int32 sign = 0;
  t_value val, xr = 0, ind = 0;
  const char *tptr;
  *status = SCPE_ARG;                                     /* assume fail */
  if (*cptr == '@') {
    ind = INST_IND;
    cptr++;
  }
  if (*cptr == '+')
    cptr++;
  else if (*cptr == '-') {
    sign = 1;
    cptr++;
  }
  val = strtotv (cptr, &tptr, 8);
  if (val > 0777777)
    return 0;
  if (sign)
    val = (~val + 1) & 0777777;
  cptr = tptr;
  if (*cptr == '(') {
    cptr++;
    xr = strtotv (cptr, &tptr, 8);
    if ((cptr == tptr) || (*tptr != ')') ||
        (xr > 017) || (xr == 0))
      return 0;
    cptr = ++tptr;
  }
  if (*cptr == 0)
    *status = SCPE_OK;
  return (ind | (xr << 18) | val);
}
/* Symbolic input
   Inputs:
   *cptr   =       pointer to input string
   addr    =       current PC
   uptr    =       pointer to unit
   *val    =       pointer to output values
   sw      =       switches
   Outputs:
   status  =       error status
*/
t_stat parse_sym (CONST char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
  int32 cflag, i, j;
  t_value ac, dev;
  t_stat r;
  char gbuf[CBUFSIZE], cbuf[2*CBUFSIZE];
  cflag = (uptr == NULL) || (uptr == &cpu_unit[0]);
  while (isspace (*cptr)) cptr++;
  memset (cbuf, '\0', sizeof(cbuf));
  strncpy (cbuf, cptr, sizeof(cbuf)-7);
  cptr = cbuf;
  if ((sw & SWMASK ('A')) || ((*cptr == '\'') && cptr++)) { /* ASCII char? */
    if (cptr[0] == 0)                                   /* must have 1 char */
      return SCPE_ARG;
    val[0] = (t_value) cptr[0];
    return SCPE_OK;
  }
  if ((sw & SWMASK ('C')) || ((*cptr == '"') && cptr++)) { /* sixbit string? */
    if (cptr[0] == 0)                                   /* must have 1 char */
      return SCPE_ARG;
    for (i = 0; i < 6; i++) {
      val[0] = (val[0] << 6);
      if (cptr[i]) val[0] = val[0] |
                     ((t_value) ((cptr[i] + 040) & 077));
    }
    return SCPE_OK;
  }
  if ((sw & SWMASK ('P')) || ((*cptr == '#') && cptr++)) { /* packed string? */
    if (cptr[0] == 0)                                   /* must have 1 char */
      return SCPE_ARG;
    for (i = 0; i < 5; i++)
      val[0] = (val[0] << 7) | ((t_value) cptr[i]);
    val[0] = val[0] << 1;
    return SCPE_OK;
  }
  /* Instruction parse */
  cptr = get_glyph (cptr, gbuf, 0);                       /* get opcode */
  for (i = 0; (opcode[i] != NULL) && (strcmp (opcode[i], gbuf) != 0) ; i++) ;
  if (opcode[i] == NULL)
    return SCPE_ARG;
  val[0] = opc_val[i] & FMASK;                            /* get value */
  j = (int32) ((opc_val[i] >> I_V_FL) & I_M_FL);          /* get class */
  switch (j) {                                            /* case on class */
  case I_V_AC:                                        /* AC + operand */
    if (strchr (cptr, ',')) {                       /* AC specified? */
      cptr = get_glyph (cptr, gbuf, ',');         /* get glyph */
      if (gbuf[0]) {                              /* can be omitted */
        ac = get_uint (gbuf, 8, 017 - 1, &r);
        if (r != SCPE_OK)
          return SCPE_ARG;
        val[0] = val[0] | (ac << INST_V_AC);
      }
    }                                           /* fall through */
  case I_V_OP:                                        /* operand */
    cptr = get_glyph (cptr, gbuf, 0);
    val[0] = val[0] | get_opnd (gbuf, &r);
    if (r != SCPE_OK)
      return SCPE_ARG;
    break;
  case I_V_IO:                                        /* I/O */
    cptr = get_glyph (cptr, gbuf, ',');             /* get glyph */
    for (dev = 0; (dev < NUMDEV) && (strcmp (devnam[dev], gbuf) != 0); dev++);
    if (dev >= NUMDEV) {
      dev = get_uint (gbuf, 8, INST_M_DEV, &r);
      if (r != SCPE_OK)
        return SCPE_ARG;
    }
    val[0] = val[0] | (dev << INST_V_DEV);
    cptr = get_glyph (cptr, gbuf, 0);
    val[0] = val[0] | get_opnd (gbuf, &r);
    if (r != SCPE_OK)
      return SCPE_ARG;
    break;
  }                                               /* end case */
  if (*cptr != 0)                                         /* junk at end? */
    return SCPE_ARG;
  return SCPE_OK;
}
