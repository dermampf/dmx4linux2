/*
 * Copyright (C) Dirk Jagdmann <doj@cubic.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "dmx_adapter.hpp"

#include <getarg.h>
#include <pointer.h>

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>
static int lircfh=-1;
struct lirc_config *lircConfig=0;
#endif

/* color names used */
enum {
  CHANNEL = 1,
  CHANNEL2,
  ZERO,
  NORM,
  FULL,
  HEADLINE,
  HEADEMPH,
  HEADERROR,

  MAXCOLOR
};

/* display modes */
enum {
  DISP_MODE_DMX = 0,
  DISP_MODE_HEX,
  DISP_MODE_DEC,
  DISP_MODE_MAX,
};

int MAXCHANNELS=512;
int MAXFKEY=12;

static dmx_t *dmx;
static dmx_t *dmxsave;
static dmx_t *dmxundo;


static struct dmx_device * dmxdev = 0;


static int display_mode = DISP_MODE_DMX;
static unsigned int current_channel = 0;	/* channel cursor is positioned on */
static int first_channel = 0;	/* channel in upper left corner */
static int channels_per_line=80/4;
static int channels_per_screen=80/4*24/2;
static int undo_possible=0;
static int current_cue=0;	/* select with F keys */
static float fadetime=1.0f;
static int fading=0;		/* percentage counter of fade process */
static int palette_number=0;
static int palette[MAXCOLOR];
static const char *errorstr=NULL;
static int channels_offset=1;


/** sleep for usec. The "select" system call is used, this routine should be portable accross most unix flavours. */
void DMXsleep(int usec)
{
  struct timeval tv;
  tv.tv_sec = usec/1000000;
  tv.tv_usec = usec%1000000;
  if(select(1, NULL, NULL, NULL, &tv) < 0)
    perror("could not select");
}

/** sleep for used. The "nanosleep" system call is used */
void DMXusleep(int usec)
{
  struct timespec time;
  time.tv_sec = usec/1000000;
  time.tv_nsec = 1000 * (usec%1000000);
  nanosleep(&time, &time);
}

/** @return the system time in milliseconds */
unsigned long timeGetTime()
{
  struct timeb t;
  ftime(&t);
  return (unsigned long)t.time*1000UL+(unsigned long)t.millitm;
}

static void update_dmx_channels(dmx_t * dmx, int start_channel, int end_channel)
{
    int port;
    for (port = start_channel/512; port < ((end_channel+512)/512); ++port)
	dmx_device_send_frame(dmxdev, port, 0, &dmx[port*512], 512);
}

/* set current DMX channel */
void set()
{
    update_dmx_channels(dmx, current_channel, 1);
}

/* set all DMX channels */
void setall()
{
    update_dmx_channels(dmx, 0, MAXCHANNELS);
}

/* get all DMX channels currently visible */
void getall()
{
    int port;
    unsigned char startcode;
    unsigned char payload[512];
    const int size = dmx_device_read_rxframe(dmxdev,
					     &port,
					     &startcode,
					     payload);
    if (size > 0)
    {
	if ((port*512+size) <= MAXCHANNELS)
	{
	    memcpy (&dmx[port*512], payload, size);
	}
    }
}

/* display the channels numbers */
void mask()
{
  int i=0,x,y,z=first_channel;

  erase();

  /* clear headline */
  attrset(palette[HEADLINE]);
  move(0,0);
  for(x=0; x<COLS; x++)
    addch(' ');

  /* write channel numbers */
  attrset(palette[CHANNEL]);
  for(y=1; y<LINES && z<MAXCHANNELS && i<channels_per_screen; y+=2)
    {
      int toggle=0, l=0;
      move(y,0);
      for(x=0; x<channels_per_line && z<MAXCHANNELS && i<channels_per_screen; x++, i++, z++, l+=4)
	{
	  const int c=z+channels_offset;
	  switch(display_mode)
	    {
	    case DISP_MODE_DMX:
	    case DISP_MODE_DEC:
	    default:
	      if(c<1000)
		printw("%03d ",c);
	      else
		{
		  if((++toggle)&1)
		    attrset(palette[CHANNEL]);
		  else
		    attrset(palette[CHANNEL2]);
		  printw("%4d",c);
		}
	      break;

	    case DISP_MODE_HEX:
	      if(c<0x1000)
		printw("%03X ",c);
	      else
		{
		  if((++toggle)&1)
		    attrset(palette[CHANNEL]);
		  else
		    attrset(palette[CHANNEL2]);
		  printw("%4X ",c);
		}
	      break;
	    }
	}
      /* fill rest of the line */
      attrset(palette[CHANNEL]);
      while(l++<COLS)
	addch(' ');
    }
}

/* update the screen */
void values()
{
  int i=0,x,y,z=first_channel;

  /* headline */
  if(COLS>24)
    {
      time_t t=time(NULL);
      struct tm *tt=localtime(&t);
      char *s=asctime(tt);
      s[strlen(s)-1]=0; /* strip newline at end of string */

      attrset(palette[HEADLINE]);
      mvprintw(0,1,"%s", s);
    }
  if(COLS>31)
    {
      attrset(palette[HEADLINE]);
      printw(" cue:");
      attrset(palette[HEADEMPH]);
      printw("%02i", current_cue+1);
    }
  if(COLS>44)
    {
      attrset(palette[HEADLINE]);
      printw(" fadetime:");

      attrset(palette[HEADEMPH]);
      printw("%1.1f", fadetime);
    }
  if(COLS>55)
    {
      if(fading)
	{
	  attrset(palette[HEADLINE]);
	  printw(" fading:");

	  attrset(palette[HEADEMPH]);
	  printw("%02i%%", (fading<100)?fading:99);
	}
      else
	{
	  attrset(palette[HEADLINE]);
	  printw("           ");
	}
    }

  if(COLS>80)
    if(errorstr)
      {
	attrset(palette[HEADERROR]);
	printw("ERROR:%s", errorstr);
      }

  /* values */
  for(y=2; y<LINES && z<MAXCHANNELS && i<channels_per_screen; y+=2)
    {
      int l=0;
      move(y,0);
      for(x=0; x<channels_per_line && z<MAXCHANNELS && i<channels_per_screen; x++, z++, i++, l+=4)
	{
	  const int d=dmx[z];
	  switch(d)
	    {
	    case 0: attrset(palette[ZERO]); break;
	    case 255: attrset(palette[FULL]); break;
	    default: attrset(palette[NORM]);
	    }
	  if(z==current_channel)
	    attron(A_REVERSE);
	  switch(display_mode)
	    {
	    case DISP_MODE_HEX:
	      if(d==0)
		addstr("    ");
	      else
		printw(" %02x ", d);
	      break;
	    case DISP_MODE_DEC:
	      if(d==0)
		addstr("    ");
	      else if(d<100)
		printw(" %02d ", d);
	      else
		printw("%03d ", d);
	      break;
	    case DISP_MODE_DMX:
	    default:
	      switch(d)
		{
		case 0: addstr("    "); break;
		case 255: addstr(" FL "); break;
		default: printw(" %02d ", (d*100)/255);
		}
	    }
	}
      /* fill rest of the line */
      attrset(palette[ZERO]);
      while(l++<COLS)
	addch(' ');
    }
}

/* save current cue into cuebuffer */
void savecue()
{
  memcpy(&dmxsave[current_cue*MAXCHANNELS], dmx, MAXCHANNELS);
}

/* get new cue from cuebuffer */
void loadcue()
{
  memcpy(dmx, &dmxsave[current_cue*MAXCHANNELS], MAXCHANNELS);
}

/* fade cue "new_cue" into current cue */
void crossfade(int new_cue)
{
  dmx_t *dmxold;
  dmx_t *dmxnew;
  int i;
  int max=MAXCHANNELS;

  /* check parameter */
  if(new_cue>MAXFKEY || new_cue<0)
    return;

  undo_possible=0;

  /* don't crossfade for small fadetimes */
  if(fadetime<0.1f)
    {
      savecue();
      current_cue=new_cue;
      loadcue();
      setall();
      return;
    }

  savecue();
  dmxold=&dmxsave[current_cue*MAXCHANNELS];
  dmxnew=&dmxsave[new_cue*MAXCHANNELS];

  /* try to find the last channel value > 0, so we don't have to
     crossfade large blocks of 0s */
  for(i=MAXCHANNELS-1; i>=0; max=i, i--)
    if(dmxold[i]||dmxnew[i])
      break;

  {
    const unsigned long tstart=timeGetTime();
    const unsigned long tend=tstart+(int)(fadetime*1000.0);
    unsigned long t=tstart;
    while(t<=tend)
      {
	/* calculate new cue */
	t=timeGetTime();
	{
	  const float p=(float)(t-tstart)/1000.0f/fadetime;
	  const float q=1.0f-p;
	  for(i=0; i<max; i++)
	    if(dmxold[i] || dmxnew[i]) /* avoid calculating with only 0 */
	      dmx[i]=(int)((float)dmxold[i]*q + (float)dmxnew[i]*p);
	  setall();

	  /* update screen */
	  fading=(int)(p*100.0f);
	  values();
	  refresh();
	  DMXsleep(100000);

	  t=timeGetTime();		/* get current time, because the last time is too old (due to the sleep) */
	}
      }
    fading=0;

    /* set the new cue */
    current_cue=new_cue;
    loadcue();
    setall();
  }
}

void save(const char *filename)
{
  FILE *file=fopen(filename, "wb");
  if(file==NULL)
    errorstr="could not open savefile";
  else
    {
      if(fwrite(dmxsave, MAXCHANNELS, MAXFKEY, file) != MAXFKEY)
	errorstr="could not write complete savefile";
      fprintf(file, "\nfadetime %i\n", (int)(fadetime*1000.0f));
      fprintf(file, "current_cue %i\n", current_cue);
      fprintf(file, "current_channel %i\n", current_channel);
      fprintf(file, "first_channel %i\n", first_channel);
      fprintf(file, "palette_number %i\n", palette_number);
      if(fclose(file) < 0)
	errorstr="could not close savefile";
    }
}

void load(const char *filename)
{
  FILE *file=fopen(filename, "rb");
  if(file==NULL)
    {
      fprintf(stderr, "unable to open %s : %s\n", filename, strerror(errno));
      exit(1);
    }
  fread(dmxsave, MAXCHANNELS, MAXFKEY, file);
  while(!feof(file))
    {
      char buf[1024];
      int i;
      if(fscanf(file, "%s %i\n", buf, &i) != 2)
	continue;
      if(!strcmp("fadetime", buf))
	fadetime=i/1000.0f;
      else if(!strcmp("current_channel", buf))
	current_channel=i;
      else if(!strcmp("first_channel", buf))
	first_channel=i;
      else if(!strcmp("current_cue", buf))
	current_cue=i;
      else if(!strcmp("palette_number", buf))
	palette_number=i;
    }
  fclose(file);
}

void undo()
{
  if(undo_possible)
    {
      memcpy(dmx, dmxundo, MAXCHANNELS);
      undo_possible=0;
    }
}

void undoprep()
{
  memcpy(dmxundo, dmx, MAXCHANNELS);
  undo_possible=1;
}

/* change palette to "p". If p is invalid new palette is number "0". */
void changepalette(int p)
{
  /* COLOR_BLACK
     COLOR_RED
     COLOR_GREEN
     COLOR_YELLOW
     COLOR_BLUE
     COLOR_MAGENTA
     COLOR_CYAN
     COLOR_WHITE

     A_NORMAL
     A_ATTRIBUTES
     A_CHARTEXT
     A_COLOR
     A_STANDOUT
     A_UNDERLINE
     A_REVERSE
     A_BLINK
     A_DIM
     A_BOLD
     A_ALTCHARSET
     A_INVIS
  */
  switch(p)
    {
    default:
      palette_number=0;
    case 0:
      init_pair(CHANNEL, COLOR_BLACK, COLOR_CYAN);
      init_pair(CHANNEL2, COLOR_BLACK, COLOR_GREEN/*COLOR_YELLOW*/);
      init_pair(ZERO, COLOR_BLACK, COLOR_WHITE);
      init_pair(NORM, COLOR_BLUE, COLOR_WHITE);
      init_pair(FULL, COLOR_RED, COLOR_WHITE);
      init_pair(HEADLINE, COLOR_WHITE, COLOR_BLUE);
      init_pair(HEADEMPH, COLOR_YELLOW, COLOR_BLUE);
      init_pair(HEADERROR, COLOR_RED, COLOR_BLUE);
      goto color;

    case 2:
      init_pair(CHANNEL, COLOR_BLACK, COLOR_WHITE);
      init_pair(CHANNEL2, COLOR_BLACK, COLOR_YELLOW);
      init_pair(ZERO, COLOR_BLUE, COLOR_BLACK);
      init_pair(NORM, COLOR_GREEN, COLOR_BLACK);
      init_pair(FULL, COLOR_RED, COLOR_BLACK);
      init_pair(HEADLINE, COLOR_WHITE, COLOR_BLACK);
      init_pair(HEADEMPH, COLOR_CYAN, COLOR_BLACK);
      init_pair(HEADERROR, COLOR_RED, COLOR_BLACK);
      goto color;

    color:
      palette[CHANNEL]=COLOR_PAIR(CHANNEL);
      palette[CHANNEL2]=COLOR_PAIR(CHANNEL2);
      palette[ZERO]=COLOR_PAIR(ZERO);
      palette[NORM]=COLOR_PAIR(NORM);
      palette[FULL]=COLOR_PAIR(FULL);
      palette[HEADLINE]=COLOR_PAIR(HEADLINE);
      palette[HEADEMPH]=COLOR_PAIR(HEADEMPH);
      palette[HEADERROR]=COLOR_PAIR(HEADERROR);
      break;

    case 1:
      palette[CHANNEL]=A_REVERSE;
      palette[CHANNEL2]=A_REVERSE|A_BOLD;
      palette[ZERO]=A_NORMAL;
      palette[NORM]=A_NORMAL;
      palette[FULL]=A_BOLD;
      palette[HEADLINE]=A_NORMAL;
      palette[HEADEMPH]=A_NORMAL;
      palette[HEADERROR]=A_BOLD;
      break;
    }

  mask();
}

void CHECK(void *p)
{
  if(p==NULL)
    {
      fprintf(stderr, "could not alloc\n");
      exit(1);
    }
}

/* signal handler for crashes. Program is aborted and cues are saved to "crash.chn". */
void crash(int sig)
{
  save("crash.chn");
  exit(1);
}

/* calculate channels_per_line and channels_per_screen from LINES and COLS */
void calcscreengeometry()
{
  if(COLS<4)
    {
      errorstr="screen to small, we need at least 4 columns";
      exit(1);
    }
  channels_per_line=COLS/4;

  int c=LINES;
  if(c<3)
    {
      errorstr="screen to small, we need at least 3 lines";
      exit(1);
    }
  c--;				/* one line for headline */
  if(c%2==1)
    c--;
  channels_per_screen=channels_per_line*c/2;
}

/* signal handler for SIGWINCH */
static volatile int resize_width=-1, resize_height=-1;
void terminalresize(int sig)
{
  struct winsize size;
  if(ioctl(0, TIOCGWINSZ, &size) < 0)
    return;
  resize_width=size.ws_col;
  resize_height=size.ws_row;
}

WINDOW *w=NULL;

void help()
{
  printf("usage: dmxconsole [--dmx <device>] [--channels <number>] [--fkeys <number>] [--js <device>] [--mm <device>] [--ms <device>] [--ps2 <device>] [--mousescale <number>] [<filename>]\n");
  exit(0);
}

#define KEYBUFSIZE 128
int keybuf[KEYBUFSIZE], keybufLen=0;
void pointerevent()
{
  static int oldX=0, oldY=0, oldZ=0, oldB=0, oldY2=0;
  int x=0, y=0, z=0, b, i;

  x=pointerX()-oldX;
  y=pointerY()-oldY;
  z=pointerZ()-oldZ;
  oldX=pointerX();
  oldY=pointerY();
  oldZ=pointerZ();

  b=pointerB();
  if(b&BUT3)
    keybuf[keybufLen++]='d';

  if(b&BUT2)
    keybuf[keybufLen++]=' ';

  /* get click edge */
  if(b&BUT1 && !(oldB&BUT1))
    oldY2=oldY;

  if(b&BUT1)
    {
      const int y=oldY-oldY2;
      if(y<0)
	for(i=0; i<-y && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]='+';
      else if(y>0)
	for(i=0; i<y && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]='-';
    }
  else
    {
      if(y<0)
	for(i=0; i<-y && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_UP;
      else if(y>0)
	for(i=0; i<y && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_DOWN;

      if(x<0)
	for(i=0; i<-x && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_LEFT;
      else if(x>0)
	for(i=0; i<x && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_RIGHT;

      if(z<0)
	for(i=0; i<-z && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_PPAGE;
      else if(z>0)
	for(i=0; i<z && keybufLen<KEYBUFSIZE; i++)
	  keybuf[keybufLen++]=KEY_NPAGE;
    }

  oldB=b;
}

int meventcmp(MEVENT a, MEVENT b)
{
  return a.id==b.id && a.x==b.x && a.y==b.y && a.z==b.z && a.bstate==b.bstate;
}

/* cleanup handler for program exit. */
void cleanup()
{
  erase();
  mvprintw(10,30,"Exiting Console....");
  mvprintw(11,30,"* Shutting Down User Interface....");

  if(w)
    {
      resetty();
      endwin();
    }

#ifdef HAVE_LIRC
  printf ("           Closing LIRC hardware...");
  if(lircfh >= 0)
    {
      if(lircConfig)
	lirc_freeconfig(lircConfig);
      lirc_deinit();
    }
#endif

  printf ("           Disconnecting from DMX interface...");
  fflush(stdout);

  close_dmx_device(dmxdev);
  printf ("Done\n");

  if(errorstr)
    puts(errorstr);
}

int main (int argc, const char **argv)
{
  int c=0, cursesmouse=0, usepointer=0;
  const char *f, *dev;
#ifdef HAVE_DMX4LINUX
  struct dmx_info info;
#endif
  int sigcrash[] = {
    SIGBUS,
    SIGFPE,
    SIGILL,
    SIGIO,
    SIGIOT,
    SIGPROF,
    SIGSEGV,
    SIGSTKFLT,
#ifdef SIGSYS
    SIGSYS,
#endif
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
  };

  /* install signal handler */
  for(c=0; c<sizeof(sigcrash)/sizeof(sigcrash[0]); c++)
    signal(sigcrash[c], crash);

  signal(SIGWINCH, terminalresize);
  atexit(cleanup);

#ifdef HAVE_DMX4LINUX
  /* get DMX device */
  dev=DMXdev(&argc, argv);
  if(dev==NULL)
    {
      fprintf(stderr, "did not find any dmx device\n");
      return 1;
    }
#endif

  /* parse command line */
  if(DMXgetarg(&argc, argv, "--help", 0, NULL)==0 ||
     DMXgetarg(&argc, argv, "-h", 0, NULL)==0)
    help();

  if(DMXgetarg(&argc, argv, "--fkeys", 1, &f)==1)
    MAXFKEY=strtoul(f, NULL, 0);
  if(MAXFKEY<1)
    MAXFKEY=1;

  /* initialize pointers */
  if(DMXgetarg(&argc, argv, "--curs", 0, NULL)==0)
    {
      cursesmouse=1;
    }
  else
    {
      usepointer=pointerinit(&argc, argv);
      if(usepointer<0)
	exit(1);
    }

  /* LIRC */
#if HAVE_LIRC
  lircfh=lirc_init("dmxconsole", 1);
  if(lircfh < 0)
    printf("could not init LIRC\n");
  else
    if(lirc_readconfig(NULL /*configfile*/, &lircConfig, NULL) < 0)
      {
	/* if there is no config file, we don't want to use lirc */
	close(lircfh);
	lircfh=-1;
      }
#endif

  dmxdev = open_dmx_device();
  if (dmxdev)
  {
      const int port_count = dmx_device_port_count(dmxdev);
      if (MAXCHANNELS < port_count * 512)
	  MAXCHANNELS = port_count * 512;
  }

  /* alloc */
  dmx=(dmx_t *)calloc(MAXCHANNELS+10, sizeof(dmx_t)); /* 10 bytes security, for file IO routines, will be optimized and checked later */
  CHECK(dmx);

  dmxsave=(dmx_t *)calloc(MAXCHANNELS*MAXFKEY, sizeof(dmx_t));
  CHECK(dmxsave);

  dmxundo=(dmx_t *)calloc(MAXCHANNELS, sizeof(dmx_t));
  CHECK(dmxundo);

  /* check for file to load */
  if(argc>1)
    {
      f=argv[1];
      load(f);
      loadcue();
      setall();
    }
  else
    {
      f="default.chn";
      getall();
    }

  /* init curses */
  w = initscr();
  if (!w)
    {
      printf ("unable to open main-screen\n");
      return 1;
    }

  savetty();
  start_color();
  noecho();
  raw();
  keypad(w, TRUE);

  calcscreengeometry();
  changepalette(palette_number);

  if(cursesmouse)
    {
      mmask_t m;
      mousemask(BUTTON1_PRESSED|BUTTON2_PRESSED|BUTTON3_PRESSED|BUTTON4_PRESSED|REPORT_MOUSE_POSITION, &m);
      mouseinterval(-1);
    }

  /* main loop */
  while(1)
    {
      int n, maxfd=0;
      fd_set rd_fds;
      struct timeval tv;
      keybufLen=0;

      FD_ZERO(&rd_fds);
      FD_SET(0, &rd_fds);

#ifdef HAVE_DMX4LINUX
      
      FD_SET(dmx_device_fd(dmxdev), &rd_fds);
      if(dmx_device_fd(dmxdev) > maxfd)
	maxfd = dmx_device_fd(dmxdev);
#endif

      if(usepointer)
	{
	  const int fd=pointergetfd();
	  FD_SET(fd, &rd_fds);
	  if(fd>maxfd)
	    maxfd=fd;
	}

#if HAVE_LIRC
      if(lircfh >= 0)
	{
	  FD_SET(lircfh, &rd_fds);
	  if(lircfh>maxfd)
	    maxfd=lircfh;
	}
#endif

      tv.tv_sec = 1;
      tv.tv_usec = 0;

      n = select(maxfd+1, &rd_fds, NULL, NULL, &tv);
      if(n>0)
        {
          if(FD_ISSET(0, &rd_fds))
	    keybuf[keybufLen++]=wgetch(w);

          if(FD_ISSET(dmx_device_fd(dmxdev), &rd_fds))
	    getall();

	  if(usepointer)
	    if(FD_ISSET(pointergetfd(), &rd_fds))
	      {
		if(pointerprocess()<0)
		  {
		    errorstr="error while reading pointer device";
		    exit(1);
		  }
		pointerevent();
	      }

#if HAVE_LIRC
	  if(lircfh >= 0 && FD_ISSET(lircfh, &rd_fds))
	    {
	      static int num=0;
	      char *s=0;
	      char *c=0;
	      if(lirc_nextcode(&c) < 0)
		goto endoflirc;
	      if(!c)
		goto endoflirc;
	      if(lirc_code2char(lircConfig, c, &s) < 0)
		goto endoflirc;
	      if(!s)
		goto endoflirc;

	      /* hier */
	      errorstr=s;
	      if(!strcmp(s, "nextslot"))
		{
		  if(current_channel<MAXCHANNELS)
		    current_channel++;
		}
	      else if(!strcmp(s, "prevslot"))
		{
		  if(current_channel>0)
		    current_channel--;
		}
	      else if(!strcmp(s, "full"))
		dmx[current_channel]=255;
	      else if(!strcmp(s, "zero"))
		dmx[current_channel]=0;
	      else if(!strcmp(s, "toggle"))
		keybuf[keybufLen++]=' ';
	      else if(!strcmp(s, "up"))
		keybuf[keybufLen++]=KEY_PPAGE;
	      else if(!strcmp(s, "down"))
		keybuf[keybufLen++]=KEY_NPAGE;
	      else if(!strcmp(s, "0"))
		num=num*10+0;
	      else if(!strcmp(s, "1"))
		num=num*10+1;
	      else if(!strcmp(s, "2"))
		num=num*10+2;
	      else if(!strcmp(s, "3"))
		num=num*10+3;
	      else if(!strcmp(s, "4"))
		num=num*10+4;
	      else if(!strcmp(s, "5"))
		num=num*10+5;
	      else if(!strcmp(s, "6"))
		num=num*10+6;
	      else if(!strcmp(s, "7"))
		num=num*10+7;
	      else if(!strcmp(s, "8"))
		num=num*10+8;
	      else if(!strcmp(s, "9"))
		num=num*10+9;
	      else if(!strcmp(s, "selectslot"))
		{
		  if(num<MAXCHANNELS)
		    current_channel=num;
		  num=0;
		}
	      else if(!strcmp(s, "blackout"))
		keybuf[keybufLen++]='b';
	      else if(!strcmp(s, "undo"))
		keybuf[keybufLen++]='u';
	      else if(!strcmp(s, "crossfade"))
		{
		  if(num<MAXFKEY)
		    keybuf[keybufLen++]=KEY_F(num);
		  num=0;
		}

	      free(c);
	    }
	endoflirc: ;
#endif

	}
      else if(n==0)
	{
	  /* timeout */
	}

      for(n=0; n<keybufLen; n++)
	{
	  c=keybuf[n];
	switch(c)
	  {
	  case KEY_PPAGE:
	    undoprep();
	    if(dmx[current_channel] < 255-0x10)
	      dmx[current_channel]+=0x10;
	    else
	      dmx[current_channel]=255;
	    set();
	    break;

	  case '+':
	    if(dmx[current_channel] < 255)
	      {
		undoprep();
		dmx[current_channel]++;
	      }
	    set();
	    break;

	  case KEY_NPAGE:
	    undoprep();
	    if(dmx[current_channel]==255)
	      dmx[current_channel]=0xe0;
	    else if(dmx[current_channel] > 0x10)
	      dmx[current_channel]-=0x10;
	    else
	      dmx[current_channel]=0;
	    set();
	    break;

	  case '-':
	    if(dmx[current_channel] > 0)
	      {
		undoprep();
		dmx[current_channel]--;
	      }
	    set();
	    break;

	  case ' ':
	    undoprep();
	    if(dmx[current_channel]<128)
	      dmx[current_channel]=255;
	    else
	      dmx[current_channel]=0;
	    set();
	    break;

	  case '0' ... '9':
	    fadetime=c-'0';
	    break;

	  case KEY_HOME:
	    current_channel=0;
	    first_channel=0;
	    mask();
	    break;

	  case KEY_END:
	    current_channel=MAXCHANNELS-1;
	    first_channel=current_channel+(channels_per_line-(current_channel%channels_per_line)) - channels_per_screen;
	    if(first_channel<0)
	      first_channel=0;
	    mask();
	    break;

	  case KEY_RIGHT:
	    if(current_channel < MAXCHANNELS-1 && (current_channel%channels_per_line)!=channels_per_line-1)
	      current_channel++;
	    break;

	  case KEY_LEFT:
	    if(current_channel > 0 && (current_channel%channels_per_line)!=0)
	      current_channel--;
	    break;

	  case KEY_DOWN:
	    current_channel+=channels_per_line;
	    if(current_channel>=MAXCHANNELS)
	      current_channel=MAXCHANNELS-1;
	    if(current_channel >= first_channel+channels_per_screen)
	      {
		first_channel+=channels_per_line;
		mask();
	      }
	    break;

	  case KEY_UP:
	    if(current_channel>=channels_per_line)
	      current_channel-=channels_per_line;
	    if(current_channel < first_channel)
	      {
		first_channel-=channels_per_line;
		if(first_channel<0)
		  first_channel=0;
		mask();
	      }
	    break;

	  case KEY_IC:
	    undoprep();
	    for(n=MAXCHANNELS-1; n>current_channel && n>0; n--)
	      dmx[n]=dmx[n-1];
	    setall();
	    break;

	  case KEY_DC:
	    undoprep();
	    for(n=current_channel; n<MAXCHANNELS-1; n++)
	      dmx[n]=dmx[n+1];
	    setall();
	    break;

	  case 'B':
	  case 'b':
	    undoprep();
	    memset(dmx, 0, MAXCHANNELS);
	    setall();
	    break;

	  case 'D':
	  case 'd':
	    dmx[current_channel]=0;
	    set();
	    break;

	  case 'F':
	  case 'f':
	    undoprep();
	    memset(dmx, 0xff, MAXCHANNELS);
	    setall();
	    break;

	  case 'M':
	  case 'm':
	    if(++display_mode>=DISP_MODE_MAX)
	      display_mode=0;
	    mask();
	    break;

	  case 'N':
	  case 'n':
	    if(++channels_offset>1)
	      channels_offset=0;
	    mask();
	    break;

	  case 'P':
	  case 'p':
	    changepalette(++palette_number);
	    break;

	  case 'Q':
	  case 'q':
            erase();
            mvprintw (10,30,"Exiting Console....");
	    exit(0);

	  case 'S':
	  case 's':
	    savecue();
	    save(f);
	    break;

	  case 'U':
	  case 'u':
	    undo();
	    break;

          case KEY_F(61): /* GO-BACK */
            crossfade((current_cue+11)%12);
            break;

          case KEY_F(62): /* GO */
            crossfade((current_cue+1)%12);
            break;

	  case KEY_MOUSE:
	    {
	      static MEVENT oldevent;
	      MEVENT event;
	      if(getmouse(&event) == OK && event.id==0 && !meventcmp(event,oldevent))
		{
#if 0
		  fprintf(stderr, "%i: %i %i %i %04X\n", event.id, event.x, event.y, event.z, event.bstate);

#endif
		  /* mouse y must be even and >=2 */
		  if((event.y&1)==0 && event.y>=2)
		    {
		      int B=0;
		      if(event.bstate & BUTTON1_PRESSED)
			B|=BUT1, fprintf(stderr, "but1\n");
		      if(event.bstate & BUTTON2_PRESSED)
			B|=BUT2, fprintf(stderr, "but2\n");
		      if(event.bstate & BUTTON3_PRESSED)
			B|=BUT3, fprintf(stderr, "but3\n");
		      if(event.bstate & BUTTON4_PRESSED)
			B|=BUT4, fprintf(stderr, "but4\n");
		      pointersetB(B);

		      /* position cursor */
		      current_channel=first_channel + channels_per_line*((event.y-2)/2) + event.x/4;
		      if(current_channel >= MAXCHANNELS)
			current_channel=MAXCHANNELS-1;

		      pointerevent();
		    }
		  oldevent=event;
		}
	    }
	    break;

	  default:
	    if(c>=KEY_F(1) && c<=KEY_F(MAXFKEY))
	      crossfade(c-KEY_F(1));
	    break;

	  } /* switch */
	} /* for */

      /* check if the screen geometry has changed */
      if(resize_height >= 0)
	{
	  resizeterm(resize_height, resize_width);
	  calcscreengeometry();
	  mask();
	  resize_height=resize_width=-1;
	}

      values();
      refresh();
    } /* while */

  return 0;
}
