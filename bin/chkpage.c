/* Program to check file for overflowing max line
** and page length.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	chkpage.c
** Date:	Sat Mar 21 15:37:32 EDT 2009
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2009/03/21 19:37:48 $
**   $RCSfile: chkpage.c,v $
**   $Revision: 1.4 $
*/

#include <stdlib.h>
#include <stdio.h>

const char * documentation [] = {
"chkpage [-cCC] [-lLL] [filename ...]",
"",
"    Checks that files contain lines with no more than"
		" CC",
"    columns and pages with no more than LL lines.",
"",
"    If no filenames are given, the standard input"
		" is",
"    checked.",
"",
"    CC defaults to 80 columns and LL to 58 lines.",
"",
"    This program interprets newline as a line sepa-,",
"    rator, horizontal tab as going to the next tab",
"    stop, form feed as starting both a new page and",
"    a new line, and carriage return as going back to",
"    the first column without starting a new line.",
"    All other control characters are illegal.  Tab",
"    stops are set every 8 columns.  All this is con-",
"    sistent with common UNIX print programs.",
"",
"    Too long lines, lines overflowing a page, lines",
"    containing illegal characters, and a non-empty",
"    last line that does not end with a line feed or",
"    form feed are output.  Nothing is output if the",
"    files are all OK.",
NULL };

void checkfile
    ( FILE * in, char * name,
      int maxcolumn, int maxline )
{
    /* Buffer holding line for error messages. */

    char buffer[82];
    char * bp = buffer;
    char * endbp = buffer + 80;

    int line_in_file = 1;
    int line_in_page = 1;
    int column = 1;
    int line_not_empty = 0;

    /* line_not_empty means line before \n or \f or EOF
    ** has something besides \r's.
    */

    int line_too_long = 0;
    int page_too_long = 0;
    int illegal_character = '.';

    /* '.' means there is no illegal character */

    while ( 1 )
    {
        /* Loop through all characters of file. */

        int c = getc ( in );

	if ( c == '\t' )
	{
	    column += 8 - ( ( column - 1 ) % 8 );
	    if ( column - 1 > maxcolumn )
		line_too_long = 1;
	    line_not_empty = 1;
	    if ( bp < endbp ) * bp ++ = c;
	}
	else if ( c == '\r' )
	{
	    column = 1;

	    /* Notes:
	    **
	    ** 1. \r disrupts printout of error by
	    **    overwriting beginning of error message
	    **    so we do NOT put it in the buffer.
	    **
	    ** 2. In \n\r\f, \r should be an empty line,
	    **    so we do NOT set line_not_empty.
	    */
	}
	else if ( c == '\n' || c == '\f' || c == EOF )
	{
	    /* End of line */

	    * bp ++ = '\n';
	    * bp = 0;

	    if ( illegal_character != '.' )
	    {
		printf ( "LINE CONTAINS \\0%o: ",
		          illegal_character );
		if ( name ) printf ( "%s: ", name );
		printf ( "%d: %s", line_in_file,
		         buffer );
		illegal_character = '.';
	    }

	    if ( line_too_long )
	    {
		printf ( "LINE TOO LONG: " );
		if ( name ) printf ( "%s: ", name );
		printf ( "%d: %s", line_in_file,
		         buffer );
		line_too_long = 0;
	    }

	    if ( line_in_page > maxline
	         &&
		 ( line_not_empty || c == '\n' )
	         &&
		 ! page_too_long )
	    {
		printf ( "PAGE TOO LONG: " );
		if ( name ) printf ( "%s: ", name );
		printf ( "%d: %s", line_in_file,
		         buffer );
		page_too_long = 1;
	    }

	    if ( c == '\f' )
	    {
		line_in_page = 1;
		page_too_long = 0;
	    }
	    else if ( c == '\n' )
	    {
	        ++ line_in_page;
	        ++ line_in_file;
	    }
	    else if ( c == EOF )
	    {
	        if ( line_not_empty )
		{
		    printf ( "NON-EMPTY LAST FILE LINE"
		    	     " DOES NOT END WITH LINE"
			     " FEED OR FORM FEED:\n"
			     "    ");
		    if ( name ) printf ( "%s: ", name );
		    printf ( "%d: %s", line_in_file,
		    	     buffer );
		}
		break;
	    }

	    column = 1;
	    line_not_empty = 0;
	    bp = buffer;
	}
	else if ( c < 040 || c >= 0177 )
	{
	    illegal_character = c;
	    line_not_empty = 1;
	    if ( bp < endbp ) * bp ++ = c;
	}
	else
	{
	    if ( column > maxcolumn )
		line_too_long = 1;
	    ++ column;
	    line_not_empty = 1;
	    if ( bp < endbp ) * bp ++ = c;
	}
    }
}


int main ( int argc, char ** argv )
{
    int maxcolumn = 80;
    int maxline = 58;

    while ( argc >= 2 && argv[1][0] == '-' )
    {
	if ( argv[1][1] == 'c' )
	{
	    maxcolumn = atoi (argv[1] + 2);
	    if ( maxcolumn == 0 )
	    {
		printf ( "chkpage: bad argument %s\n",
			 argv[1] );
		exit (1);
	    }
	}
	else if ( argv[1][1] == 'l' )
	{
	    maxline = atoi (argv[1] + 2);
	    if ( maxline == 0 )
	    {
		printf ( "chkpage: bad argument %s\n",
			 argv[1] );
		exit (1);
	    }
	}
	else
	{
	    const char ** d = documentation;
	    for ( ; * d; ++ d ) printf ( "%s\n", * d );
	    exit (1);
	}

	++ argv; -- argc;
    }

    if ( argc <= 1 )
	checkfile ( stdin, NULL, maxcolumn, maxline );
    else while ( argc >= 2 )
    {
	FILE * in = fopen ( argv[1], "r" );
	if ( in != NULL )
	    checkfile
		( in, argv[1], maxcolumn, maxline );
	else printf ( "%s: cannot open\n", argv [1] );
	-- argc; ++ argv;
    }
    exit (0);
}
