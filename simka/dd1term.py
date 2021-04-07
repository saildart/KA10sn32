#!/usr/bin/python3
# dd1term.py -*- mode: Python; coding: utf-8 -*-
"""
This file is named ~/simka/dd1term.py
It is an implementation of one Stanford Data Disc TV terminal
at DKB   keyboard #06,
at VDS TV    line #26,
located in room 233, the computer room, Mordor, at the DCPower Lab building,
1600 Arastradero Road, Palo Alto California in 1974.
"""
import tkinter as tk
from tkinter import font
from tkinter import *

import socket
import threading

import dd1k as ddd
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
    offblack="#444"
    root = Tk()
    root.geometry('1100x1140+170+670')
    root.tk_focusFollowsMouse()
    root.title("Data Disc Terminal");
    root["bg"]=offblack
    Button( root, text="Quit",
            font = font.Font(size=40,weight='bold'),
            fg="#822",
            bg="gray",
            activeforeground="white",
            activebackground="darkred",
            command=root.quit ).pack(side="bottom")
    root.banner = Label(
        root, text="banner",
        fg="gray", bg=offblack,
    )
    root.banner.pack(side="top")
    root.lab = Label(
        root, text="Label",
        fg="gray", bg=offblack,
        font = font.Font(size=20,weight='bold'),
    )
    root.lab.pack(side="bottom")
    can = [0]*1
    i=0
    can[i] = Canvas(root,
                    width=(2*512+32),
                    height=(2*480+32),takefocus=1,
                    highlightthicknes=4,
                    highlightcolor="#aa0",
                    highlightbackground="gray",
                    background=offblack,
                    name="tv26")
    ( dd, kb, ln, tag ) = ( i, 6+i, 0o26+i, "DD{:o}".format(i) )
    ( can[i].dd, can[i].kb, can[i].ln, can[i].tag ) = ( dd, kb, ln, tag )
    can[i].pack(side="top",expand=True)        
    # use one transparent pixel for raster addressing a canvas
    pixel_gif="R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"
    can[i].pixel = PhotoImage(data=pixel_gif)
    can[i].create_image(0,1,anchor=NW,image=can[i].pixel,tag=can[i].tag)
    text = "Terminal TV#{:o} keyboard#{:o} crossbar hex {:08X} atv#{:2o}".format( ln, kb, bar[kb], foo[kb] )
    root.lab.configure(text=text)
    root.can = can
    return root

class sock_service( threading.Thread ):
    def __init__( me ):
        threading.Thread.__init__( me )
    def run( me ):
        global tick,root,servsock,usersock
        print("datadisc service for one terminals at port #2026")
        servsock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        servsock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        servsock.bind(('',2026))
        servsock.listen(5)
        try:
            while True:
                print("Waiting at port #2026")
                (usersock,a) = servsock.accept()
                # print("Connected ",a,usersock)
                lkb.usersock = usersock
                ddd.usersock = usersock
                kid2 = ddd.display_service()
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
