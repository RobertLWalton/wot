/* Light Revision Control System (Light-RCS)
**
** Author:	Bob Walton (walton@acm.org)
** File:	lrcs.c
** Date:	Mon Aug 17 21:51:25 EDT 2020
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
#include <unistd.h>
#include <errno.h>
#include <assert.h>

const char * documentation[] = {
"lrcs -doc",
"lrcs [-t] in file",
"lrcs [-t] out file [revision]",
"lrcs [-t] diff file [revision] [diff-option...]",
"lrcs [-t] diff file revision:revision"
				" [diff-option...]",
"lrcs [-t] list file",
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

int trace = 0;   /* Set true by -t */
#define tprintf if ( trace ) printf

typedef unsigned long nat;
    /* Natural number.  Longest that can be read
     * in C90.
     */

int repos_line = 0;
    /* Current line number in repository being read.
     * Output in error messages that complain about
     * repository being mis-formatted.
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
 * given num as its rnum and sets scan_revision = NULL.
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
 * The following apply during body scan.  scan_
 * revision is initialized to first_revision.
 *
 * A `num' entry that does not match scan_revision->rnum
 * skips to the next `num' entry.
 *
 * A `num' entry that matches scan_revision->rnum sets
 * scan_revision = scan_revision->next and skips to
 * the string part of the next `text string' entry.
 */
typedef nat num[100];
    /* Encodes num[0].num[1].num[2]. ..., which ends
     * with first element that is 0.
     */
char id[100];  	/* entry id */

/* Return -1 if n1 < n2, 0 if n1 == n2, +1 if n1 > n2.
 */
int numcmp ( num n1, num n2 )
{
    int i;
    while ( 1 )
    {
        if ( n1[i] < n2[i] ) return -1;
	else if ( n1[i] > n2[i] ) return +1;
	else if ( n1[i] == 0 ) return 0;
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
    if ( n[0] == 0 )
        sprintf ( p, "missing-num" );
    else
    {
	p += sprintf ( p, "%lu", n[0] );
	i = 1;
	while ( n[i] != 0 )
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
    struct revision * next;

    /* Legacy info */

    num rnum;
    num date;
} revision;
revision * first_revision = NULL;

char * context = "";
void error ( const char * fmt, ... )
{
    va_list ap;
    va_start ( ap, fmt );

    fprintf ( stderr, "lrcs: error %s\n", context );
    fprintf ( stderr, "      " );
    vfprintf ( stderr, fmt, ap );
    fprintf ( stderr, "\n" );
    if ( repos_line > 0 )
	fprintf ( stderr, "      repository is at line"
	                  " %d\n", repos_line );
    exit ( 1 );
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
 * before id is ignored.  Return NULL on success
 * or error message on failure.
 *
 * If the next non-whitespace character is a digit,
 * do not skip over it and record the id as "num".
 * We assume id must be all letters.  Its an error
 * if the id is too long to fit into buffer.
 */
const char * read_id ( FILE * repos )
{
    const char * s;
    int c;
    char * p, * endp;

    skip ( repos );

    p = id;
    endp = id + sizeof ( id ) - 2;
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	}
	if ( p == id && isdigit ( c ) )
	{
	    ungetc ( c, repos );
	    strcpy ( id, "num" );
	    return NULL;
	}
	if ( ! isalpha ( c ) )
	{
	    if ( p == id )
	        return "bad id";
	    ungetc ( c, repos );
	    * p = 0;
	    return NULL;
	}
	if ( p >= endp )
	{
	    * p = 0;
	    tprintf ( "* read id %s\n", id );
	    return "id too long";
	}
	* p ++ = c;
    }
}

/* Read a natural number from repos to num.  Whitespace
 * before number is ignored.  Return NULL on success
 * (must have at least one digit), or error message
 * on failure.
 */
const char * read_natural
	( nat * natural, FILE * repos )
{
    int c;
    const char * s;

    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        return strerror ( errno );

    if ( ! isdigit ( c ) )
	return "expected number not found in"
	       " repository";

    * natural = c - '0';
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	nat old_natural = * natural;
	if ( ! isdigit ( c ) ) break;
	* natural *= 10;
	* natural += c - '0';
	if ( old_natural > * natural )
	    return "number too large in repository";
    }
    if ( ferror ( repos ) )
        return strerror ( errno );
    ungetc ( c, repos );
    return NULL;
}


/* Read a num from repos.  Whitespace before num is
 * ignored.  Return NULL on success, or error message
 * on failure.  Must have one element and not more
 * than fit in a num type.
 */
const char * read_num ( num n, FILE * repos )
{
    int c;
    const char * s;
    int i, length;

    length = sizeof ( num ) / sizeof ( int );

    skip ( repos );

    i = 0;
    while ( 1 )
    {
        if ( i >= length - 2 )
	    return "too many elements in num";
        c = fgetc ( repos );
	if ( ! isdigit ( c ) )
	    return "expected num element not found";
	ungetc ( c, repos );
	s = read_natural ( & n[i], repos );
	if ( s != NULL ) return s;
	if ( n[i] == 0 )
	    return "0 num element found";
	++i;

        c = fgetc ( repos );
	if ( c != '.' )
	{
	    ungetc ( c, repos );
	    break;
	}
    }
    n[i] = 0;
    return NULL;
}

/* Skip one string in repos.  Whitespace before string
 * is ignored.  Return NULL on success, or error message
 * on failure.
 */
const char * skip_string ( FILE * repos )
{
    int c;
    const char * s;

    skip ( repos );

    c = fgetc ( repos );
    if ( c != '@' )
        return "expected string not found";
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	}
	if ( c != '@' ) continue;
	c = fgetc ( repos );
	if ( c == '@' ) continue;
	ungetc ( c, repos );
	return NULL;
    }
}

/* Skip entry.  Specifically, skip to after next `;',
 * using skip_string to skip strings.  Return NULL on
 * success, or error message on failure.
 */
const char * skip_entry ( FILE * repos )
{
    int c;
    const char * s;

    while ( 1 )
    {
        c = fgetc ( repos );
	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	}
	else if ( c == '@' )
	{
	    ungetc ( c, repos );
	    s = skip_string ( repos );
	    if ( s != NULL ) return s;
	}
	else if ( c == ';' ) return NULL;
    }
}

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
    const char * s;

    /* Skip initial whitespace and @.
     */
    skip ( repos );

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        return strerror ( errno );
    if ( c != '@' )
	return "expected string not found in"
	       " repository";

    /* Read and copy till after final @.
     */
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
        if ( c == '@' )
	{
	    c = fgetc ( repos );
	    if ( c != '@' )
	    {
		if ( c != EOF )
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

    /* Read and copy till at EOF.
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

/* Copy a string from repos to des.  String may be
 * preceded by whitespace.  A line feed will be
 * output before and after copied string.  Returns
 * NULL on success and error message on failure.
 */
const char * copy_string ( FILE * repos, FILE * des )
{
    int c, last_c;

    skip ( repos );

    c = fgetc ( repos );
    if ( c != '@' )
	return "expected string not found in"
	       " repository";
    if ( fputc ( '\n', des ) == EOF )
	return strerror ( errno );
    if ( fputc ( '@', des ) == EOF )
	return strerror ( errno );
    last_c = 0;
    while ( 1 )
    {
        c = fgetc ( repos );
	if ( last_c == '@' && c != '@' )
	    break;
	else if ( last_c == '@' && c == '@' )
	    last_c = 0;
	else
	    last_c = c;

	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	}
        if ( fputc ( c, des ) == EOF )
	    return strerror ( errno );
    }
    if ( fputc ( '\n', des ) == EOF )
	return strerror ( errno );
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
    if ( ferror ( src ) )
        return strerror ( errno );
    if ( c != '@' )
	return "expected string not found in"
	       " repository";

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

	s = read_natural ( & location, repos );
	if ( s != NULL ) return s;
	s = read_natural ( & count, repos );
	if ( s != NULL ) return s;

	if ( count == 0 )
	    return "second command parameter == 0 in"
	           " repository";

	/* Skip to after end of line in repos
	 */
	while ( ( c = fgetc ( repos ) ) != '\n' )
	{
	    if ( ferror ( repos ) )
		return strerror ( errno );
	    if ( feof ( repos ) )
	        return "unexpected end of file"
		       " in repository";
	    if ( isspace ( c ) ) continue;
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
	     * repository string, so @@ copies to one @
	     * and a single @ ends command string.
	     */
	    while ( count > 0 ) 
	    {
	        c = fgetc ( repos );
		if ( c == EOF )
		{
		    if ( ferror ( repos ) )
			return strerror ( errno );
		    if ( feof ( repos ) )
			return "unexpected end of file"
			       " in repository";
		    break;
		}
		if ( c == '@' )
		{
		    c = fgetc ( repos );
		    if ( c != '@' )
		    {
			if ( ferror ( repos ) )
			    return strerror ( errno );
			if ( c != EOF )
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
	}
    }
    
    /* Copy rest of src.
     */
    return copy ( src, des );
}

/* Find and open the input repository file for a given
 * file.  If found repos is set to a read-only stream
 * for the file.  Otherwise repos_name is set to NULL.
 */
FILE * repos = NULL;
char * repos_name = NULL;
int repos_is_legacy = 0;
const char * repos_input_variants[4] = {
    "%s/%s,V", "%s/LRCS/%s,V",
    "%s/%s,v", "%s/LRCS/%s,v" };
void find_repos ( const char * filename )
{
    size_t len;
    char * dir, * base;
    int i;
    len = strlen ( filename );
    dir = strdup ( filename );
    base = strdup ( filename );
    dir = dirname ( dir );
    base = basename ( base );
    repos_name = (char *) malloc
        ( strlen ( filename ) + 20 );

    for ( i = 0; i < 4; ++ i )
    {
	sprintf ( repos_name, repos_input_variants[i],
	                      dir, base );
	repos = fopen ( repos_name, "r" );
	if ( repos != NULL )
	{
	    repos_is_legacy = ( i >= 2 );
	    tprintf ( "* found repository %s\n",
	              repos_name );
	    return;
	}
    }
    tprintf ( "* could not find repository\n" );
    repos_name = NULL;
}

/* Find and open the ouput repository file for a given
 * file.  If found new_repos is set to a write-only
 * stream for the file.  Otherwise new_repos is set to
 * NULL.  When complete, the new repository is renamed
 * by just deleting the + from the end of its name.
 * This may or may not delete the input repository.
 */
FILE * new_repos = NULL;
char * new_repos_name = NULL;
const char * repos_output_variants[2] = {
    "%s/LRCS/%s,V+", "%s/%s,V+" };
void find_new_repos ( const char * filename )
{
    size_t len;
    char * dir, * base;
    int i;
    len = strlen ( filename );
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
	    tprintf ( "* opened new repository %s\n",
	              new_repos_name );
	    return;
	}
    }
    tprintf ( "* could not open new repository\n" );
    new_repos_name = NULL;
}

/* Read times from the ,V file and build the revision
 * data base, but without any files.  Returns NULL on
 * success and error message on failure.
 */
const char * read_legacy_header ( void );
const char * read_header ( void )
{
    revision * last_revision = NULL;
    int c;
    const char * s;

    if ( repos_is_legacy )
        return read_legacy_header();
    
    repos_line = 1;
    sprintf ( context, "while reading %s", repos_name );
    tprintf ( "* reading header from %s\n",
              repos_name );

    first_revision = NULL;
    while ( 1 )
    {
	nat t;
	revision * next;

        skip ( repos );
	c = fgetc ( repos );
	if ( c == EOF )
	{
	    if ( ferror ( repos ) )
	        return strerror ( errno );
	    else
	        return "unexpected EOF in repository";
	}
	ungetc ( c, repos );

	if ( ! isdigit ( c ) ) break;

	s = read_natural ( & t, repos );
	if ( s != NULL ) return s;
	tprintf ( "* read %d in header\n", t );

	next = (revision *) malloc
	    ( sizeof ( revision ) );
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
        return "the repository header is empty";
    return NULL;
}

/* Ditto for legacy repository
 */
const char * read_legacy_header ( void )
{
    revision * last_revision = NULL,
             * scan_revision = NULL;
    const char * s;

    repos_line = 1;
    sprintf ( context, "while reading %s", repos_name );
    tprintf ( "* reading header from %s\n",
              repos_name );

    first_revision = NULL;

    while ( 1 )
    {
        revision * next;

        s = read_id ( repos );
	if ( s != NULL ) return s;

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
		next->next = NULL;
		next->filename = NULL;
		next->time = 0;
		if ( last_revision != NULL )
		    last_revision->next = next;
		if ( first_revision == NULL )
		    first_revision = next;
		last_revision = next;
		s = read_num ( next->rnum, repos );
		if ( s != NULL ) return s;
		tprintf ( "* read %s %s\n", id,
		          num2str ( next->rnum ) );
	    }
	}
	else if ( strcmp ( id, "num" ) == 0 )
	{
	    num n;
	    s = read_num ( n, repos );
	    if ( s != NULL ) return s;
	    tprintf ( "* read num %s\n",
	              num2str ( n ) );

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
	    s = read_num ( next->date, repos );
	    if ( s != NULL ) return s;
	    tprintf ( "* read date %s\n",
	              num2str ( next->date ) );
	}
	else if ( strcmp ( id, "desc" ) == 0 )
	{
	    s = skip_string ( repos );
	    if ( s != NULL ) return s;
	    break;
	}

	s = skip_entry ( repos );
	if ( s != NULL ) return s;
    }
    tprintf ( "* done reading header\n" );
    if ( first_revision == NULL )
        return "the repository header is empty";

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
	    time.tm_year = (int) r->date[0] - 1900;
	    time.tm_mon  = (int) r->date[1];
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

    return NULL;
}

/* Skip position in legacy repository to the text string
 * for the entry with num n.
 */
const char * num_skip ( num n )
{
    const char * s;
    int found = 0;
    num m;
    while ( 1 )
    {
	s = read_id ( repos );
	if ( s != NULL ) return s;
	if ( strcmp ( id, "num" ) == 0 )
	{
	    s = read_num ( m, repos );
	    if ( s != NULL ) return s;
	    found = ( numcmp ( m, n ) == 0 );
	}
	else if ( strcmp ( id, "text" ) == 0 )
	{
	    if ( found )
	    {
	        tprintf ( "* reading text %s\n",
		          num2str ( m ) );
		return NULL;
	    }
	    else
	    {
	        tprintf ( "* skipping text %s\n",
		          num2str ( m ) );
		s = skip_string ( repos );
		if ( s != NULL ) return s;
	    }
	}
	else 
	{
	    s = skip_string ( repos );
	    if ( s != NULL ) return s;
	}
    }
}

/* Move the current_revision pointer foward one revision
 * and creat the file of the new current_revision.
 * The file is named f,Vn where n is 1, 2, 3, ... for
 * the revisions in order.  If delete_previous is true,
 * the file of the previous revision is deleted and
 * its name is set to NULL.
 *
 * repos must be positioned so the next thing to be
 * read from it is the string of the next revision,
 * and the previous revision, if it exists, must
 * have its filename set.
 *
 * If there is no next revision, this function sets
 * current_revision to NULL.
 *
 * Errors call the error function.
 */
revision * current_revision;
int current_index = 0;
    /* Number of current revision.  0 if none yet. */
void step_revision
        ( const char * filename, int delete_previous )
{
    revision * next_revision;
    const char * s;
    FILE * des, * src;

    if ( current_index == 0 )
        next_revision = first_revision;
    else
        next_revision = current_revision->next;

    ++ current_index;
    if ( next_revision == NULL )
    {
        current_revision = NULL;
	return;
    }

    sprintf ( context,
	      "copying revision %d to file",
	      current_index );

    next_revision->filename = malloc
        ( strlen ( filename ) + 10 );
    sprintf ( next_revision->filename,
              "%s,V%d+", filename, current_index );
    des = fopen ( next_revision->filename, "w" );
    if ( des == NULL )
        error ( "could not open %s for writing",
	        next_revision->filename );

    if ( repos_is_legacy )
    {
        s = num_skip ( next_revision->rnum );
	if ( s != NULL ) error ( s );
    }

    if ( current_index == 1 )
        copy_from_string ( repos, des );
    else
    {
        src = fopen
	    ( current_revision->filename, "r" );
	if ( src == NULL )
	    error ( "could not open %s for reading",
	             current_revision->filename );
	tprintf ( "* editing %s\n",
	           current_revision->filename );
	s = edit ( repos, src, des );
	if ( s != NULL ) error ( s );
	fclose ( src );
    }
    fclose ( des );
    tprintf ( "* wrote %s\n", next_revision->filename );

    if ( delete_previous
         &&
	 current_revision != NULL )
    {
	if ( unlink ( current_revision->filename ) < 0 )
	    error ( "cannot delete %s",
		    current_revision->filename );
	tprintf ( "* deleted %s\n",
	          current_revision->filename );
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
        unlink ( new_repos_name );
	tprintf ( "* deleted %s\n", new_repos_name);
    }
    r = first_revision;
    while ( r )
    {
	if ( r->filename != NULL )
	{
	    unlink ( r->filename );
	    tprintf ( "* deleted %s\n",
		      r->filename );
	}
	r = r->next;
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

    if ( argc < 3
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
    filename = argv[2];
    context = (char *) malloc
        ( strlen ( filename ) + 100 );
    find_repos ( filename );

    if ( strcmp ( op, "list" ) == 0 )
    {
	int i = 0;
	if ( argc > 3 ) error ( "too many arguments" );
        if ( repos == NULL )
	    error ( "there is no repository for %s",
	            filename );
	s = read_header();
	if ( s != NULL ) error ( s );
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
	char * command;
	char * final_repos_name;
	char V;

	if ( argc > 3 ) error ( "too many arguments" );
        if ( repos != NULL )
	{
	    s = read_header();
	    if ( s != NULL ) error ( s );
	}
	sprintf ( context,
	          "reading file %s", filename );
	src = fopen ( filename, "r" );
	if ( src == NULL )
	    error ( "cannot open file %s for reading",
	            filename );

	if ( fstat ( fileno ( src ), & status ) < 0 )
	    error ( "cannot stat file %s",
	            filename );

	sprintf ( context,
	          "writing new repository header" );
	find_new_repos ( filename );
	if ( new_repos == NULL )
	    error ( "cannot open new repository" );

	fprintf ( new_repos, "%d\n", status.st_mtime );
	tprintf ( "* mod time of %s is %d\n",
	          filename, status.st_mtime );

	r = first_revision;
	    /* Header may be empty */
	while ( r )
	{
	    fprintf ( new_repos, "%d\n", r->time );
	    r = r->next;

	}
	tprintf ( "* wrote header of"
	          " new repository\n" );

	sprintf ( context,
	          "copying file to new repository" );
	s = copy_to_string ( src, new_repos );
	if ( s != NULL ) error ( s );
	tprintf ( "* copied %s to new repository\n",
	          filename );
	           
	fclose ( src );

	if ( repos_name != NULL )
	{
	    int c;
	    sprintf ( context,
		      "copying old revision 1 to"
		      " temporary file" );
	    step_revision ( filename, 0 );
	    sprintf ( context,
		      "diffing old and new"
		      " revision 1" );

	    command = (char *) malloc 
		( 2 * strlen ( filename ) + 100 );
	    sprintf ( command, "diff -n %s %s",
		      filename,
		      current_revision->filename );
	    diff = popen ( command, "r" );
	    if ( diff == NULL )
		error ( "cannot execute"
		        " diff -n command" );

	    c = fgetc ( diff );
	    if ( c == EOF )
	    {
		printf ( "lrcs: repository is"
			 " already up-to-date\n" );
		exit ( 0 );
	    }

	    ungetc ( c, diff );

	    s = copy_to_string  ( diff, new_repos );
	    if ( s != NULL ) error ( s );
	    pclose ( diff );
	    tprintf ( "* copied diff -n %s %s to"
	              " new repository\n",
		      filename,
		      current_revision->filename );

	    if ( repos_is_legacy )
	    {
	        r = current_revision->next;
		while ( r )
		{
		    s = num_skip ( r->rnum );
		    if ( s != NULL ) error ( s );
		    s = copy_string
		        ( repos, new_repos );
		    if ( s != NULL ) error ( s );
		    tprintf ( "* copied %s to new"
		              " repository\n",
		              num2str ( r->rnum ) );
		    r = r->next;
		}
	    }
	    else
	    {
		s = copy ( repos, new_repos );
		if ( s != NULL ) error ( s );
	    }
	    fclose ( repos );
	}

	fclose ( new_repos );

	final_repos_name = strdup ( new_repos_name );
	final_repos_name[strlen(new_repos_name)-1] = 0;
	
	if ( rename ( new_repos_name, final_repos_name )
	     < 0 )
	    error ( "cannot rename %s to %s",
	            new_repos_name, final_repos_name );
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
		    error ( "cannot remove %s",
			    repos_name );
		tprintf ( "* removed %s\n",
		          repos_name );
	    }
	}
	exit ( 0 );
    }
    else if ( strcmp ( op, "out" ) == 0 )
    {
	long rev;
	int i;
	char * final_name;
	char * name;

	if ( argc > 4 ) error ( "too many arguments" );
        if ( repos == NULL )
	    error ( "there is no repository for %s",
	            filename );
	s = read_header();
	if ( s != NULL ) error ( s );

	if ( argc == 3 ) rev = 1;
	else
	{
	    char * endptr;
	    rev = strtol ( argv[3], & endptr, 10 );
	    if ( * endptr != 0 )
	        error ( "revision argument is not"
		        " integer" );
	    if ( rev <= 0 )
	        error ( "revision argument is <= 0" );
	}

	while ( rev -- )
	    step_revision ( filename, 1 );

	name = current_revision->filename;
	final_name = strdup ( name );
	final_name[strlen(name)-1] = 0;
	
	if ( rename ( name, final_name )
	     < 0 )
	    error ( "cannot rename %s to %s",
	            name, final_name );
	tprintf ( "* renamed %s to %s\n",
	          name, final_name );
	current_revision->filename = NULL;

	exit ( 0 );
    }
}
