/* Sniffer Benchmark
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	sniffbench.c
** Date:	Fri Oct 14 23:51:22 EDT 2011
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2011/10/15 05:21:44 $
**   $RCSfile: sniffbench.c,v $
**   $Revision: 1.1 $
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

const char * documentation [] = {
"sniffbenchmark t [ -doc | -i imin imax |",
"                   -g | -G file |",
"                   -h hinterval | -H interval file ]",
"               [ duration ]",
"",
"The only command currently implemented is `t'.  This",
"interupts at somewhat random intervals and measures",
"the time between when the interrupt was scheduled",
"when the interrupt actually occurs.  Short times",
"mean a responsive CPU, long times mean an unrespon-",
"sive CPU.  -g graphs each measurement on an output",
"line with one column per 100 ms, the first column",
"being the range [0,100)ms.  -G does the same but"
  " writes",
"the graph to a file.  -h writes a histogram to the",
"standard output every interval seconds.  -H does the",
"same but writes to a file, and overwrites the pre-",
"vious contents of the file each time.  The histogram",
"is just a sequence of integers giving the count of",
"measurements in the range [0,100)ms, [100,200)ms,"
   " etc.",
"The default output is -h 10.",
"",
"The interval between interrupts is uniformly"
    " distributed",
"between imin and imax seconds (defaults 0.5 and 1.5).",
"The program runs for duration seconds, or"
 " indefinitely",
"if the duration is not given.",
NULL
};

#define MAX_HIST 51
long histogram[MAX_HIST];

FILE ** gfile;  /* May be stdout or not. */
FILE ** hfile;  /* May be stdout or not. */

static void print_histogram ( void )
{
    int endi = MAX_HIST;
    int i;
    while ( endi > 0 && histogram[endi - 1] == 0 )
        -- endi;
    for ( i = 0; i < endi; ++ i )
        fprintf ( * hfile, i == 0 ? "%ld" : " %ld",
	          histogram[i] );
    fprintf ( * hfile, "\n" );
    fflush ( * hfile );
}

static void print_graph ( int i )
{
    fprintf ( * gfile, "%*sX\n", i, "" );
    fflush ( * gfile );
}

int main ( int argc, char ** argv )
{

    if ( argc < 2
         ||
	 strncmp ( argv[1], "-doc", 4 ) == 0 )
    {
	const char ** p = documentation;
	while ( * p ) printf ( "%s\n", * p ++ );
	exit (1);
    }

    exit ( 0 );
}
