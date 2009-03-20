/* Helper program for the conf program conf_passwd and
 * conf_shadow functions.
 *
 * File:	conf_passwd_shadow.c
 * Author:	Bob Walton (walton@deas.harvard.edu)
 * Date:	Fri Mar 20 06:46:30 EDT 2009
 *
 * The authors have placed this program in the public
 * domain; they make no warranty and accept no liability
 * for this program.
 *
 * RCS Info (may not be true date or author):
 *
 *   $Author: root $
 *   $Date: 2009/03/20 12:53:02 $
 *   $RCSfile: conf_helper.c,v $
 *   $Revision: 1.3 $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* conf_passwd_shadow VERBOSE HOST TYPE COMMAND FILE \
 *		      SOURCEFILE TARGETFILE TMPFILE
 *
 * VERBOSE:	0 or 1
 * HOST		HOST variable value from `conf' program
 * TYPE		passwd or shadow
 * COMMAND	put or get
 * FILE		name of file relative to directories
 *		for error messages
 * SOURCEFILE	source file name
 * TARGETFILE	target file name
 * TMPFILE	temporary file name provided by `conf'
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
void init ( fpointer * fpp, char * bufferp )
{
    fpp->cp = bufferp;
    fpp->present = ( * bufferp != 0 );
}
#define INIT(fp,bufferp) init ( & fp, bufferp )

/* Skip a field.  Set the char after the field to NUL.
 */
void skip ( fpointer * fpp )
{
    char * cp = fpp->cp;
    if ( ! fpp->present ) return;
    while ( * cp && * cp != ':' ) ++ cp;
    fpp->present = ( * cp == ':' );
    if ( fpp->present ) * cp ++ = 0;
    fpp->cp = cp;
}
#define SKIP(fp) skip(&(fp))

/* Ditto but print the field to tmpf preceded by a ':',
 * if the field is present.
 */
FILE * tmpf;
void print ( fpointer * fpp )
{
    char * cp = fpp->cp;
    if ( ! fpp->present ) return;
    skip ( fpp );
    fprintf ( tmpf, ":%s", cp );
}
#define PRINT(fp) print(&(fp))

/* Print the rest of the fields and an end of line.
 */
void printrest ( fpointer * fpp )
{
    while ( fpp->present ) PRINT ( * fpp );
    fprintf ( tmpf, "\n" );
}
#define PRINTREST(fp) printrest ( &(fp) )


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

/* Lines are changed together from lastline via previous
 * member.
 */
line * lastline = NULL;

/* Return line with given account or NUL if none.
 */
line * find ( const char * account )
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

/* Insert line.
 */
void insert ( const char * linep, const char * des )
{
    line * newline =
        (line *) malloc ( sizeof ( line ) );
    char * p = malloc ( strlen ( linep ) + 1 );
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
    
/* Line buffer and function to read into it.
 * Returns NULL on EOF like fgets.  fname is
 * for error message.
 */
static char buffer [10002];
char * read ( FILE * f, const char * fname )
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

int main ( int argc, char ** argv )
{
    int verbose;
    const char * HOST;
    const char * TYPE;
    const char * command;
    const char * file;
    const char * source;
    const char * target;
    const char * tmpfile;
    const char * src;
    const char * des;
    FILE * desf;
    FILE * srcf;
    /* FILE * tmpf;	This is global. */
    int isput;
    int ispasswd;
    int hostlength;

    if ( argc < 9 )
    {
    	printf ( "conf_passwd_shadow VERBOSE HOST"
	         " TYPE COMMAND FILE SOURCEFILE"
		 " TARGETFILE TMPFILE\n" );
	exit ( 2 );
    }

    verbose = ( argv[1][0] == '1' );
    HOST    = argv[2];
    TYPE    = argv[3];
    command = argv[4];
    file    = argv[5];
    source  = argv[6];
    target  = argv[7];
    tmpfile = argv[8];
    
#   define vprintf if ( verbose ) printf

    /* Figure out which is the source file (src) for the
     * copy and which is the ultimate destination file
     * (des) that the tmpfile is to replace.
     */
    if ( strcmp ( command, "put" ) == 0 )
    {
        src = source;
	des = target;
    }
    else
    {	src = target;
    	des = source;
    }

    /* Read the des file and insert its lines into the
     * line list.
     */ 
    desf = fopen ( des, "r" );
    if ( desf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 des );
	exit ( 2 );
    }
    while ( read ( desf, des ) )
        insert ( buffer, des );
    fclose ( desf );

    /* Copy src to tmpfile inserting fields as necessary
     * from des.
     */
    srcf = fopen ( src, "r" );
    if ( srcf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 src );
	exit ( 2 );
    }
    tmpf = fopen ( tmpfile, "w" );
    if ( tmpf == NULL )
    {
        printf ( "ERROR: cannot open %s for writing\n",
		 tmpfile );
	exit ( 2 );
    }
    isput = ( strcmp ( command, "put" ) == 0 );
    ispasswd = ( strcmp ( TYPE, "passwd" ) == 0 );
    hostlength = strlen ( HOST );
    while ( read ( srcf, src ) )
    {
	line * desline;
        fpointer p;
	INIT ( p, buffer );
	SKIP ( p );
	desline = find ( buffer );
#	define q desline->rest

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
		    assert ( ! hostlistexists );
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

    return 0;
}
