#include "ka10_defs.h"

// Librascope disk

#define DSK_DEVNUM 0444
#define DSK_OFF (1 << DEV_V_UF)
#define PIA_CH          u3
#define PIA_FLG         07
#define CLK_IRQ         010

t_stat         dsk_devio(uint32 dev, uint64 *data);
const char     *dsk_description (DEVICE *dptr);
t_stat         dsk_srv(UNIT *uptr);
t_stat         dsk_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         dsk_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc);
t_stat         dsk_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc);

UNIT dsk_unit[] = {
  {UDATA(dsk_srv, UNIT_IDLE|UNIT_DISABLE, 0)},  /* 0 */
};
DIB dsk_dib = {DSK_DEVNUM, 1, &dsk_devio, NULL};

MTAB dsk_mod[] = {
  { MTAB_VDV, 0, "ON", "ON", dsk_set_on, dsk_show_on },
  { MTAB_VDV, DSK_OFF, NULL, "OFF", dsk_set_off },
  { 0 }
};

DEVICE dsk_dev = {
  "DSK", dsk_unit, NULL, dsk_mod, 1, 8, 0, 1, 8, 36,
  NULL, NULL, NULL, NULL, NULL, NULL,
  &dsk_dib, DEV_DISABLE | DEV_DIS | DEV_DEBUG, 0, NULL,
  NULL, NULL, NULL, NULL, NULL, &dsk_description
};

t_stat dsk_devio(uint32 dev, uint64 *data)
{
  DEVICE *dptr = &dsk_dev;
  switch(dev & 3) {
  case DATAI:
    *data = 0;
    break;
  case CONI:
    *data = 0;
    break;
  case CONO:
    dsk_unit[0].PIA_CH &= ~(PIA_FLG);
    dsk_unit[0].PIA_CH |= (int32)(*data & PIA_FLG);
    break;
  default:
    break;
  }
  return SCPE_OK;
}
t_stat dsk_srv(UNIT * uptr)
{
  return SCPE_OK;
}
const char *dsk_description (DEVICE *dptr)
{
  return "Stanford Librascope fixed head disk drive - with NO tracks available !";
}
t_stat dsk_set_on(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &dsk_dev;
  dptr->flags &= ~DSK_OFF;
  return SCPE_OK;
}
t_stat dsk_set_off(UNIT *uptr, int32 val, CONST char *cptr, void *desc)
{
  DEVICE *dptr = &dsk_dev;
  dptr->flags |= DSK_OFF;
  return SCPE_OK;
}
t_stat dsk_show_on(FILE *st, UNIT *uptr, int32 val, CONST void *desc)
{
  DEVICE *dptr = &dsk_dev;
  fprintf (st, "%s", (dptr->flags & DSK_OFF) ? "off" : "on");
  return SCPE_OK;
}
/* cut from Panofsky Facil.TED[H,DOC] for 1974-11-15
 * --------------------------------------------------------------------------------

167 HIGH SPEED CHANNEL					    SECTION 9

	The 167 is used to read data in and  out  of  memory  to  the
Librascope  disk,  and  to  read  data  into core from the television
camera analog to digital converter.  The 167 is not to be accessed by
users; this section is for system programmers only.

CONO 10,
CONI 10,
	These instructions transfer the following  data  to/from  the
processor from/to the 167.
BIT    _ ___	FUNCTION
18	|___	unused
19	|___	Early response
20     _|___	Parity error
21	|
22	|	unused
23     _|
24	|
25	|
26     _|___
27	|___	Init
28	|___	Ready
29     _|___	I/O   (0=in, 1=out)
30	|___	Data Missed
31	|___	Non-ex Mem
32     _|___	Job Done
33	|
34	|	PI Channel
35     _|___


DATAO 10,
DATAI 10,
	These instructions transfer the word  count  and  the  memory
address  to/from the 167 from/to the processor.  The word count is in
the left half of the word and the memory address is in the right.
LIBRASCOPE DISK FILE					   SECTION 10

	The Librascope disk is not to be  accessed directly by users;
the system provides  UUOs for using it which are described in the UUO
chapter of the Monitor Manual.  

CONO 444,
	This instruction sets up the PI interface.

BIT    _ ___	FUNCTION
18	|
19	|
20     _|	unused
21	|
22	|___
23     _|___	reset CND DER INT
24	|___	reset CND TI
25	|
26     _|
27	|
28	|	unused
29     _|
30	|
31	|
32     _|___
33	|
34	|	PI Channel
35     _|___

DATAO 444,
	This  instruction  sets  up  the  Disk  Address.  It  must be
executed twice, once with bit 0 on and once with bit 0 off.

BIT    _ ___	FUNCTION
0	|___	If 0, use ↓this list↓	If 1, use ↓this list↓
1	|
2      _|
3	|
4	|
5      _|
6	|
7	|
8      _|	unused
9	|
10	|
11     _|
12	|
13	|				unused
14     _|
15	|___	
16	|	Band 0
17     _|	Band 1
18	|	Band 2
19	|	Band 3
20     _|	Band 4
21	|	Band 5
22	|	Band 6
23     _|___	Band 7
24	|___	Track Set	
25	|	Sector Address 0	Sector Counter Register 0
26     _|	Sector Address 1	Sector Counter Register 1
27	|	Sector Address 2	Sector Counter Register 2
28	|	Sector Address 3	Sector Counter Register 3
29     _|	Sector Address 4	Sector Counter Register 4
30	|	Sector Address 5	Sector Counter Register 5
31	|	Sector Address 6	Sector Counter Register 6
32     _|	Sector Address 7	Sector Counter Register 7
33	|	Sector Address 8	Sector Counter Register 8
34	|	Sector Address 9	Sector Counter Register 9
35     _|___	Sector Address 10	Sector Counter Register 10

DATAI 444,
	This instruction reads the sector counter.

BIT    _ ___	FUNCTION
18	|
19	|
20     _|
21	|	unused
22	|
23     _|___
24	|___	SCI
25	|	Sector Counter 0
26     _|	Sector Counter 1
27	|	Sector Counter 2
28	|	Sector Counter 3
29     _|	Sector Counter 4
30	|	Sector Counter 5
31	|	Sector Counter 6
32     _|	Sector Counter 7
33	|	Sector Counter 8
34	|	Sector Counter 9
35     _|___	Sector Counter 10

CONI 444,
	This instruction gives the status of the disk.

BIT    _ ___	FUNCTION
18	|
19	|
20     _|
21	|	unused
22	|
23     _|
24	|___
25	|___	CND DER INT
26     _|___	CND TI
27	|___	Off line
28	|___	Hot
29     _|___	Slow down
30	|___	Sector address error
31	|___	Write error
32     _|___	Read error
33	|
34	|	PI channel
35     _|___
TELEVISION CAMERA INTERFACE				   SECTION 11

	The  television  camera  is  handled  for  the  user  through
special  UUOs described  in the  UUO chapter  of the  Monitor Manual.
However,   if you  intend  to use  the camera  you should  read  this
section because the bit format is the same as in the UUOs.

CONO 404,
	This  instruction  sets up various parameters for the picture
in the following format.

BIT    _ ___	FUNCTION
18	|
19	|	BCLIP
20     _|___
21	|
22	|	TCLIP
23     _|___
24	|
25	|	Camera Number
26     _|___
27	|
28	|	Vertical Resolution (normally 0)
29     _|___
30	|	0  
31	|	0
32     _|	0
33	|	0
34	|___	1
35     _|___	Offset left 1/2 sample

DATAO 404,
	This  instruction  sets up the size of the picture and starts
the transfer.

BIT    _ ___	FUNCTION
0	|
1	|
2      _|
3	|	First line
4	|
5      _|
6	|
7	|
8      _|___
9	|
10	|
11     _|
12	|	Horizontal offset
13	|	  (in samples)
14     _|
15	|
16	|
17     _|___
18	|
19	|
20     _|
21	|	Width (in words)
22	|
23     _|
24	|___
25	|
26     _|
27	|
28	|
29     _|
30	|	unused
31	|
32     _|
33	|
34	|
35     _|___

DATAI 404,
	This instruction reads the line counter.

BIT    _ ___	FUNCTION
18	|
19	|
20     _|
21	|
22	|	unused
23     _|
24	|
25	|
26     _|___
27	|
28	|	Line counter 1
29     _|	Line counter 2
30	|	Line counter 3
31	|	Line counter 4
32     _|	Line counter 5
33	|	Line counter 6
34	|	Line counter 7
35     _|___	Line counter 8

CONI 404,
	This  instruction is unused by the television interface so it
is utilized to read the spacewar buttons by the Hand-Eye table.
TELEVISION PAN, TILT, AND FOCUS				 SECTION 11.1

	The television is equipped with a remotely operated  pan-tilt
head  and  has  remotely  controlled  focus  and  lens  turret. These
functions can be controlled by the computer when the  switch  on  the
black  control  box  "COM-MAN"  is in the "COM" position.  To operate
these functions manually, the switch must be in the  "MAN"  position.
To  control the  camera,  you must execute:

	CONO  600,<SELECT WORD>.

The select word has the following format:

BIT NO._ ___	FUNCTION
18	|___	tilt back
19	|___	tilt forward
20     _|___	pan left
21	|___	pan right
22	|___	focus far
23     _|___	focus near
24	|___	actuate turret
25	|
26     _|
27	|
28	|
29     _|___
30	|	1
31	|	0	CHANNEL
32     _|	0
33	|	0
34	|	0	SELECT
35     _|___	0

	In order  to operate  any of  the above  functions, the  CONO
must be  sent with the bit  or bits on for  the operation required to
start the  movement.   Then  a  CONO 600,40  must  be sent  when  the
movement is required  to be stopped.  To actuate  the turret, the bit
should  be left on for  less time than it takes  to rotate the turret
to the position desired, but  for at least 15 tics.   Experiment will
quickly show how long it should be left on for proper operation.

	The position of the pan-tilt  head and of the focus mount can
be read by the computer using  the analog to digital converter.   The
focus mount is connected to A/D channel 17, the  tilt to 20,  and the
pan to  21.  See section II.E.6 THE AD-DA  CONVERTER of chapter II of
the monitor manual (SAILON No. 55) for operation of the A/D.
TELEVISION COLOR WHEEL AND TURRET POSITION		 SECTION 11.2

	The television is equipped with a four position color
wheel that can be rotated under computer control.  The computer
can also read the position of the color wheel as well as the
position of the lens turret.  The color wheel can be rotated
by sending a CONO 600,<SELECT WORD>.  The select word has the
following format:

BIT NO._ ___	FUNCTION
18	|
19	|___	color
20     _|
21	|
22	|
23     _|
24	|
25	|
26     _|
27	|
28	|
29     _|___
30	|	0
31	|	0	CHANNEL
32     _|	1
33	|	0
34	|	1	SELECT
35     _|___	0

	The filters now in the color wheel are addressed in the
following way:
BIT 18	19	COLOR
     0	0	red
     0	1	blue
     1	0	green
     1	1	clear

	The color now in position can be read along with the position
of the turret and a bit that says the color wheel is not moving.
The colors are the same as in the above chart and the turret position
is arbitrary, as the lenses may be installed in any position.  This
information can be gotten by executing a CONI 600,E and E will have
the following format:

BIT NO._ ___	FUNCTION
18	|	color wheel
19	|___	position
20     _|___	wheel not moving
21	|	turret
22	|___	position
23     _|
24	|
25	|
26     _|
27	|	unused
28	|
29     _|
30	|
31	|
32     _|
33	|
34	|
35     _|___

HAND-EYE TABLE TURNTABLE				 SECTION 11.3

	The hand-eye table has a turntable in  the  surface  to  make
stereo vision possible with only one camera by moving the world.  the
turntable has a stepping motor to drive it so that one  step  of  the
motor  equals  one  minute of arc movement of the turntable.  To move
the turntable, you execute a CONO 600,  <SELECT  WORD>.   The  select
word has the following format:

BIT NO._ ___	FUNCTION
18	|___	direction
19	|	speed
20     _|___
21	|___	GO
22	|
23     _|
24	|
25	|
26     _|
27	|
28	|
29     _|___
30	|	0
31	|	0	CHANNEL
32     _|	1
33	|	1
34	|	0	SELECT
35     _|___	1

When the direction bit is  0,  the  turntable  moves  clockwise,  and
anti-clockwise  when the bit is 1.  The speed field controls how many
steps are moved every time the CONO is executed. The  speeds  are  as
follows:

BIT 17  16  STEPS PER EXECUTION
     0	0	1
     0	1	2
     1	0	4
     1	1	8

The highest speed is unreliable in that stopping and starting it  may
miss  or  invent steps.  It can be used by starting up at a low speed
and then switching high speed, then to stop, slow down first and then
stop.   You  also  have to check if you are about to be shuffled, and
slow down.  All CONOs must have the GO bit on to have an effect.

The turntable motor can be  disengaged  to  allow  the  turntable  to
freewheel  by sending a CONO 600,16.  A CONO 600,15 engages it again.
The motor must be engaged before trying to move the turntable.
LASER MIRROR CONTROL					 SECTION 11.4

	A stepping  motor is provided  on the hand  eye table  with a
mirror  mounted on  it.  It  can be  advanced one  step at a  time in
either direction.  The shaft is stepped every time a CONO 600,<SELECT
WORD> is executed.  The select word has the following format:

BIT NO._ ___	FUNCTION
18	|___	direction
19	|
20     _|
21	|
22	|
23     _|
24	|	unused
25	|
26     _|
27	|
28	|
29     _|___
30	|	0
31	|	0	CHANNEL
32     _|	1
33	|	1
34	|	1	SELECT
35     _|___	1

*/
