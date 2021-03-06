#!/usr/bin/python3
# Filename: simka/segment.py -*- mode:Python; coding:utf-8 -*-

import tkinter as tk
from tkinter import font
from tkinter import *

root = Tk()
root.geometry('1440x1440+0+500')
root.tk_focusFollowsMouse()
root["bg"] = "#202020"
root.title("SAIL BOAT");    
# easy to hit Quit button
Button( root, text="Quit",
        font = font.Font(size=30,weight='bold'),
        fg="darkred",
        bg="white",
        activeforeground="white",
        activebackground="darkred",
        command=root.quit ).pack(side="bottom")

canvas = Canvas( root,
                 width=(2.75*512+32),
                 height=(2.75*480+32),takefocus=1,
                 highlightthicknes=4,
                 highlightcolor="gold",
                 highlightbackground="gray",
                 background="#404040",
                 name="sailboat")
canvas.pack(side="top", padx=0, pady=0)

# img = PhotoImage(file="/home/bgb/zoe/movie2.gif")
# 105.png PNG 1296x964 1296x964+0+0 8-bit sRGB 256c 330KB 0.000u 0:00.000
# img = PhotoImage(file="/home/bgb/zoe/105.png")
# canvas.create_image(0,0, anchor=NW, image=img)

sail_zero = """
........................................***.....................
.......................................**.**....................
.......................................**.**....................
.......................................**.*.*...................
......................................***.*.*...................
......................................***.*..*..................
.....................................****.****..................
.....................................****.*.***.................
....................................*****.*.***.................
...................................******.*..***................
..................................*******.*..***................
.................................********.*...***...............
...............................**********.*...****..............
.............................************.*...*****.............
...........................**************.*...******............
.........................****************.*...*******...........
......................*******************.*...********..........
...................**********************.*..**********.........
..............**************************..*..************.......
.........*******************************..******....******......
...........*.........***********........*****...........***.....
..........*****...........................*................*....
.........*******..........................*......***************
.........***************************************************....
.........************...****.*****.***.****************.........
..........**********.******.*.****.***.*************............
..........***********..***.***.***.***.***********..............
............***********.**.....***.***.*********................
.............*******...***.***.***.***....*****.................
...............*******************************..................
...................*************************....................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
"""
# 160x48
sail_four_month = """
..........................................................................*........................*...........................................................
.................................................*........................*........................*........................*..................................
.................................................*........................*........................*........................**.................................
........................................********.*.******........********.*.******........********.*.******........********.*.******...........................
.......................................*..******.*.*******.....**..******.*.*******.....**..******.*.*******.....**..******.*.*******..........................
......................................*..******..*..*****....**...******..*..******...**...******..*..******...**...******..**.*****...........................
.....................................*...........*.........**.............*.........**.............*.........**.............*.*....*...........................
................................*...*...********.*.********......********.*.********......********.*.********......********.*.*******..........................
...............................***.*.....*******.*.********.......*******.*.********.......*******.*.********.......*******.*.********.........................
.............................*..*.*.....*******..*..******.......*******..*..******.......*******..*..******.......*******..*..******.*........................
............................*...**.*.............*........................*........................*........................**....*....*.......................
..........................*.....*...*...********.*.*******.......********.*.*******.......********.*.*******.......********.*.*******...*......................
.........................*......*....*...*******.*.********.......*******.*.********.......*******.*.********.......*******.*.********...*.....................
.......................*........*.....*.******...*...*****.......******...*...*****.......******...*...*****.......******...*...******....*....................
......................*.........**....**.........*........................*........................*........................*....*....*....*...................
....................*...........*.*....***.*****.*.********.....*********.*.********.....*********.*.********.....*********.*.********.*....*..................
...................*............*..*...****.****.*.*********.....********.*.*********.....********.*.*********.....********.*.*********.*....*.................
.................*..............*...*...****.***.*.*********.....********.*.*********.....********.*.*********.....********.*.*********..*....*................
................*...............*....*..*****....*....*****.....******....*....*****.....******....*....*****.....******....*....*****....*....*...............
...............*.**.............*....**.******...*........................*........................*........................*.........*...**....*..............
...............*..***...........*.....***.*****..*.**********..**********.*.**********..**********.*.**********..**********.*.**********...**....*.............
..............*....****.........**....****.*****.*.***********..*********.*.***********..*********.*.***********..*********.*.***********...***...*............
..............*....******.......*.*....****.******..**********..*********.*.***********..*********.*.***********..*********.*.************..****...*...........
.............*......*******.....*..*...*****.....*.....******..******.....*.....******..*******....*.....******..*******....*.....******..*..*****..*..........
.............*.....**********...*...**.*******...*........................*........................*........................*..............*..*****..*.........
............*......************.*....**.*******..*.***********.**********.*.***********.**********.*.***********.**********.*.***********...*..******.*........
...........*......*************.*....***.*******.*.************.*********.*.************.*********.*.************.*********.*.************..**..******.*.......
...........*......*************.*.....***.......**.*************.********.*.*************.********.*.*************.********.*.*************..**..********......
..........*......**************.*....*****.*****.*.**************.*******.*.**************.*******.*.**************.*******.*.**************..**.*********.....
.........*......***************.*...*.*****.****.*.**************.*******.*.**************.*******.*.**************.*******.*.**************..***.*********....
.........*....*****************.*..*.*******.***.*.***************.******.*.***************.******.*.***************.******.*.***************.****.*********...
........*...*******************.*.*..********.**.*.***************.******.*.***************.******.*.***************.******.*.***************.*****.*********..
.......*.**********************.**..**********...*.....***********.**.....*.....***********.**.....*.....***********.**.....*.....***********.******.***.*****.
......*************************.*...***********..*.........******.........*.........******.........*.........******.........*.........******.********.......***
..........*.....................*..*****....****.*.............**.........*.............**.........*.............**.........*.............**.****.****...***...
.........*......................*..**.........****........................*........................*........................*...............***.......***......
........**********..............*.*.............**........................*........................*........................*.............**.......***.........
.......***********..............**...............*........................*........................*........................*...........**......***............
........*************************................*........................*........................*........................*************************..........
.........*****************************************************************************************************************************************.............
...........**************************************....**..**.**.*...*******.***.**..**.***.*.....*.**.**...**************************************...............
.............************************************.****.**.*.**.*.**.******..*..*.**.*..**.***.***.**.*.*****************************************...............
............*************************************...**.**.*.**.*..********.*.*.*.**.*.*.*.***.***....**..**************************************................
...........**************************************.****.**.*.**.*.*.*******.***.*.**.*.**..***.***.**.****.*************************************................
..........***************************************.*****..***..**.**.******.***.**..**.***.***.***.**.*...*************************************.................
..........************************************************************************************************************************************.................
...............................................................................................................................................................
...............................................................................................................................................................
"""

mx=11//4
my=11//4
width=11//4
glyph=sail_zero
glyph=sail_four_month
n=0
y=10
for ln in glyph.splitlines():
    y+=1
    x0=0
    x9=len(ln)
    while x0<x9:
        try:
            dots = ln[x0:].index("*")
        except ValueError:
            break
        x1 = x0 + dots
        try:
            stars = ln[x1:].index(".")
        except ValueError:
            stars = x9-x1
        x2 = x1 + stars
        x0 = x2 + 1
        # print("canvas.create_line({},{},{},{})".format(x1,y,x2,y))
        canvas.create_line(mx*x1,my*y,mx*x2,my*y,width=width,fill="white")
        n+=1
    # print("exit WHILE {} line segments".format(n))
# print("exit FOR")

root.mainloop()

