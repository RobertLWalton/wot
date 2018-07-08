/* Unicode to Latin1 Converter
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	unicode2latin1.c
** Date:	Sun Jul  8 04:13:57 EDT 2018
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2012/04/02 10:05:38 $
**   $RCSfile: sniffbench.c,v $
**   $Revision: 1.2 $
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

const char * documentation [] = {
"unicode2latin1 [ -doc | -v ]",
"",
"copies the standard input interpreted as UTF8 bytes",
"to the standard output interpreted as LATIN1 bytes.",
"",
"UNICODE characters that are not LATIN1 characters",
"are translated to «¦XXXX¦» where XXXX is the hex",
"character code.  Illegal encodings are translated",
"to «¦XX,XX,..¦» where the XX's are the bytes in",
"hex.  With the -v option, a message is also output",
"on the standard error.",
"",
"This program can be used as a front end to"
  " enscript(1)",
"whose default character set is LATIN1.",
NULL
};

typedef unsigned int Uchar;

/* Convert char * string to one unicode character
 * and update the string pointer.  UNKNOWN_UCHAR
 * is returned by illegal encoding.  Any ASCII
 * character (e.g., NUL) will terminate a multi-
 * byte translation.
 */
# define UNKNOWN_UCHAR 0xFFFFFFFF
Uchar utf8_to_unicode
	( const unsigned char ** s )
{
    unsigned char c = * (*s) ++;
    unsigned bytes;
    Uchar unicode;

    if ( c < 0x80 ) return c;

    bytes = 0;
    unicode = c;
    if ( c < 0xC0 )
	unicode = UNKNOWN_UCHAR;
    else if ( c < 0xE0 )
	unicode &= 0x1F, bytes = 1;
    else if ( c < 0xF0 )
	unicode &= 0x0F, bytes = 2;
    else if ( c < 0xF8 )
	unicode &= 0x07, bytes = 3;
    else if ( c < 0xFC )
	unicode &= 0x03, bytes = 4;
    else if ( c < 0xFE )
	unicode &= 0x01, bytes = 5;
    else
	unicode &= 0x00, bytes = 6;

    while ( bytes -- )
    {
	c = * (*s) ++;
	if ( c < 0x80 || 0xC0 <= c )
	{
	    unicode = UNKNOWN_UCHAR;
	    -- (*s);
	    break;
	}
	unicode <<= 6;
	unicode += ( c & 0x3F );
    }
    return unicode;
}

int main ( int argc, char ** argv )
{
    int verbose;
#   define BSIZE 10000
    unsigned char buffer[BSIZE];
    const unsigned char * s;
    unsigned char * ends, * endb;
    size_t length;
    int eof_found;
    unsigned line_number;

    verbose = 0;
    if ( argc == 2 && strcmp ( argv[1], "-v" ) == 0 )
        verbose = 1;
    else if ( argc != 1 )
    {
	const char ** p = documentation;
	while ( * p )
	    fprintf ( stderr, "%s\n", * p ++ );
	exit (1);
    }

    s = buffer;
    ends = buffer;
    endb = buffer + sizeof ( buffer );
    eof_found = 0;
    line_number = 1;

    while ( 1 )
    {
        Uchar c;
	const unsigned char * begs;

        if ( ends - s < 8 && ! eof_found )
	{
	    memmove ( buffer, s, endb - s );
	    ends -= s - buffer;
	    s = buffer;
	    length = read ( 0, ends, endb - ends );
	    if ( length == -1 )
	    {
		fprintf ( stderr,
			  "unicode2latin1:"
			  " read error at line %d:"
			  "\n    %s\n",
			  line_number,
			  strerror ( errno ) );
		exit ( 1 );
	    }
	    else if ( length == 0 )
	    {
	        eof_found = 1;
		assert ( ends < endb );
		* ends = 0;
	    }
	    else
		ends += length;
	}

	if ( eof_found && s == ends ) break;

	begs = s;
	c = utf8_to_unicode ( & s );

	/* We must use hex encoding in "..." literals
	 * that are to be output as having LATIN1
	 * characters, as otherwise they would be
	 * output in UTF8.
	 *
	 * 	\xAB	is	«
	 * 	\xA6	is	¦
	 * 	\xBB	is	»
	 */
	if ( c == UNKNOWN_UCHAR )
	{
	    char * prefix = "\xAB\xA6";
	    char * vprefix = "«¦";
	    if ( verbose )
		fprintf ( stderr,
			  "unicode2latin1: found " );
	    while ( begs != s )
	    {
	        unsigned char b = * begs ++;
		printf ( "%s%02X", prefix, b );
		if ( verbose )
		    fprintf ( stderr,
			      "%s%02X", vprefix, b );
		prefix = ",";
	    }
	    if ( verbose )
		fprintf ( stderr, 
			  "¦» in line %d\n",
			  line_number );
	    if ( printf ( "\xA6\xBB" ) >= 0 )
	        continue;
	}
	else if ( c > 0xFF )
	{
	    if ( verbose )
		fprintf ( stderr,
			  "unicode2latin1:"
			  " found «¦%02X¦»"
			  " in line %d\n",
			  c, line_number );
	    if (    printf
			( "\xAB\xA6%02X\xA6\xBB", c )
	         >= 0 )
		continue;
	}
	else
	{
	    if ( c == '\n' ) ++ line_number;
	    if ( putchar ( c ) != EOF )
		continue;
	}

	fprintf ( stderr,
		  "unicode2latin1:"
		  " write error at line %d:"
		  "\n    %s\n",
		  line_number,
		  strerror ( errno ) );
	exit ( 1 );
    }

    exit ( 0 );
}
