#!/usr/bin/python3
"""     -*- mode:python; coding:utf-8 -*-
------------------------------------------------------------------------------
        I       I       I              T       E       R       M
------------------------------------------------------------------------------
~/simka/iiiterm.py
        Triple III vector CRT display processor
with
        SAIL-WAITS Page-Printer         PPIOT
        SAIL-WAITS Piece-of-Glass       PGIOT
        SAIL-WAITS

SAIL one user Triple III display with Lester DKB display keyboard.
Bruce Guenther Baumgart, copyright(c)2010, ©2018, license GPLv2.
------------------------------------------------------------------------------
        Emulate a one user, SAIL-WAITS Triple-III display
        as was at the Stanford Artificial Intelligence Lab in 1974.
------------------------------------------------------------------------------
The original III display processor technical details,
        see SAILON #29 by William Weiher, dated 29 August 1967. Later updated somewhat,
        see SAILON #56 by Ted Panofsky,   dated 12   May  1973. When
Panofsky "tuned" the display processor to the character sizes and line widths used here.
For example, size #2 yields 85 characters per line, 42 lines per screen.
"""
from math    import *
from tkinter import font
import tkinter as tk
from locale  import *
from time    import *
from sys     import *

import os
import pdb
import re
import socket
import threading
import time

# Globals
root = tk.Tk()
root.geometry("+250+600") # Left panel
root.title("III-20 terminal")
# root.geometry("+1450+600") # smack in the middle of work9 triptych
T = 0
tick = 0
Green="#80FF80"
Blue="#8080FF"
Black="#404040"
offBlack="#404040"
servsock=None
usersock=None
(ox,oy)=(15,15)
glass = 0


def AV(x,y):    return (x&2047)<<25 | (y&2047)<<14 | 70         # LINE visible
def AI(x,y):    return AV(x,y) | 32                             # MOVE

def RV(x,y):    return (x&2047)<<25 | (y&2047)<<14 | 6          # line visible
def RI(x,y):    return AV(x,y) | 32                             # move

def ADOT(x,y):  return AV(x,y) | 16     # absolute display dot
def RDOT(x,y):  return RV(x,y) | 16     # relative display dot

def TEXTWORD(w):
    word=0
    for c in w[0:5]: # damn sure width is five characters
        word = (word<<7) | sailcode[c]  # sailcode is like ord(c) but decodes SAIL-WAITS characters from unicode into SAIL 7-bit
    return (word<<1)|1 # return III text command PDP10 word

def DPYSTR(s):
    s += "\0"*(5-len(s)%5) # pad right with nulls (not strictly necessary)
    words = [ s[p:p+5] for p in range(0,len(s),5) ]
    return [ TEXTWORD(w) for w in words ]

# The terminal available banner "TAKE ME I'M YOURS!"
# is displayed in color blue until replaced by client updates.
III_example=(1,
             0o640000012146,
             0o770010,          # select all six display consoles
             0o522031342501,0o466124044517,0o465013147653,0o512464100001,
             AI(-511,-511),
             AV( 511,-511),
             AV( 511, 511),
             AV(-511, 511),
             AV(-511,-511)
)
"""
# Screen 1024 x 1024 pixel origin 0,0 in the center +X goes right +Y goes up.
# SIZE =0 no_change     DX      DY
# ------------------------------------------
# SIZE =1  64 x 128      8      16
# SIZE =2  42 x  85     12      25
# SIZE =3  36 x  73     14      28
# SIZE =4  32 x  64     16      32
# SIZE =5  21 x  42     24      48
# SIZE =6  16 x  32     32      64
# SIZE =7  10 x  21     48     102
# ------------------------------------------
# characters per line   : 128, 85, 73, 64, 42, 32, 21
#      lines per screen :  64, 42, 36, 32, 21, 16, 10
"""
# I_char_dx = [0,  8, 12,    14,    16, 24.38, 32,  48.76]
# I_char_dy = [0, 16, 24.38, 28.44, 32, 48.76, 64, 102.4 ]
I_char_dx = [0,  8, 12,    14,    16, 24,    32,  48   ]
I_char_dy = [0, 16, 25,    28,    32, 48,    64, 102   ]
III_Font = [
    font.Font(family='Ubuntu Mono',size=-24),    
    font.Font(family='Ubuntu Mono',size=-16),
    font.Font(family='Ubuntu Mono',size=-24),
    font.Font(family='Ubuntu Mono',size=-28),    
    font.Font(family='Ubuntu Mono',size=-32),
    font.Font(family='Ubuntu Mono',size=-48),
    font.Font(family='Ubuntu Mono',size=-64),
    font.Font(family='Ubuntu Mono',size=-98),
]
III_BRT = [ "#000", "#0B0", "#0F0", "#6F6", "#8F8", "#AFA", "#DFD", "#EFE",]

def vprint(*args,**kwargs):
    # print(*args,**kwargs)
    pass

def doIIIrefresh(*args):
    global tick,canvas,T
    # print("doIIIrefresh tick=%d"%(tick))
    # canvas.itemconfig(T,text="refresh#%d"%(tick) )
    pass

class Triple_III_display_service( threading.Thread ):
    def __init__(me):
        threading.Thread.__init__( me )
    def run( me ):
        global tick,root,canvas,usersock
        canvas.delete('all')
        convert_iii_into_tk( canvas, III_example, Blue ) # Take me I'm yours!
        msgstr=""
        while True:
            data = usersock.recv( 8192 )
            if not data:
                usersock.close()
                usersock = None
                break
            msg = data.decode()
            buf = [int(x,8) for x in msg.split()] # octal words
            # print("\n{} bytes, {} words, data={}".format(len(data),len(buf),data))
            # if buf[0]==0 and buf[-1]==0:
            convert_iii_into_tk( canvas, buf, Green )
            tick+=1
            root.event_generate("<<IIIrefresh>>",when="tail")
        print("user thread finished")
        tag = "III#%d"%(glass)
        canvas.delete( tag )
        test_pattern()

class sock_service( threading.Thread ):
    def __init__( me ):
        threading.Thread.__init__( me )
    def run( me ):
        global tick,root,servsock,usersock
        print("sim-III-service at well-known-port #1974")
        servsock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        servsock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        servsock.bind(('',1974))
        servsock.listen(5)
        try:
            while True:
                print("Accept connections. Waiting at well-know-port #1974...")
                (usersock,a) = servsock.accept()
                print("Connected ",a,usersock)
                kid2 = Triple_III_display_service()
                kid2.setDaemon(True)
                kid2.start()
        finally:
            servsock.close()
        print("server thread exiting")
            
def convert_iii_into_tk( canvas, III_buffer, Fill ):
    global glass
    n = len( III_buffer )
    brt = 0
    siz = 0
    III_Font[0] = III_Font[2]
    (  ox,  oy ) = (    15,     15 ) # offset the displayed square inside its frame
    ( xxx, yyy ) = (ox+512, oy+512 ) # Middle of the III screen. The "origin" of the (x,y) coordinates.
    (  x0,  y0 ) = (    0,       0 ) # beam starting coordinates
    glass = III_buffer[0]
    tag = "III#%d"%(glass)
    canvas.delete( tag )
    # print("canvas delete tag={} glass#{}".format(tag,glass))
    # time.sleep(5)
    i = 1
    while i < n:
        word = III_buffer[i]
        # vprint("III_%d / %013o  " % (i, word),)
        if word & 1:
            msg=""
            while word & 1:
                b1 = word >> 29 & 0o177
                b2 = word >> 22 & 0o177
                b3 = word >> 15 & 0o177
                b4 = word >>  8 & 0o177
                b5 = word >>  1 & 0o177
                msg += "".join(sail_decode_for_III.get(x,chr(x)) for x in (b1,b2,b3,b4,b5))
                i += 1
                if (i >= n) or (III_buffer[i] & 1 != 1) :
                    i -= 1
                    break
                word = III_buffer[i]
                # vprint( "TXT_%d / %013o  " % (i, word),)
            vprint("DPYSTR(asciz|"+msg+"|)")            
            for snip in re.split(r'(\r|\n)',msg):
                if snip=='\r':
                    x0 = -512
                elif snip=='\n':
                    y0 = y0-I_char_dy[siz]
                elif len(snip):                
                    canvas.create_text( xxx+x0+7, yyy-y0, text=snip, font=III_Font[siz], fill=Fill, anchor="sw", tags=tag)
                    x0 += len(snip) * I_char_dx[siz] 
        elif (word & 0o17) in (4,0o12,0o14,0o16,): # JSR-JMS-SAVE, TEST, RESTORE, NOP
            pass
        elif (word & 0o17) in (8,):
            pass
            # print("SELECT set="+oct((word>>30)&63)+" clear="+oct((word>>18)&63))
        elif (word & 0o17) in (2,6,): # Vectors short or long
            x = word >> (36-11) & 0o3777
            y = word >> (36-22) & 0o3777
            brt = word >>11 & 7
            siz = word >>8  & 7
            if siz:
                III_Font[0]= III_Font[siz]
                I_char_dx[0]= I_char_dx[siz]
                I_char_dy[0]= I_char_dy[siz]
            if brt>0 or siz>0:
                brtsiz = ",BRT=%o,SIZ=%o" % (brt,siz);
            else:
                brtsiz = ""
            op = word & 0o177
            
            if x & 0o2000:
                x |= ~0o3777 # sign extend
            if y & 0o2000:
                y |= ~0o3777
                
            if op==0o106: # Absolute Visible
                vprint("LINE(%d,%d%s)" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x,y)         # to position now
                canvas.create_line( xxx+x1, yyy-y1, xxx+x0, yyy-y0, fill=Fill,tags=tag)
                
            elif op==0o006: # relative Visible
                vprint("line(%d,%d%s)" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x0+x,y0+y)   # to position
                canvas.create_line( xxx+x1, yyy-y1, xxx+x0, yyy-y0, fill=Fill,tags=tag)
                
            elif op==0o126 or op==0o166: # absolute DOT endpoint
                vprint("LINE(%d,%d%s)dot" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x,y)         # to position now
                # canvas.create_line( xxx+x1, yyy-y1, xxx+x0, yyy-y0, tags=tag)
                canvas.create_oval( xxx+x0-1, yyy-y0-1,
                                    xxx+x0+1, yyy-y0+1,
                                    fill=Fill,
                                    tags=tag)

            elif op==0o026 or op==0o066: # relative DOT endpoint
                vprint("line(%d,%d%s)dot" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x0+x,y0+y)   # to position
                # canvas.create_line( xxx+x1, yyy-y1, xxx+x0, yyy-y0, tags=tag)
                canvas.create_oval( xxx+x0-1, yyy-y0-1,
                                    xxx+x0+1, yyy-y0+1,
                                    fill=Fill,
                                    tags=tag)
                
            elif op==0o146:
                vprint("MOVE(%d,%d%s)" %(x,y,brtsiz))
                (x0,y0) = (x,y) # beam position Absolute Invisible
                
            elif op==0o046:
                vprint("move(%d,%d%s)" %(x,y,brtsiz))
                (x0,y0) = (x0+x,y0+y) # beam position Relative Invisible
            else:
                vprint("III opcode %03o" %(op & 0o177))
        i+=1    
    # tag = "III#%d"%(glass-1)
    # print("canvas.delete tag={}".format(tag))
    # canvas.delete(tag)
    
down = [False]*256

class App:
    def __init__(self, master):
        self.swrbut="0000"
        frame = tk.Frame(master)
        self.canvas = tk.Canvas( master, width=(1024+2*ox), height=(1024+2*oy), takefocus=1 );self.canvas.pack()
        self.button = tk.Button(frame, text="Quit", fg="darkred", command=frame.quit);self.button.pack(side="right")
        # self.button = tk.Button(frame, text="Restart", fg="darkblue", command=frame.quit);self.button.pack(side="right")
        # self.button = tk.Button(frame, text="Recompile PREWAITS", fg="darkgreen", command=frame.quit);self.button.pack(side="right")
        frame.pack()
        self.keylc = "␣"*10+"1234567890-=\b\tqwertyuiop[]␣␣asdfghjkl;'`␣\\zxcvbnm,./"+"␣"*194
        self.keyuc = "␣"*10+"!@#$%^&*()_+\b\tQWERTYUIOP{}␣␣ASDFGHJKL:\"~␣|ZXCVBNM<>?"+"␣"*194
        self.keytop= "␣"*10+"!@#$%^&*()_+\b\t∧∨∩∪⊂⊃↑∞⊗∂{}␣␣≤≥↓¬≠≡←→↔:\"~␣|αβελπ∀∃<>?"+"␣"*194
        self.action= {
            36:  0o33, # ↵ Enter     CR
            104: 0o33, # ↵ Return    CR
            86:  0o35, # ↴ Linefeed  LF
            65:  0o40, # ␣ Spacebar  SP
            127: 0o41, # ⎊ Break    BRK
            9:   0o42, # ⎋ Escape   ESC
            82:  0o43, # ⍓ CALL      ↑C
            63:  0o44, # ⎚ Clear    CLR
            23:  0o45, # ⇥ Tab      TAB
            81:  0o46, # ⇊ Form      FF
            79:  0o47, # ⤓ VT        VT
            22:  0o74, # ⌫ Backspace BS
            106: 0o75, # § Stanford ALT
            }
        self.action_name= {
            36:  " ↵ Enter     CR",
            104: " ↵ Return    CR",
            86:  " ↴ Linefeed  LF",
            65:  " ␣ Spacebar  SP",
            127: " ⎊ Break    BRK",
            9:   " ⎋ Escape   ESC",
            82:  " ⍓ CALL      ↑C",
            63:  " ⎚ Clear    CLR",
            23:  " ⇥ Tab      TAB",
            81:  " ⇊ Form      FF",
            79:  " ⤓ VT        VT",
            22:  " ⌫ Backspace BS",
            106: " § Stanford ALT",
            }
        sailcode[chr(0)]=0
        for x in range(0o40,0o136):
            sailcode[chr(x)]=x
            sailchar[x]=chr(x)
        for x in range(0o140,0o173):
            sailcode[chr(x)]=x
            sailchar[x]=chr(x)
            
    def spacewar_buttons(self):
        pointy_fins = [down[38],  down[40],  down[39],  down[25], ] # A,D,S,W == rot left ccw, rot right cw, thrust, torpedo
        round_back =  [down[44],  down[46],  down[45],  down[31], ] # J L K I
        birdie =      [down[119], down[117], down[115], down[110],] # Delete PgDn End Home
        flat_back =   [down[113], down[114], down[116], down[111],] # ← → ↓ ↑
        funny_fins =  [down[83],  down[85],  down[84],  down[80], ] # ◀ ▶ ● ▲
        qval = funny_fins + flat_back + birdie + round_back + pointy_fins
        # pdb.set_trace()        
        now = "{:04X}".format(int("".join(["01"[i] for i in qval]),2))
        if( self.swrbut == now ): # nothing new ?
            return
        else:
            self.swrbut = now
        swrbut_message = "swrbut=" + self.swrbut + ";"
        # print( swrbut_message )        
        if usersock is not None:
            usersock.sendall( swrbut_message.encode() )
        return
        
    def keyhit(self,ev):
        " Key Down "
        global usersock,down        
        down[ev.keycode] = True;
        self.spacewar_buttons();
        # print("     kDOWN keycode='{}' ev.char='{}' ev.keysym='{}' swrbut='{}'".format( ev.keycode, ev.char, ev.keysym, self.swrbut ))
    
    def keyrelease(self,ev):
        " Key Up "
        global usersock,down
        down[ev.keycode] = False;
        if ev.keycode in [44,46,45,21, 119,117,115,110, 113,114,116,111, 83,85,84,80,]:
            self.spacewar_buttons();
        # print("     keyUP keycode='{}' ev.char='{}' ev.keysym='{}' swrbut='{}'".format( ev.keycode, ev.char, ev.keysym, self.swrbut ))

        # Dozen+1 so called 'shift-level-keys'
        # meta           64      108      91
        # ctrl           37      105      90
        # top      66   133      134      87
        # shift          50       62      89
        if ev.keycode in [37,50,62,64,66, 87,89,90,91, 105,108,133,134]:
            return

        lkb = self.action.get(ev.keycode,0)
        lkbname = self.action_name.get(ev.keycode,"")
        sail_ascii = 0
        c = '●'
        #      Left     or Right or   Far RIGHT keypad
        meta = down[64]  or down[108] or down[91]
        ctrl = down[37]  or down[105] or down[90]
        shift= down[50]  or down[62]  or down[89]
        top  = down[133] or down[134] or down[66] or down[87]

        if not lkb:
            if     top: c = self.keytop[ev.keycode]
            elif shift: c = self.keyuc[ev.keycode]
            else:       c = self.keylc[ev.keycode]
            sail_ascii = sailcode.get(c,0)
            # print("0o{:03o} for '{}'".format(sail_ascii,c))
            lkb = anti_lkb[sail_ascii]

        if meta:
            lkb |= 0o1000
            lkbname += ' meta'
        if ctrl:
            lkb |= 0o0400
            lkbname += ' ctrl'
        if top:
            lkb |= 0o0200
            lkbname += ' top'
        if shift:
            lkb |= 0o0100
            lkbname += ' shift'
        keyline = ev.widget.dkb_line_num
        lkbstr = "DKBevent={:02o}{:04o};".format(keyline,lkb)
        if(0):
            print( "{:04o}  c={}   ev.keycode='{}' ev.char='{}' ev.keysym='{:}' lkbstr='{}' ".
                   format( lkb, c, ev.keycode, ev.char, ev.keysym, lkbname, lkbstr ))
        if usersock is not None:
            usersock.sendall( lkbstr.encode() )
        else:
            # print(lkbstr)
            pass
"""
    def tick(self):
        global i_now,t0,text,c
        try:
            canvas.itemconfig( i_now, text=strftime("%Y-%m-%d %H:%M:%S %Z")+text)
            print( "DATETIME %s" % (strftime("%Y-%m-%d %H:%M:%S %Z")))
            c = sys.stdin.read(-1)
            text = text + c                
            print( "TEXT (%s)" % text)
        except EOFError:
            eof = 1
            print( "EOF")
        finally:
            pass
#       canvas.after(500,app.tick)
"""
# Lester Green Keyboard key codes for Stanford ASCII
# 0o0077 keycodes 0,1,2,3 ... 63. are generated by the Lester Green Keyboard hardware
# 0o0100 SHIFT
# 0o0200 TOP
# 0o0400 CTRL
# 0o1000 META
anti_lkb=[
    0o000, 0o272, 0o232, 0o230,  0o136, 0o255, 0o203, 0o202, # null ↓ α β   ^ ¬ ε π
    0o226, 0o045, 0o035, 0o047,  0o046, 0o033, 0o234, 0o257, # λ tab lf vt  ff cr ∞ ∂
    0o264, 0o265, 0o263, 0o262,  0o216, 0o215, 0o252, 0o214, #  ⊂ ⊃ ∩ ∪     ∀ ∃ ⊗ ↔
    0o271, 0o213, 0o270, 0o207,  0o201, 0o223, 0o261, 0o227, #  _ → ~ ≠     ≤ ≥ ≡ ∨
    0o040, 0o254, 0o231, 0o222,  0o266, 0o267, 0o224, 0o211, #  ␣ ! " #     $ % & ' 
    0o050, 0o051, 0o052, 0o053,  0o054, 0o055, 0o056, 0o057, #  ( ) * +     , - . /
    0o060, 0o061, 0o062, 0o063,  0o064, 0o065, 0o066, 0o067, #  0 1 2 3     4 5 6 7
    0o070, 0o071, 0o072, 0o073,  0o204, 0o210, 0o206, 0o256, #  8 9 : ;     < = > ?
    #   @   A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z
    0o205, 65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,
    #   [     \     ]     ↑     ←
    0o250,0o034,0o251,0o273,0o212,
    #   `  a b c d e f g h i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z
    0o225, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
    #   {     |     §     }     ⌫
    0o217,0o253,0o075,0o220,0o074,
]
"""
      There are 68 key caps on the classic Lester green keyboard.
      =
      For most of the keys a key down event sends a six-bit code,
      combined with four "Bucky-Bits" from mode keys named TOP, SHIFT, CTRL and META.

      The pro_dkb table, is derived from a table in TTYSER[J17,SYS] part of the 1974 SYSTEM.DMP

      Note that 59 keys go into an encoding matrix ( 5 codes are NOT USED ) to generate the low order 6 bits.
      8 keys are used Left and Right for Shift, Top, Control, Meta modifier LEVEL, 4 bits, the "Bucky-Bits"
      1 key is for Shift Lock which has a mechanical latch at the physical keycap switch


        The keyboard scanning HARDWARE reads META and CONTROL at bit#26 and bit#27
        the TTYSER software returns to user level
        9-bit character values with bucky-bits META and CONTROL at bit#27 and bit#28
"""
pro_dkb=[
    [0,0,0,0],             # 000 /  .... not generated by keyboard
    [ 0o141, 0o101, 0o034, 0o034], # 001 /  'a'	'A'    '\034'  '\034'
    [ 0o142, 0o102, 0o007, 0o007], # 002 /  'b'	'B'    '\007'  '\007'
    [ 0o143, 0o103, 0o006, 0o006], # 003 /  'c'	'C'    '\006'  '\006'
    [ 0o144, 0o104, 0o074, 0o074], # 004 /  'd'	'D'	  '<'	  '<'
    [ 0o145, 0o105, 0o100, 0o100], # 005 /  'e'	'E'	  '@'	  '@'
    [ 0o146, 0o106, 0o076, 0o076], # 006 /  'f'	'F'	  '>'	  '>'
    [ 0o147, 0o107, 0o033, 0o033], # 007 /  'g'	'G'    '\033'  '\033'
    [ 0o150, 0o110, 0o075, 0o075], # 010 /  'h'	'H'	  '='	  '='
    [ 0o151, 0o111, 0o047, 0o047], # 011 /  'i'	'I'	  '''	  '''
    [ 0o152, 0o112, 0o137, 0o137], # 012 /  'j'	'J'	'_'	'_'
    [ 0o153, 0o113, 0o031, 0o031], # 013 /  'k'	'K'    '\031'  '\031'
    [ 0o154, 0o114, 0o027, 0o027], # 014 /  'l'	'L'    '\027'  '\027'
    [ 0o155, 0o115, 0o025, 0o025], # 015 /  'm'	'M'    '\025'  '\025'
    [ 0o156, 0o116, 0o024, 0o024], # 016 /  'n'	'N'    '\024'  '\024'
    [ 0o157, 0o117, 0o173, 0o173], # 017 /  'o'	'O'	  '{'	  '{'
    [ 0o160, 0o120, 0o176, 0o176], # 020 /  'p'	'P'	  '}'	  '}'
    [ 0o161, 0o121, 0o004, 0o004], # 021 /  'q'	'Q'    '\004'  '\004'
    [ 0o162, 0o122, 0o043, 0o043], # 022 /  'r'	'R'	  '#'	  '#'
    [ 0o163, 0o123, 0o035, 0o035], # 023 /  's'	'S'    '\035'  '\035'
    [ 0o164, 0o124, 0o046, 0o046], # 024 /  't'	'T'	  '&'	  '&'
    [ 0o165, 0o125, 0o140, 0o140], # 025 /  'u'	'U'	  '`'	  '`'
    [ 0o166, 0o126, 0o010, 0o010], # 026 /  'v'	'V'    '\010'  '\010'
    [ 0o167, 0o127, 0o037, 0o037], # 027 /  'w'	'W'    '\037'  '\037'
    [ 0o170, 0o130, 0o003, 0o003], # 030 /  'x'	'X'    '\003'  '\003'
    [ 0o171, 0o131, 0o042, 0o042], # 031 /  'y'	'Y'	  '"'	  '"'
    [ 0o172, 0o132, 0o002, 0o002], # 032 /  'z'	'Z'    '\002'  '\002'
    [ 0o015, 0o015, 0o015, 0o015], # 033 / RETURN
    [ 0o134, 0o134, 0o016, 0o016], # 034 /  '\'	'\'	 '\016'	 '\016'
    [ 0o012, 0o012, 0o012, 0o012], # 035 / LINE feed
    [0,0,0,0],                     # 036 / .... not generated by keyboard
    [0,0,0,0],                     # 037 / .... not generated by keyboard
    [ 0o040, 0o040, 0o040, 0o040], # 040 / SPACEBAR
    [0,0,0,0],                     # 041 / BREAK
    [0,0,0,0],                     # 042 / ESCAPE
    [ 0o600, 0o600, 0o600, 0o600], # 043 / CALL
    [0,0,0,0],                     # 044 / CLEAR
    [ 0o011, 0o011, 0o011, 0o011], # 045 / TAB
    [ 0o014, 0o014, 0o014, 0o014], # 046 / FORM
    [ 0o013, 0o013, 0o013, 0o013], # 047 / VT
    [ 0o050, 0o050, 0o133, 0o133], # 050 /  '('	'('	  '['	  '['
    [ 0o051, 0o051, 0o135, 0o135], # 051 /  ')'	')'	  ']'	  ']'
    [ 0o052, 0o052, 0o026, 0o026], # 052 /  '*'	'*'	 '\026'	 '\026'
    [ 0o053, 0o053, 0o174, 0o174], # 053 /  '+'	'+'	  '|'	  '|'
    [ 0o054, 0o054, 0o041, 0o041], # 054 /  ','	','	  '!'	  '!'
    [ 0o055, 0o055, 0o005, 0o005], # 055 /  '-'	'-'	 '\005'	 '\005'
    [ 0o056, 0o056, 0o077, 0o077], # 056 /  '.'	'.'	  '?'	  '?'
    [ 0o057, 0o057, 0o017, 0o017], # 057 /  '/'	'/'	 '\017'	 '\017'
    [ 0o060, 0o060, 0o060, 0o060], # 060 /  '0'	'0'	  '0'	  '0'
    [ 0o061, 0o061, 0o036, 0o036], # 061 /  '1'	'1'    '\036'  '\036'
    [ 0o062, 0o062, 0o022, 0o022], # 062 /  '2'	'2'    '\022'  '\022'
    [ 0o063, 0o063, 0o023, 0o023], # 063 /  '3'	'3'    '\023'  '\023'
    [ 0o064, 0o064, 0o020, 0o020], # 064 /  '4'	'4'    '\020'  '\020'
    [ 0o065, 0o065, 0o021, 0o021], # 065 /  '5'	'5'    '\021'  '\021'
    [ 0o066, 0o066, 0o044, 0o044], # 066 /  '6'	'6'	  '$'	  '$'
    [ 0o067, 0o067, 0o045, 0o045], # 067 /  '7'	'7'	  '%'	  '%'
    [ 0o070, 0o070, 0o032, 0o032], # 070 /  '8'	'8'    '\032'  '\032'
    [ 0o071, 0o071, 0o030, 0o030], # 071 /  '9'	'9'    '\030'  '\030'
    [ 0o072, 0o072, 0o001, 0o001], # 072 /  ':'	':'    '\001'  '\001'
    [ 0o073, 0o073, 0o136, 0o136], # 073 /  ';'	';'	  '^'	  '^'
    [ 0o177, 0o177, 0o177, 0o177], # 074 / BACKSPACE
    [ 0o175, 0o175, 0o175, 0o175], # 075 / 0175 Stanford ALT, not ASCII '}'
    [0,0,0,0],                 # 076 / .... not generated by keyboard
    [0,0,0,0],                 # 077 / .... not generated by keyboard
]
sailcode={
    "↓":1,"α":2,"β":3,"∧":4,"¬":5,"ε":6,"π":7,
    "λ":0o10,"\t":0o11,"\n":0o12,"\013":0o13,"\f":0o14,"\r":0o15,"∞":0o16,"∂":0o17,
    "⊂":0o20,"⊃":0o21,"∩":0o22,"∪":0o23,"∀":0o24,"∃":0o25,"⊗":0o26,"↔":0o27,
    "_":0o30,"→":0o31,"~":0o32,"≠":0o33,"≤":0o34,"≥":0o35,"≡":0o36,"∨":0o37,    
    "↑":0o136,"←":0o137,    
    "{":0o173,"|":0o174,"§":0o175,"}":0o176,"\b":0o177,
}
sailchar={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",0o10:"λ",0o11:"\t",0o12:"\n",0o13:"\013",0o14:"\f",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",
}
 # omit NUL and TAB.
 # integral \u222B at VT.
 # plusminus \u00B1 at FF.
 # tilde at RUBOUT.
sail_decode_for_III={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"",0o12:"\n",0o13:"∫",0o14:"±",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",0o177:"~"
}
def test_pattern():
    global canvas
    global III_Font
    # I.Canvas screen ruler for chosing font sizes to simulate III size table.
    r0="         1         2         3         4         5         6         7         8         9        10        11        12        13"
    r1="␣␣␣␣␣␣␣␣␣1␣␣␣␣␣␣␣␣␣2␣␣␣␣␣␣␣␣␣3␣␣␣␣␣␣␣␣␣4␣␣␣␣␣␣␣␣␣5␣␣␣␣␣␣␣␣␣6␣␣␣␣␣␣␣␣␣7␣␣␣␣␣␣␣␣␣8␣␣␣␣␣␣␣␣␣9␣␣␣␣␣␣␣100␣␣␣␣␣␣␣110␣␣␣␣␣␣␣120␣␣␣␣␣␣␣130"
    r2="1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    r3="ABCDEFGHIJKLMNOPQRSTUVWXYZ ≤ π ε ↓ ∩ ¬ ≠ ≡∞ ← → ↔ ∃∀ ⊗ ∂ ∧ ∪ ≥ ⊂ ↑ λ ∨ β ⊃ α"
    r4="abcdefghijklmnopqrstuvwxyz ~ ! @ # $ % ^ & * ( ) _ + { } | : ? > < , . / ; ' \\ \""
    # canvas.create_text(15,0,text="Stanford A. I. Lab III display server.",font="Courier-Bold 12",fill="white",anchor="nw")
    canvas.create_rectangle(ox,oy,ox+1024,oy+1024,fill=Black) # III window 0,0,1023,1023
    canvas.create_text(ox,oy,text=r0[:128]+"\n"+r2[:128]+"\n"+r3+"\n"+r4,font=III_Font[1],fill="white",anchor="nw",tags="III#0")    
    # canvas.create_text(ox,150,text=r0[:85]+"\n"+r2[:85]+"\n"+r3[:85]+"\n"+r4[:85],font=III_Font[2],fill="white",anchor="nw",tags="III#0")    
    # canvas.create_text(ox,300,text=r0[:73]+"\n"+r2[:73],font=III_Font[3],fill="white",anchor="nw",tags="III#0")
    # canvas.create_text(ox,400,text=r0[:64]+"\n"+r2[:64],font=III_Font[4],fill="white",anchor="nw",tags="III#0")
    # canvas.create_text(ox,600,text=r0[:42]+"\n"+r2[:42],font=III_Font[5],fill="white",anchor="nw",tags="III#0")
    # canvas.create_text(ox,720,text=r0[:32]+"\n"+r2[:32],font=III_Font[6],fill="white",anchor="nw",tags="III#0")
    canvas.create_text(ox,850,text=r0[:21]+"\n"+r2[:21],font=III_Font[7],fill="white",anchor="nw",tags="III#0")    
    # T=canvas.create_text(900,0,text="III-20",font="TkFixedFont 15",fill="darkred",anchor="nw")
    canvas.create_oval( 20,850,120,950, fill=III_BRT[0], outline=Black, width=0,tags="III#0");    
    canvas.create_oval(130,850,230,950, fill=III_BRT[1], outline=Black, width=0,tags="III#0");
    canvas.create_oval(240,850,340,950, fill=III_BRT[2], outline=Black, width=0,tags="III#0");
    canvas.create_oval(350,850,450,950, fill=III_BRT[3], outline=Black, width=0,tags="III#0");
    canvas.create_oval(510,850,610,950, fill=III_BRT[4], outline=Black, width=0,tags="III#0");
    canvas.create_oval(620,850,720,950, fill=III_BRT[5], outline=Black, width=0,tags="III#0");
    canvas.create_oval(730,850,830,950, fill=III_BRT[6], outline=Black, width=0,tags="III#0");
    canvas.create_oval(840,850,940,950, fill=III_BRT[7], outline=Black, width=0,tags="III#0");
        
def main():
    global root,canvas
    global servsock,usersock
    kid = sock_service()
    kid.setDaemon(True)
    kid.start()
    canvas.bind("<Any-KeyPress>",app.keyhit)
    canvas.bind("<Any-KeyRelease>",app.keyrelease)
    canvas.dkb_line_num = 0
    test_pattern()
    try:
        root.mainloop()
    finally:
        if servsock is not None:
            servsock.close()
        if usersock is not None:
            usersock.close()
    
root.bind("<<IIIrefresh>>",doIIIrefresh)
app = App(root)
root.tk_focusFollowsMouse()
root["bg"] = offBlack
canvas = app.canvas
canvas.configure(background='black')

if __name__ == "__main__" :
    main()
