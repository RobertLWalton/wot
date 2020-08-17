/* Light Revision Control System (Light-RCS)
**
** Author:	Bob Walton (walton@acm.org)
** File:	lrcs.c
** Date:	Mon Aug 17 04:40:41 EDT 2020
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
*/

#define _POSIX_C_SOURCE 2
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

/* Revisions are store in temporary files.
 */
typedef struct revision
{
    time_t time;
    char * filename;
    struct revision * next;
} revision;
revision * first_revision;

int repos_line = 0;
    /* Current line number in repository being read.
     * Output in error messages that complain about
     * repository being mis-formatted.
     */

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

/* Skip whitespace in repository.  Return NULL on
 * success or error message on failure.  Next
 * character is non-whitespace or EOF.
 */
const char * skip ( FILE * repos )
{
    int c;

    while ( ( c = fgetc ( repos ) ) != EOF )
    {
        if ( c == '\n' ) ++ repos_line;
	if ( isspace ( c ) ) continue;
	ungetc ( c, repos );
	return NULL;
    }
    if ( ferror ( repos ) )
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
    s = skip ( repos );
    if ( s != NULL ) return s;

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


/* Read a natural number from src to num.  Whilespace
 * before number is ignored.  Return NULL on success
 * (must have at least one digit), or error message
 * on failure.
 */
const char * get_number
	( unsigned long * num, FILE * repos )
{
    int c;
    const char * s;

    s = skip ( repos );
    if ( s != NULL ) return s;

    c = fgetc ( repos );
    if ( ferror ( repos ) )
        return strerror ( errno );

    if ( ! isdigit ( c ) )
	return "expected number not found in"
	       " repository";

    * num = c - '0';
    while ( ( c = fgetc ( repos ) ) != EOF )
    {
	unsigned long old_num = * num;
	if ( ! isdigit ( c ) ) break;
	* num *= 10;
	* num += c - '0';
	if ( old_num > * num )
	    return "number too large in repository";
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
        /* Operation ('a' or 'd' or ending '@')
	 * and indication that ending '@' was
	 * found in an inner loop.
	 */
    unsigned long location, count;
        /* Command is `op location count'
	 */

    /* Skip initial whitespace and @.
     */
    s = skip ( repos );
    if ( s != NULL ) return s;

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
	if ( repos != NULL ) return;
    }
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
	if ( new_repos != NULL ) return;
    }
    new_repos_name = NULL;
}

/* Read times from the ,V file and build the revision
 * data base, but without any files.  Returns NULL on
 * success and error message on failure.
 */
const char * read_header ( void )
{
    revision * last_revision = NULL;
    int c;
    const char * s;

    repos_line = 1;
    sprintf ( context, "while reading %s", repos_name );

    first_revision = NULL;
    while ( 1 )
    {
	unsigned long t;
	revision * next;

        s = skip ( repos );
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

	s = get_number ( & t, repos );
	if ( s != NULL ) return s;
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
    if ( first_revision == NULL )
        return "the repository header is empty";
    return NULL;
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
 */
revision * current_revision;
int current_index = 0;
    /* Number of current revision.  0 if none yet. */
const char * step_revision
        ( const char * filename, int delete_previous )
{
    revision * next_revision;
    const char * s;
    FILE * des, * src;

    if ( current_index == 0 )
        next_revision = first_revision;
    else
        next_revision = current_revision -> next;

    ++ current_index;
    if ( next_revision == NULL )
    {
        current_revision = NULL;
	return NULL;
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
    if ( current_index == 1 )
        copy_from_string ( repos, des );
    else
    {
        src = fopen
	    ( current_revision->filename, "r" );
	if ( src == NULL )
	    error ( "could not open %s for reading",
	             current_revision->filename );
	s = edit ( repos, src, des );
	if ( s != NULL ) error ( s );
	fclose ( src );
    }
    fclose ( des );

    if ( delete_previous
         &&
	 current_revision != NULL )
    {
	if ( unlink ( current_revision->filename ) < 0 )
	    error ( "cannot delete %s",
		    current_revision->filename );
	else
	    current_revision->filename = NULL;
    }

    current_revision = next_revision;
    return NULL;
}

/* Function executed on exit to remove temporary files.
 */
void cleanup ( void )
{
    revision * r;

    if ( new_repos_name != NULL )
        unlink ( new_repos_name );
    r = first_revision;
    while ( r )
    {
	if ( r->filename != NULL )
	    unlink ( r->filename );
	r = r->next;
    }
}

int main ( int argc, char ** argv )
{
    const char * op, * filename, * s;
    revision * r;
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

	r = first_revision;
	    /* Header may be empty */
	while ( r )
	{
	    fprintf ( new_repos, "%d\n", r->time );
	    r = r->next;

	}

	sprintf ( context,
	          "copying file to new repository" );
	s = copy_to_string ( src, new_repos );
	if ( s != NULL ) error ( s );
	fclose ( src );

	if ( repos_name != NULL )
	{
	    int c;
	    sprintf ( context,
		      "copying old revision 1 to"
		      " temporary file" );
	    s = step_revision ( filename, 0 );
	    if ( s != NULL ) error ( s );
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

	    s = copy ( repos, new_repos );
	    if ( s != NULL ) error ( s );
	    fclose ( repos );
	}

	fclose ( new_repos );

	final_repos_name = strdup ( new_repos_name );
	final_repos_name[strlen(new_repos_name)-1] = 0;
	
	if ( rename ( new_repos_name, final_repos_name )
	     < 0 )
	    error ( "cannot rename %s to %s",
	            new_repos_name, final_repos_name );
	new_repos_name = NULL;

	if ( repos_name != NULL )
	{
	    V = repos_name[strlen(repos_name)-1];
	    if ( V == 'V'
		 &&
		    strcmp ( repos_name,
		             final_repos_name )
		 != 0
		 &&
		 unlink ( repos_name ) < 0 )
		error ( "cannot remove %s",
		        repos_name );
	}
    }
}
