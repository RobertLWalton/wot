/* Helper program for the conf program, written in C
 * to achieve acceptable speed.
 *
 * File:	conf_helper.c
 * Author:	Bob Walton (walton@deas.harvard.edu)
 * Date:	Sun Mar 22 13:05:26 EDT 2009
 *
 * The authors have placed this program in the public
 * domain; they make no warranty and accept no liability
 * for this program.
 *
 * RCS Info (may not be true date or author):
 *
 *   $Author: walton $
 *   $Date: 2009/03/22 17:16:27 $
 *   $RCSfile: conf_helper.c,v $
 *   $Revision: 1.5 $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* tmpfile is declared in stdio so we need to
 * rename it. */
#define tmpfile TMPFILE

/* conf_helper TYPE COMMAND FILE \
 *                  SOURCEFILE TARGETFILE TMPFILE \
 *		    VERBOSE HOST
 *
 * TYPE		passwd or shadow
 * COMMAND	put or get
 * FILE		name of file relative to directories
 *		for error messages
 * SOURCEFILE	source file name
 * TARGETFILE	target file name
 * TMPFILE	temporary file name provided by `conf'
 * VERBOSE:	0 or 1
 * HOST		HOST variable value from `conf' program
 *
/* This program accepts the COMMANDs `put' and `get' and
 * copies the SOURCEFILE or TARGERFILE, repectively, to
 * to the TMFILE, making appropriate field substitu-
 * tions, according to the COMMAND and TYPE.
 *
 * See documentation of the `conf' program and the code
 * of the passwd_conf and shadow_conf functions in that
 * program.
 */

/* Global parameters shared among subroutines. */

/* Program arguments */
static const char * TYPE;
static const char * command;
static const char * file;
static const char * source;
static const char * target;
static const char * tmpfile;
static const char * HOST;
static int verbose;
    
#   define vprintf if ( verbose ) printf

/* True if "put" command and false if "get". */
static int isput;

/* Derived parameters:
 *
 *   src	Source of copy. == source for put,
 *				== target for get.
 *   des	Ultimate destination of copy.
 *				== target for put,
 *			        == source for get.
 *   srcf	FILE * values for src, des, tmpfile
 *   desf
 *   tmpf
 */
static const char * src;
static const char * des;
static FILE * desf;
static FILE * srcf;
static FILE * tmpf;

/* Buffer to hold input lines */
static char buffer [10002];

/* Functions to handle different TYPES */
static void passwd_and_shadow ( void );

/* Function to open one of the files and handle
 * error message on failure.  Arguments as per
 * fopen, with only "r" and "w" modes allowed.
 */
static FILE * openf
	( const char * fname, const char * mode )
{
    FILE * result = fopen ( fname, mode );
    if ( result == NULL )
    {
        printf ( "ERROR: could not open %s for %s\n",
		 fname, ( mode[0] == 'w' ?
		 	  "writing" : "reading" ) );
	exit ( 2 );
    }
    return result;
}

/* Function to read a line into `buffer'.  Returns
 * NULL on EOF and removes the line feed like `gets'.
 * Handles line too long errors.  fname is file name
 * for error message.
 */
static char * readf
	( FILE * f, const char * fname )
{
    char * result =
        fgets ( buffer, sizeof ( buffer ), f );
    int length;
    if ( result == NULL ) return result;
    buffer[sizeof(buffer)-1] = 0;
    length = strlen ( buffer );
    if ( length > sizeof ( buffer ) - 2 )
    {
        printf ( "ERROR: line too long in %s\n",
		 fname );
	exit ( 2 );
    }
    assert ( length > 0 );
    buffer[length-1] = 0;
    return buffer;
}

/* Main program.  Sets up common data and calls function
 * to process TYPE.
 */
int main ( int argc, char ** argv )
{
    /* Note that more arguments may be added later, and
     * this program should still work if there are
     * excess arguments.
     */
    if ( argc < 9 )
    {
    	printf ( "conf_helper"
	         " TYPE COMMAND FILE"
		 " SOURCEFILE TARGETFILE TMPFILE"
	         " VERBOSE HOST\n" );
	exit ( 2 );
    }

    TYPE    = argv[1];
    command = argv[2];
    file    = argv[3];
    source  = argv[4];
    target  = argv[5];
    tmpfile = argv[6];
    verbose = ( argv[7][0] == '1' );
    HOST    = argv[8];

    /* Figure out which is the source file (src) for the
     * copy and which is the ultimate destination file
     * (des) that the tmpfile is to replace.
     */
    if ( strcmp ( command, "put" ) == 0 )
        isput = 1;
    else if ( strcmp ( command, "get" ) == 0 )
        isput = 0;
    else
    {
        printf ( "ERROR: unrecognized COMMAND: %s\n",
	         command );
	exit ( 2 );
    }

    if ( isput )
    {
        src = source;
	des = target;
    }
    else
    {	src = target;
    	des = source;
    }

    /* Dispatch on TYPE.
     */
    if ( strcmp ( TYPE, "passwd" ) == 0 )
        passwd_and_shadow();
    else if ( strcmp ( TYPE, "shadow" ) == 0 )
        passwd_and_shadow();
    else
    {
        printf ( "ERROR: unrecognized TYPE: %s\n",
	         TYPE );
	exit ( 2 );
    }
    return 0;
}

/* Data and functions for passwd and shadow. */

/* Field pointer.
 */
struct fpointer_struct
{
    char * cp;		/* Points at field. */
    int present;	/* True iff field present */
    			/* (i.e., preceeded by ':' ); */
};
typedef struct fpointer_struct fpointer;

/* Initialize a field pointer to a buffer pointer.
 */
void fpinit ( fpointer * fpp, char * bufferp )
{
    fpp->cp = bufferp;
    fpp->present = ( * bufferp != 0 );
}
#define INIT(fp,bufferp) fpinit ( &(fp), bufferp )

/* Skip a field.  Set the char after the field to NUL.
 */
static void fpskip ( fpointer * fpp )
{
    char * cp = fpp->cp;
    if ( ! fpp->present ) return;
    while ( * cp && * cp != ':' ) ++ cp;
    fpp->present = ( * cp == ':' );
    if ( fpp->present ) * cp ++ = 0;
    fpp->cp = cp;
}
#define SKIP(fp) fpskip(&(fp))

/* Ditto but print the field to tmpf preceded by a ':',
 * if the field is present.
 */
static void fpprint ( fpointer * fpp )
{
    char * cp = fpp->cp;
    if ( ! fpp->present ) return;
    fpskip ( fpp );
    fprintf ( tmpf, ":%s", cp );
}
#define PRINT(fp) fpprint(&(fp))

/* Print the rest of the fields and an end of line.
 */
static void fpprintrest ( fpointer * fpp )
{
    while ( fpp->present ) PRINT ( * fpp );
    fprintf ( tmpf, "\n" );
}
#define PRINTREST(fp) fpprintrest ( &(fp) )


/* List of file lines that contain information to be
 * inserted into the file being copied, indexed by
 * account name.
 */
struct line_struct
{
    /* Line is malloced.  Account points at beginning
     * and is NUL terminated, the : having been
     * replaced in the line by NUL.  rest is a field
     * pointer pointing at the first field after
     * the account.
     */

    char * account;
    fpointer rest;		
    struct line_struct * previous;
};
typedef struct line_struct line;

/* Lines are chained together from lastline via previous
 * member.
 */
static line * lastline = NULL;

/* Return line with given account or NUL if none.
 */
static line * find ( const char * account )
{
    line * result = lastline;
    while ( result )
    {
        if ( strcmp ( result->account, account ) == 0 )
	    return result;
	result = result->previous;
    }
    return NULL;
}

/* Insert line.  Filename des is for error message.
 */
static void insert
	( const char * linep, const char * des )
{
    line * newline =
        (line *) malloc ( sizeof ( line ) );
    char * p = (char *) malloc ( strlen ( linep ) + 1 );
    strcpy ( p, linep );
    INIT ( newline->rest, p );
    newline->account = p;
    SKIP ( newline->rest );
    if ( find ( newline->account ) )
    {
        printf ( "ERROR: lines with duplicate account"
	         " named %s in %s\n",
		 newline->account, des );
	exit ( 2 );
    }
    newline->previous = lastline;
    lastline = newline;
}

static void passwd_and_shadow ( void )
{
    int ispasswd;
    int hostlength;

    /* Read the des file and insert its lines into the
     * line list.
     */ 
    lastline = NULL;
    desf = fopen ( des, "r" ); /* File may be missing. */
    if ( desf != NULL )
    {
	while ( readf ( desf, des ) )
	    insert ( buffer, des );
	fclose ( desf );
    }

    /* Copy src to tmpfile inserting fields as necessary
     * from des.
     */
    srcf = openf ( src, "r" );
    tmpf = openf ( tmpfile, "w" );
    ispasswd = ( strcmp ( TYPE, "passwd" ) == 0 );
    hostlength = strlen ( HOST );
    while ( readf ( srcf, src ) )
    {
	line * desline;
        fpointer p;
	INIT ( p, buffer );
	SKIP ( p );
	desline = find ( buffer );
#	define q (desline->rest)

	/* p points at src field, q at des field */

	if ( ispasswd )
	{
	    if ( desline == NULL )
	    {
		fprintf ( tmpf, "%s", buffer );
		PRINTREST ( p );
		continue;
	    }

	    if ( isput )
	    {
		/* account */
		fprintf ( tmpf, "%s", buffer );

		/* 5 fields from src (source) */
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );

		/* shell from des (target) */
		SKIP  ( p );
		PRINT ( q );

		/* rest of field from src (source) */
		PRINTREST ( p );
	    }
	    else
	    {
		/* account */
		fprintf ( tmpf, "%s", buffer );

		/* 5 fields from des (source) */
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );

		/* shell from src (target ) */
		SKIP  ( q );
		PRINT ( p );

		/* rest of field from des (source) */
		PRINTREST ( q );
	    }
	}
	else /* is shadow */
	{
	    /* compute whether !host!...! host list
	     * exists, whether HOST is in it, and
	     * whether HOST is first in it.  If it
	     * exists, set h to first character
	     * after the host list.
	     */
	    int hostlistexists = 0;
	    int inhostlist = 0;
	    int firsthost = 0;
	    char * h;
	    int first = 1;
	    if ( isput ) h = p.cp;
	    else if ( desline == NULL ) h = "";
	    else h = q.cp;

	    if ( * h == '!' ) while ( 1 )
	    {
	        char * k = ++ h;
		while ( * k && * k != '!' ) ++ k;
		if ( * k != '!' ) break;
		if ( k == h ) break;
		if ( k - h == hostlength
		     &&
		     strncmp ( h, HOST, hostlength )
		     == 0 )
		{
		    if ( first ) firsthost = 1;
		    inhostlist = 1;
		}
		hostlistexists = 1;
		first = 0;
		h = k;
	    }

	    if ( desline == NULL )
	    {
		/* account */
		fprintf ( tmpf, "%s", buffer );

	        if ( isput )
		{
		    /* password */
		    if ( ! hostlistexists )
			PRINT ( p );
		    else if ( inhostlist )
		    {
			SKIP ( p );
			fprintf ( tmpf, ":%s", h );
		    }
		    else /* not in existing host list */
		    {
		    	SKIP ( p );
			fprintf ( tmpf, ":!!%s", h );
		    }
		    PRINTREST ( p );
		}
		else /* is get */
		{
		    if ( hostlistexists )
		    {
		        printf ( "ERROR: account %s"
			         " in target file %s"
				 " has a host list\n",
				 buffer, src );
			exit ( 2 );
		    }

		    h = p.cp;
		    SKIP ( p );
		    fprintf
		        ( tmpf, ":!%s!%s", HOST, h );
		    PRINTREST ( p );
		}
		continue;
	    }

	    /* account */
	    fprintf ( tmpf, "%s", buffer );

	    if ( isput )
	    {
		/* password */
		if ( firsthost )
		{
		    /* password from des (target) */
		    PRINT ( q );
		    SKIP ( p );
		}
		else if ( inhostlist )
		{
		    /* password minus hostlist
		     * from src (source) */
		    p.cp = h;
		    SKIP ( q );
		    PRINT ( p );
		}
		else if ( hostlistexists )
		{
		    /* password minus hostlist
		     * with !! prefix
		     * from src (source) */
		    SKIP ( q );
		    SKIP ( p );
		    fprintf ( tmpf, ":!!%s", h );
		}
		else
		{
		    /* password
		     * from src (source) */
		    SKIP ( q );
		    PRINT ( p );
		}

		/* password change date */
		if ( firsthost )
		{
		    /* from des (target) */
		    PRINT ( q );
		    SKIP ( p );
		}
		else
		{
		    /* from src (source) */
		    SKIP ( q );
		    PRINT ( p );
		}

		/* 4 fields copied from src (source) */
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );
		PRINT ( p );
		SKIP  ( q );

		/* account disable date */
		if ( firsthost )
		{
		    /* from des (target) */
		    PRINT ( q );
		    SKIP ( p );
		}
		else
		{
		    /* from src (source) */
		    SKIP ( q );
		    PRINT ( p );
		}

		/* rest copied from src (source) */
		PRINTREST ( p );
	    }
	    else /* is get */
	    {
		/* password */
		if ( firsthost )
		{
		    /* hostlist from des (source) */
		    char save = * h;
		    * h = 0;
		    fprintf ( tmpf, ":%s", q.cp );
		    * h = save;
		    SKIP ( q );
		    /* password from src (target) */
		    h = p.cp;
		    SKIP ( p );
		    fprintf ( tmpf, "%s", h );
		}
		else
		{
		    /* password from des (source) */
		    PRINT ( q );
		    SKIP ( p );
		}

		/* password change date */
		if ( firsthost )
		{
		    /* from src (target) */
		    PRINT ( p );
		    SKIP ( q );
		}
		else
		{
		    /* from des (source) */
		    SKIP ( p );
		    PRINT ( q );
		}

		/* 4 fields copied from des (source) */
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );
		SKIP  ( p );
		PRINT ( q );

		/* account disable date */
		if ( firsthost )
		{
		    /* from src (target) */
		    PRINT ( p );
		    SKIP ( q );
		}
		else
		{
		    /* from des (source) */
		    SKIP ( p );
		    PRINT ( q );
		}

		/* rest from des ( source ) */
		PRINTREST ( q );
	    }
	}
    }
    fclose ( srcf );
    fclose ( tmpf );
}
