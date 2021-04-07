#!/usr/bin/python3
# Filename: simka/lester_keyboard.py    -*- mode: Python; coding: utf-8 -*-
"""
------------------------------------------------------------------------------
SAIL-WAITS             Lester Keyboard . python
------------------------------------------------------------------------------
Lester Green Keyboard
 Bruce Blue  Keyboard
         X11 keyboard keycodes
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

import pdb

def vprint(*args,**kwargs):
    # print(*args,**kwargs)
    pass

offBlack="#445"
down = [False]*256
usersock = None
servsock = None
paper = None

sailcode={
    "":0,
    "↓":1,"α":2,"β":3,"∧":4,"¬":5,"ε":6,"π":7,"λ":0o10,
    "\t":0o11,
    "\n":0o12,"\013":0o13,"\f":0o14,"\r":0o15,
    "∞":0o16,"∂":0o17,"⊂":0o20,"⊃":0o21,"∩":0o22,"∪":0o23,"∀":0o24,"∃":0o25,"⊗":0o26,"↔":0o27,
    "_":0o30,"→":0o31,"~":0o32,"≠":0o33,"≤":0o34,"≥":0o35,"≡":0o36,"∨":0o37,"↑":0o136,"←":0o137,    
    "{":0o173,"|":0o174,"§":0o175,"}":0o176,
    "\b":0o177,
}
sailchar={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",0o10:"λ",
    0o11:"\t",
    0o12:"\n",0o13:"\013",0o14:"\f",0o15:"\r",
    0o16:"∞",0o17:"∂",0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",0o136:"↑",0o137:"←",
    0o173:"{",0o174:"|",0o175:"§",0o176:"}",
    0o177:"\b",
}
'''
Authentic Stanford Triple-III display glyphs are believed to have been as follows:
Show nothing at NUL and TAB, display
        ∫ integral  \u222B at VT
        ± plusminus \u00B1 at FF
        ~ tilde at RUBOUT
unfortunately ~ tilde again is displayed at code position 032, ASCII name ESC

        The Dave-Pool Teletype model-35 incompatible right-curly-bracket code

Fictional display glyphs for CR LF and Stanford-ALT because the simulation software is suppose
to consume these character and change the text raster position or activate text input.
'''
sail_decode_for_III={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"",0o12:"\n",0o13:"∫",0o14:"±",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",0o177:"~"
}

class LKB_App:
    def __init__(self):
        self.swrbut="0000"
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
            77:  0o45, # ⇥ Tab      TAB
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
            77:  " ⇥ Tab      TAB",
            81:  " ⇊ Form      FF",
            79:  " ⤓ VT        VT",
            22:  " ⌫ Backspace BS",
            106: " § Stanford ALT",
        }
        for x in range(0o40,0o136):
            sailcode[chr(x)]=x
            sailchar[x]=chr(x)
        for x in range(0o140,0o173):
            sailcode[chr(x)]=x
            sailchar[x]=chr(x)
            
    def spacewar_buttons(self):
        #      Rotate  left ccw,  right cw,    thrust,  torpedo
        pointy_fins = [down[38],  down[40],  down[39],  down[25], ] # A,D,S,W
        round_back =  [down[44],  down[46],  down[45],  down[31], ] # J L K I
        birdie =      [down[119], down[117], down[115], down[110],] # Delete PgDn End Home
        flat_back =   [down[113], down[114], down[116], down[111],] # ← → ↓ ↑
        funny_fins =  [down[83],  down[85],  down[84],  down[80], ] # ◀ ▶ ● ▲
        qval = funny_fins + flat_back + birdie + round_back + pointy_fins
        buttons = int("".join(["01"[i] for i in qval]),2)
        now = "{:04X}".format(int("".join(["01"[i] for i in qval]),2))
        if( self.swrbut == now ): # nothing new ?
            return
        self.swrbut = now
        swrbut_message = "swrbut=" + self.swrbut + ";"
        # print( swrbut_message )        
        # print("buttons = %05X" % buttons)
        # canvas.itemconfigure( item_spacewar, text=("spacewar %05X" % buttons))
        if usersock is not None:
            usersock.sendall( swrbut_message.encode() )
        return
    
    def keyhit(self,ev):
        " Key Down "
        global usersock,down        
        down[ev.keycode] = True
        self.spacewar_buttons()
        vprint("  key DOWN keycode='{}' ev.char='{}' ev.keysym='{}'".format( ev.keycode, ev.char, ev.keysym ))
        
    def keyrelease(self,ev):
        " Key Up "
        global usersock,down,III
        down[ev.keycode] = False;
        self.spacewar_buttons()
        vprint("    key UP keycode='{}' ev.char='{}' ev.keysym='{}'".format( ev.keycode, ev.char, ev.keysym ))

        # There are a dozen+1 'shift-level-keys'
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

        # six octal digits: two digit Keyboard Line Number, and four digit Keyboard Key Code.
        lkbstr = "{:02o}{:04o}".format( ev.widget.kb, lkb )
        keyline = ev.widget.kb
        lkbstr = "DKBevent={:02o}{:04o};".format(keyline,lkb)        
        vprint( "SEND lkbstr={}  {:04o}  c={}   key send keycode='{}' ev.char='{}' ev.keysym='{:}' ev.keyline={} lkbname={}".
               format( lkbstr, lkb, c, ev.keycode, ev.char, ev.keysym, keyline, lkbname, ))
        if usersock is not None:
            usersock.sendall( lkbstr.encode() )
        if usersock is None:
            # print("No destination for Lester Keyboard events")
            print( "SEND lkbstr={}  {:04o}  c={}   key send keycode='{}' ev.char='{}' ev.keysym='{:}' ev.keyline={} lkbname={}".
                   format( lkbstr, lkb, c, ev.keycode, ev.char, ev.keysym, keyline, lkbname, ))
        # pdb.set_trace()
        if paper:
            paper.insert('end',ev.char)
            paper.see('end')

    def tick(self):
        global item_now,canvas
        try:
            canvas.itemconfigure( item_now, text=strftime("%Y-%m-%d %H:%M:%S %Z"))
            self.spacewar_buttons()
        finally:
            pass
        canvas.after(1500,app.tick)
'''
Lester Green Keyboard key codes for Stanford ASCII
  0o0077 keycodes 0,1,2,3 ... 63. are generated by the Lester Green Keyboard hardware
  0o0100 SHIFT
  0o0200 TOP
  0o0400 CTRL           bit position #27
  0o1000 META           bit position #26
'''
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

For most of the keys a key down event sent a six-bit code,
combined with four "Bucky-Bits" from mode keys named TOP, SHIFT, CTRL and META.

The pro_dkb table, is derived from a table in TTYSER[J17,SYS] part of the 1974 SYSTEM.DMP

Note that 59 keys go into an encoding matrix ( 5 codes are NOT USED ) to generate the low order 6 bits.
8 keys are used Left and Right for Shift, Top, Control, Meta modifier LEVEL, 4 bits, the "Bucky-Bits"
1 key is for Shift Lock which has a mechanical latch at the physical keycap switch.

The keyboard scanning HARDWARE reads       META and CONTROL at bit#26 and bit#27    hardware 1000 and 0400 then 0200 and 0100 and 077
the TTYSER software returns to user level
9-bit character values with the bucky-bits META and CONTROL at bit#27 and bit#28    software 0400 and 0200 then 0177 SAIL 7-bit ASCII
"""
pro_dkb=[
    [0,0,0,0],                     # 000 /  .... not generated by keyboard
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
    [0,0,0,0],                     # 076 / .... not generated by keyboard
    [0,0,0,0],                     # 077 / .... not generated by keyboard
]
        
def main():
    global root,canvas
    global servsock,usersock
    
    LKB = LKB_App()
    
    root = tk.Tk()
    root.title('LKB Lester Keyboard module');
    root.geometry('{}x{}+{}+{}'.format(1200,1200,3880,100))
    root.tk_focusFollowsMouse()
    root["bg"] = offBlack
    canvas = tk.Canvas( root,
                        highlightcolor='orange', highlightthickness=4,
                        width=(1024), height=(1024), takefocus=1 );
    canvas.pack() 
    frame = tk.Frame(root)
    button = tk.Button(frame, text="Quit", fg="darkred", command=frame.quit);
    button.pack()        
    frame.pack()    
    canvas.configure(background='black')    
    canvas.bind("<Any-KeyPress>",LKB.keyhit)
    canvas.bind("<Any-KeyRelease>",LKB.keyrelease)
    canvas.dd = 0
    canvas.kb = 0
    canvas.ln = 0o20
    canvas.tag= "DD{:o}".format(0)
    
    try:
        root.mainloop()
    finally:
        if servsock is not None:
            servsock.close()
        if usersock is not None:
            usersock.close()

if __name__ == "__main__" :
    main()

'''
To make a listing on wide LEDGER paper, linux shell command
lpr -P wide -o media=Custom.11x17in -o page-left=48 -o cpi=14 -o lpi=10 lkb.py
or
lpr -P wide -o media=Custom.11x17in -o page-left=36 -o cpi=10 -o lpi=8 lkb.py
'''
