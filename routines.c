/*

  routines.c

  routines for *info

  Date:        1995-04-15 23:51:55
  Last Change: 2001-02-24 23:43:21

  Copyright (C) 1995-2001 Sander van Malssen <svm@kozmix.cistron.nl>

  This software is released under the GNU Public Licence. See the file
  `COPYING' for details. Since you're probably running Linux I'm sure
  your hard disk is already infested with copies of this file, but if
  not, mail me and I'll send you one.

*/

static char *rcsid = "$Id: routines.c,v 1.24 2001/02/24 23:30:35 svm Exp svm $";

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <termcap.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>	/* for HZ -- should be in <time.h> ? */

#ifdef DEBUG
#define DMALLOC_FUNC_CHECK
#include <dmalloc.h>
#endif

#include "procinfo.h"

#ifndef NSIG
#ifdef _NSIG
#define NSIG _NSIG
#else
#define NSIG 32
#endif
#endif

extern char *cd, *ce, *cl, *cm, *ho, *se, *so, *ve, *vi;
extern int co, li, sg;
extern int fs, redrawn;
extern int nr_cpus;
extern FILE *versionfp;
extern char *version;

static struct termios oldstate, newstate;


/**** SIGNAL and SCREEN HANDLING ****/

void
window_init (void)
{
    struct sigaction sa;
    int i;

    tcgetattr (0, &oldstate);
    newstate = oldstate;
    newstate.c_lflag &= ~ICANON;
    newstate.c_lflag &= ~ECHO;
    tcsetattr (0, TCSANOW, &newstate);

    sa.sa_flags = 0;
    sigfillset (&sa.sa_mask);

    sa.sa_handler = quit;
    for (i = 1; i < NSIG; i++)
	sigaction (i, &sa, NULL);

    sa.sa_handler = winsz;
    sigaction (SIGWINCH, &sa, NULL);

    sa.sa_handler = cont;
    sigaction (SIGCONT, &sa, NULL);

    sa.sa_handler = tstp;
    sigaction (SIGTSTP, &sa, NULL);
}

void
tstp (int i)
{
    tcsetattr (0, TCSANOW, &oldstate);
    printf ("%s%s%s", ve, se, tgoto (cm, 0, li - 1));
    fflush (stdout);
    raise (SIGSTOP);
}

void
cont (int i)
{
    printf ("%s%s", cl, vi);
    tcsetattr (0, TCSANOW, &newstate);
    fflush (stdout);
}


/* This function originally stolen from top(1) (kmem-version). */

void
winsz (int i)
{
    struct winsize ws;

    co = li = 0;
    if (ioctl (1, TIOCGWINSZ, &ws) >= 0) {
	co = ws.ws_col;
	li = ws.ws_row;
    }
    if (isatty(fileno(stdout))) {
	if (co == 0)
	    co = tgetnum ("co");
	if (li == 0)
	    li = tgetnum ("li");
    }

    if (co == 0)
	co = 80;
    if (li == 0)
	li = 24;

    li -= 2;

    version = make_version (versionfp);

    redrawn = 1;
}

void
quit (int i)
{
    tcsetattr (0, TCSANOW, &oldstate);

    if (i == 0) {		/* This is an official exit. */
	printf ("%s%s%s", ve, se, tgoto (cm, 0, li + 1));
	exit (0);
    } else {
	printf ("%s%s%s", ve, se, tgoto (cm, 0, li));
	printf ("[%s]\n", sys_siglist[i]);
	exit (128 + i);
    }
}

void
set_echo (int i)
{
    if (i) {
	tcsetattr (0, TCSANOW, &oldstate);
	printf ("%s", ve);
	fflush (stdout);
    } else {
	printf ("%s", vi);
	tcsetattr (0, TCSANOW, &newstate);
    }
}

/*
  The next couple of functions are either directly stolen or else
  adapted from the GNU termcap manual, and therefore presumed
  copyrighted by the FSF.
  See the file `COPYING' for licencing details.
*/

void
fatal (const char *s,...)
{
    va_list l;

    va_start (l, s);
    vfprintf (stderr, s, l);
    va_end (l);

    exit (EXIT_FAILURE);
}

void *
my_xcalloc (size_t n, size_t s)
{
    void *p;

    p = calloc (n, s);
    if (p == 0)
	fatal ("my_xmalloc: memory exhausted\n");
    return p;
}

#ifdef __GNUC__
#define term_buffer 0
#else
static char term_buffer[2048];
#endif

void
init_terminal_data (void)
{
    char *termtype;
    int success;

    if (!(termtype = getenv ("TERM")))
	fatal ("Please specify a terminal type.\n");

    success = tgetent (term_buffer, termtype);
    if (success < 0)
	fatal ("Could not access the termcap data base.\n");
    if (success == 0)
	fatal ("Terminal type `%s' is not defined.\n", termtype);
}

char *
my_tgets (char *te)
{
#ifdef __GNUC__
#define BUFFADDR 0
#else
    /* Here we assume that an explicit term_buffer was provided to tgetent. */
    char *buffer = (char *) malloc (strlen (term_buffer));
#define BUFFADDR &buffer
#endif
    char *temp;

    /* Extract information we will use. */
    if ((temp = tgetstr (te, BUFFADDR)) != NULL)
	return temp;
    else
	return "";
}

/**** END OF FSF STUFF ****/

/**** MISCELLANEOUS ****/

char *
make_version (FILE * versionfp)
{
    char line[1024], myname[65];
    static char wheee[1024];
    char *p = line, *here, *there;
    size_t len;
    int ret; /* for gdb */

    /* These are the bits of /proc/version */
    char ver[64], host[1024], gcc[1024], date[1024], cpus[16];
    char compno[64];

    sprintf (cpus, "%dCPU", nr_cpus);

    gethostname (myname, 65);
    if (strchr(myname, '.') == NULL) {
	len = strlen(myname);
	if (len - 2 < 65) {		/* otherwise don't bother */
	    myname[len] = '.';
	    getdomainname(&myname[len+1], 65-2-len);
	}
    }

    fseek (versionfp, 0L, SEEK_SET);
    fgets (line, sizeof (line), versionfp);

    ret = sscanf (line, "Linux version %s (%[^)]) (gcc %[^)]) #%s %[^\n]",
		  ver, host, gcc, compno, date);

    if (ret != 5)	/* Damn egcs uses nested ()'s... */
	ret = sscanf (line, "Linux version %s (%[^)]) (gcc %[^(] (%*[^)])) #%s %[^\n]",
		      ver, host, gcc, compno, date);

    if (ret == 3) {	/* At least we've got ver & host right... */
	strcpy (gcc, "[can't parse]");
	strcpy (compno, "???");
	date[0] = 0;
    }

    /* BTW, from here we're free to re-use line[]. */

    here = strdup (myname);
    there = strdup (host);

    /*
      gcc[] may now be of the form
      	"version 2.7.2"
      for regular gcc releases or
      	"driver version 2.7.2p snapshot 970207 executing gcc version 2.7.2p"
      for snapshots. Or worse.
    */

    strcpy (line, gcc);

    if (strncmp (line, "version egcs", 12) == 0	||	/* This is egcs */
	(strncmp (line, "version pgcc", 12) == 0))	/* This is pgcc */
	ret = sscanf (line, "version %s", gcc);
    else if (line[0] == 'v')			/* This is "version 2.7.2" */
	strcpy (gcc, &line[8]);
    else if (line[0] == 'd') {		/* This is "driver version 2.7.2p" */
	char dee[64], dum[64];

	ret = sscanf (line, "driver version %s snapshot %s", dee, dum);
	if (ret == 2)
	    sprintf (gcc, "%s-%s", dee, dum);
	else
	    strcpy (gcc, "unknown");	/* This is "Or worse." */
    }

    /* First, let's see what happens if we put everything in. */
    sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s %s [%s]",
	     ver, host, gcc, compno, date, cpus, myname);

    /* Too long: truncate this system's name. */
    if ((len = strlen (wheee)) > (size_t) co) {
	for (p = myname; *p; p++)
	    if (*p == '.') {
		*p = '\0';
		break;
	    }
	sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s %s [%s]",
		 ver, host, gcc, compno, date, cpus, myname);
    }

    /* Too long: truncate compiling system's name. */
    if ((len = strlen (wheee)) > (size_t) co) {
	for (p = host; *p; p++)
	    if (*p == '.') {
		*p = '\0';
		break;
	    }
	sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s %s [%s]",
		 ver, host, gcc, compno, date, cpus, myname);
    }

    /* Restore hostnames, try again without date. */
    strcpy (myname, here);
    free (here);
    strcpy (host, there);
    free (there);

    /* Too long: try without date. */
    if ((len = strlen (wheee)) > (size_t) co)
	sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s [%s]",
		 ver, host, gcc, compno, cpus, myname);

    /* Too long: truncate this system's name. */
    if ((len = strlen (wheee)) > (size_t) co) {
	for (p = myname; *p; p++)
	    if (*p == '.') {
		*p = '\0';
		break;
	    }
	sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s [%s]",
		 ver, host, gcc, compno, cpus, myname);
    }

    /* Too long: truncate compiling system's name. */
    if ((len = strlen (wheee)) > (size_t) co) {
	for (p = host; *p; p++)
	    if (*p == '.') {
		*p = '\0';
		break;
	    }
	sprintf (wheee, "Linux %s (%s) (gcc %s) #%s %s [%s]",
		 ver, host, gcc, compno, cpus, myname);
    }

    return wheee;
}

FILE *
myfopen (char *name)
{
    FILE *fp;

    if ((fp = fopen (name, "r")) == NULL) {
	fprintf (stdout, "can't open file %s: %s\n", name, strerror (errno));
	exit (1);
    }
    return fp;
}

/* Note: we're using a static char array, so use this only once per printf. */
char *
hms (unsigned long t)
{
    unsigned int d, h, m, s;
    static char buf[22];

    t = t * 100 / HZ;
    d = (int) (t / 8640000);
    t = t - (long) (d * 8640000);
    h = (int) (t / 360000);
    t = t - (long) (h * 360000);
    m = (int) (t / 6000);
    t = t - (long) (m * 6000);
    s = (int) (t / 100);
    t = t - (long) (s * 100);
    if (d > 0)
	sprintf (buf, "%3ud %2u:%02u:%02u.%02u", d, h, m, s, (int) t);
    else
	sprintf (buf, "     %2u:%02u:%02u.%02u", h, m, s, (int) t);
    return buf;
}

/* Note: we're using a static char array, so use this only once per printf. */
char *
perc (unsigned long i, unsigned long t, int cpus)
{
    unsigned int v;
    static char buf[16];

    if ((signed long) i == -1 || t == 0)
	return "---.-%";

    v = (unsigned int) (i < 1000000 ?
			((1000 * i + t / 2) / t) :
			((i + t / 2000) / (t / 1000)));
    v /= cpus;

    /* if (v > 1000)
	return "+++.+%";
    else */
	sprintf (buf, "%3u.%u%%", v / 10, v % 10);
    return buf;
}


/*
   Local variables:
   rm-trailing-spaces: t
   End:
*/
