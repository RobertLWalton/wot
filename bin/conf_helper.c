/* Helper program for the conf program conf_passwd and
 * conf_shadow functions.
 *
 * File:	conf_passwd_shadow.c
 * Author:	Bob Walton (walton@deas.harvard.edu)
 * Date:	Thu Mar 19 11:42:21 EDT 2009
 *
 * The authors have placed this program in the public
 * domain; they make no warranty and accept no liability
 * for this program.
 *
 * RCS Info (may not be true date or author):
 *
 *   $Author: root $
 *   $Date: 2009/03/19 17:24:08 $
 *   $RCSfile: conf_helper.c,v $
 *   $Revision: 1.2 $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* This program accepts the COMMANDs `put' and `get' and
 * copies the source or target files, repectively, to
 * to the tmpfile, making appropriate field substitu-
 * tions.  The file TYPEs are `passwd' or `shadow'
 */

/* Move a char pointer to the next `:' or NUL.
 */
#define SKIP(p) while ( * p && * p != ':' ) ++ p; \
                if ( * p ) * p ++ = 0

/* Ditto but print field skipped over to tmpf preceeded
 * by a ':'.
 */
#define PRINT(p) print ( tmpf, & p )
void print ( FILE * tmpf, char ** pp )
{
    char * p = * pp;
    char * q = p;
    SKIP ( p );
    fprintf ( tmpf, ":%s", q );
    * pp = p;
}

/* List of file lines that contain information to be
 * inserted into the file being copied, indexed by
 * account name.
 */
struct line
{
    /* Line is malloced.  Account points at beginning
     * and is NUL terminated, the : having been
     * replaced in the line by NUL.  rest points just
     * after the NUL unless there is nothing after
     * the NUL, in which it points to NUL.
     */

    char * account;
    char * rest;		
    struct line * previous;
} * lastline = NULL;

/* Return line with given account or NUL if none.
 */
struct line * find ( const char * account )
{
    struct line * result = lastline;
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
void insert ( const char * line, const char * des )
{
    struct line * newline
        = (struct line *)
	  malloc ( sizeof ( struct line ) );
    char * p = malloc ( strlen ( line ) + 1 );
    strcpy ( p, line );
    newline->account = p;
    SKIP ( p );
    newline->rest = p;
    if ( find ( newline->account ) )
    {
        printf ( "ERROR: lines with duplicate account"
	         " names in %s\n", des );
	exit ( 2 );
    }
    newline->previous = lastline;
    lastline = newline;
}
    
static char buffer [10000];
int main ( int argc, char ** argv )
{
    if ( argc < 9 )
    {
    	printf ( "conf_passwd_shadow VERBOSE HOST"
	         " TYPE COMMAND FILE SOURCEFILE"
		 " TARGETFILE TMPFILE\n" );
	exit ( 2 );
    }

    int verbose          = ( argv[1][0] == '1' );
    const char * HOST    = argv[2];
    const char * TYPE    = argv[3];
    const char * command = argv[4];
    const char * file    = argv[5];
    const char * source  = argv[6];
    const char * target  = argv[7];
    const char * tmpfile = argv[8];
    
#   define vprintf if ( verbose ) printf

    /* Figure out which is the source file for the
     * copy (src) and which is the ultimate
     * destination (des) that the tmpfile is replace.
     */
    const char * src = NULL;
    const char * des = NULL;
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
     */ 
    FILE * desf = fopen ( des, "r" );
    if ( desf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 des );
	exit ( 2 );
    }
    while ( fgets ( buffer, sizeof ( buffer ), desf ) )
    {
        buffer[strlen(buffer)-1] = 0;
        insert ( buffer, des );
    }
    fclose ( desf );

    /* Copy src to tmpfile inserting fields as necessary
     * from des.
     */
    FILE * srcf = fopen ( src, "r" );
    if ( srcf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 src );
	exit ( 2 );
    }
    FILE * tmpf = fopen ( tmpfile, "w" );
    if ( tmpf == NULL )
    {
        printf ( "ERROR: cannot open %s for writing\n",
		 tmpfile );
	exit ( 2 );
    }
    int isput = ( strcmp ( command, "put" ) == 0 );
    int ispasswd = ( strcmp ( TYPE, "passwd" ) == 0 );
    int hostlength = strlen ( HOST );
    while ( fgets ( buffer, sizeof ( buffer ), srcf ) )
    {
        buffer[strlen(buffer)-1] = 0;
        char * p = buffer;
	SKIP ( p );
	struct line * desline = find ( buffer );

	if ( ispasswd )
	{
	    if ( desline == NULL )
	    {
		if ( * p )
		    fprintf ( tmpf, "%s:%s\n",
		              buffer, p );
		else
		    fprintf ( tmpf, "%s\n", buffer );

		continue;
	    }
	    if ( isput )
	    {
		/* shell comes from des file.
		 */
		char * q = desline->rest;
		fprintf ( tmpf, buffer );
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

		SKIP  ( p );
		PRINT ( q );

		fprintf ( tmpf, "%s\n", p );
	    }
	    else
	    {
		/* shell comes from src file.
		 */
		char * q = desline->rest;
		fprintf ( tmpf, buffer );
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

		SKIP  ( q );
		PRINT ( p );

		fprintf ( tmpf, "%s\n", q );
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
	    if ( isput ) h = p;
	    else if ( desline == NULL ) h = "";
	    else h = desline->rest;

	    int first = 1;
	    if ( * h == '!' ) while ( 1 )
	    {
	        char * k = ++ h;
		while ( * k && * k != '!' ) ++ k;
		if ( * k != '!' ) break;
		if ( k == h ) break;
		if ( k - h == hostlength
		     &&
		     strncmp ( h, HOST, hostlength ) )
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
		fprintf ( tmpf, "%s:", buffer );
	        if ( isput )
		{
		    if ( ! hostlistexists )
			fprintf ( tmpf, "%s\n", p );
		    else if ( inhostlist )
			fprintf ( tmpf, "%s\n", h );
		    else
		    {
		    	SKIP ( p );
			fprintf ( tmpf, "!!:%s", p );
		    }
		}
		else /* is get */
		{
		    assert ( ! hostlistexists );
		    fprintf ( tmpf, "!%s!%s\n",
		                    HOST, p );
		}
		continue;
	    }

	    /* account */

	    fprintf ( tmpf, "%s", buffer );
	    if ( isput )
	    {
		char * q = desline->rest;

		/* password */
		if ( firsthost )
		{
		    p = h;
		    PRINT ( q );
		    SKIP ( p );
		}
		else if ( inhostlist )
		{
		    p = h;
		    SKIP ( q );
		    PRINT ( p );
		}
		else if ( hostlistexists )
		{
		    p = h;
		    SKIP ( q );
		    SKIP ( p );
		    fprintf ( tmpf, ":!!" );
		}
		else
		{
		    SKIP ( q );
		    PRINT ( p );
		}

		/* password change date */
		if ( firsthost )
		{
		    PRINT ( q );
		    SKIP ( p );
		}
		else
		{
		    SKIP ( q );
		    PRINT ( p );
		}

		/* four fields copied from source */
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
		    PRINT ( q );
		    SKIP ( p );
		}
		else
		{
		    SKIP ( q );
		    PRINT ( p );
		}

		fprintf ( tmpf, ":%s\n", p );
	    }
	    else /* is get */
	    {
		char * q = desline->rest;

		/* password */
		if ( firsthost )
		{
		    * h = ':';
		    PRINT ( q );
		    SKIP ( q );
		    h = p;
		    SKIP ( p );
		    fprintf ( tmpf, "%s", h );
		}
		else
		{
		    PRINT ( q );
		    SKIP ( p );
		}

		/* password change date */
		if ( firsthost )
		{
		    PRINT ( p );
		    SKIP ( q );
		}
		else
		{
		    SKIP ( p );
		    PRINT ( q );
		}

		/* four fields copied from source */
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
		    PRINT ( p );
		    SKIP ( q );
		}
		else
		{
		    SKIP ( p );
		    PRINT ( q );
		}

		fprintf ( tmpf, ":%s\n", q );
	    }
	}
    }
    fclose ( srcf );
    fclose ( tmpf );

    return 0;
}
