/* Light Revision Control System (Light-RCS)
**
** Author:	Bob Walton (walton@acm.org)
** File:	lrcs.c
** Date:	Sun Aug 16 05:11:19 EDT 2020
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
*/

#include <stdlib.h>
#define __USE_POSIX2
#include <stdio.h>
    /* __USE_POSIX2 must be just before stdio.h
     * to enable popen(3).
     */
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <errno.h>

const char * documentation[] = {
"lrcs -doc",
"lrcs in file",
"lrcs out file [revision]",
"lrcs diff file [revision] [diff-option...]",
"lrcs diff file revision:revision [diff-option...]",
"lrcs list file",
"",
"A file f has revisions stored in f,V, which is",
"placed in the directory LRCS if that exists, or in",
"the current directory otherwise.",
"",
"There are no branches, only a linear sequence of",
"revisions.  Nothing is remembered about a revision",
"except its contents and its modification time.",
"",
"Revisions are numbered 1, 2, ..., with 1 being the",
"MOST-RECENT revision.",
"",
"If the out command for file f has a revision number n",
"the file produced has the name f,Vn.",
"",
"The diff(1) program is used to produce diff",
"listings.",
"",
"The diff command can compare the current file with",
"the latest revision, or with a specified revision,",
"or can compare two revisions.",
"",
"The list command lists the revisions and their",
"times.",
"",
"For a file f, if f,V or LRCS/f,V does not exist,",
"this program looks for a f,v or RCS/f,v file, and",
"if it exists, converts it to a f,V file, provided it",
"has no branches.",
"",
"The format of a ,V file is:",
"",
"    time+ string+",
"",
"where are time is an integer number of seconds since",
"the UNIX epoch followed by a line feed, and a",
"string is a line feed followed by an `@' followed",
"by a string of 8-bit bytes with `@'s doubled",
"followed by an `@' and a line feed.",
"",
"The times are the modification times of the",
"revisions.  The first string is the first revision.",
"Each subsequent string is the output of:",
"",
"    diff -n previous-revision next-revision",
"",
"On checking the file in, the file contents become",
"the new first revision, i.e., the file is pushed",
"to the BEGINNING of the revision list.",
NULL
};

/* Revisions are store in temporary files.
 */
typedef struct revision
{
    int time;
    char * filename;
    struct revision * next;
} revision;
revision * first;

int repos_line;

/* Copy a file from src to des, and stop on an EOF.
 * Returns NULL on success and error message on
 * failure.
 */
const char * copy ( FILE * src, FILE * des )
{
    int c;
    while ( ( c = fgetc ( src ) ) != EOF )
    {
        if ( fputc ( c, des ) == EOF )
	    return strerror ( errno );
    }
    if ( ferror ( src ) )
        return strerror ( errno );
    else
        return NULL;
}

/* Copy a string from repos to des, and stop after
 * trailing @.  Skip whitespace before beginning @.
 * Returns NULL on success and error message on
 * failure.
 */
const char * copy_from_string
	( FILE * repos, FILE * des )
{
    int c;

    /* Skip initial whitespace and @.
     */
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	if ( c == '\n' ) ++ repos_line;
	if ( isspace ( c ) ) continue;
        if ( c == '@' ) break;
	return "expected string not found in input";
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    if ( feof ( repos ) )
        return "unexpected EOF reading string from"
	       " input";

    /* Read an copy till after final @.
     */
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
        if ( c == '@' )
	{
	    c = fgetc ( repos );
	    if ( c != '@' )
	    {
	        ungetc ( c, repos );
		break;
	    }
	}
	if ( c == '\n' ) ++ repos_line;
        if ( fputc ( c, des ) == EOF )
	    return strerror ( errno );
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    else
        return NULL;
}

/* Copy a file from src to a string in repos.  Begin the
 * string with "\n@" and end with "@\n".  Double @'s
 * in the file.  Returns NULL on success and error
 * message on failure.
 */
const char * copy_to_string ( FILE * src, FILE * repos )
{
    int c;

    if ( fputc ( '\n', repos ) == EOF )
        return strerror ( errno );
    if ( fputc ( '@', repos ) == EOF )
        return strerror ( errno );

    /* Read an copy till after EOF.
     */
    while ( ( c = fgetc ( src ) ) != EOF )
    {
        if ( fputc ( c, repos ) == EOF )
	    return strerror ( errno );
        if ( c == '@' )
	{
	    if ( fputc ( c, repos ) == EOF )
		return strerror ( errno );
	}
    }
    if ( ferror ( src ) )
        return strerror ( errno );

    if ( fputc ( '@', repos ) == EOF )
        return strerror ( errno );
    if ( fputc ( '\n', repos ) == EOF )
        return strerror ( errno );

    return NULL;
}

/* Read a natural number from src to num.  Whilespace
 * before number is ignored.  Return NULL on success
 * (must have at least one digit), or error message
 * on failure.
 */
const char * get_number ( int * num, FILE * repos )
{
    int c;

    /* Skip initial whitespace and get next
     * non-whitespace.
     */
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	if ( c == '\n' ) ++ repos_line;
	if ( ! isspace ( c ) ) break;
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    if ( feof ( repos ) )
        return "unexpected EOF reading number from"
	       " repository";

    if ( ! isdigit ( c ) )
	return "expected number not found in"
	       " repository";

    * num = c - '0';
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	if ( ! isdigit ( c ) ) break;
	* num *= 10;
	* num += c - '0';
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    ungetc ( c, repos );
    return NULL;
}

/* Copy from file src to file des editing it on the fly
 * using a diff -n listing in a string in repos.
 * Returns NULL on success and error message on failure.
 */
const char * edit
	( FILE * repos, FILE * src, FILE * des )
{
    int c;
    const char * s;

    int feeds = 0;
        /* Number of line feeds seen so far in src.
	 */
    int op;
    int commands_done = 0;

    int location, count;

    /* Skip initial whitespace and @ in diff.
     */
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	if ( c == '\n' ) ++ repos_line;
	if ( isspace ( c ) ) continue;
        if ( c == '@' ) break;
	return "expected string not found in"
	       " repository";
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    if ( feof ( repos ) )
        return "unexpected EOF reading string"
	       " from repository";

    /* Read and execute repository commands.
     */
    while ( ! commands_done )
    {
	op = fgetc ( repos );
	if ( op == EOF )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	}
	if ( op == '\n' ) ++ repos_line;
	if ( isspace ( op ) ) continue;
        if ( op == '@' ) break;

        if ( op != 'a' && op != 'd' )
	    return "expected edit command not found in"
		   " repository";

	s = get_number ( & location, repos );
	if ( s != NULL ) return s;
	s = get_number ( & count, repos );
	if ( s != NULL ) return s;

	if ( count == 0 )
	    return "second command parameter == 0 in"
	           " repository";

	/* Skip to after end of line in repos
	 */
	while ( ( c = fgetc ( repos ) ) != '\n' )
	{
	    if ( isspace ( c ) ) continue;
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	    return "extra stuff after command"
	           " in repository string";
	}
	++ repos_line;

	if ( op == 'd' ) -- location;

	if ( location < feeds )
	    return "commands out of order in"
	           " repository";

	/* Skip src to beginning of line indicated
	 * by command.
	 */
	while ( location > feeds )
	{
	    c = fgetc ( src );
	    if ( c == EOF )
	    {
		if ( ferror ( src ) )
		    return strerror ( errno );
		if ( feof ( src ) )
		    return "unexpected input EOF";
	    }
	    if ( fputc ( c, des ) == EOF )
		return strerror ( errno );
	    if ( c == '\n' ) ++ feeds;
	}

	if ( op == 'd' )
	{
	    int at_line_beginning = 1;

	    /* Skip count lines in src.  Last line might
	     * have EOF instead of linefeed at end.
	     */
	    location += count;
	    while ( location > feeds )
	    {
		c = fgetc ( src );
		if ( c == EOF )
		{
		    if ( ferror ( src ) )
			return strerror ( errno );
		    if ( at_line_beginning
		         ||
			 feeds + 1 < location )
			return "unexpected input EOF";
		    else break;
		}
		if ( c == '\n' ) ++ feeds;
	    }
	}
	else /* op == 'a' */
	{
	    /* Copy from repository to des until count
	     * line feeds have been copied or repository
	     * string ends.  Copying is from inside a
	     * repository string, so @@ copies to one @.
	     */
	    while ( count > 0 ) 
	    {
	        c = fgetc ( repos );
		if ( c == EOF ) break;
		if ( c == '@' )
		{
		    c = fgetc ( repos );
		    if ( c == EOF ) break;
		    if ( c != '@' )
		    {
		        ungetc ( c, repos );
			commands_done = 1;
			break;
		    }
		}
		if ( fputc ( c, des ) == EOF )
		    return strerror ( errno );
		if ( c == '\n' )
		    -- count, ++ repos_line;
	    }
	    if ( c == EOF && ! commands_done )
	    {
		if ( ferror ( repos ) )
		    return strerror ( errno );
		if ( feof ( repos ) )
		    return "unexpected end of file"
			   " in repository";
	    }
	}
    }
    
    /* Copy rest of src.
     */
    return copy ( src, des );
}

int main ( int argc, char ** argv )
{
    if ( argc < 2
         ||
	 strncmp ( argv[1], "-doc", 4 ) == 0 )
    {
        FILE * doc = popen ( "less -F", "w" );
	const char ** p = documentation;
	while ( * p )
	    fprintf ( doc, "%s\n", * p ++ );
	fclose ( doc );
	exit ( 0 );
    }
}
