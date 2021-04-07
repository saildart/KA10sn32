/* cpu.c -*- mode:C;coding:utf-8; -*-
   derived from
   ka10_cpu.c: PDP-10 CPU simulator   Copyright (c) 2011-2020, Richard Cornwell
   Full copyright and license notices are in the file named LICENSE.txt
   ----------------------------------------------------------------------------------------
   Merged I/O devices from PDP-10KA sn#32 and PDP-6 sn#16
   as they were at the Stanford A.I. Lab in late 1974 
   re-enacted as a KA10 with Two-Segment Relocation/Protection.
   ----------------------------------------------------------------------------------------
   derived from SIMH by R.M.Supnick
   derived from SIMS by R.Cornwell
   some device routines by L.Brinkhof
   reduction / redaction / simplification /and/ mangled by B.g.Baumgart
*/
#include "ka10_defs.h"
#include "sim_timer.h"
#include <time.h>

// BgBaumgart KA10 tool kit
#include "../library/kayten.h"
#include "../library/kayten.c"
void hark();
#define bamboo  // Markers for scratch-work de jour

#define HIST_PC  0x40000000
#define HIST_PC2 0x80000000
#define HIST_PCE 0x20000000
#define HIST_MIN 8
#define HIST_MAX 5000000

#define TMR_RTC 0
#define TMR_QUA 1

uint64  M[MAXMEMSIZE];                        /* Memory 256K words dammit */
uint64  FM[16];                               /* Fast memory register */
uint64  AR;                                   /* Primary work register */
uint64  MQ;                                   /* Extension to AR */
uint64  BR;                                   /* Secondary operand */
uint64  AD;                                   /* Address Data */
uint64  MB;                                   /* Memory Buffer Register */
t_addr  AB;                                   /* Memory address buffer */
t_addr  PC;                                   /* Program counter */
uint32  IR;                                   /* Instruction register */
uint64  MI;                                   /* Monitor lights */
uint32  FLAGS;                                /* Flags */
uint32  AC;                                   /* Operand accumulator */
uint64  SW;                                   /* Switch register */
int     BYF5;                                 /* Flag for second half of LDB/DPB instruction */
int     uuo_cycle;                            /* Uuo cycle in progress */
int     SC;                                   /* Shift count */
int     SCAD;                                 /* Shift count extension */
int     FE;                                   /* Exponent */
t_addr  Pl, Ph, Rl, Rh, Pflag;                /* Protection/Relocation registers */
int     push_ovf;                             /* Push stack overflow */
int     mem_prot;                             /* Memory protection flag */
int     nxm_flag;                             /* Non-existant memory flag */
int     clk_flg;                              /* Clock flag */
int     ov_irq;                               /* Trap overflow */
int     fov_irq;                              /* Trap floating overflow */
uint8   PIR;                                  /* Current priority level */
uint8   PIH;                                  /* Highest priority */
uint8   PIE;                                  /* Priority enable mask */
int     pi_cycle;                             /* Executing an interrupt */
int     pi_enable;                            /* Interrupts enabled */
int     parity_irq;                           /* Parity interupt */
int     pi_pending;                           /* Interrupt pending. */
int     pi_enc;                               /* Flag for pi */
int     apr_irq;                              /* Apr Irq level */
int     clk_en;                               /* Enable clock interrupts */
int     clk_irq;                              /* Clock interrupt */
int     pi_restore;                           /* Restore previous level */
int     pi_hold;                              /* Hold onto interrupt */
int     modify;                               /* Modify cycle */
int     xct_flag;                             /* XCT flags */
int     watch_stop;                           /* Stop at memory watch point */
int     maoff = 0;                            /* Offset for traps */
uint16  dev_irq[128];                         /* Pending irq by device */

t_stat  (*dev_tab[128])(uint32 dev, uint64 *data);
t_addr  (*dev_irqv[128])(uint32 dev, t_addr addr);
t_stat  rtc_srv(UNIT * uptr);
int32   rtc_tps = 60;
int32   tmxr_poll = 10000;

typedef struct {
  uint32      pc;
  uint32      ea;
  uint64      ir;
  uint64      ac;
  uint32      flags;
  uint64      mb;
  uint64      fmb;
  uint16      prev_sect;
  uint16      AC_or_dev;
  uint16      upi;              // USER and Priority Interrupt state
} InstHistory;
int32 hst_p = 0;                         /* history pointer */
int32 hst_lnt = 0;                       /* history length */
InstHistory *hst = NULL;                 /* instruction history */

/* Forward and external declarations */
t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);
t_stat cpu_set_size (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_set_hist (UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, CONST void *desc);
t_stat cpu_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *cpu_description (DEVICE *dptr);
void set_ac_display (uint64 *acbase);
int (*Mem_read)(int flag, int cur_context, int fetch);
int (*Mem_write)(int flag, int cur_context);
t_bool build_dev_tab (void);

/* CPU data structures
   cpu_dev      CPU device descriptor
   cpu_unit     CPU unit
   cpu_reg      CPU register list
   cpu_mod      CPU modifier list
*/
UNIT cpu_unit[] = { { UDATA (&rtc_srv,
                             UNIT_IDLE|
                             UNIT_FIX|
                             UNIT_BINK|
                             UNIT_TWOSEG|
                             UNIT_WAITS|
                             UNIT_MAOFF,
                             256 * 1024) },
};
REG cpu_reg[] = {
  { ORDATAD (PC, PC, 18, "Program Counter") },
  { ORDATAD (FLAGS, FLAGS, 18, "Flags") },
  { ORDATAD (FM0, FM[00], 36, "Fast Memory") },       /* addr in memory */
  { ORDATA (FM1, FM[01], 36) },                       /* modified at exit */
  { ORDATA (FM2, FM[02], 36) },                       /* to SCP */
  { ORDATA (FM3, FM[03], 36) },
  { ORDATA (FM4, FM[04], 36) },
  { ORDATA (FM5, FM[05], 36) },
  { ORDATA (FM6, FM[06], 36) },
  { ORDATA (FM7, FM[07], 36) },
  { ORDATA (FM10, FM[010], 36) },
  { ORDATA (FM11, FM[011], 36) },
  { ORDATA (FM12, FM[012], 36) },
  { ORDATA (FM13, FM[013], 36) },
  { ORDATA (FM14, FM[014], 36) },
  { ORDATA (FM15, FM[015], 36) },
  { ORDATA (FM16, FM[016], 36) },
  { ORDATA (FM17, FM[017], 36) },
  { BRDATA (FM, FM, 8, 36, 16)},
  { ORDATAD (PIR, PIR, 8, "Priority Interrupt Request") },
  { ORDATAD (PIH, PIH, 8, "Priority Interrupt Hold") },
  { ORDATAD (PIE, PIE, 8, "Priority Interrupt Enable") },
  { ORDATAD (PIENB, pi_enable, 7, "Enable Priority System") },
  { ORDATAD (SW, SW, 36, "Console SW Register"), REG_FIT},
  { ORDATAD (MI, MI, 36, "Monitor Display"), REG_FIT},
  { FLDATAD (BYF5, BYF5, 0, "Byte Flag") },
  { FLDATAD (UUO, uuo_cycle, 0, "UUO Cycle") },
  { ORDATAD (PL, Pl, 18, "Program Limit Low") },
  { ORDATAD (PH, Ph, 18, "Program Limit High") },
  { ORDATAD (RL, Rl, 18, "Program Relation Low") },
  { ORDATAD (RH, Rh, 18, "Program Relation High") },
  { FLDATAD (PFLAG, Pflag, 0, "Relocation enable") },
  { FLDATAD (PUSHOVER, push_ovf, 0, "Push overflow flag") },
  { FLDATAD (MEMPROT, mem_prot, 0, "Memory protection flag") },
  { FLDATAD (NXM, nxm_flag, 0, "Non-existing memory access") },
  { FLDATAD (CLK, clk_flg, 0, "Clock interrupt") },
  { FLDATAD (OV, ov_irq, 0, "Overflow enable") },
  { FLDATAD (FOV, fov_irq, 0, "Floating overflow enable") },
  { FLDATA  (PIPEND, pi_pending, 0), REG_HRO},
  { FLDATA  (PARITY, parity_irq, 0) },
  { ORDATAD (APRIRQ, apr_irq, 0, "APR Interrupt number") },
  { ORDATAD (CLKIRQ, clk_irq, 0, "CLK Interrupt number") },
  { FLDATA  (CLKEN, clk_en, 0), REG_HRO},
  { FLDATA  (XCT, xct_flag, 0), REG_HRO},
  { BRDATA  (IRQV, dev_irq, 8, 16, 128 ), REG_HRO},
  { ORDATA  (PIEN,   pi_enc,     8), REG_HRO},
  { FLDATA  (PIHOLD, pi_hold,    0), REG_HRO},
  { FLDATA  (PIREST, pi_restore, 0), REG_HRO},
  { FLDATA  (PICYC,  pi_cycle,   0), REG_HRO},
  { NULL }
};
MTAB cpu_mod[] = {
  { MTAB_XTD|MTAB_VDV, 0, "IDLE", "IDLE", &sim_set_idle, &sim_show_idle },
  { UNIT_MSIZE, 16, "256K", "256K", &cpu_set_size },
  { UNIT_M_PAGE,  UNIT_TWOSEG,"TWOSEG","TWOSEG", NULL, NULL, NULL, "Two Relocation Registers"},
  { UNIT_M_WAITS, UNIT_WAITS, "WAITS", "WAITS",  NULL, NULL, NULL, "Support for WAITS XCTR and KAFIX"},
  { UNIT_MAOFF,   UNIT_MAOFF, "MAOFF", "MAOFF",  NULL, NULL, NULL, "Interrupts relocated to 140"},
  { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "HISTORY", "HISTORY", &cpu_set_hist, &cpu_show_hist },
  { 0 }
};
/* Simulator debug controls */
DEBTAB cpu_debug[] = {
  {"IRQ",    DEBUG_IRQ,    "Debug IRQ requests"},
  {"CONI",   DEBUG_CONI,   "Show coni instructions"},
  {"CONO",   DEBUG_CONO,   "Show coni instructions"},
  {"DATAIO", DEBUG_DATAIO, "Show datai and datao instructions"},
  {"DETAIL", DEBUG_DETAIL, "Show details about CPU instruction execution"},
  {0, 0}
};
DEVICE cpu_dev = {
  "CPU", &cpu_unit[0], cpu_reg, cpu_mod, 1, 8, 22, 1, 8, 36,
  &cpu_ex, &cpu_dep, &cpu_reset,
  NULL, NULL, NULL, NULL, DEV_DEBUG, 0, cpu_debug,
  NULL, NULL, &cpu_help, NULL, NULL, &cpu_description
};

// opcode table of microcode steps; "Data array" of "flags".
#define FCE    000001 // Fetch c(E)
#define FCEPSE 000002 // Fetch c(E) and Pseudo Store Effective
#define SCE    000004 // Store c(E)
#define FAC    000010 // Fetch AC     first AC
#define FAC2   000020 // Fetch AC+1   second AC
#define SAC    000040 // Store AC
#define SACZ   000100 // clear AC
#define SAC2   000200 // Store AC+1   second AC
#define SWAR   000400 // swap halves of AR
#define FBR    001000 // fetch AC into BR

// microcode steps
int opflags[] = {
  // UUO Opcodes 000 to 077
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,

  // 100 to 177 reserve space for the "future" KL double precision math opcodes, Byte operators, KA Floating point operations.
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  FCE|FBR  /* UFA */,          FCE|FAC  /* DFN */,     FAC|SAC   /* FSC */,        0 /* IBP */,
  0        /* ILDB */,         0        /* LDB */,     0         /* IDPB */,       0 /* DPB */, 
  SAC|FCE|FBR  /* FAD */,      SAC|SAC2|FCE|FBR         /* FADL */, FCEPSE|FBR        /* FADM */,     SAC|FBR|FCEPSE      /* FADB */,
  SAC|FCE|FBR  /* FADR */,     SAC|SWAR|FBR        /* FADRI */,  FCEPSE|FBR       /* FADRM */,     SAC|FBR|FCEPSE     /* FADRB */,
  SAC|FCE|FBR  /* FSB */,      SAC|SAC2|FCE|FBR         /* FSBL */, FCEPSE|FBR        /* FSBM */,     SAC|FBR|FCEPSE      /* FSBB */,
  SAC|FCE|FBR  /* FSBR */,     SAC|SWAR|FBR        /* FSBRL */,  FCEPSE|FBR       /* FSBRM */,     SAC|FBR|FCEPSE     /* FSBRB */,
  SAC|FCE|FBR  /* FMP */,      SAC|SAC2|FCE|FBR         /* FMPL */, FCEPSE|FBR        /* FMPM */,     SAC|FBR|FCEPSE      /* FMPB */,
  SAC|FCE|FBR  /* FMPR */,     SAC|SWAR|FBR        /* FMPRI */, FCEPSE|FBR       /* FMPRM */,     SAC|FBR|FCEPSE     /* FMPRB */,
  SAC|FCE|FBR  /* FDV */,      SAC|FAC2|SAC2|FCE|FBR         /* FDVL */, FCEPSE|FBR        /* FDVM */, SAC|FBR|FCEPSE      /* FDVB */,
  SAC|FCE|FBR  /* FDVR */,     SAC|SWAR|FBR        /* FDVRL */, FCEPSE|FBR       /* FDVRM */,     SAC|FBR|FCEPSE     /* FDVRB */,
  
  // 200 to 277 Full word transfers, shift, branch, integer arithmetic
  SAC|FCE  /* MOVE */,         SAC        /* MOVEI */,              FAC|SCE       /* MOVEM */,        SACZ|FCEPSE     /* MOVES */,
  SWAR|SAC|FCE  /* MOVS */,    SWAR|SAC        /* MOVSI */,         SWAR|FAC|SCE       /* MOVSM */,   SWAR|SACZ|FCEPSE     /* MOVSS */,
  SAC|FCE  /* MOVN */,         SAC        /* MOVNI */,              FAC|SCE       /* MOVNM */,        SACZ|FCEPSE     /* MOVNS */,
  SAC|FCE  /* MOVM */,         SAC        /* MOVMI */,              FAC|SCE       /* MOVMM */,        SACZ|FCEPSE     /* MOVMS */,
  SAC|FCE|FBR  /* IMUL */,     SAC|FBR        /* IMULI */,          FCEPSE|FBR       /* IMULM */,     SAC|FCEPSE|FBR     /* IMULB */,
  SAC2|SAC|FCE|FBR  /* MUL */, SAC2|SAC|FBR         /* MULI */,     FCEPSE|FBR        /* MULM */,     SAC2|SAC|FCEPSE|FBR      /* MULB */,
  SAC2|SAC|FCE|FAC  /* IDIV */,SAC2|SAC|FAC        /* IDIVI */,     FCEPSE|FAC       /* IDIVM */,     SAC2|SAC|FCEPSE|FAC     /* IDIVB */,
  SAC2|SAC|FCE|FAC|FAC2/*DIV*/,SAC2|SAC|FAC|FAC2         /* DIVI */, FCEPSE|FAC|FAC2        /* DIVM */, SAC2|SAC|FCEPSE|FAC|FAC2      /* DIVB */,  
  FAC|SAC  /* ASH */,          FAC|SAC         /* ROT */,          FAC|SAC         /* LSH */,        FAC       /* JFFO */,
  FAC|SAC|SAC2|FAC2/* ASHC */, FAC|SAC|SAC2|FAC2         /* ROTC */, FAC|SAC|SAC2|FAC2         /* LSHC */,  FBR|SAC          /* WAITS KAFIX uuo247 */,
  FAC|FCE  /* EXCH */,         FAC        /* BLT */,              FAC|SAC         /* AOBJP */,        FAC|SAC     /* AOBJN */,
  0  /* JRST */,               0        /* JFCL */,                0        /* XCT */,              0       /* CONS uuo257 mythical WAITS-PDP6-LISP */,
  FAC|SAC  /* PUSHJ */,        FAC|FCE|SAC       /* PUSH */,      FAC|SAC        /* POP */,        FAC|SAC       /* POPJ */,
  0  /* JSR */,                SAC         /* JSP */,              FBR|SCE         /* JSA */,        0       /* JRA */,
  FBR|SAC|FCE  /* ADD */,      FBR|SAC         /* ADDI */,          FBR|FCEPSE        /* ADDM */,     FBR|SAC|FCEPSE      /* ADDB */,
  FBR|SAC|FCE  /* SUB */,      FBR|SAC         /* SUBI */,          FBR|FCEPSE        /* SUBM */,     FBR|SAC|FCEPSE      /* SUBB */,
  
  // 300 to 377 compare jump skip increment decrement jump skip
  FBR  /* CAI */,              FBR         /* CAIL */,              FBR        /* CAIE */,           FBR      /* CAILE */,
  FBR  /* CAIA */,             FBR        /* CAIGE */,              FBR       /* CAIN */,            FBR      /* CAIG */,
  FBR|FCE  /* CAM */,          FBR|FCE         /* CAML */,          FBR|FCE        /* CAME */,       FBR|FCE      /* CAMLE */,
  FBR|FCE  /* CAMA */,         FBR|FCE        /* CAMGE */,          FBR|FCE       /* CAMN */,        FBR|FCE      /* CAMG */,  
  FAC  /* JUMP */,             FAC        /* JUMPL */,              FAC       /* JUMPE */,           FAC     /* JUMPLE */,
  FAC  /* JUMPA */,            FAC       /* JUMPGE */,              FAC      /* JUMPN */,            FAC     /* JUMPG */,
  SACZ|FCE  /* SKIP */,        SACZ|FCE        /* SKIPL */,         SACZ|FCE       /* SKIPE */,      SACZ|FCE     /* SKIPLE */,
  SACZ|FCE  /* SKIPA */,       SACZ|FCE       /* SKIPGE */,         SACZ|FCE      /* SKIPN */,       SACZ|FCE     /* SKIPG */,  
  SAC|FAC  /* AOJ */,          SAC|FAC         /* AOJL */,          SAC|FAC        /* AOJE */,       SAC|FAC      /* AOJLE */,
  SAC|FAC  /* AOJA */,         SAC|FAC        /* AOJGE */,          SAC|FAC       /* AOJN */,        SAC|FAC      /* AOJG */,
  SACZ|FCEPSE  /* AOS */,      SACZ|FCEPSE         /* AOSL */,      SACZ|FCEPSE        /* AOSE */,   SACZ|FCEPSE      /* AOSLE */,
  SACZ|FCEPSE  /* AOSA */,     SACZ|FCEPSE        /* AOSGE */,      SACZ|FCEPSE       /* AOSN */,    SACZ|FCEPSE      /* AOSG */,
  SAC|FAC  /* SOJ */,          SAC|FAC         /* SOJL */,          SAC|FAC        /* SOJE */,       SAC|FAC      /* SOJLE */,
  SAC|FAC  /* SOJA */,         SAC|FAC        /* SOJGE */,          SAC|FAC       /* SOJN */,        SAC|FAC      /* SOJG */,  
  SACZ|FCEPSE  /* SOS */,      SACZ|FCEPSE         /* SOSL */,      SACZ|FCEPSE        /* SOSE */,   SACZ|FCEPSE      /* SOSLE */,
  SACZ|FCEPSE  /* SOSA */,     SACZ|FCEPSE        /* SOSGE */,      SACZ|FCEPSE       /* SOSN */,    SACZ|FCEPSE      /* SOSG */,
  
  // Boolean operators 400 to 477
  SAC  /* SETZ */,             SAC        /* SETZI */,             SCE       /* SETZM */,            SAC|SCE     /* SETZB */,
  FBR|SAC|FCE  /* AND */,      FBR|SAC         /* ANDI */,         FBR|FCEPSE        /* ANDM */,     FBR|SAC|FCEPSE      /* ANDB */,
  FBR|SAC|FCE  /* ANDCA */,    FBR|SAC       /* ANDCAI */,         FBR|FCEPSE      /* ANDCAM */,     FBR|SAC|FCEPSE    /* ANDCAB */,
  SAC|FCE  /* SETM */,         SAC        /* SETMI */,             0       /* SETMM */,              SAC|FCE     /* SETMB */,
  FBR|SAC|FCE  /* ANDCM */,    FBR|SAC       /* ANDCMI */,         FBR|FCEPSE      /* ANDCMM */,     FBR|SAC|FCEPSE    /* ANDCMB */,
  FBR|SAC  /* SETA */,         FBR|SAC        /* SETAI */,         FBR|SCE       /* SETAM */,        FBR|SAC|SCE     /* SETAB */,
  FBR|SAC|FCE  /* XOR */,      FBR|SAC         /* XORI */,         FBR|FCEPSE        /* XORM */,     FBR|SAC|FCEPSE      /* XORB */,
  FBR|SAC|FCE  /* IOR */,      FBR|SAC         /* IORI */,         FBR|FCEPSE        /* IORM */,     FBR|SAC|FCEPSE      /* IORB */,
  FBR|SAC|FCE  /* ANDCB */,    FBR|SAC       /* ANDCBI */,         FBR|FCEPSE      /* ANDCBM */,     FBR|SAC|FCEPSE    /* ANDCBB */,
  FBR|SAC|FCE  /* EQV */,      FBR|SAC         /* EQVI */,         FBR|FCEPSE        /* EQVM */,     FBR|SAC|FCEPSE      /* EQVB */,
  FBR|SAC  /* SETCA */,        FBR|SAC       /* SETCAI */,         FBR|SCE      /* SETCAM */,        FBR|SAC|SCE    /* SETCAB */,
  FBR|SAC|FCE  /* ORCA */,     FBR|SAC        /* ORCAI */,         FBR|FCEPSE       /* ORCAM */,     FBR|SAC|FCEPSE     /* ORCAB */,
  SAC|FCE  /* SETCM */,        SAC       /* SETCMI */,             FCEPSE      /* SETCMM */,         SAC|FCEPSE    /* SETCMB */,
  FBR|SAC|FCE  /* ORCM */,     FBR|SAC        /* ORCMI */,         FBR|FCEPSE       /* ORCMM */,     FBR|SAC|FCEPSE     /* ORCMB */,
  FBR|SAC|FCE  /* ORCB */,     FBR|SAC        /* ORCBI */,         FBR|FCEPSE       /* ORCBM */,     FBR|SAC|FCEPSE     /* ORCBB */,
  SAC  /* SETO */,             SAC        /* SETOI */,             SCE       /* SETOM */,            SAC|SCE     /* SETOB */,
  
  // Half word operators 500 to 577
  FBR|SAC|FCE  /* HLL */,      FBR|SAC         /* HLLI */,         FAC|FCEPSE        /* HLLM */,     SACZ|FCEPSE      /* HLLS */,
  SWAR|FBR|SAC|FCE  /* HRL */, SWAR|FBR|SAC         /* HRLI */,    FAC|SWAR|FCEPSE        /* HRLM */,SACZ|FCEPSE      /* HRLS */,
  SAC|FCE  /* HLLZ */,         SAC        /* HLLZI */,             FAC|SCE       /* HLLZM */,        SACZ|FCEPSE     /* HLLZS */,
  SWAR|SAC|FCE  /* HRLZ */,    SWAR|SAC        /* HRLZI */,        FAC|SWAR|SCE       /* HRLZM */,   SWAR|SACZ|FCEPSE     /* HRLZS */,
  SAC|FCE  /* HLLO */,         SAC        /* HLLOI */,             FAC|SCE       /* HLLOM */,        SACZ|FCEPSE     /* HLLOS */,
  SWAR|SAC|FCE  /* HRLO */,    SWAR|SAC        /* HRLOI */,        FAC|SWAR|SCE       /* HRLOM */,   SWAR|SACZ|FCEPSE     /* HRLOS */,
  SAC|FCE  /* HLLE */,         SAC        /* HLLEI */,             FAC|SCE       /* HLLEM */,        SACZ|FCEPSE     /* HLLES */,
  SWAR|SAC|FCE  /* HRLE */,    SWAR|SAC        /* HRLEI */,        FAC|SWAR|SCE       /* HRLEM */,   SWAR|SACZ|FCEPSE     /* HRLES */,
  FBR|SAC|FCE  /* HRR */,      FBR|SAC         /* HRRI */,         FAC|FCEPSE        /* HRRM */,     SACZ|FCEPSE      /* HRRS */,
  SWAR|FBR|SAC|FCE  /* HLR */, SWAR|FBR|SAC         /* HLRI */,    FAC|SWAR|FCEPSE        /* HLRM */,SACZ|FCEPSE      /* HLRS */,
  SAC|FCE  /* HRRZ */,         SAC        /* HRRZI */,             FAC|SCE       /* HRRZM */,        SACZ|FCEPSE     /* HRRZS */,
  SWAR|SAC|FCE  /* HLRZ */,    SWAR|SAC        /* HLRZI */,        FAC|SWAR|SCE       /* HLRZM */,   SWAR|SACZ|FCEPSE     /* HLRZS */,
  SAC|FCE  /* HRRO */,         SAC        /* HRROI */,             FAC|SCE       /* HRROM */,        SACZ|FCEPSE     /* HRROS */,
  SWAR|SAC|FCE  /* HLRO */,    SWAR|SAC        /* HLROI */,        FAC|SWAR|SCE       /* HLROM */,   SWAR|SACZ|FCEPSE     /* HLROS */,
  SAC|FCE  /* HRRE */,         SAC        /* HRREI */,             FAC|SCE       /* HRREM */,        SACZ|FCEPSE     /* HRRES */,
  SWAR|SAC|FCE  /* HLRE */,    SWAR|SAC        /* HLREI */,        FAC|SWAR|SCE       /* HLREM */,   SWAR|SACZ|FCEPSE     /* HLRES */,
  
  // Test operators 600 to 677
  FBR          /* TRN  */,     FBR|SWAR         /* TLN */,         FBR         /* TRNE */,           FBR|SWAR          /* TLNE */,
  FBR          /* TRNA */,     FBR|SWAR        /* TLNA */,         FBR        /* TRNN */,            FBR|SWAR          /* TLNN */,
  FBR|FCE      /* TDN  */,     FBR|SWAR|FCE         /* TSN */,     FBR|FCE         /* TDNE */,       FBR|SWAR|FCE      /* TSNE */,
  FBR|FCE      /* TDNA */,     FBR|SWAR|FCE        /* TSNA */,     FBR|FCE        /* TDNN */,        FBR|SWAR|FCE      /* TSNN */,
  FBR|SAC      /* TRZ  */,     FBR|SWAR|SAC         /* TLZ */,     FBR|SAC         /* TRZE */,       FBR|SWAR|SAC      /* TLZE */,
  FBR|SAC      /* TRZA */,     FBR|SWAR|SAC        /* TLZA */,     FBR|SAC        /* TRZN */,        FBR|SWAR|SAC      /* TLZN */,
  FBR|SAC|FCE  /* TDZ  */,     FBR|SWAR|SAC|FCE         /* TSZ */, FBR|SAC|FCE         /* TDZE */,   FBR|SWAR|SAC|FCE  /* TSZE */,
  FBR|SAC|FCE  /* TDZA */,     FBR|SWAR|SAC|FCE        /* TSZA */, FBR|SAC|FCE        /* TDZN */,    FBR|SWAR|SAC|FCE  /* TSZN */,
  FBR|SAC      /* TRC  */,     FBR|SWAR|SAC         /* TLC */,     FBR|SAC         /* TRCE */,       FBR|SWAR|SAC      /* TLCE */,
  FBR|SAC      /* TRCA */,     FBR|SWAR|SAC        /* TLCA */,     FBR|SAC        /* TRCN */,        FBR|SWAR|SAC      /* TLCN */,
  FBR|SAC|FCE  /* TDC  */,     FBR|SWAR|SAC|FCE         /* TSC */, FBR|SAC|FCE         /* TDCE */,   FBR|SWAR|SAC|FCE  /* TSCE */,
  FBR|SAC|FCE  /* TDCA */,     FBR|SWAR|SAC|FCE        /* TSCA */, FBR|SAC|FCE        /* TDCN */,    FBR|SWAR|SAC|FCE  /* TSCN */,
  FBR|SAC      /* TRO  */,     FBR|SWAR|SAC         /* TLO */,     FBR|SAC         /* TROE */,       FBR|SWAR|SAC      /* TLOE */,
  FBR|SAC      /* TROA */,     FBR|SWAR|SAC        /* TLOA */,     FBR|SAC        /* TRON */,        FBR|SWAR|SAC      /* TLON */,
  FBR|SAC|FCE  /* TDO  */,     FBR|SWAR|SAC|FCE         /* TSO */, FBR|SAC|FCE         /* TDOE */,   FBR|SWAR|SAC|FCE  /* TSOE */,
  FBR|SAC|FCE  /* TDOA */,     FBR|SWAR|SAC|FCE        /* TSOA */, FBR|SAC|FCE        /* TDON */,    FBR|SWAR|SAC|FCE  /* TSON */,
  
  // IOT  Instructions 700 to 777
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,
};
#define PC_CHANGE 
#define SWAP_AR ((RMASK & AR) << 18) | ((AR >> 18) & RMASK)
#define SMEAR_SIGN(x) x = ((x) & SMASK) ? (x) | EXPO : (x) & MANT
#define GET_EXPO(x) ((((x) & SMASK) ? 0377 : 0 ) ^ (((x) >> 27) & 0377))
#define AOB(x) (x + 01000001LL)
#define SOB(x) (x + 0777776777777LL)

/*
 * Set device to interrupt on a given level 1-7
 * Level 0 means that device interrupt is not enabled
 */
void set_interrupt(int dev, int lvl) {
  lvl &= 07;
  if (lvl) {
    dev_irq[dev>>2] = 0200 >> lvl;
    pi_pending = 1;
    sim_debug(DEBUG_IRQ, &cpu_dev, "set irq %o %o %03o %03o %03o\n",
              dev & 0774, lvl, PIE, PIR, PIH);
  }
}
/*
 * Clear the interrupt flag for a device
 */
void clr_interrupt(int dev) {
  dev_irq[dev>>2] = 0;
  if (dev > 4)
    sim_debug(DEBUG_IRQ, &cpu_dev, "clear irq %o\n", dev & 0774);
}
/*
 * Check if there is any pending interrupts return 0 if none,
 * else set pi_enc to highest level and return 1.
 */
int check_irq_level() {
  int i, lvl;
  int pi_req;
  /* If PXCT don't check for interrupts */
  if (xct_flag != 0)
    return 0;
  check_apr_irq();
  /* If not enabled, check if any pending Processor IRQ */
  if (pi_enable == 0) {
    if (PIR != 0) {
      pi_enc = 1;
      for(lvl = 0100; lvl != 0; lvl >>= 1) {
        if (lvl & PIH)
          break;
        if (PIR & lvl)
          return 1;
        pi_enc++;
      }
    }
    return 0;
  }
  /* Scan all devices */
  for(i = lvl = 0; i < 128; i++)
    lvl |= dev_irq[i];
  if (lvl == 0)
    pi_pending = 0;
  pi_req = (lvl & PIE) | PIR;
  /* Handle held interrupt requests */
  i = 1;
  for(lvl = 0100; lvl != 0; lvl >>= 1, i++) {
    if (lvl & PIH)
      break;
    if (pi_req & lvl) {
      pi_enc = i;
      return 1;
    }
  }
  return 0;
}
/*
 * Recover from held interrupt.
 */
void restore_pi_hold() {
  int lvl;
  if (!pi_enable)
    return;
  /* Clear HOLD flag for highest interrupt */
  for(lvl = 0100; lvl != 0; lvl >>= 1) {
    if (lvl & PIH) {
      PIR &= ~lvl;
      sim_debug(DEBUG_IRQ, &cpu_dev, "restore irq %o %03o\n", lvl, PIH);
      PIH &= ~lvl;
      break;
    }
  }
  pi_pending = 1;
}
/*
 * Hold interrupts at the current level.
 */
void set_pi_hold() {
  int pi = pi_enc;
  PIR &= ~(0200 >> pi);
  if (pi_enable)
    PIH |= (0200 >> pi);
}
/*
 * PI device for KA
 */
t_stat dev_pi(uint32 dev, uint64 *data) {
  uint64 res = 0;
  switch(dev & 3) {
  case CONO:
    /* Set PI flags */
    res = *data;
    if (res & 010000) { /* Bit 23 */
      PIR = PIH = PIE = 0;
      pi_enable = 0;
      parity_irq = 0;
    }
    if (res & 0200) {  /* Bit 28 */
      pi_enable = 1;
    }
    if (res & 0400)    /* Bit 27 */
      pi_enable = 0;
    if (res & 01000) { /* Bit 26 */
      PIE &= ~(*data & 0177);
    }
    if (res & 02000)   /* Bit 25 */
      PIE |= (*data & 0177);
    if (res & 04000) { /* Bit 24 */
      PIR |= (*data & 0177);
      pi_pending = 1;
    }
    if (res & 040000)   /* Bit 21 */
      parity_irq = 1;
    if (res & 0100000)  /* Bit 20 */
      parity_irq = 0;
    check_apr_irq();
    sim_debug(DEBUG_CONO, &cpu_dev, "CONO PI %012llo\n", *data);
    break;
  case CONI:
    res = PIE;
    res |= (pi_enable << 7);
    res |= (PIH << 8);
    res |= (parity_irq << 15);
    *data = res;
    sim_debug(DEBUG_CONI, &cpu_dev, "CONI PI %012llo\n", *data);
    break;
  case DATAO:
    MI = *data;
    break;
  case DATAI:
    break;
  }
  return SCPE_OK;
}
/*
 * Non existent device
 */
t_stat null_dev(uint32 dev, uint64 *data) {
  switch(dev & 3) {
  case CONI:
  case DATAI:
    *data = 0;
    break;
  case CONO:
  case DATAO:
    break;
  }
  return SCPE_OK;
}
/*
 * Check if the last operation caused a APR IRQ to be generated.
 */
void check_apr_irq() {
  if (pi_enable && apr_irq) {
    int flg = 0;
    clr_interrupt(0);
    flg |= ((FLAGS & OVR) != 0) & ov_irq;
    flg |= ((FLAGS & FLTOVR) != 0) & fov_irq;
    flg |= nxm_flag | mem_prot | push_ovf;
    if (flg)
      set_interrupt(0, apr_irq);
  }
}
/*
 * APR Device for KA10.
 */
t_stat dev_apr(uint32 dev, uint64 *data) {
  uint64 res = 0;
  switch(dev & 3) {
  case CONI:
    /* Read trap conditions */
    /* 000007 33-35 PIA */
    /* 000010 32 Overflow * */
    /* 000020 31 Overflow enable */
    /* 000040 30 Trap offset */
    /* 000100 29 Floating overflow * */
    /* 000200 28 Floating overflow enable */
    /* 000400 27 */
    /* 001000 26 Clock * */
    /* 002000 25 Clock enable */
    /* 004000 24 */
    /* 010000 23 NXM * */
    /* 020000 22 Memory protection * */
    /* 040000 21 Address break * */
    /* 100000 20 User In-Out */
    /* 200000 19 Push overflow * */
    /* 400000 18 */
    res = apr_irq | (((FLAGS & OVR) != 0) << 3) | (ov_irq << 4) ;
    res |= (((FLAGS & FLTOVR) != 0) << 6) | (fov_irq << 7) ;
    res |= (clk_flg << 9) | (((uint64)clk_en) << 10) | (nxm_flag << 12);
    res |= (mem_prot << 13) | (((FLAGS & USERIO) != 0) << 15);
    res |= (push_ovf << 16) | (maoff >> 1);
    *data = res;
    sim_debug(DEBUG_CONI, &cpu_dev, "CONI APR %012llo\n", *data);
    break;
  case CONO:
    /* Set trap conditions */
    res = *data;
    clk_irq = apr_irq = res & 07;
    clr_interrupt(0);
    if (res & 010)
      FLAGS &= ~OVR;
    if (res & 020)
      ov_irq = 1;
    if (res & 040)
      ov_irq = 0;
    if (res & 0100)
      FLAGS &= ~FLTOVR;
    if (res & 0200)
      fov_irq = 1;
    if (res & 0400)
      fov_irq = 0;
    if (res & 0001000) {
      clk_flg = 0;
      clr_interrupt(4);
    }
    if (res & 0002000) {
      clk_en = 1;
      if (clk_flg)
        set_interrupt(4, clk_irq);
    }
    if (res & 0004000) {
      clk_en = 0;
      clr_interrupt(4);
    }
    if (res & 010000)
      nxm_flag = 0;
    if (res & 020000)
      mem_prot = 0;
    if (res & 0200000) {
      reset_all(1);
    }
    if (res & 0400000)
      push_ovf = 0;
    check_apr_irq();
    sim_debug(DEBUG_CONO, &cpu_dev, "CONO APR %012llo\n", *data);
    break;
  case DATAO:
    /* Set protection registers */
    Rh = (0377 & (*data >> 1)) << 10;
    Rl = (0377 & (*data >> 10)) << 10;
    Pflag = 01 & (*data >> 18);
    Ph = ((0377 & (*data >> 19)) << 10) + 01777;
    Pl = ((0377 & (*data >> 28)) << 10) + 01777;
    sim_debug(DEBUG_DATAIO, &cpu_dev, "DATAO APR %012llo\n", *data);
    sim_debug(DEBUG_DATAIO, &cpu_dev, "Rl=%06o Pl=%06o, Rh=%06o, Ph=%06o\n", Rl, Pl, Rh, Ph);
    break;
  case DATAI:
    /* Read switches */
    *data = SW;
    sim_debug(DEBUG_DATAIO, &cpu_dev, "DATAI APR %012llo\n", *data);
    break;
  }
  return SCPE_OK;
}
#define get_reg(reg) FM[(reg) & 017]
#define set_reg(reg,value) FM[(reg) & 017] = value
int page_lookup_waits(t_addr addr, int flag, t_addr *loc, int wr, int cur_context, int fetch) {
  int      uf = (FLAGS & USER) != 0;
  /* If this is modify instruction use write access */
  wr |= modify;
  /* Figure out if this is a user space access */
  if (flag)
    uf = 0;
  else if (xct_flag != 0 && !fetch) {
    if (xct_flag & 010 && cur_context)   /* Indirect */
      uf = 1;
    if (xct_flag & 004 && wr == 0)       /* XR */
      uf = 1;
    if (xct_flag & 001 && (wr == 1 || BYF5))  /* XW or XLB or XDB */
      uf = 1;
  }
  if (uf) {
    if (addr <= Pl) {
      *loc = (addr + Rl) & RMASK;
      return 1;
    }
    if ((addr & 0400000) != 0 && (addr <= Ph)) {
      if ((Pflag == 0) || (Pflag == 1 && wr == 0)) {
        *loc = (addr + Rh) & RMASK;
        return 1;
      }
    }
    mem_prot = 1;
    return 0;
  } else {
    *loc = addr;
  }
  return 1;
}
int Mem_read_waits(int flag, int cur_context, int fetch) {
  t_addr addr;
  if (AB < 020 && ((xct_flag == 0 || fetch || cur_context || (FLAGS & USER) != 0))) {
    MB = get_reg(AB);
    return 0;
  }
  if (!page_lookup_waits(AB, flag, &addr, 0, cur_context, fetch))
    return 1;
  if (addr >= (int)MEMSIZE) {
    nxm_flag = 1;
    return 1;
  }
  if (sim_brk_summ && sim_brk_test(AB, SWMASK('R')))
    watch_stop = 1;
  sim_interval--;
  MB = M[addr];
  return 0;
}
/*
 * Write a location in memory.
 *
 * Return of 0 if successful, 1 if there was an error.
 */
int Mem_write_waits(int flag, int cur_context) {
  t_addr addr;
  /* If not doing any special access, just access register */
  if (AB < 020 && ((xct_flag == 0 || cur_context || (FLAGS & USER) != 0))) {
    set_reg(AB, MB);
    return 0;
  }
  if (!page_lookup_waits(AB, flag, &addr, 1, cur_context, 0))
    return 1;
  if (addr >= (int)MEMSIZE) {
    nxm_flag = 1;
    return 1;
  }
  if (sim_brk_summ && sim_brk_test(AB, SWMASK('W')))
    watch_stop = 1;
  sim_interval--;
  M[addr] = MB;
  return 0;
}
int page_lookup_ka(t_addr addr, int flag, t_addr *loc, int wr, int cur_context, int fetch) {
  if (!flag && (FLAGS & USER) != 0) {
    if (addr <= Pl) {
      *loc = (addr + Rl) & RMASK;
      return 1;
    }
    if (cpu_unit[0].flags & UNIT_TWOSEG &&
        (addr & 0400000) != 0 && (addr <= Ph)) {
      if ((Pflag == 0) || (Pflag == 1 && wr == 0)) {
        *loc = (addr + Rh) & RMASK;
        return 1;
      }
    }
    mem_prot = 1;
    return 0;
  } else {
    *loc = addr;
  }
  return 1;
}
int Mem_read_ka(int flag, int cur_context, int fetch) {
  t_addr addr;
  if (AB < 020) {
    MB = get_reg(AB);
  } else {
    if (!page_lookup_ka(AB, flag, &addr, 0, cur_context, fetch))
      return 1;
    if (addr >= (int)MEMSIZE) {
      nxm_flag = 1;
      return 1;
    }
    if (sim_brk_summ && sim_brk_test(AB, SWMASK('R')))
      watch_stop = 1;
    sim_interval--;
    MB = M[addr];
  }
  return 0;
}
/*
 * Write a location in memory.
 *
 * Return of 0 if successful, 1 if there was an error.
 */
int Mem_write_ka(int flag, int cur_context) {
  t_addr addr;
  if (AB < 020) {
    set_reg(AB, MB);
  } else {
    if (!page_lookup_ka(AB, flag, &addr, 1, cur_context, 0))
      return 1;
    if (addr >= (int)MEMSIZE) {
      nxm_flag = 1;
      return 1;
    }
    if (sim_brk_summ && sim_brk_test(AB, SWMASK('W')))
      watch_stop = 1;
    sim_interval--;
    M[addr] = MB;
  }
  return 0;
}
/*
 * Read a location directly from memory.
 *
 * Return of 0 if successful, 1 if there was an error.
 */
int Mem_read_nopage() {
  if (AB < 020) {
    MB =  get_reg(AB);
  } else {
    if (AB >= (int)MEMSIZE) {
      nxm_flag = 1;
      return 1;
    }
    sim_interval--;
    MB = M[AB];
  }
  return 0;
}
/*
 * Write a directly to a location in memory.
 *
 * Return of 0 if successful, 1 if there was an error.
 */
int Mem_write_nopage() {
  if (AB < 020) {
    set_reg(AB, MB);
  } else {
    if (AB >= (int)MEMSIZE) {
      nxm_flag = 1;
      return 1;
    }
    sim_interval--;
    M[AB] = MB;
  }
  return 0;
}
/*
 * Access main memory. Returns 0 if access ok, 1 if out of memory range.
 * On KI10 and KL10, optional EPT flag indicates address relative to ept.
 */
int Mem_read_word(t_addr addr, uint64 *data, int ept)
{
  if (addr >= MEMSIZE)
    return 1;
  *data = M[addr];
  return 0;
}
int Mem_write_word(t_addr addr, uint64 *data, int ept)
{
  if (addr >= MEMSIZE)
    return 1;
  M[addr] = *data;
  return 0;
}
/*
 * Function to determine number of leading zero bits in a PDP10 word
 */
int nlzero(uint64 w) {
  int n = 0;
  if (w == 0) return 36;
  if ((w & 00777777000000LL) == 0) { n += 18; w <<= 18; }
  if ((w & 00777000000000LL) == 0) { n += 9;  w <<= 9;  }
  if ((w & 00770000000000LL) == 0) { n += 6;  w <<= 6;  }
  if ((w & 00700000000000LL) == 0) { n += 3;  w <<= 3;  }
  if ((w & 00600000000000LL) == 0) { n ++;    w <<= 1;  }
  if ((w & 00400000000000LL) == 0) { n ++; }
  return n;
}
/*
 * Find First One.
 * JFFO is the OPCODE for coders suffering from chronic CBFD, Compulsive Bit Fiddling Disorder.
 * Digital is said to have implemented the JFFO on the PDP-6
 * while attempting to sell a machine to AT&T.
 */
int32 ffo (uint64 val)
{
  const unsigned char bit_position[64]={
    0,5,4,4,3,3,3,3,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  };
return val ?
  ( val & 0777700000000 ? // left third ?
  ( val & 0770000000000 ? // left six bits of left third ?
    bit_position[val>>30] : 
    bit_position[(val>>24)&077]+6 ) :
      val & 077770000 ? // middle third ?
    ( val & 077000000 ? // left six bits of middle third ?
      bit_position[(val>>18)&077]+12 :
      bit_position[(val>>12)&077]+18 ) :
    ( val & 07700 ? // left six bits of right third ?
      bit_position[(val>>6)&077]+24 :
      bit_position[val&077]+30 )
    ) : 0 ;
}
// combo
int nlz(uint64 w){
  int n=0;
  const unsigned char bit_position[64]={
    0,5,4,4,3,3,3,3,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  };
  if (w == 0) return 36;
  if ((w & 00777777000000LL) == 0) { n += 18; w <<= 18; }
  if ((w & 00777000000000LL) == 0) { n += 9;  w <<= 9;  }
  if ((w & 00770000000000LL) == 0) { n += 6;  w <<= 6;  }
  n += bit_position[ w>>30 ];
  return n;
}

t_stat sim_instr (void)
{
  t_stat reason;
  int     i_flags;                 /* Instruction mode flags */
  int     pi_rq;                   /* Interrupt request */
  int     pi_ov;                   /* Overflow during PI cycle */
  int     ind;                     /* Indirect bit */
  int     ix;                      /* Index register */
  int     f_load_pc;               /* Load AB from PC at start of instruction */
  int     f_inst_fetch;            /* Fetch new instruction */
  int     f_pc_inh;                /* Inhibit PC increment after instruction */
  int     nrf;                     /* Normalize flag */
  int     fxu_hold_set;            /* Negative exponent */
  int     sac_inh;                 /* Inhibit saving AC after instruction */
  int     f;                       /* Temporary variables */
  int     flag1;
  int     flag3;
  int     instr_count = 0;         /* Number of instructions to execute */
  uint32  IA;                      /* Initial address of first fetch */
  if (sim_step != 0) {
    instr_count = sim_step;
    sim_cancel_step();
  }
  /* Build device table */
  if ((reason = build_dev_tab ()) != SCPE_OK)            /* build, chk dib_tab */
    return reason;
  /* Main instruction fetch/decode loop: check clock queue, intr, trap, bkpt */
  f_load_pc = 1;
  f_inst_fetch = 1;
  ind = 0;
  uuo_cycle = 0;
  pi_cycle = 0;
  pi_rq = 0;
  pi_ov = 0;
  BYF5 = 0;
  watch_stop = 0;
  while ( reason == 0) {                                /* loop until ABORT */
    AIO_CHECK_EVENT;                                   /* queue async events */
    if (sim_interval <= 0) {                           /* check clock queue */
      if ((reason = sim_process_event()) != SCPE_OK) {/* error?  stop sim */
        return reason;
      }
    }
    if (sim_brk_summ && f_load_pc && sim_brk_test(PC, SWMASK('E'))) {
      reason = STOP_IBKPT;
      break;
    }
    if (watch_stop) {
      reason = STOP_IBKPT;
      break;
    }
    check_apr_irq();
    /* Normal instruction */
    if (f_load_pc) {
      modify = 0;
      xct_flag = 0;
      AB = PC;
      uuo_cycle = 0;
      f_pc_inh = 0;
    }
    if (f_inst_fetch) {
    fetch:
      if (Mem_read(pi_cycle | uuo_cycle, 1, 1)) {
        pi_rq = check_irq_level();
        if (pi_rq)
          goto st_pi;
        goto last;
      }
    no_fetch:
      IR = (MB >> 27) & 0777;
      AC = (MB >> 23) & 017;
      AD = MB;  /* Save for historical sake */
      IA = AB;
      i_flags = opflags[IR];
      BYF5 = 0;
    }
    /* Handle indirection repeat until no longer indirect */
    do {
      if ((!pi_cycle) & pi_pending
          ) {
        pi_rq = check_irq_level();
      }
      ind = TST_IND(MB) != 0;
      AR = MB;
      AB = MB & RMASK;
      ix = GET_XR(MB);
      if (ix) {
        /* For KA & KI */
        AR = MB = (AB + get_reg(ix)) & FMASK;
        AB = MB & RMASK;
      }
      if (ind & !pi_rq) {
        if (Mem_read(pi_cycle | uuo_cycle, 1, 0))
          goto last;
      }
      /* Handle events during a indirect loop */
      AIO_CHECK_EVENT;                                   /* queue async events */
      if (sim_interval-- <= 0) {
        if ((reason = sim_process_event()) != SCPE_OK) {
          return reason;
        }
      }
    } while (ind & !pi_rq);
    if (IR != 0254) {
      AR &= RMASK;
    }
    /* If there is a interrupt handle it. */
    if (pi_rq) {
    st_pi:
      sim_debug(DEBUG_IRQ, &cpu_dev, "trap irq %o %03o %03o \n", pi_enc, PIR, PIH);
      pi_cycle = 1;
      pi_rq = 0;
      pi_hold = 0;
      pi_ov = 0;
      AB = 040 | (pi_enc << 1) | maoff;
      xct_flag = 0;
      goto fetch;
    }
    /* Check for idle loop */
    if (sim_idle_enab &&(
         (
          (FLAGS & USER) &&
          PC < 020 &&
          (IR & 0777) == 0254 // JRST
          )
         ||  (uuo_cycle && (IR & 0740)==0 && IA==041)
         // Executive DDT busy wait for CTY input.
         // SYSTEM.DMP[J17,SYS] 162434/ JRST 162433
         || (PC == 0162434)
         // busy wait for CTY TYO
         // || (PC == 0164405)
         // JEN @24067
         // || (PC == 0050035)
         ))
      {
        sim_idle (TMR_RTC, FALSE);
      }
    
    /* Update history */
    if (hst_lnt && PC > 017) {
      hst_p = hst_p + 1;
      if (hst_p >= hst_lnt) {
        hst_p = 0;
      }
      hst[hst_p].pc = HIST_PC | ((BYF5)? (HIST_PC2|PC) : IA);
      hst[hst_p].ea = AB;
      hst[hst_p].ir = AD;
      hst[hst_p].flags = (FLAGS << 5)
        | pi_enable
        | (nxm_flag << 1)
        | (clk_flg  << 2)        
        | (push_ovf << 3)
        | (mem_prot << 4)
        ;
      hst[hst_p].ac = get_reg(AC);
      hst[hst_p].AC_or_dev =  (IR&0700)==0700 ? (((IR & 077) << 1) | ((AC & 010) != 0))<<2 : AC; // IOT device or accumulator AC
      hst[hst_p].upi = ((FLAGS & USER)!=0 ? 0400:0) | (pi_enable?0100:0) | (pi_level[PIH]<<3) | pi_level[PIR];
    }
    
    /* Set up to execute instruction */
    f_inst_fetch = 1;
    f_load_pc = 1;
    nrf = 0;
    fxu_hold_set = 0;
    sac_inh = 0;
    modify = 0;
    f_pc_inh = 0;
    
    // microsteps - Load pseudo registers based on flags
    if (i_flags & (FCEPSE|FCE)) {
      if (i_flags & FCEPSE)
        modify = 1;
      if (Mem_read(0, 0, 0))
        goto last;
      AR = MB;
    }
    if (i_flags & FAC) {
      BR = AR;
      AR = get_reg(AC);
    }
    if (i_flags & FBR) {
      BR = get_reg(AC);
    }
    if (hst_lnt && PC >= 020) {
      hst[hst_p].mb = AR;
    }
    if (i_flags & FAC2) {
      MQ = get_reg(AC + 1);
    } else if (!BYF5) {
      MQ = 0;
    }
    if (i_flags & SWAR) {
      AR = SWAP_AR;
    }

    // BgBaumgart J17SYS watch point near ACCENT: 0100375/ "DSKSER#2349"
    if(PC>=0100377 && !(FLAGS & USER))
    switch(PC){
      char jobnam[8], filnam[8], ext[4], ppn[8];
      char filename_string[20]; // "FILNAM.EXT[PRJ,PRG]"
      int job;
    case 0100377:
      sixbit_word_into_ascii( filnam, FM[1] );
      break;
    case 0100401:
      sixbit_halfword_into_ascii( ext, LRZ(FM[1]) );
      break;
    case 0100412:
      job = M[024061];
      if( job ){
        sixbit_word_into_ascii( jobnam, M[M[0225]+job] );
      } else {
        jobnam[0]=0; // empty
      }
      sixbit_word_into_ascii( ppn, FM[1] );
      fprintf(stderr,"job#%d %6.6s Looking up filename %s.%s[%s]\r\n", job, jobnam, filnam, ext, ppn);
      break;
    default: break;
    }
    
    /* Process the instruction */
    switch (IR) {
    case 0052: case 0053:
      /* Fall through */
    muuo:
    case 0000: /* UUO */
    case 0040: case 0041: case 0042: case 0043:
    case 0044: case 0045: case 0046: case 0047:
    case 0050: case 0051:
    case 0054: case 0055: case 0056: case 0057:
    case 0060: case 0061: case 0062: case 0063:
    case 0064: case 0065: case 0066: case 0067:
    case 0070: case 0071: case 0072: case 0073:
    case 0074: case 0075: case 0076: case 0077:
      /* MUUO */
      uuo_cycle = 1;
      /* LUUO */
    case 0001: case 0002: case 0003:
    case 0004: case 0005: case 0006: case 0007:
    case 0010: case 0011: case 0012: case 0013:
    case 0014: case 0015: case 0016: case 0017:
    case 0020: case 0021: case 0022: case 0023:
    case 0024: case 0025: case 0026: case 0027:
    case 0030: case 0031: case 0032: case 0033:
    case 0034: case 0035: case 0036: case 0037:
      MB = ((uint64)(IR) << 27) | ((uint64)(AC) << 23) | (uint64)(AB);
      AB = 040;
      if (maoff && uuo_cycle)
        AB |= maoff;
      Mem_write(uuo_cycle, 1);
      AB += 1;
      f_load_pc = 0;
      f_pc_inh = 1;
      break;
    case 0100:
    case 0101:
    case 0102:
    case 0103:
    case 0104:
      goto unasign;
    case 0247: /* KAFIX Poole */
      // The WAITS-KA unused opcode 0247 was rewired by David Poole
      // 'FIX AC,233000' returns integer floor is half of UFA AC,[233000]
      // 'FIX AC,211000' returns portion of mantissa as integer
      // 'FIX AC,200000' returns mantissa as integer
      // FBR flag loads BR = get_reg(AC);
    kafix:
      MQ = 0;
      AR = (MB & RMASK) << 18; // integer 'FIX 233000', spacewar does 'FIX 200000' for its sine table
      if (hst_lnt) {
        hst[hst_p].mb = AR;
      }
      sim_debug( DEBUG_DETAIL, &cpu_dev,"      KAFIX uuo247 AR=%012llo BR=%012llo AC=%o c(AC)=%llo MB=%012llo\r\n",AR,BR,AC,get_reg(AC),MB);
      goto ufa;
      /* UUO */
    case 0105: case 0106: case 0107:
    case 0110: case 0111: case 0112: case 0113:
    case 0114: case 0115: case 0116: case 0117:
    case 0120: case 0121: case 0122: case 0123:
    case 0124: case 0125: case 0126: case 0127:
    unasign:
        MB = ((uint64)(IR) << 27) | ((uint64)(AC) << 23) | (uint64)(AB);
        AB = 060 | maoff;
        uuo_cycle = 1;
        Mem_write(uuo_cycle, 0);
        AB += 1;
        f_load_pc = 0;
        break;
    case 0133: /* IBP/ADJBP */
    case 0134: /* ILDB */
    case 0136: /* IDPB */
      if ((FLAGS & BYTI) == 0) {      /* BYF6 */
        modify = 1;
        if (Mem_read(0, 1, 0)) {
          goto last;
        }
        AR = MB;
        SCAD = (AR >> 30) & 077;
        SC = (AR >> 24) & 077;
        SCAD = (SCAD + (0777 ^ SC) + 1) & 0777;
        if (SCAD & 0400) {
          SCAD = ((0777 ^ SC) + 044 + 1) & 0777;
          AR = (AR + 1) & FMASK;
        }
        AR &= PMASK;
        AR |= (uint64)(SCAD & 077) << 30;
        MB = AR;
        if (Mem_write(0, 1))
          goto last;
        if ((IR & 04) == 0)
          break;
        modify = 0;
        goto ldb_ptr;
      }
      /* Fall through */
    case 0135:/* LDB */
    case 0137:/* DPB */
      if ((FLAGS & BYTI) == 0 || !BYF5) {
        if (Mem_read(0, 1, 0))
          goto last;
        AR = MB;
        SC = (AR >> 24) & 077;
        SCAD = (AR >> 30) & 077;
      ldb_ptr:
        f_load_pc = 0;
        f_inst_fetch = 0;
        f_pc_inh = 1;
        FLAGS |= BYTI;
        BYF5 = 1;
      } else {
        if ((IR & 06) == 6)
          modify = 1;
        AB = AR & RMASK;
        MQ = (uint64)(1) << SC;
        MQ -= 1;
        if (Mem_read(0, 0, 0))
          goto last;
        AR = MB;
        if ((IR & 06) == 4) {
          AR = AR >> SCAD;
          AR &= MQ;
          set_reg(AC, AR);
        } else {
          BR = get_reg(AC);
          BR = BR << SCAD;
          MQ = MQ << SCAD;
          AR &= CM(MQ);
          AR |= BR & MQ;
          MB = AR & FMASK;
          if (Mem_write(0, 0))
            goto last;
        }
        FLAGS &= ~BYTI;
        BYF5 = 0;
      }
      break;
    case 0131:/* DFN */
      AD = (CM(BR) + 1) & FMASK;
      SC = (BR >> 27) & 0777;
      BR = AR;
      AR = AD;
      AD = (CM(BR) + ((AD & MANT) == 0)) & FMASK;
      AR &= MANT;
      AR |= ((uint64)(SC & 0777)) << 27;
      BR = AR;
      AR = AD;
      MB = BR;
      set_reg(AC, AR);
      if (Mem_write(0, 0))
        goto last;
      break;
    case 0132:/* FSC */
      SC = ((AB & RSIGN) ? 0400 : 0) | (AB & 0377);
      SCAD = GET_EXPO(AR);
      SC = (SCAD + SC) & 0777;
      flag1 = 0;
      if (AR & SMASK)
        flag1 = 1;
      SMEAR_SIGN(AR);
      AR <<= 34;
      goto fnorm;
    case 0150:  /* FSB */
    case 0151:  /* FSBL */
    case 0152:  /* FSBM */
    case 0153:  /* FSBB */
    case 0154:  /* FSBR */
    case 0155:  /* FSBRI */
    case 0156:  /* FSBRM */
    case 0157:  /* FSBRB */
      AD = (CM(AR) + 1) & FMASK;
      AR = BR;
      BR = AD;
      /* Fall through */
    case 0130:  /* UFA */
    ufa:
    case 0140:  /* FAD */
    case 0141:  /* FADL */
    case 0142:  /* FADM */
    case 0143:  /* FADB */
    case 0144:  /* FADR */
    case 0145:  /* FADR */
    case 0146:  /* FADRM */
    case 0147:  /* FADRB */
      flag3 = 0;
      SC = ((BR >> 27) & 0777);
      if ((BR & SMASK) == (AR & SMASK)) {
        SCAD = SC + (((AR >> 27) & 0777) ^ 0777) + 1;
      } else {
        SCAD = SC + ((AR >> 27) & 0777);
      }
      SCAD &= 0777;
      if (((BR & SMASK) != 0) == ((SCAD & 0400) != 0)) {
        AD = AR;
        AR = BR;
        BR = AD;
      }
      if ((SCAD & 0400) == 0) {
        if ((AR & SMASK) == (BR & SMASK))
          SCAD = ((SCAD ^ 0777) + 1) & 0777;
        else
          SCAD = (SCAD ^ 0777);
      } else {
        if ((AR & SMASK) != (BR & SMASK))
          SCAD = (SCAD + 1) & 0777;
      }
      /* Get exponent */
      SC = GET_EXPO(AR);
      /* Smear the signs */
      SMEAR_SIGN(AR);
      SMEAR_SIGN(BR);
      AR <<= 34;
      BR <<= 34;
      /* Shift smaller right */
      if (SCAD & 0400) {
        SCAD = (01000 - SCAD);
        if (SCAD < 61) {
          AD = (BR & FPSBIT)? FPFMASK : 0;
          BR = (BR >> SCAD) | (AD << (61 - SCAD));
        } else {
          if (SCAD < 65)   /* Under limit */
            BR = (BR & FPSBIT)? FPFMASK: 0;
          else
            BR = 0;
        }
      }
      /* Do the addition now */
      AR = (AR + BR);
      /* Set flag1 to sign and make positive */
      flag1 = (AR & FPSBIT) != 0;
    fnorm:
      if (((AR & FPSBIT) != 0) != ((AR & FPNBIT) != 0)) {
        SC += 1;
        flag3 = AR & 1;
        AR = (AR & FPHBIT) | (AR >> 1);
      }
      if (AR != 0) {
        AR &= ~077;  /* Save one extra bit */
        if (((SC & 0400) != 0) ^ ((SC & 0200) != 0))
          fxu_hold_set = 1;
        if ( IR!=0130 && IR!=0247) {  /* not UFA and not FIX*/
        fnormx:
          while (AR != 0 && ((AR & FPSBIT) != 0) == ((AR & FPNBIT) != 0) &&
                 ((AR & FPNBIT) != 0) == ((AR & FP1BIT) != 0)) {
            SC --;
            AR <<= 1;
          }
          /* Handle edge case of a - and overflow bit */
          if ((AR & 000777777777600000000000LL) == (FPSBIT|FPNBIT)) {
            SC ++;
            AR = (AR & FPHBIT) | (AR >> 1);
          }
          if (!nrf && ((IR & 04) != 0)) {
            f = (AR & FP1BIT) != 0;
            if ((AR & FPRBIT2) != 0) {
              /* FADR & FSBR do not round if negative and equal round */
              /* FMPR does not round if result negative and equal round */
              if (((IR & 070) != 070 &&
                   (AR & FPSBIT) != 0 &&
                   (AR & FPRMASK) != FPRBIT2) ||
                  (AR & FPSBIT) == 0 ||
                  (AR & FPRMASK) != FPRBIT2)
                AR += FPRBIT1;
              nrf = 1;
              AR &= ~FPRMASK;
              flag3 = 0;
              if (((AR & FP1BIT) != 0) != f) {
                SC += 1;
                flag3 = AR & 1;
                AR = (AR & FPHBIT) | (AR >> 1);
              }
              goto fnormx;
            }
          }
        }
        MQ = AR & FPRMASK;
        AR >>= 34;
        if (flag1)
          AR |= SMASK;
      } else {
        AR = MQ = 0;
        SC = 0;
      }
      if (((SC & 0400) != 0) && !pi_cycle) {
        FLAGS |= OVR|FLTOVR|TRP1;
        if (!fxu_hold_set) {
          FLAGS |= FLTUND;
          MQ = 0;
        }
        check_apr_irq();
      }
      SCAD = SC ^ ((AR & SMASK) ? 0377 : 0);
      AR &= SMASK|MMASK;
      AR |= ((uint64)(SCAD & 0377)) << 27;
      /* FADL FSBL FMPL */
      if ((IR & 07) == 1) {
        SC = (SC + (0777 ^  26)) & 0777;
        if ((SC & 0400) != 0)
          MQ = 0;
        MQ = (MQ >> 7) & MMASK;
        if (MQ != 0) {
          SC ^= (SC & SMASK) ? 0377 : 0;
          MQ |= ((uint64)(SC & 0377)) << 27;
        }
      }
      if ((AR & MMASK) == 0)
        AR = 0;
      /* Handle UFA */
      if (IR == 0130) {
        set_reg(AC + 1, AR);
        break;
      }
      // Finalize KAFIX uuo247 SAIL-WAITS-KA David Poole hack to the Stanford PDP-10 KA sn#32.
      // kafixfinal:
      if (IR == 0247){
        sim_debug(DEBUG_DETAIL,&cpu_dev,"  bp8 KAFIX uuo247 AR=%012llo BR=%o AC=%o MQ=%llo\r\n",AR,(int)BR,AC,MQ);
        BR >>= 34;
        if(AR & SMASK){ // when negative
          AR = (CM(AR)+1) & MMASK; // mantissa part of the UFA result
          AR = (CM(AR)+1) & FMASK; // negative PDP-10 integer the 36-bit word
        }else{
          AR &= MMASK; // the SAC flag will store result into the chosen AC register
        }
        sim_debug( DEBUG_DETAIL, &cpu_dev,"Final KAFIX uuo247 AR=%012llo BR=%012llo AC=%o c(AC)=%llo MB=%012llo\r\n\r\n",AR,BR,AC,get_reg(AC),MB);
        break;
      }
      break;
    case 0160:      /* FMP */
    case 0161:      /* FMPL */
    case 0162:      /* FMPM */
    case 0163:      /* FMPB */
    case 0164:      /* FMPR */
    case 0165:      /* FMPRI, FMPRL on PDP6 */
    case 0166:      /* FMPRM */
    case 0167:      /* FMPRB */
      /* Compute exponent */
      SC = (((BR & SMASK) ? 0777 : 0) ^ (BR >> 27)) & 0777;
      SC += (((AR & SMASK) ? 0777 : 0) ^ (AR >> 27)) & 0777;
      SC += 0600;
      SC &= 0777;
      /* Make positive and compute result sign */
      flag1 = 0;
      flag3 = 0;
      if (AR & SMASK) {
        if ((AR & MMASK) == 0) {
          AR = BIT9;
          SC++;
        } else
          AR = CM(AR) + 1;
        flag1 = 1;
        flag3 = 1;
      }
      if (BR & SMASK) {
        if ((BR & MMASK) == 0) {
          BR = BIT9;
          SC++;
        } else
          BR = CM(BR) + 1;
        flag1 = !flag1;
      }
      AR &= MMASK;
      BR &= MMASK;
      AR = (AR * BR) << 7;
      if (flag1) {
        AR = (AR ^ FPFMASK) + 1;
      }
      goto fnorm;
    case 0170:      /* FDV */
    case 0172:      /* FDVM */
    case 0173:      /* FDVB */
    case 0174:      /* FDVR */
    case 0175:      /* FDVRI */
    case 0176:      /* FDVRM */
    case 0177:      /* FDVRB */
      flag1 = 0;
      flag3 = 0;
      SC = (int)((((BR & SMASK) ? 0777 : 0) ^ (BR >> 27)) & 0777);
      SCAD = (int)((((AR & SMASK) ? 0777 : 0) ^ (AR >> 27)) & 0777);
      if ((BR & (MMASK)) == 0) {
        if (BR == SMASK) {
          BR = BIT9;
          SC--;
        } else {
          AR = BR;
          break;
        }
      }
      if (BR & SMASK) {
        BR = CM(BR) + 1;
        flag1 = 1;
      }
      if (AR & SMASK) {
        if ((AR & MMASK) == 0) {
          AR = BIT9;
          SC--;
        } else
          AR = CM(AR) + 1;
        flag1 = !flag1;
      }
      SC = (SC + ((0777 ^ SCAD) + 1) + 0201) & 0777;
      /* Clear exponents */
      AR &= MMASK;
      BR &= MMASK;
      /* Check if we need to fix things */
      if (BR >= (AR << 1)) {
        if (!pi_cycle)
          FLAGS |= OVR|NODIV|FLTOVR|TRP1;
        check_apr_irq();
        sac_inh = 1;
        break;      /* Done */
      }
      BR = (BR << 28);
      MB = AR;
      AR = BR / AR;
      if (AR != 0) {
        if ((AR & BIT7) != 0) {
          AR >>= 1;
        } else {
          SC--;
        }
        if (((SC & 0400) != 0) ^ ((SC & 0200) != 0) || SC == 0600)
          fxu_hold_set = 1;
        if (IR & 04) {
          AR++;
        }
        AR >>= 1;
        while ((AR & BIT9) == 0) {
          AR <<= 1;
          SC--;
        }
      } else if (flag1) {
        AR =  SMASK | BIT9;
        SC++;
        flag1 = 0;
      } else {
        AR = 0;
        SC = 0;
      }
      if (((SC & 0400) != 0) && !pi_cycle) {
        FLAGS |= OVR|FLTOVR|TRP1;
        if (!fxu_hold_set) {
          FLAGS |= FLTUND;
        }
        check_apr_irq();
      }
      if (flag1)  {
        AR = ((AR ^ MMASK) + 1) & MMASK;
        AR |= SMASK;
      }
      SCAD = SC ^ ((AR & SMASK) ? 0377 : 0);
      AR |= ((uint64)(SCAD & 0377)) << 27;
      break;
    case 0171:      /* FDVL */
      flag1 = flag3 = 0;
      SC = (int)((((BR & SMASK) ? 0777 : 0) ^ (BR >> 27)) & 0777);
      SC += (int)((((AR & SMASK) ? 0 : 0777) ^ (AR >> 27)) & 0777);
      SC = (SC + 0201) & 0777;
      FE = (int)((((BR & SMASK) ? 0777 : 0) ^ (BR >> 27)) & 0777) - 26;
      if (BR & SMASK) {
        MQ = (CM(MQ) + 1) & MMASK;
        BR = CM(BR);
        if (MQ == 0)
          BR = BR + 1;
        flag1 = 1;
        flag3 = 1;
      }
      MQ &= MMASK;
      if (AR & SMASK) {
        AR = CM(AR) + 1;
        flag1 = !flag1;
      }
      /* Clear exponents */
      AR &= MMASK;
      BR &= MMASK;
      /* Check if we need to fix things */
      if (BR >= (AR << 1)) {
        if (!pi_cycle)
          FLAGS |= OVR|NODIV|FLTOVR|TRP1;
        check_apr_irq();
        sac_inh = 1;
        break;      /* Done */
      }
      BR = (BR << 27) + MQ;
      MB = AR;
      AR <<= 27;
      AD = 0;
      if (BR < AR) {
        BR <<= 1;
        SC--;
        FE--;
      }
      for (SCAD = 0; SCAD < 27; SCAD++) {
        AD <<= 1;
        if (BR >= AR) {
          BR = BR - AR;
          AD |= 1;
        }
        BR <<= 1;
      }
      MQ = BR >> 28;
      AR = AD;
      SC++;
      if (AR != 0) {
        if ((AR & BIT8) != 0) {
          SC++;
          FE++;
          AR >>= 1;
        }
        while ((AR & BIT9) == 0) {
          AR <<= 1;
          SC--;
        }
        if (((SC & 0400) != 0) ^ ((SC & 0200) != 0))
          fxu_hold_set = 1;
        if (flag1)  {
          AR = (AR ^ MMASK) + 1;
          AR |= SMASK;
        }
      } else if (flag1) {
        FE = SC = 0;
      } else {
        AR = 0;
        SC = 0;
        FE = 0;
      }
      if (((SC & 0400) != 0) && !pi_cycle) {
        FLAGS |= OVR|FLTOVR|TRP1;
        if (!fxu_hold_set) {
          FLAGS |= FLTUND;
        }
        check_apr_irq();
      }
      SCAD = SC ^ ((AR & SMASK) ? 0377 : 0);
      AR &= SMASK|MMASK;
      AR |= ((uint64)(SCAD & 0377)) << 27;
      if (MQ != 0) {
        MQ &= MMASK;
        if (flag3) {
          MQ = (MQ ^ MMASK) + 1;
          MQ |= SMASK;
        }
        if (FE < 0 /*FE & 0400*/) {
          MQ = 0;
          FE = 0;
        } else
          FE ^= (flag3) ? 0377 : 0;
        MQ |= ((uint64)(FE & 0377)) << 27;
      }
      break;
      /* FWT */
    case 0200:     /* MOVE */
    case 0201:     /* MOVEI */
    case 0202:     /* MOVEM */
    case 0203:     /* MOVES */
    case 0204:     /* MOVS */
    case 0205:     /* MOVSI */
    case 0206:     /* MOVSM */
    case 0207:     /* MOVSS */
    case 0503:     /* HLLS */
    case 0543:     /* HRRS */
      break;
    case 0214:     /* MOVM */
    case 0215:     /* MOVMI */
    case 0216:     /* MOVMM */
    case 0217:     /* MOVMS */
      if ((AR & SMASK) == 0)
        break;
      /* Fall though */
    case 0210:     /* MOVN */
    case 0211:     /* MOVNI */
    case 0212:     /* MOVNM */
    case 0213:     /* MOVNS */
      flag1 = flag3 = 0;
      AD = CM(AR) + 1;
      if ((CCM(AR) + 1) & SMASK) {
        FLAGS |= CRY1;
        flag1 = 1;
      }
      if (AD & C1) {
        FLAGS |= CRY0;
        flag3 = 1;
      }
      if (flag1 != flag3 && !pi_cycle) {
        FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      AR = AD & FMASK;
      break;
    case 0220:      /* IMUL */
    case 0221:      /* IMULI */
    case 0222:      /* IMULM */
    case 0223:      /* IMULB */
    case 0224:      /* MUL */
    case 0225:      /* MULI */
    case 0226:      /* MULM */
    case 0227:      /* MULB */
      flag3 = 0;
      if (AR & SMASK) {
        AR = (CM(AR) + 1) & FMASK;
        flag3 = 1;
      }
      if (BR & SMASK) {
        BR = (CM(BR) + 1) & FMASK;
        flag3 = !flag3;
      }
      if ((AR == 0) || (BR == 0)) {
        AR = MQ = 0;
        break;
      }
      if (BR == SMASK)                /* Handle special case */
        flag3 = !flag3;
      MQ = AR * (BR & RMASK);         /* 36 * low 18 = 54 bits */
      AR = AR * ((BR >> 18) & RMASK); /* 36 * high 18 = 54 bits */
      MQ += (AR << 18) & LMASK;       /* low order bits */
      AR >>= 18;
      AR = (AR << 1) + (MQ >> 35);
      MQ &= CMASK;                   /* low order only has 35 bits */
      if ((IR & 4) == 0) {           /* IMUL */
        if (AR > flag3 && !pi_cycle) {
          FLAGS |= OVR|TRP1;
          check_apr_irq();
        }
        if (flag3) {
          MQ ^= CMASK;
          MQ++;
          MQ |= SMASK;
        }
        AR = MQ;
        break;
      }
      if ((AR & SMASK) != 0 && !pi_cycle) {
        FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      if (flag3) {
        AR ^= FMASK;
        MQ ^= CMASK;
        MQ += 1;
        if ((MQ & SMASK) != 0) {
          AR += 1;
          MQ &= CMASK;
        }
      }
      AR &= FMASK;
      MQ = (MQ & ~SMASK) | (AR & SMASK);
      if (BR == SMASK && (AR & SMASK))  /* Handle special case */
        FLAGS |= OVR;
      break;
    case 0230:       /* IDIV */
    case 0231:       /* IDIVI */
    case 0232:       /* IDIVM */
    case 0233:       /* IDIVB */
      flag1 = 0;
      flag3 = 0;
      if (BR & SMASK) {
        BR = (CM(BR) + 1) & FMASK;
        flag1 = !flag1;
      }
      if (BR == 0) {          /* Check for overflow */
        FLAGS |= OVR|NODIV; /* Overflow and No Divide */
        sac_inh=1;          /* Don't touch AC */
        check_apr_irq();
        break;              /* Done */
      }
      if (AR == SMASK && BR == 1) {
        FLAGS |= OVR|NODIV; /* Overflow and No Divide */
        sac_inh=1;          /* Don't touch AC */
        check_apr_irq();
        break;              /* Done */
      }
      if (AR & SMASK) {
        AR = (CM(AR) + 1) & FMASK;
        flag1 = !flag1;
        flag3 = 1;
      }
      MQ = AR % BR;
      AR = AR / BR;
      if (flag1)
        AR = (CM(AR) + 1) & FMASK;
      if (flag3)
        MQ = (CM(MQ) + 1) & FMASK;
      break;
    case 0234:       /* DIV */
    case 0235:       /* DIVI */
    case 0236:       /* DIVM */
    case 0237:       /* DIVB */
      flag1 = 0;
      if (AR & SMASK) {
        AD = (CM(MQ) + 1) & FMASK;
        MQ = AR;
        AR = AD;
        AD = (CM(MQ)) & FMASK;
        MQ = AR;
        AR = AD;
        if ((MQ & CMASK) == 0)
          AR = (AR + 1) & FMASK;
        flag1 = 1;
      }
      if (BR & SMASK)
        AD = (AR + BR) & FMASK;
      else
        AD = (AR + CM(BR) + 1) & FMASK;
      MQ = (MQ << 1) & FMASK;
      MQ |= (AD & SMASK) != 0;
      SC = 35;
      if ((AD & SMASK) == 0) {
        FLAGS |= OVR|NODIV|TRP1; /* Overflow and No Divide */
        i_flags = 0;
        sac_inh=1;
        check_apr_irq();
        break;      /* Done */
      }
      while (SC != 0) {
        if (((BR & SMASK) != 0) ^ ((MQ & 01) != 0))
          AD = (AR + CM(BR) + 1);
        else
          AD = (AR + BR);
        AR = (AD << 1) | ((MQ & SMASK) ? 1 : 0);
        AR &= FMASK;
        MQ = (MQ << 1) & FMASK;
        MQ |= (AD & SMASK) == 0;
        SC--;
      }
      if (((BR & SMASK) != 0) ^ ((MQ & 01) != 0))
        AD = (AR + CM(BR) + 1);
      else
        AD = (AR + BR);
      AR = AD & FMASK;
      MQ = (MQ << 1) & FMASK;
      MQ |= (AD & SMASK) == 0;
      if (AR & SMASK) {
        if (BR & SMASK)
          AD = (AR + CM(BR) + 1) & FMASK;
        else
          AD = (AR + BR) & FMASK;
        AR = AD;
      }
      if (flag1)
        AR = (CM(AR) + 1) & FMASK;
      if (flag1 ^ ((BR & SMASK) != 0)) {
        AD = (CM(MQ) + 1) & FMASK;
        MQ = AR;
        AR = AD;
      } else {
        AD = MQ;
        MQ = AR;
        AR = AD;
      }
      break;
      /* Shift */
    case 0240: /* ASH */
      SC = ((AB & RSIGN) ? (0377 ^ AB) + 1 : AB) & 0377;
      if (SC == 0)
        break;
      AD = (AR & SMASK) ? FMASK : 0;
      if (AB & RSIGN) {
        if (SC < 35)
          AR = ((AR >> SC) | (AD << (36 - SC))) & FMASK;
        else
          AR = AD;
      } else {
        if (((AD << SC) & ~CMASK) != ((AR << SC) & ~CMASK)) {
          FLAGS |= OVR|TRP1;
          check_apr_irq();
        }
        AR = ((AR << SC) & CMASK) | (AR & SMASK);
      }
      break;
    case 0241: /* ROT */
      SC = (AB & RSIGN) ?
        ((AB & 0377) ? (((0377 ^ AB) + 1) & 0377) : 0400)
        : (AB & 0377);
      if (SC == 0)
        break;
      SC = SC % 36;
      if (AB & RSIGN)
        SC = 36 - SC;
      AR = ((AR << SC) | (AR >> (36 - SC))) & FMASK;
      break;
    case 0242: /* LSH */
      SC = ((AB & RSIGN) ? (0377 ^ AB) + 1 : AB) & 0377;
      if (SC != 0) {
        if (SC > 36){
          AR = 0;
        } else if (AB & RSIGN) {
          AR = AR >> SC;
        } else {
          AR = (AR << SC) & FMASK;
        }
      }
      break;
    case 0243:  /* JFFO */
      SC = 0;
      if (AR != 0) {
        PC = AB;
        f_pc_inh = 1;
        SC = nlzero(AR);
      }
      set_reg(AC + 1, SC);
      break;
    case 0244: /* ASHC */
      SC = ((AB & RSIGN) ? (0377 ^ AB) + 1 : AB) & 0377;
      if (SC == 0)
        break;
      if (SC > 70)
        SC = 70;
      AD = (AR & SMASK) ? FMASK : 0;
      AR &= CMASK;
      MQ &= CMASK;
      if (AB & RSIGN) {
        if (SC >= 35) {
          MQ = ((AR >> (SC - 35)) | (AD << (70 - SC))) & FMASK;
          AR = AD;
        } else {
          MQ = (AD & SMASK) | (MQ >> SC) |
            ((AR << (35 - SC)) & CMASK);
          AR = ((AD & SMASK) |
                ((AR >> SC) | (AD << (35 - SC)))) & FMASK;
        }
      } else {
        if (SC >= 35) {
          if (((AD << SC) & ~CMASK) != ((AR << SC) & ~CMASK)) {
            FLAGS |= OVR|TRP1;
            check_apr_irq();
          }
          AR = (AD & SMASK) | ((MQ << (SC - 35)) & CMASK);
          MQ = (AD & SMASK);
        } else {
          if ((((AD & CMASK) << SC) & ~CMASK) != ((AR << SC) & ~CMASK)) {
            FLAGS |= OVR|TRP1;
            check_apr_irq();
          }
          AR = (AD & SMASK) | ((AR << SC) & CMASK) |
            (MQ >> (35 - SC));
          MQ = (AD & SMASK) | ((MQ << SC) & CMASK);
        }
      }
      break;
    case 0245: /* ROTC */
      SC = (AB & RSIGN) ?
        ((AB & 0377) ? (((0377 ^ AB) + 1) & 0377) : 0400)
        : (AB & 0377);
      if (SC == 0)
        break;
      SC = SC % 72;
      if (AB & RSIGN)
        SC = 72 - SC;
      if (SC >= 36) {
        AD = MQ;
        MQ = AR;
        AR = AD;
        SC -= 36;
      }
      AD = ((AR << SC) | (MQ >> (36 - SC))) & FMASK;
      MQ = ((MQ << SC) | (AR >> (36 - SC))) & FMASK;
      AR = AD;
      break;
    case 0246: /* LSHC */
      SC = ((AB & RSIGN) ? (0377 ^ AB) + 1 : AB) & 0377;
      if (SC == 0)
        break;
      if (SC > 71) {
        AR = 0;
        MQ = 0;
      } else {
        if (SC > 36) {
          if (AB & RSIGN) {
            MQ = AR;
            AR = 0;
          } else {
            AR = MQ;
            MQ = 0;
          }
          SC -= 36;
        }
        if (AB & RSIGN) {
          MQ = ((MQ >> SC) | (AR << (36 - SC))) & FMASK;
          AR = AR >> SC;
        } else {
          AR = ((AR << SC) | (MQ >> (36 - SC))) & FMASK;
          MQ = (MQ << SC) & FMASK;
        }
      }
      break;
      /* Branch */
    case 0250:  /* EXCH */
      MB = AR;
      if (Mem_write(0, 0)) {
        goto last;
      }
      set_reg(AC, BR);
      break;
    case 0251: /* BLT */
      BR = AB;
      do {
        AIO_CHECK_EVENT;                    /* queue async events */
        if (sim_interval <= 0) {
          if ((reason = sim_process_event()) != SCPE_OK) {
            f_pc_inh = 1;
            f_load_pc = 0;
            f_inst_fetch = 0;
            set_reg(AC, AR);
            break;
          }
        }
        /* Allow for interrupt */
        if (pi_pending) {
          pi_rq = check_irq_level();
          if (pi_rq) {
            f_pc_inh = 1;
            f_load_pc = 0;
            f_inst_fetch = 0;
            set_reg(AC, AR);
            break;
          }
        }
        AB = (AR >> 18) & RMASK;
        if (Mem_read(0, 0, 0)) {
          f_pc_inh = 1;
          goto last;
        }
        AB = (AR & RMASK);
        if (Mem_write(0, 0)) {
          f_pc_inh = 1;
          goto last;
        }
        AD = (AR & RMASK) + CM(BR) + 1;
        AR = AOB(AR);
      } while ((AD & C1) == 0);
      break;
    case 0252: /* AOBJP */
      AR = AOB(AR);
      if ((AR & SMASK) == 0) {
        PC_CHANGE
          PC = AB;
        f_pc_inh = 1;
      }
      break;
    case 0253: /* AOBJN */
      AR = AOB(AR);
      if ((AR & SMASK) != 0) {
        PC_CHANGE
          PC = AB;
        f_pc_inh = 1;
      }
      break;
    case 0254: /* JRST */      /* AR Frm PC */
      if (uuo_cycle | pi_cycle) {
        FLAGS &= ~USER; /* Clear USER */
      }
      /* JEN */
      if (AC & 010) { /* Restore interrupt level. */
        if ((FLAGS & (USER|USERIO)) == USER) {
          goto muuo;
        } else {
          pi_restore = 1;
        }
      }
      /* HALT */
      if (AC & 04) {
        if ((FLAGS & (USER|USERIO)) == USER) {
          goto muuo;
        } else {
          reason = STOP_HALT;
        }
      }
      PC = AR & RMASK;
      PC_CHANGE
        /* JRSTF */
        if (AC & 02) {
          FLAGS &= ~(OVR|NODIV|FLTUND|BYTI|FLTOVR|CRY1|CRY0|TRP1|TRP2|PCHNG|ADRFLT);
          AR >>= 23; /* Move into position */
          /* If executive mode, copy USER and UIO */
          if ((FLAGS & USER) == 0)
            FLAGS |= AR & (USER|USERIO);
          /* Can always clear UIO */
          if ((AR & USERIO) == 0) {
            FLAGS &= ~USERIO;
          }
          FLAGS |= AR & (OVR|NODIV|FLTUND|BYTI|FLTOVR|CRY1|CRY0|TRP1|TRP2|PCHNG|ADRFLT);
          check_apr_irq();
        }
      if (AC & 01) {  /* Enter User Mode */
        FLAGS |= USER;
      }
      f_pc_inh = 1;
      break;
    case 0255: /* JFCL */
      if ((FLAGS >> 9) & AC) {
        PC = AR & RMASK;
        f_pc_inh = 1;
      }
      FLAGS &=  037777 ^ (AC << 9);
      break;
    case 0256: /* XCT */
      f_load_pc = 0;
      f_pc_inh = 1;
      xct_flag = 0;
      if ((FLAGS & USER) == 0) // executive ? XCTR relocated when AC≠0
        xct_flag = AC;
      break;
    case 0257:  /* U257, CONS, MAP */
      break;
      /* Stack, JUMP */
    case 0260:  /* PUSHJ */     /* AR Frm PC */
      MB = (((uint64)(FLAGS) << 23) & LMASK) | ((PC + !pi_cycle) & RMASK);
      AR = AOB(AR);
      FLAGS &= ~ (BYTI|ADRFLT|TRP1|TRP2);
      if (AR & C1) {
        push_ovf = 1;
        check_apr_irq();
      }
      AB = AR & RMASK;
      if (hst_lnt)
        hst[hst_p].mb = MB;
      if (Mem_write(uuo_cycle | pi_cycle, 0))
        goto last;
      if (uuo_cycle | pi_cycle) {
        FLAGS &= ~(USER); /* Clear USER */
      }
      PC = BR & RMASK;
      PC_CHANGE
        f_pc_inh = 1;
      break;
    case 0261: /* PUSH */
      AR = AOB(AR);
      if (AR & C1) {
        push_ovf = 1;
        check_apr_irq();
      }
      AB = AR & RMASK;
      MB = BR;
      if (hst_lnt)
        hst[hst_p].mb = MB;
      if (Mem_write(0, 0))
        goto last;
      break;
    case 0262: /* POP */
      /* Fetch top of stack */
      AB = AR & RMASK;
      if (Mem_read(0, 0, 0))
        goto last;
      if (hst_lnt)
        hst[hst_p].mb = MB;
      /* Save in location */
      AB = BR & RMASK;
      if (Mem_write(0, 0))
        goto last;
      AR = SOB(AR);
      if ((AR & C1) == 0) {
        push_ovf = 1;
        check_apr_irq();
      }
      break;
    case 0263: /* POPJ */
      AB = AR & RMASK;
      AR = SOB(AR);
      if (Mem_read(0, 0, 0))
        goto last;
      if (hst_lnt) {
        hst[hst_p].ea = AB;
        hst[hst_p].mb = MB;
      }
      f_pc_inh = 1;
      PC_CHANGE
        PC = MB & RMASK;
      if ((AR & C1) == 0) {
        push_ovf = 1;
        check_apr_irq();
      }
      break;
    case 0264: /* JSR */       /* AR Frm PC */
      MB = (((uint64)(FLAGS) << 23) & LMASK) | ((PC + !pi_cycle) & RMASK);
      if (uuo_cycle | pi_cycle) {
        FLAGS &= ~(USER); /* Clear USER */
      }
      if (Mem_write(0, 0))
        goto last;
      FLAGS &= ~ (BYTI|ADRFLT|TRP1|TRP2);
      PC_CHANGE
        PC = (AR + 1) & RMASK;
      f_pc_inh = 1;
      break;
    case 0265: /* JSP */       /* AR Frm PC */
      AD = (((uint64)(FLAGS) << 23) & LMASK) |
        ((PC + !pi_cycle) & RMASK);
      FLAGS &= ~ (BYTI|ADRFLT|TRP1|TRP2);
      if (uuo_cycle | pi_cycle) {
        FLAGS &= ~(USER); /* Clear USER */
      }
      PC_CHANGE
        PC = AR & RMASK;
      AR = AD;
      f_pc_inh = 1;
      break;
    case 0266: /* JSA */       /* AR Frm PC */
      set_reg(AC, (AR << 18) | ((PC + 1) & RMASK));
      if (uuo_cycle | pi_cycle) {
        FLAGS &= ~(USER); /* Clear USER */
      }
      PC_CHANGE
        PC = AR & RMASK;
      AR = BR;
      break;
    case 0267: /* JRA */
      AD = AB;
      AB = (get_reg(AC) >> 18) & RMASK;
      if (Mem_read(uuo_cycle | pi_cycle, 0, 0))
        goto last;
      set_reg(AC, MB);
      PC_CHANGE
        PC = AD & RMASK;
      f_pc_inh = 1;
      break;
    case 0270: /* ADD */
    case 0271: /* ADDI */
    case 0272: /* ADDM */
    case 0273: /* ADDB */
      flag1 = flag3 = 0;
      if (((AR & CMASK) + (BR & CMASK)) & SMASK) {
        FLAGS |= CRY1;
        flag1 = 1;
      }
      AR = AR + BR;
      if (AR & C1) {
        if (!pi_cycle)
          FLAGS |= CRY0;
        flag3 = 1;
      }
      if (flag1 != flag3) {
        if (!pi_cycle)
          FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      break;
    case 0274: /* SUB */
    case 0275: /* SUBI */
    case 0276: /* SUBM */
    case 0277: /* SUBB */
      flag1 = flag3 = 0;
      if ((CCM(AR) + (BR & CMASK) + 1) & SMASK) {
        FLAGS |= CRY1;
        flag1 = 1;
      }
      AR = CM(AR) + BR + 1;
      if (AR & C1) {
        if (!pi_cycle)
          FLAGS |= CRY0;
        flag3 = 1;
      }
      if (flag1 != flag3) {
        if (!pi_cycle)
          FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      break;
    case 0300:    /* CAI   */
    case 0301:    /* CAIL  */
    case 0302:    /* CAIE  */
    case 0303:    /* CAILE */
    case 0304:    /* CAIA  */
    case 0305:    /* CAIGE */
    case 0306:    /* CAIN  */
    case 0307:    /* CAIG  */
    case 0310:    /* CAM   */
    case 0311:    /* CAML  */
    case 0312:    /* CAME  */
    case 0313:    /* CAMLE */
    case 0314:    /* CAMA  */
    case 0315:    /* CAMGE */
    case 0316:    /* CAMN  */
    case 0317:    /* CAMG  */
      f = 0;
      AD = (CM(AR) + BR) + 1;
      if (((BR & SMASK) != 0) && (AR & SMASK) == 0)
        f = 1;
      if (((BR & SMASK) == (AR & SMASK)) &&
          (AD & SMASK) != 0)
        f = 1;
      goto skip_op;
    case 0320:    /* JUMP   */
    case 0321:    /* JUMPL  */
    case 0322:    /* JUMPE  */
    case 0323:    /* JUMPLE */
    case 0324:    /* JUMPA  */
    case 0325:    /* JUMPGE */
    case 0326:    /* JUMPN  */
    case 0327:    /* JUMPG  */
      AD = AR;
      f = ((AD & SMASK) != 0);
      goto jump_op;                   /* JUMP, SKIP */
    case 0330:    /* SKIP   */
    case 0331:    /* SKIPL  */
    case 0332:    /* SKIPE  */
    case 0333:    /* SKIPLE */
    case 0334:    /* SKIPA  */
    case 0335:    /* SKIPGE */
    case 0336:    /* SKIPN  */
    case 0337:    /* SKIPG  */
      AD = AR;
      f = ((AD & SMASK) != 0);
      goto skip_op;                   /* JUMP, SKIP */
    case 0340:     /* AOJ   */
    case 0341:     /* AOJL  */
    case 0342:     /* AOJE  */
    case 0343:     /* AOJLE */
    case 0344:     /* AOJA  */
    case 0345:     /* AOJGE */
    case 0346:     /* AOJN  */
    case 0347:     /* AOJG  */
    case 0360:     /* SOJ   */
    case 0361:     /* SOJL  */
    case 0362:     /* SOJE  */
    case 0363:     /* SOJLE */
    case 0364:     /* SOJA  */
    case 0365:     /* SOJGE */
    case 0366:     /* SOJN  */
    case 0367:     /* SOJG  */
      flag1 = flag3 = 0;
      AD = (IR & 020) ? FMASK : 1;
      if (((AR & CMASK) + (AD & CMASK)) & SMASK) {
        if (!pi_cycle)
          FLAGS |= CRY1;
        flag1 = 1;
      }
      AD = AR + AD;
      if (AD & C1) {
        if (!pi_cycle)
          FLAGS |= CRY0;
        flag3 = 1;
      }
      if (flag1 != flag3  && !pi_cycle) {
        FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      f = ((AD & SMASK) != 0);
    jump_op:
      AD &= FMASK;
      AR = AD;
      f |= ((AD == 0) << 1);
      f = f & IR;
      if (((IR & 04) != 0) == (f == 0)) {
        PC_CHANGE
          PC = AB;
        f_pc_inh = 1;
      }
      break;
    case 0350:     /* AOS   */
    case 0351:     /* AOSL  */
    case 0352:     /* AOSE  */
    case 0353:     /* AOSLE */
    case 0354:     /* AOSA  */
    case 0355:     /* AOSGE */
    case 0356:     /* AOSN  */
    case 0357:     /* AOSG  */
    case 0370:     /* SOS   */
    case 0371:     /* SOSL  */
    case 0372:     /* SOSE  */
    case 0373:     /* SOSLE */
    case 0374:     /* SOSA  */
    case 0375:     /* SOSGE */
    case 0376:     /* SOSN  */
    case 0377:     /* SOSG  */
      flag1 = flag3 = 0;
      AD = (IR & 020) ? FMASK : 1;
      if (((AR & CMASK) + (AD & CMASK)) & SMASK) {
        if (!pi_cycle)
          FLAGS |= CRY1;
        flag1 = 1;
      }
      AD = AR + AD;
      if (AD & C1) {
        if (!pi_cycle)
          FLAGS |= CRY0;
        flag3 = 1;
      }
      if (flag1 != flag3 && !pi_cycle) {
        FLAGS |= OVR|TRP1;
        check_apr_irq();
      }
      f = ((AD & SMASK) != 0);
    skip_op:
      AD &= FMASK;
      AR = AD;
      f |= ((AD == 0) << 1);
      f = f & IR;
      if (((IR & 04) != 0) == (f == 0)) {
        PC_CHANGE
          PC = (PC + 1) & RMASK;
      }
      break;
      /* Bool */
    case 0400:    /* SETZ  */
    case 0401:    /* SETZI */
    case 0402:    /* SETZM */
    case 0403:    /* SETZB */
      AR = 0;                   /* SETZ */
      break;
    case 0404:    /* AND  */
    case 0405:    /* ANDI */
    case 0406:    /* ANDM */
    case 0407:    /* ANDB */
      AR = AR & BR;             /* AND */
      break;
    case 0410:    /* ANDCA  */
    case 0411:    /* ANDCAI */
    case 0412:    /* ANDCAM */
    case 0413:    /* ANDCAB */
      AR = AR & CM(BR);         /* ANDCA */
      break;
    case 0415:    /* SETMI */
    case 0414:    /* SETM  */
    case 0416:    /* SETMM */
    case 0417:    /* SETMB */
      /* SETM */
      break;
    case 0420:    /* ANDCM  */
    case 0421:    /* ANDCMI */
    case 0422:    /* ANDCMM */
    case 0423:    /* ANDCMB */
      AR = CM(AR) & BR;         /* ANDCM */
      break;
    case 0424:    /* SETA  */
    case 0425:    /* SETAI */
    case 0426:    /* SETAM */
    case 0427:    /* SETAB */
      AR = BR;                  /* SETA */
      break;
    case 0430:    /* XOR  */
    case 0431:    /* XORI */
    case 0432:    /* XORM */
    case 0433:    /* XORB */
      AR = AR ^ BR;             /* XOR */
      break;
    case 0434:    /* IOR  */
    case 0435:    /* IORI */
    case 0436:    /* IORM */
    case 0437:    /* IORB */
      AR = CM(CM(AR) & CM(BR)); /* IOR */
      break;
    case 0440:    /* ANDCB  */
    case 0441:    /* ANDCBI */
    case 0442:    /* ANDCBM */
    case 0443:    /* ANDCBB */
      AR = CM(AR) & CM(BR);     /* ANDCB */
      break;
    case 0444:    /* EQV  */
    case 0445:    /* EQVI */
    case 0446:    /* EQVM */
    case 0447:    /* EQVB */
      AR = CM(AR ^ BR);         /* EQV */
      break;
    case 0450:    /* SETCA  */
    case 0451:    /* SETCAI */
    case 0452:    /* SETCAM */
    case 0453:    /* SETCAB */
      AR = CM(BR);              /* SETCA */
      break;
    case 0454:    /* ORCA  */
    case 0455:    /* ORCAI */
    case 0456:    /* ORCAM */
    case 0457:    /* ORCAB */
      AR = CM(CM(AR) & BR);     /* ORCA */
      break;
    case 0460:    /* SETCM  */
    case 0461:    /* SETCMI */
    case 0462:    /* SETCMM */
    case 0463:    /* SETCMB */
      AR = CM(AR);              /* SETCM */
      break;
    case 0464:    /* ORCM  */
    case 0465:    /* ORCMI */
    case 0466:    /* ORCMM */
    case 0467:    /* ORCMB */
      AR = CM(AR & CM(BR));     /* ORCM */
      break;
    case 0470:    /* ORCB  */
    case 0471:    /* ORCBI */
    case 0472:    /* ORCBM */
    case 0473:    /* ORCBB */
      AR = CM(AR & BR);         /* ORCB */
      break;
    case 0474:    /* SETO  */
    case 0475:    /* SETOI */
    case 0476:    /* SETOM */
    case 0477:    /* SETOB */
      AR = FMASK;               /* SETO */
      break;
    case 0547:    /* HLRS */
      BR = SWAP_AR;
      /* Fall Through */
    case 0501:    /* HLLI */
    case 0500:    /* HLL  */
    case 0502:    /* HLLM */
    case 0504:    /* HRL  */
    case 0505:    /* HRLI */
    case 0506:    /* HRLM */
      AR = (AR & LMASK) | (BR & RMASK);
      break;
    case 0510:    /* HLLZ  */
    case 0511:    /* HLLZI */
    case 0512:    /* HLLZM */
    case 0513:    /* HLLZS */
    case 0514:    /* HRLZ  */
    case 0515:    /* HRLZI */
    case 0516:    /* HRLZM */
    case 0517:    /* HRLZS */
      AR = (AR & LMASK);
      break;
    case 0520:    /* HLLO  */
    case 0521:    /* HLLOI */
    case 0522:    /* HLLOM */
    case 0523:    /* HLLOS */
    case 0524:    /* HRLO  */
    case 0525:    /* HRLOI */
    case 0526:    /* HRLOM */
    case 0527:    /* HRLOS */
      AR = (AR & LMASK) | RMASK;
      break;
    case 0530:    /* HLLE  */
    case 0531:    /* HLLEI */
    case 0532:    /* HLLEM */
    case 0533:    /* HLLES */
    case 0534:    /* HRLE  */
    case 0535:    /* HRLEI */
    case 0536:    /* HRLEM */
    case 0537:    /* HRLES */
      AD = ((AR & SMASK) != 0) ? RMASK : 0;
      AR = (AR & LMASK) | AD;
      break;
    case 0507:    /* HRLS */
      BR = SWAP_AR;
      /* Fall Through */
    case 0540:    /* HRR  */
    case 0541:    /* HRRI */
    case 0542:    /* HRRM */
    case 0544:    /* HLR  */
    case 0545:    /* HLRI */
    case 0546:    /* HLRM */
      AR = (BR & LMASK) | (AR & RMASK);
      break;
    case 0550:    /* HRRZ  */
    case 0551:    /* HRRZI */
    case 0552:    /* HRRZM */
    case 0553:    /* HRRZS */
    case 0554:    /* HLRZ  */
    case 0555:    /* HLRZI */
    case 0556:    /* HLRZM */
    case 0557:    /* HLRZS */
      AR = (AR & RMASK);
      break;
    case 0560:    /* HRRO  */
    case 0561:    /* HRROI */
    case 0562:    /* HRROM */
    case 0563:    /* HRROS */
    case 0564:    /* HLRO  */
    case 0565:    /* HLROI */
    case 0566:    /* HLROM */
    case 0567:    /* HLROS */
      AR = LMASK | (AR & RMASK);
      break;
    case 0570:    /* HRRE  */
    case 0571:    /* HRREI */
    case 0572:    /* HRREM */
    case 0573:    /* HRRES */
    case 0574:    /* HLRE  */
    case 0575:    /* HLREI */
    case 0576:    /* HLREM */
    case 0577:    /* HLRES */
      AD = ((AR & RSIGN) != 0) ? LMASK: 0;
      AR = AD | (AR & RMASK);
      break;
    case 0600:     /* TRN  */
    case 0601:     /* TLN  */
    case 0602:     /* TRNE */
    case 0603:     /* TLNE */
    case 0604:     /* TRNA */
    case 0605:     /* TLNA */
    case 0606:     /* TRNN */
    case 0607:     /* TLNN */
    case 0610:     /* TDN  */
    case 0611:     /* TSN  */
    case 0612:     /* TDNE */
    case 0613:     /* TSNE */
    case 0614:     /* TDNA */
    case 0615:     /* TSNA */
    case 0616:     /* TDNN */
    case 0617:     /* TSNN */
      MQ = AR;            /* N */
      goto test_op;
    case 0620:     /* TRZ  */
    case 0621:     /* TLZ  */
    case 0622:     /* TRZE */
    case 0623:     /* TLZE */
    case 0624:     /* TRZA */
    case 0625:     /* TLZA */
    case 0626:     /* TRZN */
    case 0627:     /* TLZN */
    case 0630:     /* TDZ  */
    case 0631:     /* TSZ  */
    case 0632:     /* TDZE */
    case 0633:     /* TSZE */
    case 0634:     /* TDZA */
    case 0635:     /* TSZA */
    case 0636:     /* TDZN */
    case 0637:     /* TSZN */
      MQ = CM(AR) & BR;   /* Z */
      goto test_op;
    case 0640:     /* TRC  */
    case 0641:     /* TLC  */
    case 0642:     /* TRCE */
    case 0643:     /* TLCE */
    case 0644:     /* TRCA */
    case 0645:     /* TLCA */
    case 0646:     /* TRCN */
    case 0647:     /* TLCN */
    case 0650:     /* TDC  */
    case 0651:     /* TSC  */
    case 0652:     /* TDCE */
    case 0653:     /* TSCE */
    case 0654:     /* TDCA */
    case 0655:     /* TSCA */
    case 0656:     /* TDCN */
    case 0657:     /* TSCN */
      MQ = AR ^ BR;       /* C */
      goto test_op;
    case 0660:     /* TRO  */
    case 0661:     /* TLO  */
    case 0662:     /* TROE */
    case 0663:     /* TLOE */
    case 0664:     /* TROA */
    case 0665:     /* TLOA */
    case 0666:     /* TRON */
    case 0667:     /* TLON */
    case 0670:     /* TDO  */
    case 0671:     /* TSO  */
    case 0672:     /* TDOE */
    case 0673:     /* TSOE */
    case 0674:     /* TDOA */
    case 0675:     /* TSOA */
    case 0676:     /* TDON */
    case 0677:     /* TSON */
      MQ = AR | BR;       /* O */
    test_op:
      AR &= BR;
      f = ((AR == 0) & ((IR >> 1) & 1)) ^ ((IR >> 2) & 1);
      if (f) {
        PC_CHANGE
          PC = (PC + 1) & RMASK;
      }
      AR = MQ;
      break;
      /* IOT */
    case 0700: case 0701: case 0702: case 0703:
    case 0704: case 0705: case 0706: case 0707:
    case 0710: case 0711: case 0712: case 0713:
    case 0714: case 0715: case 0716: case 0717:
    case 0720: case 0721: case 0722: case 0723:
    case 0724: case 0725: case 0726: case 0727:
    case 0730: case 0731: case 0732: case 0733:
    case 0734: case 0735: case 0736: case 0737:
    case 0740: case 0741: case 0742: case 0743:
    case 0744: case 0745: case 0746: case 0747:
    case 0750: case 0751: case 0752: case 0753:
    case 0754: case 0755: case 0756: case 0757:
    case 0760: case 0761: case 0762: case 0763:
    case 0764: case 0765: case 0766: case 0767:
    case 0770: case 0771: case 0772: case 0773:
    case 0774: case 0775: case 0776: case 0777:
      if ((FLAGS & (USER|USERIO)) == USER && !pi_cycle) {
        /* User and not User I/O */
        goto muuo;
      } else {
        int d = ((IR & 077) << 1) | ((AC & 010) != 0);
      fetch_opr:
        switch(AC & 07) {
        case 0:     /* 00 BLKI */
        case 2:     /* 10 BLKO */
          if (Mem_read(pi_cycle, 0, 0))
            goto last;
          AR = MB;
          if (hst_lnt) {
            hst[hst_p].mb = AR;
          }
          AC |= 1;    /* Make into DATAI/DATAO */
          AR = AOB(AR);
          if (AR & C1) {
            pi_ov = 1;
          }
          else if (!pi_cycle)
            PC = (PC + 1) & RMASK;
          AR &= FMASK;
          MB = AR;
          if (Mem_write(pi_cycle, 0))
            goto last;
          AB = AR & RMASK;
          goto fetch_opr;
        case 1:     /* 04 DATAI */
          dev_tab[d](DATAI|(d<<2), &AR);
          MB = AR;
          if (Mem_write(pi_cycle, 0))
            goto last;
          break;
        case 3:     /* 14 DATAO */
          if (Mem_read(pi_cycle, 0, 0))
            goto last;
          AR = MB;
          dev_tab[d](DATAO|(d<<2), &AR);
          break;
        case 4:     /* 20 CONO */
          dev_tab[d](CONO|(d<<2), &AR);
          break;
        case 5:     /* 24 CONI */
          dev_tab[d](CONI|(d<<2), &AR);
          MB = AR;
          if (Mem_write(pi_cycle, 0))
            goto last;
          break;
        case 6:     /* 30 CONSZ */
          dev_tab[d](CONI|(d<<2), &AR);
          AR &= AB;
          if (AR == 0)
            PC = (PC + 1) & RMASK;
          break;
        case 7:     /* 34 CONSO */
          dev_tab[d](CONI|(d<<2), &AR);
          AR &= AB;
          if (AR != 0)
            PC = (PC + 1) & RMASK;
          break;
        }
      }
      break;
    }

    // STORE pseudo registers based on opcode-flags
    AR &= FMASK;
    if (!sac_inh && (i_flags & (SCE|FCEPSE))) {
      MB = AR;
      if (Mem_write(0, 0)) {
        goto last;
      }
    }
    if (!sac_inh && ((i_flags & SAC) || ((i_flags & SACZ) && AC != 0)))
      set_reg(AC, AR);    /* blank, I, B */
    if (!sac_inh && (i_flags & SAC2)) {
      MQ &= FMASK;
      set_reg(AC+1, MQ);
    }
    if (hst_lnt && PC >= 020) {
      hst[hst_p].fmb = (i_flags & SAC) ? AR: MB;
    }
    
  last:
    if (!f_pc_inh && !pi_cycle) {
      PC = (PC + 1) & RMASK;
    }
    /* Dismiss an interrupt */
    if (pi_cycle) {
      if ((IR & 0700) == 0700 && ((AC & 04) == 0)) {
        pi_hold = pi_ov;
        if ((!pi_hold) & f_inst_fetch) {
          pi_cycle = 0;
        } else {
          AB = 040 | (pi_enc << 1) | maoff | pi_ov;
          Mem_read(1, 0, 1);
          goto no_fetch;
        }
      } else if (pi_hold && !f_pc_inh) {
        if ((IR & 0700) == 0700) {
          (void)check_irq_level();
        }
        AB = 040 | (pi_enc << 1) | maoff | pi_ov;
        pi_ov = 0;
        pi_hold = 0;
        Mem_read(1, 0, 1);
        goto no_fetch;
      } else {
        if (f_pc_inh)
          set_pi_hold(); /* Hold off all lower interrupts */
        pi_cycle = 0;
        f_inst_fetch = 1;
        f_load_pc = 1;
      }
    }
    if (pi_restore) {
      restore_pi_hold();
      pi_restore = 0;
    }
    sim_interval--;
    if (!pi_cycle && instr_count != 0 && --instr_count == 0) {
      return SCPE_STEP;
    }
  }
  /* Should never get here */
  return reason;
}
t_stat
rtc_srv(UNIT * uptr)
{
  int32 t;
  static int n=0;
  t = sim_rtcn_calb (rtc_tps, TMR_RTC);
  sim_activate_after(uptr, 1000000/rtc_tps);
  tmxr_poll = t/2;
  clk_flg = 1;
  if (clk_en) {
    sim_debug(DEBUG_CONO, &cpu_dev, "CONO timer\n");
    set_interrupt(4, clk_irq);
  }
  // bamboo // if(n++ % 60 ==0)fprintf(stderr,"x\r\n");
  // bamboo // fprintf(stderr,".");
  hark(); // poll III and DDD sockets for keyboard events
  return SCPE_OK;
}
/*
 * This sequence of instructions is a mix that hopefully
 * represents a resonable instruction set that is a close
 * estimate to the normal calibrated result.
 */
static const char *pdp10_clock_precalibrate_commands[] = {
  "-m 100 ADDM 0,110",
  "-m 101 ADDI 0,1",
  "-m 102 JRST 100",
  "PC 100",
  NULL};
/* Reset routine */
t_stat cpu_reset (DEVICE *dptr)
{
  int     i;
  sim_debug(DEBUG_CONO, dptr, "CPU reset\n");
  BYF5 = uuo_cycle = 0;
  Pl = Ph = 01777;
  Rl = Rh = Pflag = 0;
  push_ovf = mem_prot = 0;
  nxm_flag = clk_flg = 0;
  PIR = PIH = PIE = pi_enable = parity_irq = 0;
  pi_pending = pi_enc = apr_irq = 0;
  ov_irq =fov_irq =clk_en =clk_irq = 0;
  pi_restore = pi_hold = 0;
  FLAGS = 0;
  for(i=0; i < 128; dev_irq[i++] = 0);
  sim_brk_types = SWMASK('E') | SWMASK('W') | SWMASK('R');
  sim_brk_dflt = SWMASK ('E');
  sim_clock_precalibrate_commands = pdp10_clock_precalibrate_commands;
  sim_vm_initial_ips = 4 * SIM_INITIAL_IPS;
  sim_rtcn_init_unit (&cpu_unit[0], cpu_unit[0].wait, TMR_RTC);
  sim_activate(&cpu_unit[0], 10000);
  sim_vm_interval_units = "cycles";
  sim_vm_step_unit = "instruction";
  return SCPE_OK;
}
/* Memory examine */
t_stat cpu_ex (t_value *vptr, t_addr ea, UNIT *uptr, int32 sw)
{
  if (vptr == NULL)
    return SCPE_ARG;
  if (ea < 020)
    *vptr = FM[ea] & FMASK;
  else {
    if (ea >= MEMSIZE)
      return SCPE_NXM;
    *vptr = M[ea] & FMASK;
  }
  return SCPE_OK;
}
/* Memory deposit */
t_stat cpu_dep (t_value val, t_addr ea, UNIT *uptr, int32 sw)
{
  if (ea < 020)
    FM[ea] = val & FMASK;
  else {
    if (ea >= MEMSIZE)
      return SCPE_NXM;
    M[ea] = val & FMASK;
  }
  return SCPE_OK;
}
/* Memory size change */
t_stat cpu_set_size (UNIT *uptr, int32 sval, CONST char *cptr, void *desc)
{
  int32 i;
  int32 val = (int32)sval;
  if ((val <= 0) || ((val * 16 * 1024) > MAXMEMSIZE))
    return SCPE_ARG;
  val = val * 16 * 1024;
  if (val < (int32)MEMSIZE) {
    uint64 mc = 0;
    for (i = val-1; i < (int32)MEMSIZE; i++)
      mc = mc | M[i];
    if ((mc != 0) && (!get_yn ("Really truncate memory [N]?", FALSE)))
      return SCPE_OK;
  }
  for (i = (int32)MEMSIZE; i < val; i++)
    M[i] = 0;
  cpu_unit[0].capac = (uint32)val;
  return SCPE_OK;
}
/* Build device dispatch table */
t_bool build_dev_tab (void)
{
  DEVICE *dptr;
  DIB    *dibp;
  uint32 i, j, d;
  int     rh20;
  int     rh_idx;
  /* Set trap offset based on MAOFF flag */
  maoff = (cpu_unit[0].flags & UNIT_MAOFF)? 0100 : 0;
  /* Set up memory access routines. */
  Mem_read = &Mem_read_waits;
  Mem_write = &Mem_write_waits;
  /* Clear device and interrupt table */
  for (i = 0; i < 128; i++) {
    dev_tab[i] = &null_dev;
    dev_irqv[i] = NULL;
  }
  /* Set up basic devices. */
  dev_tab[0] = &dev_apr;
  dev_tab[1] = &dev_pi;
  /* Assign all devices (no Mass Bus RH10s on KA in 1974 */
  for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
    dibp = (DIB *) dptr->ctxt;
    if (dibp && !(dptr->flags & DEV_DIS)) {             /* enabled? */
      for (j = 0; j < dibp->num_devs; j++) {         /* loop thru disp */
        if (dibp->io) {                           /* any dispatch? */
          d = dibp->dev_num;
          if (d & (RH10_DEV|RH20_DEV))         /* Skip RH10 & RH20 devices */
            continue;
          if (dev_tab[(d >> 2) + j] != &null_dev) {
            /* already filled? */
            sim_printf ("%s device number conflict at %02o\n",
                        sim_dname (dptr), d + (j << 2));
            return TRUE;
          }
          dev_tab[(d >> 2) + j] = dibp->io;    /* fill */
          dev_irqv[(d >> 2) + j] = dibp->irq;
        }                                         /* end if dsp */
      }                                            /* end for j */
    }                                               /* end if enb */
  }                                                   /* end for i */
  return FALSE;
}
/* Set history */
t_stat cpu_set_hist (UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  int32 i, lnt;
  t_stat r;
  if (cptr == NULL) {
    for (i = 0; i < hst_lnt; i++)
      hst[i].pc = 0;
    hst_p = 0;
    return SCPE_OK;
  }
  lnt = (int32) get_uint (cptr, 10, HIST_MAX, &r);
  if ((r != SCPE_OK) || (lnt && (lnt < HIST_MIN)))
    return SCPE_ARG;
  hst_p = 0;
  if (hst_lnt) {
    free (hst);
    hst_lnt = 0;
    hst = NULL;
  }
  if (lnt) {
    hst = (InstHistory *) calloc (lnt, sizeof (InstHistory));
    if (hst == NULL)
      return SCPE_MEM;
    hst_lnt = lnt;
  }
  return SCPE_OK;
}
// Show history
t_stat cpu_show_hist (FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
  //                    10        20        30        40        50        60        70        80        90       100       110       120       130       140       150
  //            123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.
  char *banner="jobname source#line         ␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣;uPI      PC / IR AiX     Y AC        c(AC) ␣      Y             AR        Result   flags\n";
  //            system  DSKSER#0000  start: ␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣;...  777000 / 777000777000 00 000111222333 ␣ 123456   000111222333  000111222333  700000
  int32 k, di, lnt;
  char *cptr = (char *) desc;
  t_stat r;
  t_value sim_eval;
  InstHistory *h;
  if (hst_lnt == 0)                                       /* enabled? */
    return SCPE_NOFNC;
  if (cptr) {
    lnt = (int32) get_uint (cptr, 10, hst_lnt, &r);
    if ((r != SCPE_OK) || (lnt == 0))
      return SCPE_ARG;
  }
  else lnt = hst_lnt;

  verbose=1;
  if(!labtab[0200]) read_labtab();
  if(!urltab[0200]) read_urltab();
  
  // length
  di = hst_p - lnt; // work forward
  if (di < 0)
    di = di + hst_lnt;  
  for (k = 0; k < lnt; k++) {      // print specified
    if((k&63)==0) fputs(banner,st);
    h = &hst[(++di) % hst_lnt];    // entry pointer
    if (h->pc & HIST_PC) {         // instruction?
      int pc = h->pc & 0777777;
      
      if((pc <= 0165332) && ((h->flags&USER)==0)){ // system executive J17SYS.DMP symbol tables
      /* JOB */         fprintf(st,"SYSTEM  ");
      /* SRC */         fprintf(st,"%11.11s ",  urltab[pc] ? urltab[pc] : "" );
      /* TAG */         fprintf(st,"%6.6s%1s ", labtab[pc] ? labtab[pc] : "" , labtab[pc] ? ":" : " " );
      }else{
      /* JOB */         fprintf(st,"<user>  ");
      /* SRC */         fprintf(st,"%11.11s ","");
      /* TAG */         fprintf(st,"%6.6s%1s ","","");
      }
      /* IR Instruction */
      sim_eval = h->ir; // fetch instruction word
      if ((h->pc & HIST_PC2) == 0) {
        if ((fprint_sym (st, h->pc & RMASK, &sim_eval, &cpu_unit[0], SWMASK ('M'))) > 0) {
          //      123456789.123456789.12
          fputs ("   ( undefined )     ;", st);
        }
      }else{
        fputs("                     ;",st); // 22 wide
      }
      /* upi */         fprintf(st,"%3o",h->upi); fputs ("  ", st);
      
      /* PC */  fprintf (st, "%6o / ",   pc );
                fprint_val (st, sim_eval, 8, 36, PV_RZRO);
      
      /* AC or device */  fprintf (st, "%3o ",     h->AC_or_dev );
      /* c(AC) */         fprintf (st, "%12llo %s ", h->ac, (h->ac > 0200)|(h->ac==0) ? " " : SAIL7CODE_lpt[h->ac] );
      /* EA */            fprintf (st, "%6o   ",   h->ea    );
      /* AR */            fprintf (st, "%12llo  ", h->mb    );
      /* Result */        fprintf (st, "%12llo  ", h->fmb   );
      /* Flags */         fprintf (st, "%6o  ",    h->flags );
      
      fputc ('\n', st); /* end line */
    } // end else instruction
  } // for loop
  return SCPE_OK;
}
t_stat
cpu_help(FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{
  fprintf(st, "%s\n\n", cpu_description(dptr));
  fprint_set_help(st, dptr);
  fprint_show_help(st, dptr);
  return SCPE_OK;
}
const char *
cpu_description (DEVICE *dptr)
{
  return "Reënact KA10 CPU at SAIL in 1974 with two user memory segments.";
}
