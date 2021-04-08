#!/usr/bin/python3
# ddd4term.py -*- mode: Python; coding: utf-8 -*-
"""
This file is named ~/simka/ddd4term.py
It is an implementation of the first four Stanford Data Disc TV terminals
at DKB   keyboards #06, #07, #10 and #11
at VDS TV    lines #26, #27, #30 and #31
assigned to DCPower Lab building rooms 233, 230B, 206 and 215.

DKB#
TV#     Room#   ext     phone   occupant        
26	233	69	74975	PDP-6 area      Mordor
27	230B	41	74971   Binford         ..
30	206	25	74202   Earnest         
31	215	37	74116   Colby           ..

If we try to ignore the video crossbar switch, VDS,
 the  first  four DD channels   0,   1,   2,   3 might have been
wired directly to TV monitors #26, #27, #30, #31 octal;
 with           DKB keyboards   6,   7,  10,  11 placed next to each television set.

Inserting the VDS crossbar switch,
the initial SYSTEM state is that DD channel#9, octal 011,
displays on all 64 television monitors
( well actually 58 TV sets, since the Triple-III has the first six DKB numbers )
the "Available Status Screen" showing the "Take Me I'am Yours!" message
with Date, Time and System Load Status.

This is what SYSTEM.DMP [ J17, SYS ] displays after a reboot;
until it is asked to do otherwise; as users (losers) log in the crossbar switch
assigns an available DD channel to each DKB television set.

For this preliminary hack, 
there are four canvas objects representing the physical television sets, at TV#26 #27 #30 and #31,
the datadisc program writes TK text tagged by DD channel numbers selected in the DD code,
for current DD enabled TV set canvas;

Writing to a DD channel prior to its TV set being switched into the crossbar is IGNORED.

When a VDS crossbar word arrives, via the YAML message stream,
it enables (or disables), the marking of TK text as visible (or hidden).

DD erase channel number, delete TK tagged text,
DD select channel number, create TK tagged text,
some attempt to avoid EVEN and ODD over writing is made by the reënactment,
but is not really necessary since the final image results are the same.
"""
import tkinter as tk
from tkinter import font
from tkinter import *

import socket
import threading

import ddd_tk as ddd
import lester_keyboard as lkb
import pdb

# global
root=None
servsock=None
usersock=None
roomlabel=[
    "TV-26 PDP-6 area",
    "TV-27 Binford",
    "TV-30 Earnest",
    "TV-31 Colby",
    ]
# VDS crossbar switch has one 36-bit PDP10 word for each of 64(58) video lines
# video lines are TV set destinations, corresponding to DKB keyboard numbers.
bar=[1<<(31-9)]*64 # high order 32 bits, digital video from data disc channels
foo=[0]*64 # low order bits for selecting an analog video source

def four_panes():
    global root
    root = Tk()
    # root.geometry("1200x1200+116+800")
    root.geometry("1200x1200+0+0")
    root.tk_focusFollowsMouse()
    root.title("DDD four TV terminals");
    Button( root, text="Quit",
            font = font.Font(size=40,weight='bold'),
            fg="darkred",
            bg="white",
            activeforeground="white",
            activebackground="darkred",
            command=root.quit ).pack(side="bottom")               
    row0 = Frame(root)
    row1 = Frame(root)
    box = [0]*4
    can = [0]*4
    label = [0]*4
    for i in range(0,4):
        box[i] = Frame( (row0,row1)[i>=2], borderwidth=2, relief="solid")
        can[i] = Canvas(
            box[i],width=512,height=480,background='black',takefocus=1,
            highlightthicknes=10,
            highlightcolor="gold",
            highlightbackground="gray"
        )
        ( dd, kb, ln, tag ) = ( can[i].dd, can[i].kb, can[i].ln, can[i].tag ) = ( i, 6+i, 0o26+i, "DD{:o}".format(i) )
        can[i].pack(side="top",expand=True)
        box[i].pack(side="left",expand=True, fill="both")
        text="Terminal TV#{:o} keyboard#{:o} crossbar {:08X} {:2o}".format( ln, kb, bar[kb], foo[kb] )
        label[i] = Label(box[i],text=text)
        label[i].pack()
        row0.pack(side="top", expand=True, fill="both")
        row1.pack(side="top", expand=True, fill="both")
    root.can = can
    root.lab = label
    return root

class sock_service( threading.Thread ):
    def __init__( me ):
        threading.Thread.__init__( me )
    def run( me ):
        global tick,root,servsock,usersock
        print("sim-DD-service for four terminals at port #2026")
        servsock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        servsock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        servsock.bind(('',2026))
        servsock.listen(5)
        try:
            while True:
                print("Accept connections. Waiting at port #2026")
                (usersock,a) = servsock.accept()
                print("Connected ",a,usersock)
                lkb.usersock = usersock
                ddd.usersock = usersock
                kid2 = ddd.Triple_DDD_display_service()
                kid2.setDaemon(True)
                kid2.start()
        finally:
            servsock.close()
            print("ddd6term display server thread exiting")            

def main():
    global root,foo,bar
    root= four_panes()    
    DDD = ddd.DDD_App()
    LKB = lkb.LKB_App()    
    kid = sock_service()
    kid.setDaemon(True)
    kid.start()
    ddd.app = DDD
    ddd.root = root
    ddd.tk = root
    ddd.foo = foo
    ddd.bar = bar
    for canv in root.can:
        canv.bind("<Any-KeyPress>",   LKB.keyhit)
        canv.bind("<Any-KeyRelease>", LKB.keyrelease)
        ddd.test_pattern(canv)
    try:
        root.mainloop()
    finally:
        if servsock is not None:
           servsock.close()
        if ddd.usersock is not None:
            ddd.usersock.close()

if __name__ == "__main__" :
    main()
