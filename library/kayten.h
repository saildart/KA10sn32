/* path /data/KA.Ten.lib/kayten.h
   -*- coding: utf-8; mode: C; -*-
   -------------------------------------------------------------------
        k       a       y       t       e       n       .       h                                100
   -------------------------------------------------------------------    --------------------------
123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.
// Bruce Guenther Baumgart, copyright©2019-2020, license GPLv3.
 */
#ifndef _KAYTEN_H
#define _KAYTEN_H 1
#define TRUE   1
#define FALSE  0
#define null   0
typedef unsigned int        bool;
typedef   signed char       int8;
typedef unsigned char      uint8;
typedef   signed short      int16;
typedef unsigned short     uint16;
typedef   signed int        int32;
typedef unsigned int       uint32;
typedef   signed long long  int64;
typedef unsigned long long uint64;
typedef unsigned long long d10; // Kword 36-bit PDP-10  word   right-adjusted into 64 bits
typedef unsigned int       a10; // Kaddr 18-bit PDP-10 address right-adjusted into 32 bits

/*
 *                                              ==================================================
 *      PDP-10 KA operation attributes                  O  P  _  A  T  T  R
 *                                              ==================================================
 */
typedef struct {
  uint32 :1, // Specific width AND alignment is not needed yet.
    omiteazero:1, // to disassemble as "popj p," or "RESCAN"
    imm:1,    // EA immediate
    trap:1,
    floating:1, // Floating point op codes
    afield:1, // AC field sub-function TTYUUO, PTYUUO, MAIL, INTUUO, PGIOT, PPIOT, JRST, JFCL
    read:1,   //  read memory from ma Y also on for SELF + BOTH address mode
    write:1,  // write memory into ma Y also on for SELF + BOTH address mode
    jump:1,   // jump Y (so omit POPJ, as well as the famous JUMP which does not jump)
    skip:1,   // skip (omit the famous SKIP which does not skip)
    jumpa:1,  // jump always
    skipa:1,  // skip always
    always:1; // always (unconditional) skip or jump
} opcode_attributes_t;

extern const uint16 pi_bit[8];
extern const uint16 pi_level[128];

// indexed by narrow op code 713. < (512+256) = 768.
extern opcode_attributes_t op_attr[];
extern char *op_string[];

#if 1
// declare (solo) newer-narrow enum set
#include  "n_enum.c"
#else
// declare (many) other enum sets
#include "user-opcode-u-enum.c"
#include       "op_user_enum.c"
#include        "narrow_enum.c"
#include          "wide_enum.c"
#endif

/* SAIL-WAITS                                   ==================================================
 * 36-bit PDP-10 word as a 64-bit object.               W       O       R       D
 *                                              ==================================================
 */
// 456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.
typedef union{
  d10 fw;
  char byte[8];
  struct { uint64 word:36,C_code:1,:27;		       	} full;         // PDP-10 full word 36-bits
  struct { uint64 right:18,left:18,hobits:28;         	} half;         // Half words 18-bits 
  struct {  int64 right:18,left:18,:28;         	} sign;         // Half words 18-bits signed
  struct { uint64     y:18,x:4,i:1,a:4,op:9,:28;      	} instruction;  // PDP-10 instruction format
  struct { uint64 c6:6,c5:6,c4:6,c3:6,c2:6,c1:6,:28;	} sixbit;       // ASCII sixbit+040
  struct { uint64 bit35:1,a5:7,a4:7,a3:7,a2:7,a1:7,:28;	} seven;        // seven bit ASCII
  struct { uint64 text:32,type:4,:28;} symbol;
  struct { uint64 foo:23,iop:3,device:7,seven:3,:28;} iop;

  // Little endian x86 mythical 9-track tape frame solid packing for 72 bits into 9 bytes
  // also mention 6 tape frames of 7-track
  // also mention 5 tape frames of 9-track for DART reels #3000 to #3228
  struct { uint64 nibble:4,byte4:8,byte3:8,byte2:8,byte1:8,:28; } tape; // 9-track tape bytes
  struct { uint64  byte1:8, byte2:8, byte3:8, byte4:8, nibble_lo:4,:28; } even;
  struct { uint64 nibble_hi:4, byte6:8, byte7:8, byte8:8, byte9:8, :28; } odd;

  // execution flow analysis, name high order bit positions ( the so called "hobits" ).
  struct { uint64 :36, // skip data 36-bits
      C_code:1, // This word has been translated into 'C' programming language notation.
      octal:1,  sixbit:1,   asciz:1, // type of data word for disassembly
      stepper:1, skipper:1, jumper:1, stopper:1,
      stepin:1,  skipin:1,  jumpin:1, pushjin:1,
      resume:1,      x:1,        r:1,       w:1, // Executed Read Write
      no_a:1,  // ac field NOT for ac register   ( UUO iot decode -or- iop device bits )
      no_y:1,  //  y field NOT for address       ( UUO calli decode )
      narr:10; // narrow opcode enum
  } flow;
  // SAIL file RIB "Retrieval Information Block" format
  struct { uint64 :15,date_hi:3,:46; } word2;
  struct { uint64 date_lo:12,time:11,mode:4,prot:9,:28; } word3;
  struct { uint64 count:22,:42; } word6;
  // HEAD TAIL block bit fields
  struct { uint64 date_lo:12,time:11,:9,prev_media_flag:1,date_hi:3,:28; } head_tail_word3;  
} pdp10_word_t;

/*                                              ==================================================
 *      SAIL-WAITS Core Image object                    C       O       R       E
 *                                              ==================================================
 */
typedef char dasmline[58];
typedef char name_tag[8];
typedef struct{
  pdp10_word_t *memory; // physical memory
  char *psymstr;
  char **symbolic; // table of symbolic addresses.
  
  dasmline *disassembled;
  a10 start_address, first_unused, ddt;
  a10 sym_address_low, sym_address_high, sym_count;
  int word_count,user,raid_version;
  
  a10 lower_bot; // bottom and top of lower segment
  a10 lower_top;
  a10 upper_bot; // bottom and top of upper segment
  a10 upper_top;
  int upper_segment_read_only; // boolean

  char jobname[8];

} core_image_t;

typedef struct {
  long long Tick;
  a10 main_pc;
  a10 main_start_address; // from JOBSA
  int main_step_countdown;
  int running_steps_per_tick;
  int sleeping_tick_countdown;
  
  a10 spacewar_pc;  
  a10 spacewar_start_address;
  int spacewar_step_countdown;
  int spacewar_steps_per_tick; // RUNNING
  int spacewar_ticks_between_startups;
  int spacewar_tick_countdown;
} user_state_t;

/*                                              ==================================================
 *  Toy SAIL-WAITS file system in GNU/Linux             D       I       S       K
 *  DATA8 format 8-bytes per PDP-10 word       ==================================================
 * Define the four word entry for a UFD, User File Directory.
 * UFD are also files, the master file diectory is in track #1
 * and its filename is '  1  1.UFD' it contains further UFD entries
 * foreach [ project , programmer ] pair.
 */
typedef struct UFDent{ 
  uint64 filnam:36,:28; // six uppercase FILNAM sixbit
  uint64 date_written_high:3, // hack-pack high-order 3 bits, overflowed 4 Jan 1975.
    creation_date:15,     // Right side
    ext:18,:28;           // Left side three uppercase .EXT extension
  uint64 date_written:12, // Right side
    time_written:11,
    mode:4,
    prot:9,:28; // Left side
  uint64 track:36,:28;
} UFDent_t;
/* The Toy file system does not need Tracks, Ribs or the SAT table.
 * see /data/Ralph.filebase/ for an implementation in 'D' lang notation
 */
typedef struct RIB{ 
// RIB = Retrieval-Information-Block for the SAIL-WAIT Ralph File System
  uint64 filnam:36,:28;                 // word  0. FILNAM      DDNAM
  uint64 c_date:18,ext:18,:28;          // word  1. EXT         DDEXT
  uint64 date_lo:12,time:11,mode:4,prot:9,:28; // word  2.      DDPRO
  uint64 prg:18,prj:18,:28;             // word  3.             DDPPN
  uint64 track:36,:28;                  // word  4.             DDLOC
  uint64 count:22,:42;                  // word  5.             DDLNG           file size word count
  uint64 refdate:12, reftime:11, :41;   // word  6.             DREFTM          access date time
  uint64 dumpdate:15, dumptape:12, :2,  // word  7.             DDMPTM           dump  date time
    never:1, reap:1, P_invalid:1, P_count:3, TP:1, // dart bookkeeping inside the disk file system
    :28;
  uint64 DGRP1R:36,:28; //  8. group 1
  uint64 DNXTGP:36,:28; //  9. next group track block
  uint64 DSATID:36,:28; // 10.
  // DQINFO five words
  uint64 word11:36,:28;
  uint64 word12:36,:28;
  uint64 write_program:36,:28;  // word 13. WPROGRM              WRTool
  uint64 write_ppn:36,:28;      // word 14. WPPN                 WRTppn
  uint64 word17:36,:28;         // word 15.
  uint64 dptr[16];              // plus 16 more words is a table of Track pointers for rest of the file.
} RIB_t;  
/*
 * Extern declarations for kayten
 */
extern int verbose;
extern int quiet;
extern int bamboo;
#define VERBOSE(format,args...)  if(verbose)fprintf(stdout,format,## args)
#define NOTquiet(format,args...) if(!quiet )fprintf(stdout,format,## args)
#define ERROR(format,args...)               fprintf(stderr,format,## args)

// simulated user memory space
extern  d10 **vm; // 2^18 PDP-10 words of virtual memory READ permitted.
extern  d10 **vw; // 2^18 PDP-10 words of virtual memory WRITE permitted.
extern  d10   *m; // 2^18 PDP-10 words of physical memory which in 1974 was 2usec magnetic core.

// 'C' UTF8 strings for each 7-bit SAIL ASCII character code
extern char *SAIL7CODE_utf8[];
extern char *SAIL7CODE_look[];
extern char *SAIL7CODE_iii[];
extern char *SAIL7CODE_lpt[];
//
extern char pro_dkb[];  // DKB keycodes into SAIL7BIT.
extern int anti_dkb[]; // SAIL7BIT into DKB keycodes.
//
extern char *sixbit_table;
extern char *sixbit_fname;
extern char *sixbit_uname;
extern char *sixbit_ppn  ;
extern char *sixbit_sail ;
//
extern char *sixbit_mod64;
extern char *sixbit_xx   ;
extern char *sixbit_rfc  ;
extern char *sixbit4648  ;
extern char *sixURL4648  ;
/*
 * Forward declarations for kayten routines
 * (object methods for the core_image_t *C).
 */
 int load_dump_file(core_image_t *C,char *dmp_pathname);
void init_opcode_attributes();
 int narrow_opcode(int user,pdp10_word_t *w);

void memory_scan(core_image_t *C);
void dasm_make(core_image_t *C,FILE *outdsm);
void dasm_write(core_image_t *C,FILE *outdsm);

void read_symbol_table(core_image_t *C);

char *sixbit_word_into_ascii(char *p0,d10 d);
char *sixbit_halfword_into_ascii(char *p0,int halfword);

void iso_date(char *dest,int sysdate,int systime);

#define SIGN  0400000000000 // sign bit
#ifndef LRZ
#define AMASK       0777777 // addr mask
#define RMASK       0777777
#define LRZ(x)  (((x) >> 18) & RMASK)
#define RRZ(x)  ( (x)        & RMASK)
#endif

/*
TODO:
implement a few more opcode cases over in kayten

    case 0700600000200:sprintf(disasmstr,"CONO PI,PION");break;
    case 0700600000400:sprintf(disasmstr,"CONO PI,PIOFF");break;
XCTR
        read
        write
        read_byte
        write_byte
*/

#endif /* _KAYTEN_H */
