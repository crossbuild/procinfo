/*

  procinfo.c

  Displays general info from /proc.

  Date:        1995-07-08 05:30:34
  Last Change: 2001-02-25 00:27:21

  Copyright (c) 1994-2001 svm@kozmix.cistron.nl

  This software is released under the GNU Public Licence. See the file
  `COPYING' for details. Since you're probably running Linux I'm sure
  your hard disk is already infested with copies of this file, but if
  not, mail me and I'll send you one.

*/

static char *rcsid = "$Id: procinfo.c,v 1.56 2001/02/25 11:29:13 svm Exp svm $";

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <termcap.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>	/* for HZ -- should be in <time.h> ? */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>

#ifdef DEBUG
#define DMALLOC_FUNC_CHECK
#include <dmalloc.h>
#endif

#include "procinfo.h"

void init_terminal_data (void);
char *my_tgets (char *te);

char *cd;			/* clear to eos */
char *ce;			/* clear to eol */
char *cl;			/* clear screen */
char *cm;			/* move cursor */
char *ho;			/* home */
char *se;			/* standout off */
char *so;			/* standout on */
char *ve;			/* cursor on */
char *vi;			/* cursor off */
int co;				/* columns */
int li;				/* lines */
int sg;				/* cookie width */

int fs = 0, redrawn = 0;
int show_stat = 0;
int show_moddev = 0;
int show_sys = 0;
int show_all = 0;
int show_diff = 0;
int show_new_mem = 0;
int show_real_mem = 0;
int show_from_baseline = 0;
int io_or_blk = 0;
int linux_version_code;
int per_sec = 0;
int squeeze_irqs = 1;
int nr_irqs = 0;
int nr_cpus = 0;

FILE *loadavgfp, *meminfofp, *modulesfp, *statfp, *uptimefp,
    *devicesfp, *filesystemsfp, *interruptsfp, *dmafp, *cmdlinefp,
    *versionfp, *cpuinfofp;

char line[1024], cmdline[1024], booted[40], *version = NULL, *message = NULL;

float rate = 1.0;		/* per interval or per sec */

struct info new, old, base;
struct utsname ut;


static void
first_page (long sl)
{
    unsigned long elapsed;
    char loadavg[32];
    int i;
    static int have_m_c = -1;	/* Do we have cache info? */
    static int have_m_l = -1;	/* Do we have new-style-one-datum-per-line? */


/**** /proc/uptime ****/

    fclose (uptimefp); uptimefp = myfopen (PROC_DIR "uptime");

    fgets (line, sizeof (line), uptimefp);
    new.uptime =
	(unsigned long) (atof (strtok (line, " ")) * (unsigned long) HZ);


/**** /proc/meminfo ****/

    fclose (meminfofp); meminfofp = myfopen (PROC_DIR "meminfo");

    fgets (line, sizeof (line), meminfofp);

    if (have_m_c == -1) {
	if (strstr (line, "cached:"))
	    have_m_c = 1;
	else
	    have_m_c = 0;
    }

    if (have_m_l == -1) {
	char kickme[1024];
	/* God, this next stuff needs AT LEAST a macro to hide behind. */
	fclose (meminfofp); meminfofp = myfopen (PROC_DIR "meminfo");
	fread (kickme, 1024, 1, meminfofp);
	if (strstr (kickme, "MemTotal:"))
	    have_m_l = 1;
	else
	    have_m_l = 0;
	fclose (meminfofp); meminfofp = myfopen (PROC_DIR "meminfo");
    }


    printf ("Memory:      Total        Used        Free      "
	    "Shared     Buffers      %s\n", have_m_c ? "Cached" : "");

    if (have_m_l) {

	new.m_to = new.m_us = new.m_fr = new.m_sh = new.m_bu = 0;
	new.s_to = new.s_us = new.s_fr = 0;

	do {
	    char *type = strtok (line, ":");

	    if (ISSTR ("MemTotal"))
		new.m_to = VAL;
	    else if (ISSTR ("MemFree"))
		new.m_fr = VAL;
	    else if (ISSTR ("MemShared"))
		new.m_sh = VAL;
	    else if (ISSTR ("Buffers"))
		new.m_bu = VAL;
	    else if (ISSTR ("Cached"))
		new.m_ca = VAL;
	    else if (ISSTR ("SwapTotal"))
		new.s_to = VAL;
	    else if (ISSTR ("SwapFree"))
		new.s_fr = VAL;
	} while (fgets (line, sizeof (line), meminfofp));

	new.m_us = new.m_to - new.m_fr;
	new.s_us = new.s_to - new.s_fr;

    } else {

	fgets (line, sizeof (line), meminfofp);
	strtok (line, " ");
	new.m_to = VAL / 1024;
	new.m_us = VAL / 1024;
	new.m_fr = VAL / 1024;
	new.m_sh = VAL / 1024;
	new.m_bu = VAL / 1024;
	if (have_m_c)
	    new.m_ca = VAL / 1024;
    }

    if (show_new_mem) {
	printf ("Mem:  %12ld%12ld%12ld%12ld%12ld",
		new.m_to, new.m_us, new.m_fr, new.m_sh,
		new.m_bu);
	if (have_m_c)
	    printf ("%12ld\n", new.m_ca);
	else
	    putchar ('\n');
    } else {
	/*
	  We're going to cheat a bit by replacing rate with sl/1e6 in
	  the DIFF macro, so that we'll end up with nice multiples of
	  4 when using the -d option.
	*/
#define rate (sl>1000000 ? sl/1000000 : 1)
	printf ("Mem:  %12ld%12ld%12ld%12ld%12ld",
		DIFF (m_to), DIFF (m_us), DIFF (m_fr), DIFF (m_sh), DIFF (m_bu));
	if (have_m_c)
	    printf ("%12ld\n", DIFF (m_ca));
	else
	    putchar ('\n');
#undef rate
    }

    if (have_m_l) {
	/* Taken care of above... */
    } else {
    	fgets (line, sizeof (line), meminfofp);
    	strtok (line, " ");
    	new.s_to = VAL / 1024;
    	new.s_us = VAL / 1024;
    	new.s_fr = VAL / 1024;
    }

    if (show_new_mem) {
        if (show_real_mem)
	    printf ("-/+ buffers%s %11ld%12ld\n",
                    have_m_c ? "/cache:" : ":      ",
		    new.m_us - new.m_bu - new.m_ca,
		    new.m_fr + new.m_bu + new.m_ca);

	printf ("Swap: %12ld%12ld%12ld\n\n",
		new.s_to, new.s_us, new.s_fr);
    } else {
	if (show_real_mem)
	    printf ("-/+ buffers%s %11ld%12ld\n",
	    	    have_m_c ? "/cache:" : ":      ",
		    DIFF (m_us) - DIFF (m_bu) - DIFF (m_ca),
		    DIFF (m_fr) + DIFF (m_bu) + DIFF (m_ca));

	printf ("Swap: %12ld%12ld%12ld\n\n",
		DIFF (s_to), DIFF (s_us), DIFF (s_fr));
    }


/**** /proc/loadavg ****/

    fseek (loadavgfp, 0L, SEEK_SET);
    fgets (line, sizeof (line), loadavgfp);
    strcpy (loadavg, line);
    loadavg[strlen (loadavg) - 1] = '\0';
    fgets (line, sizeof (line), loadavgfp);

    printf ("Bootup: %s    Load average: %s\n\n", booted, loadavg);


/**** /proc/stat ****/

    fseek (statfp, 0L, SEEK_SET);
    while (fgets (line, sizeof (line), statfp)) {
	char *type = strtok (line, " ");

	if (ISSTR ("cpu")) {
	    new.cpu_user = VAL;
	    new.cpu_nice = VAL;
	    new.cpu_sys = VAL;
	    new.cpu_idle = VAL;
	    /*
	     * according to bug #1959, sometimes the cpu_idle
	     * seems to go backwards(!) on SMP boxes.  This may
	     * have been fixed in later kernel releases.  This
	     * is just a sanity check.
	     */
	    if (new.cpu_idle < old.cpu_idle)
	      new.cpu_idle = old.cpu_idle;
	} else if (ISSTR ("disk")) {
	    new.disk[0] = VAL;
	    new.disk[1] = VAL;
	    new.disk[2] = VAL;
	    new.disk[3] = VAL;
	} else if (ISSTR ("disk_rio") && io_or_blk == 0) {
	    new.disk_r[0] = VAL;
	    new.disk_r[1] = VAL;
	    new.disk_r[2] = VAL;
	    new.disk_r[3] = VAL;
	} else if (ISSTR ("disk_wio") && io_or_blk == 0) {
	    new.disk_w[0] = VAL;
	    new.disk_w[1] = VAL;
	    new.disk_w[2] = VAL;
	    new.disk_w[3] = VAL;
	} else if (ISSTR ("disk_rblk") && io_or_blk == 1) {
	    new.disk_r[0] = VAL;
	    new.disk_r[1] = VAL;
	    new.disk_r[2] = VAL;
	    new.disk_r[3] = VAL;
	} else if (ISSTR ("disk_wblk") && io_or_blk == 1) {
	    new.disk_w[0] = VAL;
	    new.disk_w[1] = VAL;
	    new.disk_w[2] = VAL;
	    new.disk_w[3] = VAL;
	} else if (ISSTR ("disk_io:")) {
	    int disk_counter = 0, ret;
	    unsigned int q, w, e, r, t, y, u; /* I'm NOT in the mood today. */
	    char *barf;

	    while ((barf = strtok (NULL, " "))) {

		if (++disk_counter > 4) /* 4 is all we have room for */
		    continue;

		ret = sscanf (barf, "(%d,%d):(%d,%d,%d,%d,%d)",
			      &q, &w, &e, &r, &t, &y, &u);
		if (ret != 7)
		    continue;

		if (io_or_blk == 1) {
		    new.disk_r[disk_counter-1] = t;
		    new.disk_w[disk_counter-1] = u;
		} else {
		    new.disk_r[disk_counter-1] = r;
		    new.disk_w[disk_counter-1] = y;
		}
	    }
	} else if (ISSTR ("page")) {
	    new.pgin = VAL;
	    new.pgout = VAL;
	} else if (ISSTR ("swap")) {
	    new.swin = VAL;
	    new.swout = VAL;
	} else if (ISSTR ("intr")) {
	    if (nr_irqs) {
		VAL;		/* First value is total of all interrupts,
				   for compatibility with rpc.rstatd. We
				   ignore it. */
		for (i = 0; i < nr_irqs; i++) {
		    new.intr[i] = VAL;
		}
	    } else
		new.old_intr = VAL;
	} else if (ISSTR ("ctxt")) {
	    new.ctxt = VAL;
	} else if (ISSTR ("syscalls")) {
	    new.syscalls = VAL;
	}
    }

    elapsed = new.uptime;

/* XXX Is this stuff still relevant/true? */

#ifdef __i386__		/* IRQ 0 is timer tick on i386's... */
    if (nr_irqs) {
	if (fs && old.uptime)
	    elapsed = DIFF (intr[0]);
    } else
#endif
#ifdef __sparc__	/* IRQ 10 is timer tick on sparc's... */
    if (nr_irqs) {
	if (fs && old.uptime)
	    elapsed = DIFF (intr[10]);
    } else
#endif
    {
	/* This won't be exact... */
	if (fs && old.uptime)
	    elapsed = DIFF (uptime);
    }

    printf ("user  : %s %s",
	    hms (bDIFF (cpu_user)), perc (bDIFF (cpu_user), elapsed, nr_cpus));
    printf ("  page in :%9lu", bDIFF (pgin));
    if (new.disk_r[0])
	printf ("  disk 1: %8lur%8luw\n", bDIFF (disk_r[0]),
		bDIFF (disk_w[0]));
    else if (new.disk[0])
	printf ("  disk 1: %8lu\n", bDIFF (disk[0]));
    else
	putchar ('\n');

    printf ("nice  : %s %s",
	    hms (bDIFF (cpu_nice)), perc (bDIFF (cpu_nice), elapsed, nr_cpus));
    printf ("  page out:%9lu", bDIFF (pgout));
    if (new.disk_r[1])
	printf ("  disk 2: %8lur%8luw\n", bDIFF (disk_r[1]),
		bDIFF (disk_w[1]));
    else if (new.disk[1])
	printf ("  disk 2: %8lu\n", bDIFF (disk[1]));
    else
	putchar ('\n');

    printf ("system: %s %s",
	    hms (bDIFF (cpu_sys)), perc (bDIFF (cpu_sys), elapsed, nr_cpus));
    printf ("  swap in :%9lu", bDIFF (swin));
    if (new.disk_r[2])
	printf ("  disk 3: %8lur%8luw\n", bDIFF (disk_r[2]),
		bDIFF (disk_w[2]));
    else if (new.disk[2])
	printf ("  disk 3: %8lu\n", bDIFF (disk[2]));
    else
	putchar ('\n');

    printf ("idle  : %s %s",
	    hms (bDIFF (cpu_idle)), perc (bDIFF (cpu_idle), elapsed, nr_cpus));
    printf ("  swap out:%9lu", bDIFF (swout));
    if (new.disk_r[3])
	printf ("  disk 4: %8lur%8luw\n", bDIFF (disk_r[3]),
		bDIFF (disk_w[3]));
    else if (new.disk[3])
	printf ("  disk 4: %8lu\n", bDIFF (disk[3]));
    else
	putchar ('\n');

    printf ("uptime: %s         context :%9lu", hms (new.uptime),
	    bDIFF (ctxt));
    if (new.syscalls)	/* If we have this, we can use the old interrupts spot. */
	printf ("  syscalls: %16lu", bDIFF (syscalls));

/****= /proc/interrupts =****/

    if (nr_irqs) {
	char irq_label[nr_irqs][22];

	memset (irq_label, 0, nr_irqs * 22);

	if (interruptsfp) {
	    int i;

	    fseek (interruptsfp, 0L, SEEK_SET);
	    while (fgets (line, sizeof (line), interruptsfp)) {
		char *p;

		if (!strchr(line, ':'))		/* skip "           CPU0" */
		    continue;

		i = atol (strtok (line, ":")); /* Get the IRQ no. */

		p = strtok (NULL, "\n");

		/*
		  Left: p = "      31273 + serial"
		  or    p = "         1   XT PIC   serial"
		  or    p = "         1  IO-APIC   serial"
		  or whatever.
		  Anyway, let's first gobble up...
		*/

		while (*p == ' ')		/* ...spaces... */
		    p++;
		while (*p >= '0' && *p <= '9')	/* ...digits... */
		    p++;
		while (*p == ' ' || *p == '+')	/* ...and the rest. */
		    p++;

		/* Left: "serial" or "XT PIC   serial" */

		if (linux_version_code >= 0x20150) {
		    /*
		      I don't really like hardcoding version numbers, but
		      since the label itself may contain spaces, I can't
		      think of a fool-proof algorithm to detect a "XT PIC"
		      style /proc/interrupts.
		    */
		    char *q;

		    if ((q = strstr (p, "PIC"))) {
			while (*q != ' ')	/* eat up "PIC" or "PIC-foo" */
			    q++;
			while (*q == ' ')	/* eat up spaces */
			    q++;
			p = q;
		    }
		}

		/* XXX Don't let NMI:, IPI: overwrite real values */
		if (irq_label[i][0] == 0)
		    strncpy (irq_label[i], p, 20);
	    }
	}


/**** /proc/dma ****/

	if (dmafp) {
	    int i;
	    size_t tmplen;
	    char tmp[22];

	    fseek (dmafp, 0L, SEEK_SET);
	    while (fgets (line, sizeof (line), dmafp)) {
		int foo = strcspn (&line[4], " \n");

		for (i = 0; i < nr_irqs; i++) {
		    if (strncmp (&line[4], irq_label[i], foo) == 0) {
			tmplen = sprintf (tmp, " [%ld]",
					  atol (strtok (line, ":")));

			if (strlen (irq_label[i]) > (21 - tmplen)) {
			    irq_label[i][21 - tmplen] = 0;

			}
			strcat (irq_label[i], tmp);
		    }
		}
	    }
	}

	fputs ("\n\n", stdout);

	if (squeeze_irqs) {
	    int howmany = 0, rows;
	    struct {
		int nr;
		unsigned long count;
		char *label;
	    } squirqs[nr_irqs];

	    for (i = 0; i < nr_irqs; i++) {
		if (new.intr[i] || irq_label[i][0]) {
		    squirqs[howmany].nr = i;
		    squirqs[howmany].count = bDIFF(intr[i]);
		    squirqs[howmany].label = irq_label[i];
		    howmany++;
		}
	    }

	    rows = (howmany + 1) / 2;

	    for (i = 0; i < rows; i++) {
		/* The last row may be incomplete if howmany is odd, hence: */
		if (i == rows - 1 && howmany & 1) {
		    printf ("irq%3d:%10lu %-21s\n",
			    squirqs[i].nr, squirqs[i].count, squirqs[i].label);
		} else {
		    printf ("irq%3d:%10lu %-21s "
			    "irq%3d:%10lu %-21s\n",
			    squirqs[i].nr, squirqs[i].count, squirqs[i].label,
			    squirqs[i+rows].nr,
			    squirqs[i+rows].count,
			    squirqs[i+rows].label);
		}
	    }
	} else {
	    for (i = 0; i < nr_irqs / 2; i++)
		printf ("irq%3d:%10lu %-21s "
			"irq%3d:%10lu %-21s\n",
			i, bDIFF (intr[i]), irq_label[i],
			i + (nr_irqs / 2),
			bDIFF (intr[i + (nr_irqs / 2)]),
			irq_label[i + (nr_irqs / 2)]);
	}

    } else
	printf ("\tinterrupts: %8lu\n", bDIFF (old_intr));

}

static void
second_page (void)
{
    if (show_all)
	putchar ('\n');


/**** /proc/cmdline ****/

    /*
      We're not fclosing cmdlinefp because we're using it later as a
      check.
    */

    fgets (cmdline, 1024, cmdlinefp);

    if (cmdlinefp)
	printf ("Kernel Command Line:\n  %s\n", cmdline);


/**** /proc/modules ****/

    if (modulesfp) {
	int barf = 0;

	fseek (modulesfp, 0L, SEEK_SET);
	printf ("Modules:\n");
	while (fgets (line, sizeof (line), modulesfp)) {
	    char *mod;
	    long pg;
	    int used = 0;

	    if (co - barf < 20 ) {
		barf = 0;
		putchar('\n');
	    }
	    barf += 20;

	    mod = strtok (line, " ");
	    if (strlen (mod) > 14)
		mod[14] = '\0';
	    if (linux_version_code < 0x20100)
		pg = VAL * 4;			/* size is in 4kb pages */
	    else
		pg = (VAL + 512) / 1024;	/* size is in bytes */
	    used = VAL;
	    printf ("%3ld %c%-14s ",
		    pg, used ? '*' : ' ', mod);
	}

	/*if (co && (barf % co))*/
	    printf ("%s\n", fs ? ce : "");
    }


/**** /proc/devices ****/

    if (devicesfp) {
	int maj[2][MAX_DEV];
	char *n[2][MAX_DEV];
	int count[2] =
	{0, 0};
	int which = 0, i, j;

	memset (n, 0, sizeof (n));
	fseek (devicesfp, 0L, SEEK_SET);
	printf ("%s\nCharacter Devices:                      "
		"Block Devices:\n",
		fs ? ce : "");
	while (fgets (line, sizeof (line), devicesfp)) {
	    switch (line[0]) {
	    case 'C':
		which = CDRV;
		break;
	    case 'B':
		which = BDRV;
		break;
	    case '\n':
		break;
	    default:
		maj[which][count[which]] =
		    atoi (strtok (line, " "));
		n[which][count[which]] =
		    strdup (strtok (NULL, "\n"));
		count[which]++;
		break;
	    }
	}

	j = (1 + MAX (count[0], count[1])) / 2;
	for (i = 0; i < j; i++) {
	    if (n[CDRV][i]) {
		printf ("%3d %-15s ", maj[CDRV][i], n[CDRV][i]);
		free (n[CDRV][i]);
	    } else
		fputs ("                    ", stdout);
	    if (n[CDRV][i + j]) {
		printf ("%3d %-15s ",
			maj[CDRV][i + j], n[CDRV][i + j]);
		free (n[CDRV][i + j]);
	    } else
		fputs ("                    ", stdout);
	    if (n[BDRV][i]) {
		printf ("%3d %-15s ", maj[BDRV][i], n[BDRV][i]);
		free (n[BDRV][i]);
	    } else
		fputs ("                     ", stdout);
	    if (n[BDRV][i + j]) {
		printf ("%3d %-15s ",
			maj[BDRV][i + j], n[BDRV][i + j]);
		free (n[BDRV][i + j]);
	    }
	    printf ("%s\n", fs ? ce : "");
	    if (i >= count[CDRV] && i >= count[BDRV])
		break;
	}
    }


/**** /proc/filesystems ****/

    if (filesystemsfp) {
	int barf = 0;

	fseek (filesystemsfp, 0L, SEEK_SET);
	printf ("%s\n", fs ? ce : "");
	printf ("File Systems:%s\n", fs ? ce : "");
	while (fgets (line, sizeof (line), filesystemsfp)) {
	    char *fs;
	    char tmp[21];

	    if (co - barf < 20 ) {
		barf = 0;
		putchar('\n');
	    }
	    barf += 20;

	    fs = strchr (line, '\t');
	    fs = strtok (fs + 1, "\n");
	    if (line[0] == 'n') {
		sprintf (tmp, "[%s]", fs);
		printf ("%-20s", tmp);
	    } else
		printf ("%-20s", fs);
	}
	/* if (co && (barf % co)) */
	    printf ("%s\n", fs ? ce : "");
    }
}


int
main (int ac, char **av)
{
    long sl = 5000000L;
    int getoptopt;
    struct timeval then, now;
#ifndef DEBUG
    char outbuf[4096];
#endif

    uname(&ut);
    linux_version_code = (atol(strtok(ut.release, ".")) * 0x10000) +
	(atol(strtok(NULL, ".")) * 0x100) +
	atol(strtok(NULL, ""));

    while ((getoptopt = getopt (ac, av, "sfn:madirDSF:bChv")) != EOF) {
	switch (getoptopt) {
	case 'n':
	    sl = (long) (atof (optarg) * 1000000.0);
	   /* PLUMMET */
	case 'f':
	    fs = 1;
	    break;
	case 's':
	    show_stat = 1;
	    show_moddev = show_all = 0;
	    break;
	case 'm':
	    show_moddev = 1;
	    break;
	case 'a':
	    show_all = 1;
	    break;
	case 'S':
	    per_sec = 1;
	    break;
	case 'F':
	    if ((freopen (optarg, "r", stdin)) == NULL ||
	        (freopen (optarg, "w", stdout)) == NULL ||
	        (freopen (optarg, "w", stderr)) == NULL) {
		fprintf (stderr, "%s: ", av[0]); /* What happens if the third one failed, anyway? */
		perror ("unable to open new output file");
		exit (errno);
	    }
	    break;
	case 'D':
	    show_new_mem = 1;
	    /* PLUMMET */
	case 'd':
	    show_diff = fs = 1;
	    break;
	case 'i':
	    squeeze_irqs = 0;
	    break;
	case 'r':
	    show_real_mem = 1;
	    break;
	case 'b':
	    io_or_blk = 1;
	    break;
	case 'v':
	    printf ("This is procinfo version " VERSION "\n");
	    exit (0);
	case 'h':
	default:
	    printf ("procinfo version %s\n"
		    "usage: %s [-fsmadiDSbhv] [-nN] [-Ffile]\n"
		    "\n"
		    "\t-s\tdisplay memory, disk, IRQ & DMA info (default)\n"
		    "\t-m\tdisplay module and device info\n"
		    "\t-a\tdisplay all info\n"
		    "\t-f\trun full screen\n"
		    "\n"
		    "\t-i\tshow all IRQ channels, not just those used\n"
		    "\t-nN\tpause N second between updates (implies -f)\n"
		    "\t-d\tshow differences rather than totals (implies -f)\n"
		    "\t-D\tshow current memory/swap usage, differences on rest\n"
		    "\t-S\twith -nN and -d/-D, always show values per second\n"
		    "\t-r\tshow memory usage -/+ buffers/cache\n"
		    "\t-Ffile\tprint output to file -- normally a tty\n"
		    "\t-v\tprint version info\n"
		    "\t-h\tprint this help\n",
		    VERSION, av[0]);
	    exit (getoptopt == 'h' ? 0 : 1);
	}
    }

    if (sl == 0)
	nice (-20);


    versionfp = myfopen (PROC_DIR "version");
    uptimefp = myfopen (PROC_DIR "uptime");
    loadavgfp = myfopen (PROC_DIR "loadavg");
    meminfofp = myfopen (PROC_DIR "meminfo");
    statfp = myfopen (PROC_DIR "stat");
    /* These may be missing, so check for NULL later. */
    modulesfp = fopen (PROC_DIR "modules", "r");
    devicesfp = fopen (PROC_DIR "devices", "r");
    filesystemsfp = fopen (PROC_DIR "filesystems", "r");
    interruptsfp = fopen (PROC_DIR "interrupts", "r");
    dmafp = fopen (PROC_DIR "dma", "r");
    cmdlinefp = fopen (PROC_DIR "cmdline", "r");

#ifndef DEBUG
    setvbuf (stdout, outbuf, _IOFBF, sizeof (outbuf));
#endif

    init_terminal_data ();
    if (fs)
	window_init ();
    winsz (0);

    if (fs) {
	cd = my_tgets ("cd");
	ce = my_tgets ("ce");
	cl = my_tgets ("cl");
	cm = my_tgets ("cm");
	ho = my_tgets ("ho");
	se = my_tgets ("se");
	so = my_tgets ("so");
	ve = my_tgets ("ve");
	vi = my_tgets ("vi");
	sg = tgetnum ("sg");	/* Oh god. */

/*
   If you're suffering from terminals with magic cookies, 1) you have
   my sympathy, and 2) you might wish just to forget about standout
   altogether. In that case, define COOKIE_NOSOSE.
 */

#ifdef COOKIE_NOSOSE
	if (sg > 0) {
	    sg = 0;			/* Just forget about se/so */
	    se = so = "";
	}
#endif
	if (sg < 0)
	    sg = 0;
    }

    /* Count number of CPUs */
    cpuinfofp = myfopen (PROC_DIR "cpuinfo");
    if (cpuinfofp) {
	while (fgets (line, sizeof (line), cpuinfofp))
	    if (!strncmp ("processor", line, 9))          /* intel */
		nr_cpus++;
	    else if (!strncmp ("ncpus ", line, 6))  /* sparc */
		nr_cpus = atoi(line+19);
	    else if (!strncmp ("cpus detected", line, 13)) /* alpha */
		nr_cpus = atoi(line+27);
	fclose (cpuinfofp);
    }
    if (nr_cpus == 0)
	nr_cpus = 1;

    /* Gets called from winsz(), but in case stdout is redirected: */
    version = make_version (versionfp);

    /* See what the intr line in /proc/stat says. */
    while (fgets (line, sizeof (line), statfp)) {
	if (!strncmp ("intr", line, 4)) {
	    /* Counting number of spaces after line[5] gives us */
	    /* number of interrupt entries in new-style intr. */
	    /* If zero: old style intr. */
	    int i, len = strlen(line);
	    for(i = 5; i < len; i++)
		if(line[i] == ' ')
		    nr_irqs++;
	    new.intr = my_xcalloc (nr_irqs, sizeof (unsigned int));
	    old.intr = my_xcalloc (nr_irqs, sizeof (unsigned int));
	    memset (&base, 0, sizeof (struct info));
	    base.intr = my_xcalloc (nr_irqs, sizeof (unsigned int));
	    continue;
	}
	/* While we're at it, fill in booted. */
	else if (!strncmp ("btime", line, 5)) {
	    time_t btime;

	    btime = (time_t) atol (&line[6]);
	    strftime (booted, sizeof (booted), "%c", localtime (&btime));
	    continue;
	}
    }

    if (fs)
	printf ("%s%s", cl, vi);

    gettimeofday (&now, 0);
    while (42) {

	if (fs && redrawn) {
	    redrawn = 0;
	    fputs (cl, stdout);
	}

	if (fs) {
	    printf ("%s%s%*s%s\n", ho, so, -(co - 2 * sg), version, se);
	    printf ("%s", ce);
	} else
	    printf ("%s\n", version);

	if (message) {
	    printf ("[%s]", message);
	    message = NULL;
	}
	putchar ('\n');

	if (show_moddev + show_sys == 0)
	    show_stat = 1;
	if (show_all)
	    show_stat = show_moddev = show_sys = 1;

	if (show_stat)
	    first_page (sl);

	if (show_moddev)
	    second_page ();

	fflush (stdout);

	if (fs) {
	    fd_set readfd;
	    struct timeval tv;
	    int ret;

	    tv.tv_sec = sl / 1000000;
	    tv.tv_usec = sl - (tv.tv_sec * 1000000);

	    FD_ZERO(&readfd);
	    FD_SET(0, &readfd);

	    ret = select (1, &readfd, NULL, NULL, &tv);

	    /* Just ignore any errors. */
	    if (ret > 0) {
		char key;

		ret = read (0, &key, 1);
		switch (key) {

		case 's':
		    show_stat = 1;
		    show_moddev = show_all = 0;
		    redrawn = 1;
		    message = "showing system status";
		    break;
		case 'm':
		    show_moddev = 1;
		    show_stat = show_all = 0;
		    redrawn = 1;
		    message = "showing modules & devices";
		    break;
		case 'a':
		    show_all = show_stat = show_moddev = 1;
		    redrawn = 1;
		    message = "showing all info";
		    break;

		case 't':
		    show_diff = show_new_mem = 0;
		    redrawn = 1;
		    message = "showing totals";
		    break;
		case 'd':
		    show_diff = 1;
		    show_new_mem = 0;
		    redrawn = 1;
		    message = "showing diffs per second/interval";
		    break;
		case 'D':
		    show_diff = show_new_mem = 1;
		    redrawn = 1;
		    message = "showing diffs per second/interval, totals for memory";
		    break;
		case 'S':
		    per_sec = per_sec ? 0 : 1;
		    redrawn = 1;
		    message = per_sec? "showing diffs per second" :
			"showing diffs per interval";
		    break;
		case 'r':
		    show_real_mem = show_real_mem ? 0 : 1;
		    redrawn = 1;
		    message = show_real_mem ?
			"showing memory usage -/+ buffers/cache" :
			    "showing memory usage";
		    break;
		case 'i':
		    squeeze_irqs = squeeze_irqs ? 0 : 1;
		    redrawn = 1;
		    message = squeeze_irqs ? "squeezed IRQ display"
			: "full IRQ display";
		    break;
		case 'b':
		    io_or_blk = io_or_blk ? 0 : 1;
		    /* Fudge things along a little... */
		    new.disk_r[0] = new.disk_w[0] =
			new.disk_r[1] = new.disk_w[1] =
			new.disk_r[2] = new.disk_w[2] =
			new.disk_r[3] = new.disk_w[3] = 0;
		    redrawn = 1;
		    message = io_or_blk ? "showing I/O in blocks" :
			"showing I/O per requests";
		    break;

		case 'n':
		{
		    char line[41];
		    double tmpsl;

		    printf ("delay: ");
		    set_echo (1);
		    fgets (line, 40, stdin);
		    set_echo (0);
		    if (strlen (line) > 1) {
			tmpsl = atof (line);
			sl = (long) (tmpsl * 1000000.0);
			sprintf (line, "delay set to %f", tmpsl);
			message = line;
		    }
		    redrawn = 1;
		    break;
		}

		case 'C':
		{
		    show_from_baseline = 1;
		    if (nr_irqs)
			COPYINFO(base, new)
			else
			    memcpy (&base, &new, sizeof (struct info));
		    message = "checkpoint set";
		    break;
		}
		case 'R':
		    show_from_baseline = 0;
		    message = "checkpoint released";
		    break;

		case ' ':
		    printf ("[hold]"); fflush(stdout);
		    read (0, &key, 1);
		    printf ("\r      "); fflush(stdout);
		    break;

		case 12:	/* ^L */
		    redrawn = 1;
		    break;

		case 'h':
		case '?':
		    message = "s/m/a   t/d/D   S i b n  <space> ^L ? q";
		    break;
		case 'q':
		    quit (0);
		    break;

		case 0:		/* happens if stdin = /dev/null */
		    break;

		default:
		    putchar (7);
		    message = "unknown command";
		    break;
		}
	    }

	    then = now;
	    gettimeofday (&now, 0);
	    if (per_sec)
		rate = (float) now.tv_sec + (float) now.tv_usec / 1.0e6 -
		    (float) then.tv_sec - (float) then.tv_usec / 1.0e6;

	} else {
	    putchar ('\n');
	    exit (0);
	}
	if (nr_irqs) {
	    if (show_from_baseline == 0)
		COPYINFO (old, new)
	}
	else {

	    if (show_from_baseline == 0)
		memcpy (&old, &new, sizeof (struct info));
	}
    }				/* 42 */
}

/*
   Local variables:
   rm-trailing-spaces: t
   End:
*/
