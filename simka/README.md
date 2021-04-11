# SIMKA

# Build and Run a demonstration

        makefile target | description
        --------------- | -----------
        make all        | bash script "build"
        make go         | bash script "demo"
        make stop       | kills simulator and display jobs
        make clean      | rm *~
        
# Synopsis

I have reduced SIMS from github (by Richard Cornwell) to simulate
only one machine, the KA serial #32 that was at Stanford in 1974,
which had type=3330-2 disk packs, 200 Megabytes each,
at files in ~/ckd/SYS00[0-1].ckd

which is on github.com:saildart/waits-disk as Chickadee DISKPACK[0-1].ckd
The TLA "SYS" is over-used and ambigous;
I will try to replace it with a short specific term, here *"diskpack"*
or a short memorable word *"chickadee"*
to remind us of "ckd" from Cornwell's Sky world.

# pythonic consoles

## Data Disc raster display console

   * dd1term.py         sim Data Disc for one terminal
   * dd4term.py         sim Data Disc for four terminals
   * dd32term.py        Assume you have a 1440x2560 monitor and want to see all 32 Data Disc Screens.

## Triple-III vector display console

   Symbolic links to iii6.py to implement, one by one,
   each of the six Triple-III stations at Stanford A.I. in 1974.

   * iii20.py
   * iii21.py
   * iii22.py
   * iii23.py
   * iii24.py
   * iii25.py

## local to simka import packages

   * import ddd_tk as ddd
   * import iii_tk as iii
   * import lester_keyboard as lkb

## temporary display development hacking

   * segment.py
   * dd1k_fat_devo.py
   * xgp_tk.py                  place holder
   



