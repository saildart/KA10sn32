#!/usr/bin/python3
# ddd1term.py -*- mode: Python; coding: utf-8 -*-
"""
This file is named ~/simka/ddd4term.py
It is an implementation of one Stanford Data Disc TV terminals
at DKB   keyboards #06,
at VDS TV    lines #26,
assigned to DCPower Lab building rooms 233 Mordor
"""
import tkinter as tk
from tkinter import font
from tkinter import *

import socket
import threading

import dd_tk as ddd
import lester_keyboard as lkb
import pdb

# global
root=None
servsock=None
usersock=None
roomlabel=[ "TV-26 PDP-6 area", "TV-27 Binford", "TV-30 Earnest", "TV-31 Colby", ]
# VDS crossbar switch has one 36-bit PDP10 word for each of 64(58) video lines
# video lines are TV set destinations, corresponding to DKB keyboard numbers.
bar=[1<<(31-9)]*64 # high order 32 bits, digital video from data disc channels
foo=[0]*64 # low order bits for selecting an analog video source

def one_panel():
    global root
    root = Tk()
    root.geometry('1440x1540+0+0') # front and center
    root.tk_focusFollowsMouse()
    root.title("Data Disc Terminal");
    Button( root, text="Quit",
            font = font.Font(size=40,weight='bold'),
            fg="darkred",
            bg="white",
            activeforeground="white",
            activebackground="darkred",
            command=root.quit ).pack(side="bottom")               
    row0 = Frame(root)
    row1 = Frame(root)
    box = [0]*1
    can = [0]*1
    label = [0]*1
    for i in range(0,1):
        box[i] = Frame( (row0,row1)[i>=2], borderwidth=2, relief="solid")
        can[i] = Canvas(
            box[i],
            width=(2.75*512+32),
            height=(2.75*480+32),takefocus=1,
            highlightthicknes=4,
            highlightcolor="gold",
            highlightbackground="gray",
            background="#404040",
            name="tv26")
        ( dd, kb, ln, tag ) = ( can[i].dd, can[i].kb, can[i].ln, can[i].tag ) = ( i, 6+i, 0o26+i, "DD{:o}".format(i) )
        can[i].pack(side="top",expand=True)
        box[i].pack(side="left",expand=True, fill="both")
        text="Terminal TV#{:o} keyboard#{:o} crossbar hex {:08X} atv#{:2o}".format( ln, kb, bar[kb], foo[kb] )
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
        print("sim-DD-service for one terminals at port #2026")
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
    root= one_panel()
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
