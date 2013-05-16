/*

  procinfo.h

  Date:        1995-04-15 23:54:06
  Last Change: 2001-02-24 23:43:08

  Copyright (C) 1995-2001 Sander van Malssen <svm@kozmix.cistron.nl>

  This software is released under the GNU Public Licence. See the file
  `COPYING' for details. Since you're probably running Linux I'm sure
  your hard disk is already infested with copies of this file, but if
  not, mail me and I'll send you one.

  $Id: procinfo.h,v 1.25 2001/02/24 23:30:35 svm Exp svm $

*/

#ifndef _PROCINFO_H
#define _PROCINFO_H

#define VERSION		"18 (2001-03-02)"

#ifndef PROC_DIR
#define PROC_DIR	"/proc/"	/* Needs the trailing slash. */
#endif

#define ISSTR(s)	(!strcmp(s, type))

#if 0
#define VAL		(strtoul(strtok(NULL, "\t "), NULL, 10))
#else
static inline unsigned long find_val(void)
{
    char *cp;

    cp = strtok(NULL, "\t ");
    if (!cp)
	return 0L;
    return (strtoul (cp, NULL, 10));
}
#define VAL	find_val()
#endif

#define DIFF(x)		(show_diff ? \
			 (unsigned long) (((new.x)-(old.x))/rate) : \
			  new.x)

#define bDIFF(x)	(show_diff ? \
			 (unsigned long) (((new.x)-(old.x))/rate) : \
		 	 (show_from_baseline ? \
			  new.x - base.x : new.x))

#define COPYINFO(to,from)	{ \
    unsigned long *foo; \
    foo = to.intr; \
    memcpy (&to, &from, sizeof (struct info)); \
    to.intr = foo; \
    memcpy (to.intr, from.intr, nr_irqs * sizeof (unsigned long)); \
}



#define IS_ODD(x)	(x & 1)

#undef  MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))

#define CDRV		0
#define BDRV		1
#ifndef MAX_CHRDEV
#define MAX_CHRDEV	32
#endif
#ifndef MAX_BLKDEV
#define MAX_BLKDEV	32
#endif
#define MAX_DEV		MAX(MAX_CHRDEV, MAX_BLKDEV)

struct info
{
    unsigned long uptime;
    long m_to, m_us, m_fr, m_sh, m_bu, m_ca;
    long s_to, s_us, s_fr;
    unsigned long cpu_user, cpu_nice, cpu_sys, cpu_idle;
    unsigned long disk[5];
    unsigned long disk_r[5];
    unsigned long disk_w[5];
    unsigned long pgin, pgout, swin, swout;
    unsigned long *intr;	/* Phew. That's better. */
    unsigned long old_intr;
    unsigned long ctxt;
    unsigned long syscalls;
};

/* Prototypes. */

void window_init (void);
void tstp (int i);
void cont (int i);
void winsz (int i);
void quit (int i);
void set_echo (int i);

void fatal (const char *s,...);
void *my_xcalloc (size_t n, size_t s);
void init_terminal_data (void);
char *my_tgets (char *te);

char *make_version (FILE *verionfp);
FILE *myfopen (char *name);
char *hms (unsigned long t);
char *perc (unsigned long i, unsigned long t, int cpus);

#endif /* _PROCINFO_H */

/*
   Local variables:
   rm-trailing-spaces: t
   End:
 */
