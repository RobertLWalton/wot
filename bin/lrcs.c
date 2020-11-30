/* Light Revision Control System (Light-RCS)
**
** Author:	Bob Walton (walton@acm.org)
** File:	lrcs.c
** Date:	Mon Nov 30 00:35:19 EST 2020
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
*/

#define _POSIX_C_SOURCE 200112L
#define __STDC_WANT_LIB_EXT2__ 1

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>

const char * documentation[] = {
"lrcs -doc",
"lrcs [-t] list file",
"lrcs [-t] in file",
"lrcs [-t] out file [revision]",
"lrcs [-t] diff file [revision] [diff-option...]",
"lrcs [-t] diff file revision:revision"
				" [diff-option...]",
"lrcs [-t] git [committer e-mail]",
"",
"The Light Revision Control System (LRCS) is for use",
"with files that make no reference to other files and",
"are not referenced by other files.  LRCS is a",
"successor to RCS and can accept RCS ,v repositories.",
"LRCS is strictly linear with no branching, locking,",
"or meta-data other than revision modification times.",
"",
"A file f has revisions stored in repository f,V,",
"which is placed in the directory LRCS if that",
"exists, or in the current directory otherwise.",
"",
"There are no branches, only a linear sequence of",
"revisions.  Nothing is remembered about a revision",
"except its contents and its modification time.",
"",
"Revisions are numbered 1, 2, ..., with 1 being the",
"MOST-RECENT revision.  Revision number 0 can be used",
"to refer to the file itself, as opposed revision 1",
"as it is stored in the repository.",
"",
"The `list' command lists the revisions and their",
"times.",
"",
"The `in' command pushes file f to the beginning of",
"the revision list in the repository, making it",
"revision 1.",
"",
"The `out' command for file f and revision number n",
"produces revision n of file f in a file named f,Vn.",
"But `lrcs out f' and `lrcs out f 0' produce revision",
"1 in the file named f, not f.V1.",
"",
"The `diff' command can compare the current file with",
"the latest revision, or with a specified revision,",
"or can compare two revisions.  When comparing two",
"revisions, 0 can be used to denote the current file,",
"as in using 0:5 to find `diff f f.V5'.",
"",
"The diff(1) program is used to produce diff listings.",
"The `diff' command diff-options are passed to",
"diff(1).",
"",
"The `git' command imports all the ,V and ,v files",
"in the current directory and its subdirectory tree",
"into the git repository of the current directory",
"(which may be in a .git subdirectory of either the",
"current directory or one of it ancestors).  If the",
"committer and e-mail arguments are given, a new git",
"repository in the current directory is created first.",
"",
"For a file f, if f,V and LRCS/f,V do not exist,",
"this program looks for a legacy f,v or RCS/f,v file,",
"and uses it if it exists.  In the case of an `in'",
"command, the ,v repository will be input but a",
"separate ,V repository will be output.",
"",
"The -t option causes execution to be traced.  It",
"can be used to elicidate error messages.",
"",
"This program makes temporary files for file f under",
"names such as f,Vn+ and f,V+.  It deletes these when",
"the program terminates, even if it terminates with",
"an error.",
"",
"The format of a ,V file is:",
"",
"    time+ string+",
"",
"where a time is an integer number of seconds since",
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
"",
"The header of a ,v legacy repository can be examined",
"with an editor and edited.  The legacy repository",
"must have accurate head and next entries and well-",
"formatted date entries for the revisions used.",
"Legacy date entry values are GMT.  The head and",
"next entries are used to determine the revisions",
"seen by lrcs.  If the repository has branches, head",
"may be edited to get different revision lists.",
NULL
};

/* lrcs [-t] glob file mark
 *
 * is a suboperation of `lrcs git' that appends to
 * import,git with the versions in the repos of file
 * as globs with marks `mark+1', `mark+2', etc., and
 * appends to index,git a line for each version of the
 * form `time:mark:file'.  In this case `file' may
 * contain any character except newline.'  This
 * operation returns the last mark used in its output.
 */

int trace = 0;   /* Set true by -t */

typedef unsigned long nat;
    /* Natural number.  Longest that can be declared
     * in C90.
     */

int repos_line = 0;
    /* Current line number in repository being read.
     * Output in error messages some of which complain
     * about repository being mis-formatted.
     */

/* A legacy RCS repository is viewed as a sequence of
 * entries, each of the form `num', `id ...;', or
 * `id string'.
 *
 * The header consists of all entries before the
 * `desc string' entry.  The body consists of all
 * entries after.
 *
 * The following apply during header scan.
 *
 * A `head num' entry creates a first revision with the
 * given num as its rnum (revision number) and sets
 * scan_revision = NULL.
 *
 * A `num' entry that matches last_revision->rnum sets
 * scan_revision to last_revision.  If it does not
 * match it sets scan_revision to NULL.
 *
 * A `date num' entry with scan_revision non-NULL sets
 * scan_revision->date.
 *
 * A `next num' entry with scan_revision non-NULL
 * creates a new revision with num as its rnum.
 *
 * Revision times are set from revision dates at the
 * end of header scan.
 *
 * During the body scan rnums are read from the database
 * created by the header and for each rnum the reposi-
 * tory is skipped forward to the text string of that
 * rnum's deltatext.
 */
typedef nat num[100];
    /* Encodes num[0].num[1].num[2]. ..., which ends
     * with first element that is NUMEND.
     */
const nat NUMEND = (nat) -1;
char id[100];  	/* entry id */

/* Return -1 if n1 < n2, 0 if n1 == n2, +1 if n1 > n2.
 */
int numcmp ( num n1, num n2 )
{
    int i = 0;
    while ( 1 )
    {
	if ( n1[i] == n2[i] ) return 0;
	else if ( n1[i] == NUMEND ) return -1;
	else if ( n2[i] == NUMEND ) return +1;
        else if ( n1[i] < n2[i] ) return -1;
	else if ( n1[i] > n2[i] ) return +1;
	++ i;
	assert ( i < 100 );
    }
}

/* Put a printable representation of n in num_buffer and
 * return a pointer to it.
 */
char num_buffer[10000];
const char * num2str ( num n )
{
    int i;
    char * p = num_buffer;
    if ( n[0] == NUMEND )
        sprintf ( p, "missing-num" );
    else
    {
	p += sprintf ( p, "%lu", n[0] );
	i = 1;
	while ( n[i] != NUMEND )
	    p += sprintf ( p, ".%lu", n[i++] );
    }
    return num_buffer;
}

/* Revisions are store in temporary files.
 */
typedef struct revision
{
    time_t time;
    char * filename;
    struct revision * previous;
    struct revision * next;

    /* Legacy info */

    num rnum;
    num date;
} revision;
revision * first_revision = NULL;
revision * last_revision = NULL;

const char * vprefix = "";
void verror ( const char * fmt, va_list ap )
{
    fprintf ( stderr, "lrcs: error: %s", vprefix );
    vfprintf ( stderr, fmt, ap );
    fprintf ( stderr, "\n" );
    if ( errno != 0 )
	fprintf ( stderr, "      %s\n",
	          strerror ( errno ) );
    if ( repos_line > 0 )
	fprintf ( stderr, "      repository is at line"
	                  " %d\n", repos_line );
    if ( ! trace )
	fprintf ( stderr, "      rerun with -t option"
	                  " for more details\n" );
    fprintf ( stderr, "      run with -doc option for"
                      " documentation\n" );
    exit ( 1 );
}
void error ( const char * fmt, ... )
{
    va_list ap;
    va_start ( ap, fmt );
    errno = 0;
    verror ( fmt, ap );
}
void errorno ( const char * fmt, ... )
{
    va_list ap;
    va_start ( ap, fmt );
    /* leave errno untouched */
    verror ( fmt, ap );
}
void erroreof ( const char * fmt, ... )
{
    va_list ap;
    va_start ( ap, fmt );
    errno = 0;
    vprefix = "unexpected end of file while ";
    verror ( fmt, ap );
}
void errornf ( const char * kind )
{
    error ( "expected %s not found in repository",
            kind );
}
void tprintf ( const char * fmt, ... )
{
    va_list ap;
    if ( ! trace ) return;
    va_start ( ap, fmt );
    vfprintf ( stderr, fmt, ap );
}

/* The following manage a command executed by popen.
 */
char * command = NULL;
size_t command_length;
size_t command_size = 0;
    /* Size is allocated size, length is used length. */

/* Initialize command.
 */
void init_command ( void )
{
    command_length = 0;
}

void append_helper ( const char * argument, int quote );

/* Append argument to command, separated from previous
 * command part by a single space.
 */
void append ( const char * argument )
{
    append_helper ( argument, 0 );
}

/* Ditto but quote argument, and escape characters
 * that are special inside quotes ($, `, \, !).
 */
void append_quoted ( const char * argument )
{
    append_helper ( argument, 1 );
}

/* Ditto but append long integer.
 */
void append_long ( long argument )
{
    char buffer[200];
    sprintf ( buffer, "%ld", argument );
    append_helper ( buffer, 0 );
}

void append_helper ( const char * argument, int quote )
{
    size_t len = strlen ( argument );
    char * p;
    while ( command_length + 2*len + 4 > command_size )
    {
        command_size += 4096;
	command = (char *) realloc
	    ( command, command_size );
	if ( command == NULL )
	    errorno ( "while allocating memory" );
    }

    p = command + command_length;

    if ( p != command ) * p ++ = ' ';

    if ( quote )
    {
	* p ++ = '"';
	while ( * argument )
	{
	    char c = * argument ++;
	    if ( c == '"' || c == '`' || c == '\\'
	                  || c == '!' )
		* p ++ = '\\';
	    * p ++ = c;
	}
	* p ++ = '"';
    }
    else
	while ( * argument ) * p ++ = * argument ++;

    command_length = p - command;
    * p = 0;
}

/* Popen command with given mode, check for error,
 * and return FILE *.
 */
FILE * open_command ( const char * mode )
{
    FILE * f;
    tprintf ( "* executing `%s'\n", command );
    f = popen ( command, mode );
    if ( f == NULL )
	errorno ( "executing `%s'", command );
    return f;
}

/* Pclose FILE * and check for errors.
 */
void close_command ( FILE * fd )
{
    int exit_status;
    exit_status = pclose ( fd );
    if ( exit_status < 0 )
	errorno ( "waiting for `%s'", command );
    exit_status &= 0xFF;
        /* Apparently pclose sometimes returns 256 */
    if ( exit_status != 0 )
	error ( "exit status %d returned by `%s'",
		exit_status, command );
}


/* Skip whitespace in repository.  Return the next
 * character AFTER the current position (may be EOF).
 */
char skip ( FILE * repos )
{
    int c;

    while ( 1 )
    {
	c = fgetc ( repos );
	if ( ! isspace ( c ) )
	{
	    ungetc ( c, repos );
	    return c;
	}
	if ( c == '\n' ) ++ repos_line;
    }
}

/* Read an id into the global id buffer.  Whitespace
 * before id is ignored.  Id must exist.
 *
 * If the next non-whitespace character is a digit,
 * do not skip over it and record the id as "num".
 * We assume id must be all letters.  Its an error
 * if the id is too long to fit into buffer.
 */
void read_id ( FILE * repos )
{
    int c;
    char * p, * endp;

    skip ( repos );

    p = id;
    endp = id + sizeof ( id ) - 2;
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( ferror ( repos ) )
	    errorno ( "reading id from repository" );

	if ( p == id && isdigit ( c ) )
	{
	    ungetc ( c, repos );
	    strcpy ( id, "num" );
	    return;
	}
	if ( ! isalpha ( c ) )
	{
	    if ( p == id ) errornf ( "id" );
	    ungetc ( c, repos );
	    * p = 0;
	    return;
	}
	if ( p >= endp )
	{
	    * p = 0;
	    tprintf ( "* read id %s...\n", id );
	    error ( "id in repository too long" );
	}
	* p ++ = c;
    }
}

/* Read a natural number from repos to num.  Whitespace
 * before number is ignored.  Number must have at least
 * one digit and != NUMEND.
 */
void read_natural ( nat * natural, FILE * repos )
{
    int c;

    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        errorno ( "reading number in repository" );

    if ( ! isdigit ( c ) )
	errornf ( "number" );

    * natural = c - '0';
    while ( 1 )
    {
	nat old_natural = * natural;
	c = fgetc ( repos );
	if ( ! isdigit ( c ) ) break;
	* natural *= 10;
	* natural += c - '0';
	if ( old_natural > * natural
	     ||
	     * natural == NUMEND )
	    error ( "number in repository too large" );
    }
    if ( ferror ( repos ) )
        errorno ( "reading number in repository" );
    ungetc ( c, repos );
}


/* Read a num from repos.  Whitespace before num is
 * ignored.  Must have one component and not more than
 * fit in a num type.  All num elements are zeroed
 * before read, so after read at most one has the
 * value NUMEND.
 */
void read_num ( num n, FILE * repos )
{
    int c;
    int i, length;

    length = sizeof ( num ) / sizeof ( nat );
    for ( i = 0; i < length; ++ i ) n[i] = 0;

    skip ( repos );

    i = 0;
    while ( 1 )
    {
        if ( i >= length - 2 )
	    error ( "num in repository has too many"
	            " components" );
        c = fgetc ( repos );
	if ( ferror ( repos ) )
	    errorno ( "reading repository" );
	if ( ! isdigit ( c ) )
	    errornf ( "num" );
	ungetc ( c, repos );
	read_natural ( & n[i], repos );
	++i;

        c = fgetc ( repos );
	if ( c != '.' )
	{
	    ungetc ( c, repos );
	    break;
	}
    }
    n[i] = NUMEND;
}

/* Skip one string in repos.  Whitespace before string
 * is ignored.  String must exist.
 */
void skip_string ( FILE * repos )
{
    int c;
    int last_c = 0;

    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        errorno ( "reading repository" );
    if ( c != '@' )
	errornf ( "string" );
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( last_c == '@' && c != '@' )
	{
	    ungetc ( c, repos );
	    break;
	}
	else if ( last_c == '@' && c == '@' )
	    last_c = 0;
	else
	    last_c = c;

	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository string" );
	    else
		erroreof
		    ( "reading repository string" );
	}

	if ( c == '\n' ) ++ repos_line;
    }
}

/* Skip entry.  Specifically, skip to after next `;',
 * using skip_string to skip strings.  `;' must exist.
 */
void skip_entry ( FILE * repos )
{
    int c;

    while ( 1 )
    {
        c = fgetc ( repos );
	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository entry" );
	    else
		erroreof ( "reading repository entry" );
	}
	else if ( c == '@' )
	{
	    ungetc ( c, repos );
	    skip_string ( repos );
	}
	else if ( c == ';' )
	    break;

	if ( c == '\n' ) ++ repos_line;
    }
}

/* Copy a file from src to des, and stop on an EOF.
 * File names are used in error messages.
 */
void copy ( FILE * src, const char * srcname,
            FILE * des, const char * desname )
{
    int c;
    while ( ( c = fgetc ( src ) ) != EOF )
    {
        if ( fputc ( c, des ) == EOF )
	    errorno ( "writing to %s", desname );
    }
    if ( ferror ( src ) )
	errorno ( "reading from %s", srcname );
}

/* Copy the contents of a string from repos to des, and
 * stop after trailing @.  Skip whitespace before
 * beginning @.  String must exist.  Extra `@'s are not
 * copied.  File name is used in error messages.
 */
void copy_from_string
	( FILE * repos,
	  FILE * des, const char * desname )
{
    int c;
    int last_c = 0;

    /* Skip initial whitespace and @.
     */
    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        errorno ( "reading from repository" );
    if ( c != '@' )
	errornf ( "string" );

    /* Read and copy till after final @.
     */
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( last_c == '@' && c != '@' )
	{
	    ungetc ( c, repos );
	    break;
	}
	else if ( last_c == '@' && c == '@' )
	{
	    last_c = 0;
	    /* Fall through to copy '@' */
	}
	else if ( c == '@' )
	{
	    /* last_c != '@' */
	    last_c = c;
	    continue;
	}
	else
	    last_c = c;

	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository string" );
	    else
		erroreof
		    ( "reading repository string" );
	}

        if ( fputc ( c, des ) == EOF )
	    errorno ( "writing %s", desname );

	if ( c == '\n' ) ++ repos_line;
    }
}

/* Copy a file from src to a string in repos.  Begin the
 * string with "\n@" and end with "@\n".  Double @'s
 * in the file.  Filename is used for error messages.
 */
void copy_to_string ( FILE * src, const char * srcname,
                      FILE * repos )
{
    int c;

    if ( fputc ( '\n', repos ) == EOF )
        errorno ( "writing repository" );
    if ( fputc ( '@', repos ) == EOF )
        errorno ( "writing repository" );

    /* Read and copy till at EOF.
     */
    while ( ( c = fgetc ( src ) ) != EOF )
    {
        if ( fputc ( c, repos ) == EOF )
	    errorno ( "writing repository" );
        if ( c == '@' )
	{
	    if ( fputc ( c, repos ) == EOF )
		errorno ( "writing repository" );
	}
    }
    if ( ferror ( src ) )
	errorno ( "reading %s", srcname );

    if ( fputc ( '@', repos ) == EOF )
        errorno ( "writing repository" );
    if ( fputc ( '\n', repos ) == EOF )
        errorno ( "writing repository" );
}

/* Copy a string from repos to des.  String may be
 * preceded by whitespace.  A line feed will be
 * output before and after copied string.  Filename
 * is for error messages.
 */
void copy_string ( FILE * repos,
                   FILE * des, const char * desname )
{
    int c, last_c;

    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        errorno ( "reading repository" );
    if ( c != '@' )
	errornf ( "string" );
    if ( fputc ( '\n', des ) == EOF )
	errorno ( "writing %s", desname );
    if ( fputc ( '@', des ) == EOF )
	errorno ( "writing %s", desname );
    last_c = 0;
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( last_c == '@' && c != '@' )
	{
	    ungetc ( c, repos );
	    break;
	}
	else if ( last_c == '@' && c == '@' )
	    last_c = 0;
	else
	    last_c = c;

	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository" );
	    else
		erroreof
		    ( "reading repository string" );
	}
        if ( fputc ( c, des ) == EOF )
	    errorno ( "writing %s", desname );
    }
    if ( fputc ( '\n', des ) == EOF )
	errorno ( "writing %s", desname );
}


/* Copy from file src to file des editing it on the fly
 * using a diff -n listing in a string in repos.
 * File names are for error messages.
 */
void edit ( FILE * repos,
	    FILE * src, const char * srcname,
	    FILE * des, const char * desname )
{
    int c;

    int feeds = 0;
        /* Number of line feeds seen so far in src.
	 */

    int op;
    int commands_done = 0;
        /* Operation ('a' or 'd' or ending '@')
	 * and indication that ending '@' was
	 * found in an inner loop.
	 */
    nat location, count;
        /* Command is `op location count'
	 */

    /* Skip initial whitespace and @.
     */
    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        errorno ( "reading repository" );
    if ( c != '@' )
	errornf ( "string" );

    /* Read and execute repository commands.
     */
    while ( ! commands_done )
    {
	op = fgetc ( repos );
	if ( op == EOF )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository" );
	    else
		erroreof
		    ( "reading repository string" );
	}
	if ( op == '\n' ) ++ repos_line;
	if ( isspace ( op ) ) continue;

        if ( op == '@' ) break;
        if ( op != 'a' && op != 'd' )
	    errornf ( "edit command" );

	read_natural ( & location, repos );
	read_natural ( & count, repos );

	if ( count == 0 )
	    error ( "second command parameter == 0 in"
	            " repository" );
	if ( op == 'd' && location == 0 )
	    error ( "first delete parameter == 0 in"
	            " repository" );

	/* Skip to after end of line in repos
	 */
	while ( ( c = fgetc ( repos ) ) != '\n' )
	{
	    if ( ferror ( repos ) )
		errorno ( "reading repository" );
	    if ( feof ( repos ) )
		erroreof
		    ( "reading repository string" );
	    if ( isspace ( c ) ) continue;
	    error ( "extra stuff after command"
	            " in repository string" );
	}
	++ repos_line;

	if ( op == 'd' ) -- location;

	if ( location < feeds )
	    error ( "commands out of order in"
	            " repository" );

	/* Skip src to beginning of line indicated
	 * by command.
	 */
	while ( location > feeds )
	{
	    c = fgetc ( src );
	    if ( c == EOF )
	    {
		if ( ferror ( src ) )
		    errorno ( "reading %s", srcname );
		else
		    erroreof ( "reading %s", srcname );
	    }
	    if ( fputc ( c, des ) == EOF )
		errorno ( "writing %s", desname );
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
			errorno
			    ( "reading %s", srcname );
		    if ( at_line_beginning
		         ||
			 feeds + 1 < location )
			erroreof
			    ( "reading %s", srcname );
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
	     * repository string, so @@ copies to one @
	     * and a single @ ends repository command
	     * string.
	     */
	    int last_c = 0;
	    while ( count > 0 ) 
	    {
	        c = fgetc ( repos );
		if ( last_c == '@' && c != '@' )
		{
		    if ( count != 1 || c != EOF )
		        error ( "append command"
			        " prematurely"
				" terminated in"
				" repository" );
		    ungetc ( c, repos );
		    commands_done = 1;
		    break;
		}
		else if ( last_c == '@' && c == '@' )
		{
		    last_c = 0;
		    /* Fall though to copy '@'. */
		}
		else if ( c == '@' )
		{
		    /* last_c != '@' */
		    last_c = c;
		    continue;
		}
		else
		    last_c = c;

		if ( c == EOF )
		{
		    if ( ferror ( repos ) )
		        errorno
			    ( "reading repository" );
		    else
		        erroreof ( "reading repository"
			           " string" );
		}
		if ( fputc ( c, des ) == EOF )
		    errorno ( "writing %s", desname );
		if ( c == '\n' )
		    -- count, ++ repos_line;
	    }
	}
    }
    
    /* Copy rest of src.
     */
    copy ( src, srcname, des, desname );
}


/* Find and open the input repository file for a given
 * file.  If found repos is set to a read-only stream
 * for the file.  Otherwise repos_name is set to NULL.
 */
FILE * repos = NULL;
char * repos_name = NULL;
int repos_is_legacy = 0;
const char * repos_input_variants[4] = {
    "%s/LRCS/%s,V",   "%s/%s,V",
    "%s/RCS/%s,v",    "%s/%s,v" };
void find_repos ( const char * filename )
{
    char * dir, * base;
    int i;
    dir = strdup ( filename );
    base = strdup ( filename );
    dir = dirname ( dir );
    base = basename ( base );
    repos_name = (char *) malloc
        ( strlen ( filename ) + 40 );

    for ( i = 0; i < 4; ++ i )
    {
	sprintf ( repos_name, repos_input_variants[i],
	                      dir, base );
	repos = fopen ( repos_name, "r" );
	if ( repos != NULL )
	{
	    repos_is_legacy = ( i >= 2 );
	    tprintf ( "* opened input repository %s\n",
	              repos_name );
	    return;
	}
    }
    tprintf ( "* could not find repository\n" );
    repos_name = NULL;
}

/* Find and open the ouput repository file for a given
 * file.  When found new_repos is set to a write-only
 * stream for the file.  It is an error if not found.
 *
 * When complete, the new repository should be renamed
 * by just deleting the + from the end of its name.
 * This may or may not delete the input repository.
 */
FILE * new_repos = NULL;
char * new_repos_name = NULL;
const char * repos_output_variants[2] = {
    "%s/LRCS/%s,V+", "%s/%s,V+" };
void find_new_repos ( const char * filename )
{
    char * dir, * base;
    int i;
    dir = strdup ( filename );
    base = strdup ( filename );
    dir = dirname ( dir );
    base = basename ( base );
    new_repos_name = (char *) malloc
        ( strlen ( filename ) + 20 );

    for ( i = 0; i < 2; ++ i )
    {
	sprintf ( new_repos_name,
	          repos_output_variants[i], dir, base );
	new_repos = fopen ( new_repos_name, "w" );
	if ( new_repos != NULL )
	{
	    tprintf ( "* opened new output"
	              " repository %s\n",
	              new_repos_name );
	    return;
	}
    }
    error ( "could not open new output repository" );
}

/* Read times from the ,V file and build the revision
 * data base, but without any files.  It is an error
 * if repository header is empty.  Calls read_legacy_
 * header if repos_is_legacy.
 */
void read_legacy_header ( void );
void read_header ( void )
{
    int c;

    if ( repos_is_legacy )
    {
        read_legacy_header();
	return;
    }
    
    repos_line = 1;
    tprintf ( "* reading header from %s\n",
              repos_name );

    first_revision = NULL;
    last_revision = NULL;
    while ( 1 )
    {
	nat t;
	revision * next;

        skip ( repos );
	c = fgetc ( repos );
	if ( ferror ( repos ) )
	    errorno ( "reading repository" );
	ungetc ( c, repos );

	if ( ! isdigit ( c ) ) break;

	read_natural ( & t, repos );
	tprintf ( "* read %lu in header\n", t );

	next = (revision *) malloc
	    ( sizeof ( revision ) );
	next->previous = last_revision;
	next->next = NULL;
	next->filename = NULL;
	next->time = (time_t) t;
	if ( last_revision != NULL )
	    last_revision->next = next;
	if ( first_revision == NULL )
	    first_revision = next;
	last_revision = next;
    }
    tprintf ( "* done reading header\n" );
    if ( first_revision == NULL )
        error ( "the repository header is empty" );
}

/* Ditto for legacy repository
 */
void read_legacy_header ( void )
{
    revision * scan_revision = NULL;
    int head_count = 0;
    int next_count = 0;
    int date_count = 0;

    repos_line = 1;
    tprintf ( "* reading header from %s\n",
              repos_name );

    first_revision = NULL;
    last_revision = NULL;

    while ( 1 )
    {
        revision * next;

        read_id ( repos );

	if ( strcmp ( id, "head" ) == 0 )
	{
	    if ( head_count > 0 )
	        error ( "too many head entries in"
		        " repository" );
	    ++ head_count;
	}

	if ( strcmp ( id, "next" ) == 0 )
	{
	    if ( next_count > 0 )
	        error ( "too many next entries in"
		        " repository delta" );
	    ++ next_count;
	}

	if ( strcmp ( id, "date" ) == 0 )
	{
	    if ( date_count > 0 )
	        error ( "too many date entries in"
		        " repository delta" );
	    ++ date_count;
	}

	if ( ( strcmp ( id, "head" ) == 0
	       &&
	       first_revision == NULL )
	     ||
	     ( strcmp ( id, "next" ) == 0
	       &&
	       scan_revision != NULL ) )
	{
	    if ( isdigit ( skip ( repos ) ) )
	    {
		next = (revision *) malloc
		    ( sizeof ( revision ) );
		next->previous = last_revision;
		next->next = NULL;
		next->filename = NULL;
		next->time = 0;
		next->date[0] = NUMEND;
		if ( last_revision != NULL )
		    last_revision->next = next;
		if ( first_revision == NULL )
		    first_revision = next;
		last_revision = next;
		read_num ( next->rnum, repos );
		tprintf ( "* read %s %s\n", id,
		          num2str ( next->rnum ) );
	    }
	}
	else if ( strcmp ( id, "num" ) == 0 )
	{
	    num n;
	    read_num ( n, repos );
	    tprintf ( "* read num %s\n",
	              num2str ( n ) );

	    next_count = 0;
	    date_count = 0;

	    if ( last_revision != NULL
	         &&
		    numcmp ( n, last_revision->rnum )
		 == 0 )
	        scan_revision = last_revision;
	    else
	        scan_revision = NULL;

	    continue;
	}
	else if ( strcmp ( id, "date" ) == 0
	          &&
		  scan_revision != NULL )
	{
	    read_num ( scan_revision->date, repos );
	    tprintf ( "* read date %s\n",
	              num2str ( scan_revision->date ) );
	    if ( scan_revision->date[6] != NUMEND )
	        error ( "date with other than"
		        " 6 components" );
	}
	else if ( strcmp ( id, "desc" ) == 0 )
	{
	    skip_string ( repos );
	    break;
	}

	skip_entry ( repos );
    }
    tprintf ( "* done reading header\n" );
    if ( first_revision == NULL )
        error ( "the repository header is empty" );

    /* Convert dates to times. */

    {
        char * tz;
	revision * r;
	struct tm time;

        tz = getenv ( "TZ" );
	setenv ( "TZ", "UTC+0", 1 );
	tzset();

	r = first_revision;
	time.tm_isdst  = 0;
	while ( r != NULL )
	{
	    if ( r->date[0] == NUMEND )
	        error ( "no date for %s",
		         num2str ( r->rnum ) );
	    if ( r->date[0] < 70 )
	        r->date[0] += 2000;
	    else if ( r->date[0] < 100 )
	        r->date[0] += 1900;
	        /* Some older RCS files only record
		 * the last two digits of the year. */
	    time.tm_year = (int) r->date[0] - 1900;
	    time.tm_mon  = (int) r->date[1] - 1;
	    time.tm_mday = (int) r->date[2];
	    time.tm_hour = (int) r->date[3];
	    time.tm_min  = (int) r->date[4];
	    time.tm_sec  = (int) r->date[5];
	    r->time = mktime ( & time );
	    r = r->next;
	}

	if ( tz != NULL ) setenv ( "TZ", tz, 1 );
	else unsetenv ( "TZ" );
	tzset();
    }
}

/* Skip position in legacy repository to the text string
 * for the entry with num n.  The entry must exist.
 */
void num_skip ( num n )
{
    int found = 0;
    num m;
    while ( 1 )
    {
	read_id ( repos );
	if ( strcmp ( id, "num" ) == 0 )
	{
	    read_num ( m, repos );
	    found = ( numcmp ( m, n ) == 0 );
	}
	else if ( strcmp ( id, "text" ) == 0 )
	{
	    if ( found )
	    {
	        tprintf ( "* reading text %s\n",
		          num2str ( m ) );
		return;
	    }
	    else
	    {
	        tprintf ( "* skipping text %s\n",
		          num2str ( m ) );
		skip_string ( repos );
	    }
	}
	else 
	    skip_string ( repos );
    }
}

/* Move the current_revision pointer foward one revision
 * and creat the file of the new current_revision.
 * The file is named f,Vn+ where n is 1, 2, 3, ... for
 * the revisions in order.  If delete_previous is true,
 * the file of the previous revision is deleted and
 * its name is set to NULL.
 *
 * For a non-legacy repository, repos must be positioned
 * so the next thing to be read from it is the string
 * of the next revision.
 *
 * For a legacy repository, repos must be positioned so
 * that num_skip for the next revision's rnum will
 * place repos before the next revision's string.
 *
 * The previous revision, if it exists, must have its
 * filename set.
 *
 * If there is no next revision, this function sets
 * current_revision to NULL and does nothing else.
 */
revision * current_revision;
int current_index = 0;
    /* Number of current revision.  0 if none yet. */
void step_revision
        ( const char * filename, int delete_previous )
{
    revision * next_revision;
    FILE * des, * src;
    const char * desname, * srcname;

    if ( current_index == 0 )
        next_revision = first_revision;
    else if ( current_revision == NULL )
        return;
    else
    {
        next_revision = current_revision->next;
        srcname = current_revision->filename;
    }

    ++ current_index;
    if ( next_revision == NULL )
    {
        current_revision = NULL;
	return;
    }

    next_revision->filename = malloc
        ( strlen ( filename ) + 20 );
    sprintf ( next_revision->filename,
              "%s,V%d+", filename, current_index );
    desname = next_revision->filename;
    des = fopen ( desname, "w" );
    if ( des == NULL )
        errorno ( "could not open %s for writing",
	          desname );

    if ( repos_is_legacy )
        num_skip ( next_revision->rnum );

    if ( current_index == 1 )
        copy_from_string ( repos, des, desname );
    else
    {
        src = fopen ( srcname, "r" );
	if ( src == NULL )
	    errorno ( "could not open %s for reading",
	              srcname );
	tprintf ( "* editing %s to make %s\n",
	          srcname, desname );
	edit ( repos, src, srcname, des, desname );
	fclose ( src );
    }
    fclose ( des );
    tprintf ( "* wrote %s\n", desname );

    if ( delete_previous
         &&
	 current_index != 1 )
    {
	if ( unlink ( srcname ) < 0 )
	    errorno ( "cannot delete %s", srcname );
	tprintf ( "* deleted %s\n", srcname );
	current_revision->filename = NULL;
    }

    current_revision = next_revision;
}

/* Function executed on exit to remove temporary files.
 */
void cleanup ( void )
{
    revision * r;

    if ( new_repos_name != NULL )
    {
        if ( unlink ( new_repos_name ) < 0 )
	    errorno ( "cannot delete %s",
	              new_repos_name );
	tprintf ( "* deleted %s\n", new_repos_name);
    }
    r = first_revision;
    while ( r )
    {
	if ( r->filename != NULL )
	{
	    if ( unlink ( r->filename ) < 0 )
		errorno ( "cannot delete %s",
			  r->filename );
	    tprintf ( "* deleted %s\n", r->filename );
	}
	r = r->next;
    }
}

/* Find all repositories in the directory tree
 * rooted at '.' and perform the action: either
 * execute 'lrcs glob ...' or 'rm' for each
 * repository.
 */  
typedef enum { GLOB, REMOVE } action;
long for_all_repos_in_directory
        ( const char * directory,
	  action act, long mark );
void for_all_repos ( action act )
{
    for_all_repos_in_directory ( ".", act, 0 );
}
long for_all_repos_in_directory
        ( const char * directory,
	  action act, long mark )
{
    DIR * dir;
    struct dirent * ent;
    size_t s;
    char * name;
    struct stat status;

    assert ( sizeof ( ent->d_name ) >= 256 );
        /* Size should be 256; if its zero,
	 * things have changed. */

    tprintf ( "* searching directory %s\n",
              directory );

    dir = opendir ( directory );
    if ( dir == NULL )
        errorno ( "cannot open %s", directory );
    s = strlen ( directory );
    name = malloc ( s + 1 + sizeof(ent->d_name) );
    strcpy ( name, directory );
    name[s] = '/';
    ++ s;
    if ( strcmp ( directory, "." ) == 0 )
        name[0] = 0, s = 0;
	/* Names to lrcs glob must be clean because
	 * they are passed to git.
	 */
    while ( 1 )
    {
	size_t len;

	errno = 0;
        ent = readdir ( dir );
	if ( ent == NULL )
	{
	    if ( errno == 0 ) break;
	    else
	        errorno ( "reading %s", directory );
	}
	if ( strcmp ( ent->d_name, "." ) == 0 )
	    continue;
	if ( strcmp ( ent->d_name, ".." ) == 0 )
	    continue;
	strcpy ( name + s, ent->d_name );
	if ( stat ( name, & status ) < 0 )
	    errorno ( "getting status of %s", name );

	if ( S_ISDIR ( status.st_mode ) )
	{
	    mark = for_all_repos_in_directory
	        ( name, act, mark );
	    continue;
	}
	else if ( ! S_ISREG ( status.st_mode ) )
	    continue;

	len = strlen ( name );
	if ( strcmp ( name + len - 2, ",v" ) != 0
	     &&
	     strcmp ( name + len - 2, ",V" ) != 0 )
	    continue;
	if ( act == GLOB )
	{
	    FILE * glob;
	    char result[100];
	    char * endp;
	    int exit_status;

	    name[len-2] = 0;
	    init_command();
	    append ( "lrcs" );
	    if ( trace ) append ( "-t" );
	    append ( "glob" );
	    append_quoted ( name );
	    append_long ( mark );
	    glob = open_command ( "r" );
	    if ( fgets ( result, sizeof ( result ),
	                 glob ) == NULL )
	        errorno ( "reading output of %s",
		          command );
	    mark = strtol ( result, & endp, 10 );
	    if (    * endp != '\n'
	         || ! isdigit ( result[0] ) )
	        error ( "bad output '%s' from %s",
		        result, command );
	    close_command ( glob );
	}
	else
	{
	    if ( unlink ( name ) < 0 )
	        errorno ( "removing %s", name );
	}
    }
    free ( name );
    closedir ( dir );

    tprintf ( "* done searching directory %s\n",
              directory );

    return mark;
}

/* Index data base.
 */
typedef struct element {
    time_t time;
    long mark;
    char * filename;
} element;
element * index = NULL;
int index_length;
int index_size = 0;
    /* Size is allocated size, length is used length. */

/* Compare two index elements by time for qsort so 
 * index is sorted in ascending order by time.
 */
int time_compare ( const void * e1, const void * e2 )
{
    const element * E1 = (const element *) e1;
    const element * E2 = (const element *) e2;
    time_t diff = E1->time - E2->time;
    if ( diff < 0 ) return -1;
    else if ( diff > 0 ) return + 1;
    else return 0;
}

/* Read in file into index database.  Filename is for
 * error message.
 */
void read_index ( FILE * in, const char * filename )
{
    char * line = NULL;
    ssize_t line_length;
    size_t line_size = 0;
    index_length = 0;

    tprintf ( "* reading %s\n", filename );

    while ( 1 )
    {
	char * p, * endp;
	element * e;

        line_length = getline
	    ( & line, & line_size, in );
	if ( line_length < 0 )
	{
	    if ( ferror ( in ) )
	        errorno ( "error reading %s",
		          filename );
	    else break;  /* EOF */
	}

	if ( index_length >= index_size )
	{
	    index_size += 4096;
	    index = realloc
	        ( index,
		  index_size * sizeof ( element ) );
	    if ( index == NULL )
		errorno ( "while allocating memory" );
	}

	e = index + ( index_length ++ );
	p = line;
	e->time = (time_t) strtol ( p, & endp, 10 );
	if ( endp[0] != ':' )
	    error ( "badly formatted line in %s: %s",
	            filename, line );
	p = endp + 1;
	e->mark = strtol ( p, & endp, 10 );
	if ( endp[0] != ':' )
	    error ( "badly formatted line in %s: %s",
	            filename, line );
	p = endp + 1;

	if ( line[line_length-1] != '\n' )
	    error ( "missing line feed for line in %s:"
	            " %s", filename, line );
	line[line_length-1] = 0;
	e->filename = strdup ( p );
    }
}

int main ( int argc, char ** argv )
{
    const char * op, * filename, * s;
    revision * r;

    if ( argc >= 2 && strcmp ( argv[1], "-t" ) == 0 )
    {
        trace = 1;
	-- argc, ++ argv;
    }

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

    atexit ( cleanup );

    op = argv[1];

    if ( strcmp ( op, "git" ) == 0 )
    {
	FILE * git, * index;
	int exit_status;

	if ( argc > 4 ) error ( "too many arguments" );
	if ( argc != 2 )
	{
	    static const char * param[2] =
	        { "user.name", "user.email" };
	    const char * config[2];
	        /* config[i] is `user.<param[i]>'
		 * git configuration parameter. */
	    const char * committer, * email;
	    struct stat status;
	    int i;
	    if ( argc < 4 )
	        error ( "too few arguments" );
	    config[0] = committer = argv[2];
	    config[1] = email = argv[3];
	    if ( ! isalpha ( committer[0] ) )
		error ( "committer %s does not begin"
			" with a letter", committer );
	    if ( strchr ( email, '@' ) == NULL )
		error ( "email %s does not contain @",
			email );

	    if ( stat ( ".git", & status ) >= 0 )
		error ( ".git pre-exists;"
			" delete it first" );
	    if ( errno != ENOENT )
		errorno ( "stat'ing .git" );

	    init_command();
	    append ( "git init" );
	    git = open_command ( "w" );
	    close_command ( git );

	    for ( i = 0; i < 2; ++ i )
	    {
		init_command();
		append ( "git config" );
		append ( param[i] );
		append_quoted ( config[i] );
		git = open_command ( "w" );
		close_command ( git );
	    }
	}
	else
	{
	    struct stat status;

	    if ( stat ( ".git", & status ) < 0 )
	    {
		if ( errno != ENOENT )
		    errorno ( "stat'ing .git" );
		else
		    error ( ".git does not exist" );
	    }
	}

	tprintf ( "* deleting import,git and"
		  " index,git if they exist\n" );
	if ( unlink ( "import,git" ) < 0
	     &&
	     errno != ENOENT )
	    errorno ( "deleting import,git" );
	if ( unlink ( "index,git" ) < 0
	     &&
	     errno != ENOENT )
	    errorno ( "deleting index,git" );

	for_all_repos ( GLOB );

	index = fopen ( "index,git", "r" );
	if ( index == NULL )
	    errorno ( "cannot open file index,git"
	              " for reading" );
	read_index ( index, "index,git" );
	fclose ( index );

	qsort ( index, index_length, sizeof (element),
	        time_compare );

        exit ( 0 );
    }

    /* All the operations but "git" take a filename
     * argument.
     */
    if ( argc < 3 ) error ( "too few arguments" );
    filename = argv[2];
    find_repos ( filename );

    if ( strcmp ( op, "list" ) == 0 )
    {
	int i = 0;
	if ( argc > 3 ) error ( "too many arguments" );
        if ( repos == NULL )
	    error ( "there is no repository for %s",
	            filename );
	read_header();
	printf ( "%s:\n", repos_name );
	r = first_revision;
	while ( r )
	{
	    ++ i;
	    if ( repos_is_legacy )
		printf ( "%4d: %10s:   %s", i,
		         num2str ( r->rnum ),
			 ctime ( &r->time ) );
	    else
		printf ( "%4d: %s", i,
			 ctime ( &r->time ) );
	    r = r->next;

	}
	exit ( 0 );

    }
    else if ( strcmp ( op, "in" ) == 0 )
    {
	const char * s;
	revision * r;
	FILE * src, * diff;
	struct stat status;
	char * final_repos_name;
	char V;

	if ( argc > 3 ) error ( "too many arguments" );
        if ( repos != NULL )
	    read_header();

	src = fopen ( filename, "r" );
	if ( src == NULL )
	    errorno ( "cannot open file %s for reading",
	              filename );

	if ( fstat ( fileno ( src ), & status ) < 0 )
	    errorno ( "cannot stat file %s",
	              filename );

	find_new_repos ( filename );

	fprintf ( new_repos, "%ld\n",
	          (long) status.st_mtime );
	tprintf ( "* mod time of %s is %ld\n",
	          filename, (long) status.st_mtime );

	r = first_revision;
	    /* Header may be empty */
	while ( r )
	{
	    fprintf ( new_repos, "%ld\n",
	              (long) r->time );
	    r = r->next;

	}
	tprintf ( "* wrote header of"
	          " new repository\n" );

	copy_to_string ( src, filename, new_repos );
	tprintf ( "* copied %s to new repository\n",
	          filename );
	           
	fclose ( src );

	if ( repos_name != NULL )
	{
	    int c;
	    step_revision ( filename, 0 );

	    init_command();
	    append ( "diff -n" );
	    append_quoted ( filename );
	    append_quoted
	        ( current_revision->filename );
	    tprintf ( "* copying diff -n to"
	              " new repository\n" );
	    diff = open_command ( "r" );

	    c = fgetc ( diff );
	    if ( c == EOF )
	    {
		printf ( "lrcs: repository is"
			 " already up-to-date\n" );
		exit ( 0 );
	    }
	    ungetc ( c, diff );

	    copy_to_string
	        ( diff, "diff -n output", new_repos );
	    close_command ( diff );

	    if ( repos_is_legacy )
	    {
	        r = current_revision->next;
		while ( r )
		{
		    num_skip ( r->rnum );
		    copy_string
		        ( repos,
			  new_repos, new_repos_name );
		    tprintf ( "* copied %s to new"
		              " repository\n",
		              num2str ( r->rnum ) );
		    r = r->next;
		}
	    }
	    else
		copy ( repos, repos_name,
		       new_repos, new_repos_name );
	    fclose ( repos );
	}

	fclose ( new_repos );

	final_repos_name = strdup ( new_repos_name );
	final_repos_name[strlen(new_repos_name)-1] = 0;
	
	if ( rename ( new_repos_name, final_repos_name )
	     < 0 )
	    errorno ( "cannot rename %s to %s",
	              new_repos_name,
		      final_repos_name );
	tprintf ( "* renamed %s to %s\n",
	          new_repos_name, final_repos_name );
	new_repos_name = NULL;

	if ( repos_name != NULL )
	{
	    V = repos_name[strlen(repos_name)-1];
	    if ( V == 'V'
		 &&
		    strcmp ( repos_name,
		             final_repos_name )
		 != 0 )
	    {
	        if ( unlink ( repos_name ) < 0 )
		    errorno ( "cannot remove %s",
			      repos_name );
		tprintf ( "* removed %s\n",
		          repos_name );
	    }
	}
	exit ( 0 );
    }
    else if ( strcmp ( op, "out" ) == 0 )
    {
	long rev, i;
	char * final_name;
	char * name;
	struct utimbuf ut;
	struct stat status;

	if ( argc > 4 ) error ( "too many arguments" );
        if ( repos == NULL )
	    error ( "there is no repository for %s",
	            filename );
	read_header();

	if ( argc == 3 ) rev = 0;
	else
	{
	    char * endptr;
	    rev = strtol ( argv[3], & endptr, 10 );
	    if ( * endptr != 0 )
	        error ( "revision argument is not"
		        " integer" );
	    if ( rev < 0 )
	        error ( "revision argument is < 0" );
	}

	if ( rev == 0
	     &&
	     stat ( filename, & status ) >= 0 )
	    error ( "%s already exists", filename );

	i = 0;
	do step_revision ( filename, 1 );
	while ( ++ i < rev );
	    /* If rev == 0 or 1 this takes 1 step. */

	if ( current_revision == NULL )
	    error ( "revision argument too large" );

	name = current_revision->filename;
	if ( rev == 0 )
	    final_name = strdup ( filename );
	else
	{
	    final_name = strdup ( name );
	    final_name[strlen(name)-1] = 0;
	}

	ut.actime = time ( NULL );
	ut.modtime = current_revision->time;
	if ( utime ( name, & ut ) < 0 )
	    printf ( "WARNING: failed to set"
	             " modification time of %s\n",
	             name );
	
	if ( rename ( name, final_name )
	     < 0 )
	    errorno ( "cannot rename %s to %s",
	              name, final_name );
	tprintf ( "* renamed %s to %s\n",
	          name, final_name );
	current_revision->filename = NULL;

	exit ( 0 );
    }
    else if ( strcmp ( op, "diff" ) == 0 )
    {
	long rev[2];
	const char * file[2];
	int i, j, del;
	int diff_argc;
	FILE * diff;

	if ( argc == 3 || ! isdigit ( argv[3][0] ) )
	    rev[0] = 1, rev[1] = 0, diff_argc = 3;
	else
	{
	    char * endp;
	    diff_argc = 4;
	    rev[0] = strtol ( argv[3], & endp, 10 );
	    if ( * endp == 0 )
	        rev[1] = 0;
	    else if ( * endp != ':' )
	        error ( "bad argument %s", argv[3] );
	    else
	    {
	        rev[1] = strtol
		    ( endp + 1, & endp, 10 );
		if ( * endp != 0 )
		    error ( "bad argument %s",
		            argv[3] );
	    }
	    if ( rev[0] < 0 || rev[1] < 0 )
		error ( "bad argument %s", argv[3] );
	    if ( rev[0] == rev[1] )
	        error ( "comparing a file to itself" );
	}

        if ( repos == NULL )
	    error ( "there is no repository for %s",
	            filename );
	read_header();

	for ( i = 0; i < 2; ++ i )
	{
	    if ( rev[i] == 0 ) file[i] = filename;
	    else file[i] = NULL;
	}

	j = 0;
	del = 1;
	while ( file[0] == NULL || file[1] == NULL )
	{
	    ++ j;
	    step_revision ( filename, del );
	    if ( current_revision == NULL )
	        error ( "a revision number is too"
		        " large" );
	    del = 1;
	    for ( i = 0; i < 2; ++ i )
	    {
	        if ( j == rev[i] )
		{
		    file[i] =
		         current_revision->filename;
		    del = 0;
		}
	    }
	}

	init_command();
	append ( "diff" );
	if ( diff_argc >= argc )
	    append ( "-u" );
	else for ( i = diff_argc; i < argc; ++ i )
	    append_quoted ( argv[i] );
	append_quoted ( file[0] );
	append_quoted ( file[1] );
	append ( "| less -F" );
	printf ( "%s\n", command );
	diff = open_command ( "w" );
	close_command ( diff );

	exit ( 0 );
    }
    else if ( strcmp ( op, "glob" ) == 0 )
    {
	long mark;
	char * endp;
	FILE * import, * index, * src;
	struct stat status;

	if ( argc < 4 ) error ( "too few arguments" );
	if ( argc > 4 ) error ( "too many arguments" );
	mark = strtol ( argv[3], & endp, 10 );
	if ( * endp || mark < 0 )
	    error ( "%s is not a natural number"
	            " argument", argv[3] );
	read_header();

	import = fopen ( "import,git", "a" );
	if ( import == NULL )
	    errorno ( "could not open import,git"
	              " for appending" );

	index = fopen ( "index,git", "a" );
	if ( index == NULL )
	    errorno ( "could not open index,git"
	              " for appending" );

	tprintf ( "* begin appends to import,git"
	          " and index,git for %s\n",
		  filename );
	while ( 1 )
	{
	    step_revision ( filename, 1 );
	    if ( current_revision == NULL ) break;

	    src = fopen
	        ( current_revision->filename, "r" );
	    if ( src == NULL )
		errorno ( "cannot open file %s for"
		          " reading",
			  current_revision->filename );

	    if (   fstat ( fileno ( src ), & status )
	         < 0 )
		errorno ( "cannot stat file %s",
			  current_revision->filename );

	    ++ mark;
	    fprintf ( import, "blob\n" );
	    fprintf ( import, "mark :%ld\n", mark );
	    fprintf ( import, "data %ld\n",
	              (long) status.st_size );
		      /* off_t type is signed */
	    copy ( src, current_revision->filename,
	           import, "import,git" );
	    fprintf ( import, "\n" );
	    fclose ( src );

	    fprintf ( index, "%ld:%ld:%s\n",
	              (long) current_revision->time,
		      mark, filename );
	    tprintf ( "*     appended version with time"
	              " %ld and mark %ld\n",
		      current_revision->time, mark );
	}
	tprintf ( "* end appends to import,git"
	          " and index,git for %s\n",
		  filename );
	if ( ferror ( import ) )
	    error ( "error writing import,git" );
	if ( ferror ( index ) )
	    error ( "error writing index,git" );
	fclose ( import );
	fclose ( index );
	printf ( "%ld\n", mark );
	exit ( 0 );
    }
    else
        error ( "bad operation `%s'", op );
}
