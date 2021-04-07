#!/usr/bin/python3

from tkinter import *

root = Tk()
root.geometry("1440x2300+0+0")

button = Button( root, text="Quit", fg="darkred", command=root.quit )
button.pack(side="bottom")
                 
left  = Frame(root, borderwidth=0, relief="solid")
right = Frame(root, borderwidth=0, relief="solid")

#//container = Frame(left, borderwidth=2, relief="solid")
box = label = can = [0]*7

box1 = Frame(left, borderwidth=2, relief="solid")
box2 = Frame(left, borderwidth=2, relief="solid")
box3 = Frame(left, borderwidth=2, relief="solid")

box4 = Frame(right, borderwidth=2, relief="solid")
box5 = Frame(right, borderwidth=2, relief="solid")
box6 = Frame(right, borderwidth=2, relief="solid")

label1 = Label(box1, text="box1")
label2 = Label(box2, text="box2")
label3 = Label(box3, text="box3")
label4 = Label(box4, text="box4")
label5 = Label(box5, text="box5")
label6 = Label(box6, text="box6")

can1 = Canvas(box1,width=512,height=512)
can1.pack(side="top",expand=True)
can1.configure(background='black')

can2 = Canvas(box2,width=512,height=512)
can2.pack(side="top",expand=True)
can2.configure(background='black')

can3 = Canvas(box3,width=512,height=512)
can3.pack(side="top",expand=True)
can3.configure(background='black')

can4 = Canvas(box4,width=512,height=512)
can4.pack(side="top",expand=True)
can4.configure(background='black')

can5 = Canvas(box5,width=512,height=512)
can5.pack(side="top",expand=True)
can5.configure(background='black')

can6 = Canvas(box6,width=512,height=512)
can6.pack(side="top",expand=True)
can6.configure(background='black')

left.pack(side="left",  expand=True, fill="both")
right.pack(side="right", expand=True, fill="both")

#//container.pack(expand=True, fill="both", padx=5, pady=5)

box1.pack(expand=True, fill="both", padx=10, pady=10)
box2.pack(expand=True, fill="both", padx=10, pady=10)
box3.pack(expand=True, fill="both", padx=10, pady=10)

box4.pack(expand=True, fill="both", padx=10, pady=10)
box5.pack(expand=True, fill="both", padx=10, pady=10)
box6.pack(expand=True, fill="both", padx=10, pady=10)

label1.pack()
label2.pack()
label3.pack()

label4.pack()
label5.pack()
label6.pack()

root.mainloop()
