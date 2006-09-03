/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	efm.c
** Date:	Sun Sep  3 05:07:13 EDT 2006
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2006/09/03 09:06:52 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.52 $
*/

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
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED
#include <time.h>

char documentation [] =
"efm -doc\n"
"\n"
"efm moveto target file ...\n"
"efm movefrom source file ...\n"
"efm copyto target file ...\n"
"efm copyfrom source file ...\n"
"efm check source file ...\n"
"efm remove target file ...\n"
"\n"
"efm list [file ...]\n"
"efm listkeys [file ...]\n"
"efm listfiles [file ...]\n"
"efm md5check file ...\n"
"\n"
"efm start\n"
"efm kill\n"
"\n"
"efm trace on\n"
"efm trace off\n"
"efm trace\n"
"\n"
"efm listall [file ...]\n"
"efm listallkeys [file ...]\n"
"efm listcurfiles [file ...]\n"
"efm listobsfiles [file ...]\n"
"efm listallfiles [file ...]\n"
"\n"
"efm cur file ...\n"
"efm obs file ...\n"
"efm add file ...\n"
"efm sub file ...\n"
"efm del source file ...\n"
"\f\n"
"    A file in the current directory may have an en-\n"
"    crypted version in the target/source directory.\n"
"    The file can be moved or copied to/from that\n"
"    directory.  It is encrypted when moved or copied\n"
"    to the target directory, and decrypted when\n"
"    moved or copied from the source directory.  The\n"
"    target/source directory may be \".\" to encrypt\n"
"    or decrypt in place.\n"
"\n"
"    The \"remove\" command is like \"movefrom\" fol-\n"
"    lowed by discarding the decrypted file.  The\n"
"    \"check\" command is like \"copyfrom\" followed\n"
"    by discarding the decrypted file.  Neither of\n"
"    these two commands affects any existing de-\n"
"    crypted file.\n"
"\n"
"    File names must not contain any '/'s (files must\n"
"    be in the current directory).  Source and target\n"
"    names can be any directory names acceptable to\n"
"    scp.  Efm makes temporary files in the current\n"
"    directory whose base names are the 32 character\n"
"    MD5 sums of the decrypted files.  No two encryp-\n"
"    ted files may have the same MD5 sum.  It is ex-\n"
"    pected that files will be tar files of director-\n"
"    ies.\n"
"\n"
"    Efm maintains an index of encrypted files.  This\n"
"    index is itself encrypted, and is stored in the\n"
"    file named \"EFM-INDEX.gpg\".  You must create\n"
"    and encrypt an initial empty index by executing:\n"
"\n"
"               echo > EFM-INDEX\n"
"               gpg -c EFM-INDEX\n"
"\n"
"    You choose the password that protects the index\n"
"    when you do this.  You may put comments at the\n"
"    beginning of EFM-INDEX before you encrypt.  All\n"
"    comment lines must have `#' as their first char-\n"
"    acter, and there can be no blank lines.\n"
"\f\n"
"    The index is REQUIRED to decrypt files, as each\n"
"    encrypted file has its own unique random encryp-\n"
"    tion key listed in the index.  If a user wants\n"
"    to change the password used to protect the en-\n"
"    cryted files, the user changes just the password\n"
"    of the index, and not the keys encrypting the\n"
"    files.\n"
"\n"
"    Once listed in the index, files are not normally\n"
"    removed from the index.  Instead index entries\n"
"    are marked as being either current or obsolete.\n"
"    The \"moveto\" and \"copyto\" commands make a\n"
"    file's entry current.  The \"movefrom\" and"
					" \"re-\n"
"    move\" commands make a file's entry obsolete.\n"
"    The \"copyfrom\" command does not change index.\n"
"\n"
"    The \"list\" command lists for current index\n"
"    entries the file name, protection mode, modifi-\n"
"    cation time, and MD5 sum.  The \"listkeys\" com-\n"
"    mand does the same but includes the file encryp-\n"
"    tion key.  The \"listfiles\" command only lists\n"
"    file names, and produces no error messages if\n"
"    the named files are not current in the index.\n"
"    If no file arguments are given to these com-\n"
"    mands, all current index entries are listed.\n"
"\n"
"    The list commands can also be given encrypted\n"
"    file names (that consist of MD sum basenames\n"
"    plus .gpg extension).\n"
"\n"
"    The \"md5check\" command checks that every file\n"
"    exists, is in the index, and has an md5sum that\n"
"    matches that in the index.\n"
"\n"
"    This program returns exit status 0 if there is\n"
"    no error and if there is an error, returns exit\n"
"    status 1 and prints the error message to the\n"
"    standard output.\n"
"\f\n"
"    The efm program asks for a password to decrypt\n"
"    the index only the first time it is run during\n"
"    a login session.  It then sets up a background\n"
"    program holding the password that is accessible\n"
"    through the socket \"EFM-INDEX.sock\".  The\n"
"    \"kill\" command may be use to kill the back-\n"
"    ground process if it exists.  The \"start\" com-\n"
"    mand just starts the background process if it is\n"
"    not already running, but this is also done by\n"
"    other commands implicitly.\n"
"\n"
"    It is recommended that you put\n"
"\n"
"            ( cd backup-directory; efm kill )\n"
"\n"
"    in your .logout or .bash_logout file to kill any\n"
"    background process on logout.\n"
"\n"
"    The \"trace\" commands turn tracing on/off.\n"
"    When on, actions are annotated on the standard\n"
"    output by lines beginning with \"* \".  The\n"
"    \"trace\" command without any \"on\" or \"off\"\n"
"    argument just prints the current trace status.\n"
"\n"
"    The index file contains three line entries of\n"
"    the form:\n"
"\n"
"	 indicator filename\n"
"            mode mtime\n"
"            MD5sum key\n"
"\n"
"    where the first entry line is not indented and\n"
"    the other lines are.  The filename may be quoted\n"
"    with \"'s if it contains special characters,\n"
"    and a quote in such a filename is represented\n"
"    by a pair of quotes (\"\").  The mtime (file\n"
"    modification time) will always be quoted, and\n"
"    is Greenwich Mean Time (GMT).\n"
"\f\n"
"    Lines at the beginning of the index file whose\n"
"    first character is # are comment lines, and are\n"
"    preserved.  Blank lines are forbidden.  Comment\n"
"    lines must be inserted in the initial file made\n"
"    with gpg, or changed by using gpg to decrypt\n"
"    and re-encrypt the file.\n"
"\n"
"    The indicator is + if the entry is current, and\n"
"    - if the entry is obsolete.  The mode is 4 octal\n"
"    digits, and is used to set the file mode when\n"
"    the file is decrypted.  The mtime is quoted GMT\n"
"    time and is used to set the file modification\n"
"    time when the file is decrypted.  The MD5sum is\n"
"    the 32 hexadecimal digit MD5 sum of the decryp-\n"
"    ted file, and is used as the basename of the en-\n"
"    crypted file, as the name of a temporary decryp-\n"
"    ted file, and to check the integrity of decryp-\n"
"    tion.  The key is the symmetric encryption/de-\n"
"    cryption password for the file, and is the"
					" upper-\n"
"    case 32 digit hexadecimal representation of a\n"
"    128 bit random number.  However, it is this 32\n"
"    character representation, and NOT the random\n"
"    number, that is the key.\n"
"\n"
"    No two current files in the index are allowed to\n"
"    have the same MD5 sum.  Two files (not both cur-\n"
"    rent) with the same MD5 sum will have the same\n"
"    key.\n"
"\f\n"
"    The \"listall\" command is like \"list\" but\n"
"    lists both obsolete and current entries and also\n"
"    includes indicators (+ or -).  The"
				" \"listallkeys\"\n"
"    command is like \"listall\" but includes keys.\n"
"    The \"listobsfiles\" command is like"
				" \"listfiles\"\n"
"    but only lists obsolete entries, instead of only\n"
"    current entries.  The \"listallfiles\" command\n"
"    is like \"listfiles\" but lists both obsolete\n"
"    and current entries.  The \"listcurfiles\" com-\n"
"    mand lists only current entries, and is just an-\n"
"    other name for \"listfiles\".\n"
"\n"
"    The following commands may be used to edit the\n"
"    index in ways that may get the index out of sync\n"
"    with existing files.  So be careful if you use\n"
"    these commands.\n"
"\n"
"    The \"cur\" command makes index entries cur-\n"
"    rent.  The \"obs\" command makes index entries\n"
"    obsolete.  The \"add\" command creates index en-\n"
"    tries without encrypting or moving files.  The\n"
"    \"sub\" command deletes index entries without\n"
"    decrypting or moving files.  The \"del\" command\n"
"    deletes encrypted files without changing index\n"
"    entries.\n"
"\n"
"    An external program is used to encrypt/decrypt\n"
"    files.  By default this is gpg.  The encrypted\n"
"    file name is MD5SUM.gpg with this default.  In\n"
"    general the encrypted file basename is the\n"
"    MD5sum of the file contents and the extension\n"
"    denotes the encrypting program.\n"
"\n"
"    Similarly the extension of the index indicates\n"
"    the program used to encrypt the index.\n"
"\f\n"
"    Currently only gpg is supported as an encryp-\n"
"    ing program.\n"
;

int trace = 0;		/* 1 if trace on, 0 if off. */

#define MAX_LEXEME_SIZE 2000
#define MAX_LINE_SIZE ( 2 * MAX_LEXEME_SIZE + 10 )

typedef char line_buffer[MAX_LINE_SIZE+2];

const char * time_format = "%Y/%m/%d %H:%M:%S";
#define END_STRING "\001\002\003\004"

/* The following lines are filtered out of the output
 * (we could find no other way to keep gpg quiet).
 */
char * ofilter[] = {
    "gpg: WARNING: using insecure memory!",
    "gpg: please see http://www.gnupg.org/faq.html"
    " for more information",
    "gpg: BLOWFISH encrypted data",
    "gpg: WARNING: message was not integrity protected",
    NULL };

/* Get Line from input stream into line buffer.
 * Delete any \n from end of line.  Signal error if
 * line is too long.  Return true if line found, and
 * false on end of file.
 */
int get_line ( line_buffer buffer, FILE * in )
{
    int length;

    if ( ! fgets ( buffer, MAX_LINE_SIZE+2, in ) )
        return 0;
    length = strlen ( buffer );
    if ( buffer[length-1] != '\n' );
    {
	printf ( "ERROR: line too long or no"
	         " line feed at end of file:\n" );
	printf ( "%-60.60s...\n", buffer );
	exit ( 1 );
    }
    buffer[length-1] = 0;
    return 1;
}

/* The following is set if the index is modified and
 * needs to be rewritten.
 */
int index_modified;

/* Index is a circular list of entries.  All char *'s
 * are malloc'ed.
 */
struct entry {

    int current;
        /* 1 if current, 0 if obsolete. */

    char * filename;
        /* Filename may not contain \0 or \n and may
	 * not be more than MAX_LEXEME_SIZE characters
	 * long (so its quoted lexeme will fit on one
	 * line).
	 */

    unsigned mode;
    time_t mtime;

    char * md5sum;
    char * key;

    struct entry * previous, * next;
};
struct entry * first_entry = NULL;

/* Comment lines are just a circular list of lines.
 */
struct comment {
    char * line;	/* Malloc'ed. */

    struct comment * previous, * next;
};
struct comment * first_comment = NULL;

/* Given a pointer into the line buffer, scan the next
 * lexeme.  A lexeme is a sequence of non-whitespace
 * characters, or is " quoted.  "" denotes " in quoted
 * lexemes, but there are no other escapes.  A pointer
 * to the NUL terminated lexeme is returned.  Lexemes
 * must be separated by whitespace.  This function
 * modifies the buffer pointer to point to the next
 * lexeme.  If there are no lexemes, NULL is returned.
 *
 * This function modifies the lexeme in the buffer to
 * produce its result.  You must copy the returned
 * lexeme before reusing the buffer.
 */
char * get_lexeme ( char ** buffer )
{
    char * p, * result;

    p = * buffer;
    while ( isspace ( * p ) ) ++ p;
    result = p;
    if ( * p == '"' )
    {
	char * q = result;
	++ p;
	while ( * p )
	{
	    if ( * p == '"' )
	    {
		++ p;
	        if ( * p != '"' ) break;
	    }
	    * q ++ = * p ++;
	}
	* buffer = p;
	* q = 0;
    }
    else
    {
        while ( * p && ! isspace ( * p ) ) ++ p;
	if ( isspace ( * p ) ) * p ++ = 0;
	* buffer = p;
    }
    return * result ? result : NULL;
}

/* Given a pointer into the line buffer and the char-
 * acter string of a lexeme, write the lexeme into the
 * buffer.  The lexeme is quoted if it contains any
 * non-graphic characters, #, or ".  It is also quoted
 * if it is the empty string.  The buffer pointer is
 * updated to point after the lexeme.  There is no buf-
 * fer size check, so its up to the caller to be sure
 * the lexeme will fit.
 */
void put_lexeme ( char ** buffer, const char * lexeme )
{
    const char * p = lexeme;
    int quote = ( * p == 0 );
    while ( * p && ! quote )
    {
        char c = * p ++;
	quote = ( c <= ' ' || c == 0177 || c == '"'
				        || c == '#' );
    }
    if ( ! quote )
    {
        strcpy ( * buffer, lexeme );
	* buffer += strlen ( * buffer );
    }
    else
    {
	char * q = * buffer;
	* q ++ = '"';
        p = lexeme;
	while ( * p )
	{
	    char c = * p ++;
	    if ( c == '"' ) * q ++ = '"';
	    * q ++ = c;
	}
	* q ++ = '"';
	* buffer = q;
    }
}

/* Read index from file stream.  On error print error
 * message to stdout and exit ( 1 );
 */
void read_index ( FILE * f )
{
    line_buffer buffer;
    int begin = 1;
    char * filename;
    while ( get_line ( buffer, f ) )
    {
	struct tm td;
	char * b, * c, * fn, * mode, * mtime, * q,
	     * md5sum, * key;
	int current;
	unsigned long m;
	const char * ts;
	time_t d;
	struct entry * e;

	if ( begin && buffer[0] == '#' )
	{
	    struct comment * c =
	        (struct comment *)
		malloc ( sizeof ( struct comment ) );
	    c->line = strdup ( buffer );
	    if ( first_comment == NULL )
	        first_comment = c->previous
		              = c->next = c;
	    else
	    {
		c->previous = first_comment->previous;
		c->next = first_comment;
		c->previous->next = c->next->previous
		                  = c;
	    }
	    continue;
	}
	begin = 0;

	b = buffer;
	if ( * b == 0 || isspace ( * b ) )
	{
	    printf ( "ERROR: index entry first line"
	             " begins with space, or there"
		     " is a blank line in"
		     " index\n    %s\n", buffer );
	    exit ( 1 );
	}
	c = get_lexeme ( & b );
	if ( c == NULL
	     ||
	     ( strcmp ( c, "+" ) != 0
	       &&
	       strcmp ( c, "-" ) != 0 ) )
	{
	    printf ( "ERROR: index entry begins badly"
		     "\n    %s\n", buffer );
	    exit ( 1 );
	}
	current = ( c[0] == '+' ? 1 : 0 );
	fn = get_lexeme ( & b );
	if ( fn == NULL )
	{
	    printf ( "ERROR: index entry begins badly"
		     "\n    %s\n", buffer );
	    exit ( 1 );
	}
	filename = strdup ( fn );
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " filename,\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}

	if ( ! get_line ( buffer, f ) )
	{
	    printf ( "ERROR: premature end of entry,\n"
		     "    for file %s\n", filename );
	    exit ( 1 );
	}
	b = buffer;
	if ( ! isspace ( * b ) )
	{
	    printf ( "ERROR: index entry non-first line"
	             " does not begin with"
		     " space,\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	mode = get_lexeme ( & b );
	if ( mode == NULL )
	{
	    printf ( "ERROR: index entry mode missing"
		     "\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	mtime = get_lexeme ( & b );
	if ( mtime == NULL )
	{
	    printf ( "ERROR: index entry modification"
	             "time missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " mode and mtime,\n"
		     "    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	m = strtoul ( mode, & q, 8 );
	if ( * q || m >= ( 1 << 16 ) )
	{
	    printf ( "ERROR: bad EFM-INDEX mode (%s),\n"
	             "    for file %s\n",
		     mode, filename );
	    exit ( 1 );
	}
	ts = (const char *)
	     strptime ( mtime, time_format, & td );
	d = ( ts == NULL || * ts != 0 ) ?
	    -1 : mktime ( & td );
	if ( d == -1 )
	{
	    printf ( "ERROR: bad EFM-INDEX mtime"
	             " (%s),\n    for file %s\n",
		     mtime, filename );
	    exit ( 1 );
	}

	if ( ! get_line ( buffer, f ) )
	{
	    printf ( "ERROR: premature end of entry,\n"
		     "    for file %s\n", filename );
	    exit ( 1 );
	}
	b = buffer;
	if ( ! isspace ( * b ) )
	{
	    printf ( "ERROR: index entry non-first line"
	             " does not begin with"
		     " space,\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	md5sum = get_lexeme ( & b );
	if ( md5sum == NULL )
	{
	    printf ( "ERROR: index entry MD5 sum"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	key = get_lexeme ( & b );
	if ( key == NULL )
	{
	    printf ( "ERROR: index entry key"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " MD5 sum and key,\n"
		     "    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( strlen ( md5sum ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX md5sum (%s),"
	             "\n    for file %s\n",
		     md5sum, filename );
	    exit ( 1 );
	}
	if ( strlen ( key ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX key (%s),\n"
	             "    for file %s\n",
		     key, filename );
	    exit ( 1 );
	}

	e = (struct entry * )
	    malloc ( sizeof ( struct entry ) );
	e->current = current;
	e->filename = filename;
	e->mode = m;
	e->mtime = d;
	e->md5sum = strdup ( md5sum );
	e->key = strdup ( key );
	if ( first_entry == NULL )
	    first_entry = e->previous = e->next = e;
	else
	{
	    e->previous = first_entry->previous;
	    e->next = first_entry;
	    e->previous->next = e->next->previous = e;
	}
	index_modified = 1;
    }
}

/* Write index entry into file stream.  The mode
 * is a sum of flags that indicate what to list:
 *
 *	+0	List filename (always listed)
 *	+1	List current/obsolete indicator.
 *	+2	List mode, date, md5sum
 *	+4	List key.
 *
 * Prefix is prefixed to each line output.
 */
void write_index_entry
	( FILE * f, struct entry * e, int mode,
	  const char * prefix )
{
    line_buffer buffer;
    char * b = buffer;
    if ( ( mode & 1 ) != 0 )
    {
        * b ++ = ( e->current ? '+' : '-' );
	* b ++ = ' ';
    }
    put_lexeme ( & b, e->filename );
    * b = 0;
    fprintf ( f, "%s%s\n", prefix, buffer );

    b = buffer;
    strcpy ( b, "    " );
    b += 4;

    if ( ( mode & 2 ) != 0 )
    {
	char tbuffer [100];

	sprintf ( b, "%04o", e->mode );
	b += 4;
	* b ++ = ' ';
	strftime ( tbuffer, 100, time_format, 
		   gmtime ( & e->mtime ) );
	put_lexeme ( & b, tbuffer );
	* b = 0;
	fprintf ( f, "%s%s\n", prefix, buffer );
	b = buffer;
	strcpy ( b, "    " );
	b += 4;
	put_lexeme ( & b, e->md5sum );
    }

    if ( ( mode & 4 ) != 0 )
    {
	* b ++ = ' ';
	put_lexeme ( & b, e->key );
    }

    if ( ( mode & (2+4) ) != 0 )
    {
	* b = 0;
	fprintf ( f, "%s%s\n", prefix, buffer );
    }
}

/* Write index into file stream.  Mode is as per
 * write_index_entry.  Comments are not listed if
 * mode == 0.  Entry current must match the current
 * argument, unless that is -1.
 */
void write_index ( FILE * f, int mode, int current )
{
    struct entry * e;

    if ( mode != 0 )
    {
	struct comment * c = first_comment;
	if ( c ) do
	{
	    fprintf ( f, "%s\n", c->line );
	} while ( ( c = c->next ) != first_comment );
    }

    e = first_entry;
    if ( e ) do
    {
        if ( current == -1 || e->current == current )
	    write_index_entry ( f, e, mode, "" );
    }
    while ( ( e = e->next ) != first_entry );
}

/* Check if index has entry with given filename.
 * Return entry if yes, NULL if no.
 */
struct entry * find_filename ( const char * filename )
{
    struct entry * e = first_entry;
    if ( e ) do
    {
        if ( strcmp ( filename, e->filename ) == 0 )
	    return e;
    } while ( ( e = e->next ) != first_entry );
    return NULL;
}

/* Check if index has entry with given MD5sum.
 * Return entry if yes, NULL if no.
 *
 * If current_only is 1, ignore obsolete entries.
 */
struct entry * find_md5sum
	( const char * md5sum, int current_only )
{
    struct entry * e = first_entry;
    if ( e ) do
    {
        if ( current_only && ! e->current ) continue;
        if ( strcmp ( md5sum, e->md5sum ) == 0 )
	    return e;
    } while ( ( e = e->next ) != first_entry );
    return NULL;
}

/* Print error message from errno value and then call
 * exit ( 1 );
 */
void error ( int err_no )
{
    const char * s = strerror ( err_no );
    printf ( "ERROR: %s\n", s );
    exit ( 1 );
}

/* Wait for a child to terminate.  Return -1 if child
 * suffered error and 0 otherwise.
 */
int cwait ( pid_t child )
{
    int status;
    if ( waitpid ( child, & status, 0 ) < 0 )
        error ( errno );
    if ( WIFEXITED ( status )
         &&
	 WEXITSTATUS ( status ) == 0 )
        return 0;
    else
        return -1;
}

/* Create a random 32 hexadecimal digit key.  Buffer
 * must be at least 33 characters to hold key and
 * trailing NUL.
 */
void newkey ( char * buffer )
{
    int fd, size;
    unsigned char b[16];

    fd = open ( "/dev/random", O_RDONLY );
    if ( fd < 0 ) error ( errno );
    size = read ( fd, b, 16 );
    if ( size < 0 ) error ( errno );
    assert ( size == 16 );
    close ( fd );

    sprintf ( buffer,
              "%02x%02x%02x%02x%02x%02x%02x%02x"
              "%02x%02x%02x%02x%02x%02x%02x%02x",
	      b[0], b[1], b[2], b[3],
	      b[4], b[5], b[6], b[7],
	      b[8], b[9], b[10], b[11],
	      b[12], b[13], b[14], b[15] );
}

/* Encrypt/decrypt file.  If input file is NULL, return
 * file descriptor to write input into.  If output file
 * is NULL, return file descriptor to read output from.
 * If a descriptor is returned, the child process on
 * the other end of the descriptor pipe has its pid
 * returned in the child argument.  This can be passed
 * to cwait after the returned descriptor is closed.
 *
 * Output files are created, truncated if they exist,
 * and given user only read/write mode when they are
 * created.  Password is plength string of bytes.  If
 * error, returns -1 and writes error messages to
 * stdout.
 */
int crypt ( int decrypt,
            const char * input,
            const char * output,
	    const char * password, int plength,
	    pid_t * child )
{
    int infd, outfd, passfd, passwritefd, result;
    assert ( input != NULL || output != NULL );

    if ( input == NULL )
    {
        int fd[2];
	if ( pipe ( fd ) < 0 ) error ( errno );
	result = fd[1];
	infd = fd[0];
    }
    else
    {
        infd = open ( input, O_RDONLY );
	if ( infd < 0 )
	{
	    printf ( "ERROR: cannot open %s"
	             " for reading\n", input );
	    return -1;
	}
    }

    if ( output == NULL )
    {
        int fd[2];
	if ( pipe ( fd ) < 0 ) error ( errno );
	result = fd[0];
	outfd = fd[1];
    }
    else
    {
        outfd = open ( output,
	               O_WRONLY + O_CREAT + O_TRUNC,
		       S_IWUSR + S_IRUSR );
	if ( outfd < 0 )
	{
	    printf ( "ERROR: cannot open %s"
	             " for writing\n", output );
	    return -1;
	}
    }

    {
        int fd[2];
	if ( pipe ( fd ) < 0 ) error ( errno );
	passfd = fd[0];
	passwritefd = fd[1];
    }

    fflush ( stdout );

    * child = fork();
    if ( * child < 0 ) error ( errno );

    if ( * child == 0 )
    {
        int fd;

	/* Set fd's as follows:
	 * 	0 -> infd
	 *	1 -> outfd
	 *	2 -> parent's 1
	 *	3 -> passfd
	 */
	close ( 0 );
	if ( dup2 ( infd, 0 ) < 0 ) error ( errno );
	close ( infd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	close ( 1 );
	if ( dup2 ( outfd, 1 ) < 0 ) error ( errno );
	close ( outfd );
	if ( passfd != 3 )
	{
	    close ( 3 );
	    if ( dup2 ( passfd, 3 ) < 0 )
		error ( errno );
	    close ( passfd );
	}
	fd = getdtablesize() - 1;
	while ( fd > 3 ) close ( fd -- );

	if ( ( decrypt ?
	       execlp ( "gpg", "gpg",
	                "--passphrase-fd", "3",
		        "--batch", "-q", "--no-tty",
		        NULL ) :
	       execlp ( "gpg", "gpg",
		        "--cipher-algo", "BLOWFISH",
	                "--passphrase-fd", "3",
		        "--batch", "-q", "--no-tty",
			"-c", NULL )
	     ) < 0 )
	    error ( errno );
    }

    close ( infd );
    close ( outfd );
    close ( passfd );

    if ( write ( passwritefd, password, plength ) < 0 )
        error ( errno );
    close ( passwritefd );

    if ( input != NULL && output != NULL )
        return cwait ( * child );
    else
	return result;
}

/* Compute the MD5 sum of a file.  The filename may have
 * any format acceptable to scp, and must not be longer
 * than MAX_LEXEME_SIZE.  The 32 character md5sum
 * followed by a NUL is returned in the buffer, which
 * must be at least 33 characters long.  0 is returned
 * on success, -1 on error.  Error messages are written
 * on stdout.
 */
int md5sum ( char * buffer,
             const char * filename )
{
    int fd[2];
    int child, e;
    FILE * inf;
    line_buffer line;
    if ( pipe ( fd ) < 0 ) error ( errno );

    fflush ( stdout );

    child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
        int newfd, d, at_found;
	char * p;
	line_buffer buffer;

        close ( fd[0] );

	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> fd[1]
	 *	2 -> parent's 1
	 */
	newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	close ( 1 );
	if ( dup2 ( fd[1], 1 ) < 0 ) error ( errno );
	close ( fd[1] );
	d = getdtablesize() - 1;
	while ( d > 2 ) close ( d -- );

	strcpy ( buffer, filename );
	p = buffer;
	at_found = 0;
	for ( ; * p; ++ p )
	{
	    if ( * p == '@' )
	    {
		if ( at_found ) break;
		at_found = 1;
	    }
	    else if ( * p == ':' ) break;
	}

	if ( * p != ':' || ! at_found )
	{
	    /* Not a remote file. */

	    if ( execlp ( "md5sum", "md5sum",
			  filename, NULL ) < 0 )
		error ( errno );
	}
	else
	{
	    /* Remote file. */

	    * p ++ = 0;

	    if ( execlp ( "ssh", "ssh", buffer,
	                  "md5sum", p, NULL ) < 0 )
		error ( errno );
	}
    }

    close ( fd[1] );
    inf = fdopen ( fd[0], "r" );

    e = -1;

    if ( get_line ( line, inf ) )
    {
        char * p = line;
	for ( ; * p; ++ p )
	{
	    char c = * p;
	    if ( '0' <= c && c <= '9' ) continue;
	    if ( 'a' <= c && c <= 'f' ) continue;
	    if ( 'A' <= c && c <= 'F' ) continue;
	    break;
        }
	if ( p == line + 32 )
	{
	    strncpy ( buffer, line, 32 );
	    buffer[32] = 0;
	    e = 0;
	}
	else
	{
	    /* Print output that may contain error
	     * messages.
	     */
	    do {
	        printf ( "%s\n", line );
	    } while ( get_line ( line, inf ) );
	    break;
	}
    }
    fclose ( inf );
    if ( e < 0 )
    {
        printf ( "ERROR: cannot compute MD5 sum of"
	         " %s\n", filename );
    }

    return cwait ( child ) < 0 ? -1 : e;
}

/* Copy file.  0 is returned on success, -1 on error.
 * Error messages are written on stdout.  The mode
 * and mtime of the file are preserved.  The filenames
 * may have any format acceptable to scp.
 */
int copyfile
	( const char * source, const char * target )
{
    int child;

    fflush ( stdout );

    child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
        int newfd, d;

	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> parent's 1
	 *	2 -> parent's 1
	 */
	newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	d = getdtablesize() - 1;
	while ( d > 2 ) close ( d -- );

	if ( execlp ( "scp", "scp", "-p",
	              source, target, NULL ) < 0 )
	    error ( errno );
    }

    return cwait ( child );
}

/* Delete file.  0 is returned on success, -1 on error.
 * Error messages are written on stdout.  The filename
 * may have the format acceptable to scp, and must
 * not be longer than MAX_LEXEME_SIZE.
 */
int delfile ( const char * filename )
{
    int at_found, child;
    char * p;

    line_buffer buffer;
    strcpy ( buffer, filename );
    p = buffer;
    at_found = 0;
    for ( ; * p; ++ p )
    {
        if ( * p == '@' )
	{
	    if ( at_found ) break;
	    at_found = 1;
	}
	else if ( * p == ':' ) break;
    }
    if ( * p != ':' || ! at_found )
    {
        /* Not a remote file. */
	if ( unlink ( filename ) < 0
	     &&
	     errno != ENOENT )
	{
	    const char * s = strerror ( errno );
	    printf ( "ERROR: %s\n", s );
	    printf ( "    could not delete %s\n",
	             filename );
	    return -1;
	}
	else
	    return 0;
    }

    * p ++ = 0;

    /* Now buffer points at account and p at file
     * within account.
     */

    fflush ( stdout );
    child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
        int newfd, d;

	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> parent's 1
	 *	2 -> parent's 1
	 */
	newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	d = getdtablesize() - 1;
	while ( d > 2 ) close ( d -- );

	if ( execlp ( "ssh", "ssh", buffer,
	              "rm", "-f", p, NULL ) < 0 )
	    error ( errno );
    }

    return cwait ( child );
}

/* Add file entry to index.  Return -1 on error, 0 on
 * success.
 */
int add ( const char * filename )
{
    const char * p;
    struct entry * e;
    char sum [33];
    struct stat s;
    char key[33];

    p = filename;
    for ( ; * p; ++ p )
    {
        if ( * p == '/' )
	{
	    printf ( "ERROR: filename contains '/':"
	             " %s\n", filename );
	    return -1;
	}
        if ( * p == '\n' )
	{
	    printf ( "ERROR: filename contains"
	             " linefeed: %s\n", filename );
	    return -1;
	}
    }
    if ( find_filename ( filename ) )
    {
        printf ( "ERROR: index entry already exists"
	         " for %s\n", filename );
	return -1;
    }

    if ( stat ( filename, & s ) < 0 )
    {
        printf ( "ERROR: cannot stat %s\n", filename );
	return -1;
    }

    if ( md5sum ( sum, filename ) < 0 ) return -1;
    e = find_md5sum ( sum, 1 );
    if ( e != NULL )
    {
        printf ( "ERROR: cannot make index entry for"
	         " %s\n    as it has the same MD5 sum"
		 " as existing entry for %s\n",
		 filename, e->filename );
	return -1;
    }


    e = find_md5sum ( sum, 0 );
    if ( e != NULL )
        strcpy ( key, e->key );
    else
	newkey ( key );

    e = (struct entry *)
        malloc ( sizeof ( struct entry ) );
    e->current = 1;
    e->filename = strdup ( filename );
    e->md5sum = strdup ( sum );
    e->mode =  s.st_mode & 07777;
    e->mtime = s.st_mtime;
    e->key = strdup ( key );
    if ( first_entry == NULL )
	first_entry = e->previous = e->next = e;
    else
    {
	e->previous = first_entry->previous;
	e->next = first_entry;
	e->previous->next = e->next->previous = e;
    }
    index_modified = 1;
    if ( trace )
    {
        printf ( "* added index entry:\n" );
	write_index_entry ( stdout, e, 7, "* " );
    }
    return 0;
}

/* Subtract file entry from index.  Return -1 on error,
 * 0 on success.
 */
int sub ( const char * filename )
{
    struct entry * e = find_filename ( filename );
    if ( e == NULL )
    {
        printf ( "ERROR: subtracting nonexistent file:"
	         " %s\n", filename );
	return -1;
    }
    if ( trace )
    {
        printf ( "* removing index entry:\n" );
	write_index_entry ( stdout, e, 7, "* " );
    }
    e->next->previous = e->previous;
    e->previous->next = e->next;
    if ( first_entry == e ) first_entry = e->next;
    if ( first_entry == e ) first_entry = NULL;
    free ( e->filename );
    free ( e->md5sum );
    free ( e->key );
    free ( e );
    index_modified = 1;
    return 0;
}


/* Fetch argument from input stream into line_buffer.
 * Return a pointer to the NUL terminated argument,
 * or NULL if there is no argument.
 *
 * Eol_found must be set to 0 before getting first
 * argument.  If more arguments are gotten than are
 * available, NULL is returned repeatedly.
 */
int eol_found;
char * get_argument ( line_buffer buffer, FILE * in )
{
    char * b, *r, * j;
    if ( eol_found ) return NULL;
    if ( ! get_line ( buffer, in ) )
    {
        eol_found = 1;
	return NULL;
    }
    b = buffer;
    r = get_lexeme ( & b );
    j = get_lexeme ( & b );
    if ( j != NULL )
    {
        printf ( "ERROR: junk at end of argument"
	         " line:\n    %s\n", j );
	exit ( 1 );
    }
    if ( r == NULL ) eol_found = 1;
    return r;
}

/* Execute one command.  Arguments are gotten from the
 * input stream via get_argument, and results are writ-
 * ten to stdout.  Return 1 to if kill command proces-
 * sed (do nothing else for kill), 0 if command proces-
 * sed without error, and -1 if command processed with
 * error.
 */
int execute_command ( FILE * in )
{
    int result = 0;
    char * arg;
    line_buffer buffer, directory;

    eol_found = 0;
    arg = get_argument ( buffer, in );

    if ( arg == NULL ) return 0;
    else if ( strcmp ( arg, "start" ) == 0 )
        /* Do Nothing */;
    else if ( strcmp ( arg, "kill" ) == 0 )
    {
	printf ( "efm killed\n" );
        result = 1;
    }
    else if ( strcmp ( arg, "listfiles" ) == 0
              ||
	      strcmp ( arg, "listcurfiles" ) == 0
              ||
	      strcmp ( arg, "listobsfiles" ) == 0
              ||
	      strcmp ( arg, "listallfiles" ) == 0
              ||
	      strcmp ( arg, "list" ) == 0
              ||
	      strcmp ( arg, "listkeys" ) == 0
              ||
	      strcmp ( arg, "listall" ) == 0
              ||
	      strcmp ( arg, "listallkeys" ) == 0 )
    {
	int mode =
	    ( strcmp ( arg, "listall" ) == 0     ? 3 :
	      strcmp ( arg, "list" ) == 0        ? 2 :
	      strcmp ( arg, "listallkeys" ) == 0 ? 7 :
	      strcmp ( arg, "listkeys" ) == 0    ? 6 :
	      					   0 );
	int current =
	    ( strcmp ( arg, "listall" ) == 0      ? -1 :
	      strcmp ( arg, "listallkeys" ) == 0  ? -1 :
	      strcmp ( arg, "listallfiles" ) == 0 ? -1 :
	      strcmp ( arg, "listobsfiles" ) == 0 ?  0 :
	      				             1 );

	arg = get_argument ( buffer, in );
	if ( arg == NULL )
	    write_index ( stdout, mode, current );
	else do
	{
	    int printed = 0;
	    struct entry * e = find_filename ( arg );
	    if ( e != NULL )
	    {
		if ( current == -1
		     ||
		     current == e->current )
		{
		    write_index_entry
			( stdout, e, mode, "" );
		    printed = 1;
		}
	    }
	    else
	    {
	        /* Try for encrypted file name. */

	        int len = strlen ( arg );
		if ( len == 32 + 4
		     &&
		     strcmp ( arg + 32, ".gpg" ) == 0 )
		{
		    struct entry * e = first_entry;
		    if ( e != NULL ) do
		    {
		        if ( strncmp
			         ( arg, e->md5sum, 32 )
			     != 0 )
			    continue;
			if ( current == -1
			     ||
			     current == e->current )
			{
			    write_index_entry
				( stdout, e, mode, "" );
			    printed = 1;
			}
		    } while ( ( e = e->next )
		    	      != first_entry );
		}
	    }

	    if ( mode != 0 && printed == 0 )
	    {
		printf ( "ERROR: %s not in index\n",
			 arg );
		result = -1;
	    }
	} while ( arg = get_argument ( buffer, in ) );
    }
    else if ( strcmp ( arg, "md5check" ) == 0 )
    {
        while ( arg = get_argument ( buffer, in ) )
	{
	    struct entry * e = find_filename ( arg );
	    struct stat st;
	    char sum [33];

	    if ( e == NULL )
	    {
	        printf ( "ERROR: %s is not in"
		         " index\n", arg );
		result = -1;
		continue;
	    }
	    if ( stat ( arg, & st ) < 0 )
	    {
	        printf ( "ERROR: %s does not"
		         " exist\n", arg );
		result = -1;
		continue;
	    }
	    if ( md5sum ( sum, arg ) < 0 )
	    {
		result = -1;
		continue;
	    }
	    if ( strcmp ( sum, e->md5sum ) != 0 )
	    {
		printf ( "ERROR: MD5 sum %s\n"
			 "    of existing file"
			 " %s\n"
			 "    does not match"
			 " the MD5 sum %s in"
			 " the index\n",
			 sum, arg, e->md5sum );
		result = -1;
		continue;
	    }
	}
    }
    else if ( strcmp ( arg, "obs" ) == 0
              ||
	      strcmp ( arg, "cur" ) == 0 )
    {
        int current =
	    ( strcmp ( arg, "obs" ) == 0 ? 0 : 1 );

        while ( arg = get_argument ( buffer, in ) )
	{
	    struct entry * e = find_filename ( arg );
	    if ( e == NULL )
	    {
		printf ( "ERROR: index entry for %s"
		         " does not exist\n", arg );
		result = -1;
		continue;
	    }
	    if ( e->current == current ) continue;

	    if ( ! e->current )
	    {
		struct entry * e2 =
		    find_md5sum ( e->md5sum, 1 );
		if ( e2 != NULL )
		{
		    printf ( "ERROR: cannot make the"
		             " index entry\n"
			     "    for %s\n"
			     "    current as it has the"
			     " same MD5 sum as the"
			     " existing current entry"
			     "\n    for %s\n",
			     arg, e2->filename );
		    result = -1;
		    continue;
		}
	    }

	    e->current = current;
	    index_modified = 1;
	    if ( trace )
	    {
		printf ( "* made index entry %s\n",
		         e->current ? "current"
			            : "obsolete" );
		write_index_entry ( stdout, e, 7, "* " );
	    }
	}
    }
    else if ( strcmp ( arg, "add" ) == 0 )
    {
        while ( arg = get_argument ( buffer, in ) )
	{
	    if ( add ( arg ) < 0 ) result = -1;
	}
    }
    else if ( strcmp ( arg, "sub" ) == 0 )
    {
        while ( arg = get_argument ( buffer, in ) )
	{
	    if ( sub ( arg ) < 0 ) result = -1;
	}
    }
    else if (    strcmp ( arg, "copyto" ) == 0
              || strcmp ( arg, "copyfrom" ) == 0
              || strcmp ( arg, "moveto" ) == 0
              || strcmp ( arg, "movefrom" ) == 0
              || strcmp ( arg, "remove" ) == 0
              || strcmp ( arg, "check" ) == 0
              || strcmp ( arg, "del" ) == 0 )
    {
        char op = ( arg[0] == 'c' && arg[1] == 'h' ?
	            'k' : arg[0] );
	char direction = ( ( op == 'm' || op == 'c' ) ?
	                   arg[4] : 'f' );
	char * dbegin = get_argument ( directory, in );
	if ( dbegin == NULL )
	{
	    printf ( "ERROR: missing directory" );
	    result = -1;
	}
	else
	{
	    int current_directory =
	        ( strcmp ( dbegin, "." ) == 0 );
	    char * dend = dbegin + strlen ( dbegin );
	    * dend ++ = '/';
	    while ( arg = get_argument ( buffer, in ) )
	    {
	        /* Get valid index entry. */

		struct entry * e =
		    find_filename ( arg );
		struct stat st;
		char efile [40];
		pid_t child;

		/* On copyto or moveto with an obsolete
		 * entry, delete the entry here so it
		 * can be recomputed below.  Be sure
		 * file exists and can have its MD5 sum
		 * computed before deleting entry.
		 */
		if ( e != NULL && ! e->current
		     && direction == 't' )
		{
		    char sum [33];

		    if ( stat ( arg, & st ) < 0 )
		    {
		        printf ( "ERROR: file %s does"
			         " not exist\n", arg );
			result = -1;
			continue;
		    }
		    if ( md5sum ( sum, arg ) < 0 )
		    {
			result = -1;
			continue;
		    }

		    e = find_md5sum ( sum, 1 );
		    if ( e != NULL )
		    {
			printf ( "ERROR: cannot remake"
			         " index entry for %s\n"
				 "    as it would have"
				 " the same MD5 sum as"
				 " the existing current"
				 " entry\n"
				 "    for %s\n",
				 arg, e->filename );
			return -1;
		    }

		    sub ( arg );
		    e = NULL;
		}

		if ( e == NULL )
		{
		    if ( direction != 't' )
		    {
		        printf ( "ERROR: no index entry"
			         " exists for %s\n",
				 arg );
			result = -1;
			continue;
		    }
		    if ( add ( arg ) < 0 )
		    {
			result = -1;
			continue;
		    }
		    e = find_filename ( arg );
		    assert ( e != NULL );
		}
		else if ( stat ( arg, & st ) >= 0 )
		{
		    char sum [33];
		    if ( md5sum ( sum, arg ) < 0 )
		    {
			result = -1;
			continue;
		    }
		    if ( strcmp ( sum, e->md5sum )
		         != 0 )
		    {
		        printf ( "ERROR: MD5 sum %s\n"
			         "    of existing file"
				 " %s\n"
				 "    does not match"
				 " the MD5 sum %s in"
				 " the index\n",
				 sum, arg, e->md5sum );
			result = -1;
			continue;
		    }
		} else if ( direction == 't' ) {
		    printf ( "ERROR: file not found:"
		             " %s\n", arg );
		    result = -1;
		    continue;
		}

		/* Perform Copying */

		strcpy ( efile, e->md5sum );
		strcpy ( efile + 32, ".gpg" );
		strcpy ( dend, efile );
		if ( direction == 't' )
		{
		    if ( trace )
		        printf ( "* encrypting %s\n"
			         "*     to make %s\n",
				 arg, efile );
		    unlink ( efile );
		    if ( crypt ( 0, arg, efile,
		                 e->key, 32,
				 & child ) < 0 )
		    {
		        printf ( "ERROR: could not"
			         " encrypt %s\n", arg );
			result = -1;
			continue;
		    }
		    if ( trace )
		        printf ( "* changing mode of"
			         " %s\n"
			         "*     to user-only"
				 " read-only\n",
				 efile );
		    if ( chmod ( efile, S_IRUSR ) < 0 )
		    {
		        printf ( "ERROR: cannot chmod"
			         " %s\n", efile );
			result = -1;
			continue;
		    }
		    if ( ! current_directory )
		    {
			char efile_sum[33];
			char dbegin_sum[33];

			if ( trace )
			    printf ( "* deleting %s\n",
			             dbegin );
			if ( delfile ( dbegin ) < 0 )
			{
			    result = -1;
			    continue;
			}
			if ( trace )
			    printf ( "* copying %s\n"
			             "*     to %s\n",
			             efile, dbegin );
			if ( copyfile ( efile, dbegin )
			     < 0 )
			{
			    result = -1;
			    continue;
			}
			if ( trace )
			    printf ( "* comparing MD5"
			             " sums of %s\n"
				     "*     and %s\n",
			             efile, dbegin );
			if ( md5sum ( efile_sum,
			              efile )
			     < 0 )
			{
			    result = -1;
			    continue;
			}
			if ( md5sum ( dbegin_sum,
				      dbegin )
			     < 0 )
			{
			    result = -1;
			    continue;
			}
			if ( strcmp ( efile_sum,
				      dbegin_sum )
			     != 0 )
			{
			    printf ( "ERROR: MD5 sum of"
			             " %s (%s)\n"
				     "    does not"
				     " match that of %s"
				     " (%s)\n",
			             efile, efile_sum,
				     dbegin,
				     dbegin_sum );
			    result = -1;
			    continue;
			}
			if ( trace )
			    printf ( "* deleting %s\n",
			             efile );
			unlink ( efile );
		    }
		}
		else if ( op == 'm' || op == 'c'
		                    || op == 'k' )
		{
		    char sum [33];

		    if ( ! current_directory )
		    {
			if ( trace )
			    printf ( "* copying %s\n"
			             "*     to %s\n",
			             dbegin, efile );
			unlink ( efile );
			if ( copyfile ( dbegin, efile )
			     < 0 )
			{
			    result = -1;
			    continue;
			}
		    }
		    else if ( stat ( efile, & st ) < 0 )
		    {
		        printf ( "ERROR: encrypted %s\n"
			         "    (%s) does not"
				 " exist\n",
				 arg, efile );
			result = -1;
			continue;
		    }
		    if ( trace )
		        printf ( "* decrypting %s\n"
			         "*     to make %s\n",
				 efile, e->md5sum );
		    unlink ( e->md5sum );
		    if ( crypt ( 1, efile, e->md5sum,
				 e->key, 32,
				 & child ) < 0 )
		    {
			printf ( "ERROR: could not"
				 " decrypt %s\n"
				 "    for %s\n",
				 efile, arg );
			result = -1;
			continue;
		    }

		    if ( ! current_directory )
		    {
			if ( trace )
			    printf ( "* deleting %s\n",
				     efile );
			unlink ( efile );
		    }

		    if ( trace )
		        printf ( "* checking MD5 sum of"
			         " %s\n", e->md5sum );
		    if ( md5sum ( sum, e->md5sum ) < 0 )
		    {
		        printf ( "ERROR: cannot compute"
			         " MD5 sum of %s\n    "
				 "which is the"
				 " retrieval of %s\n",
				 e->md5sum, arg );
			result = -1;
			continue;
		    }
		    if ( strcmp ( sum, e->md5sum )
		         != 0 )
		    {
		        printf ( "ERROR: MD5 sum of %s"
			         " is %s\n    "
				 "bad retrieval of"
				 " %s\n", e->md5sum,
				 sum, arg );
			result = -1;
			continue;
		    }
		    if ( op != 'k' )
		    {
			struct utimbuf ut;

			if ( trace )
			    printf
				( "* linking %s\n"
				  "*     to %s\n",
				  e->md5sum, arg );
		        unlink ( arg );
			if ( link ( e->md5sum, arg )
			     < 0 )
			{
			    printf ( "ERROR: cannot"
			             " rename %s\n"
				     "    to %s\n",
				     e->md5sum, arg );
			    result = -1;
			    continue;
			}
			if ( trace )
			    printf
				( "* changing mode and"
				  " modification time"
				  " of %s\n", arg );
			if ( chmod ( arg, e->mode )
			     < 0 )
			{
			    printf ( "ERROR: cannot"
			             " chmod %s\n",
				     arg );
			    result = -1;
			}
			ut.actime = time ( NULL );
			ut.modtime = e->mtime;
			if ( utime ( arg, & ut ) < 0 )
			{
			    printf ( "ERROR: cannot set"
				     " modification"
				     " time of %s\n",
				     arg );
			    result = -1;
			}
		    }
		    if ( trace )
			printf
			    ( "* removing %s\n",
			      e->md5sum );
		    unlink ( e->md5sum );
		}

		/* Perform source file deletion. */

		if ( ( op == 'm' && direction == 'f' )
		     || op == 'r' || op == 'd' )
		{
		    if ( trace )
			printf
			    ( "* removing %s\n",
			      dbegin );
		    if ( delfile ( dbegin ) < 0 )
		    {
			result = -1;
			continue;
		    }
		}
		else if (    op == 'm'
		          && direction == 't' )
		{
		    if ( trace )
			printf
			    ( "* removing %s\n", arg );
		    if ( delfile ( arg ) < 0 )
		    {
			result = -1;
			continue;
		    }
		}

		/* Delete entry if necessary. */

		if ( ( op == 'm' && direction == 'f' )
		     || op == 'r' )
		{
		    e->current = 0;
		    index_modified = 1;
		    if ( trace )
		    {
			printf ( "* made index entry"
			         " obsolete\n" );
			write_index_entry
			    ( stdout, e, 7, "* " );
		    }
		}
	    }
	}
    }
    else if ( strcmp ( arg, "trace" ) == 0 )
    {
	arg = get_argument ( buffer, in );
	if ( arg == NULL )
	{
	    printf ( "* efm trace %s\n",
	             trace ? "on" : "off" );
	}
	else if ( strcmp ( arg, "on" ) == 0 )
	{
	    trace = 1;
	    printf ( "* efm trace on\n" );
	}
	else if ( strcmp ( arg, "off" ) == 0 )
	{
	    if ( trace ) printf ( "* efm trace off\n" );
	    trace = 0;
	}
	else
	{
	    printf ( "ERROR: bad argument to trace:"
	             " %s\n", arg );
	    result = -1;
	}
    }
    else
    {
        printf ( "ERROR: bad command (ignored):"
	         " %s\n", arg );
	result = -1;
    }

    arg = get_argument ( buffer, in );
    if ( arg != NULL && result != -1 )
    {
	printf ( "ERROR: extra argument (ignored):"
		 " %s\n", arg );
	result = -1;
    }

    while ( arg != NULL )
        arg = get_argument ( buffer, in );

    return result;
}

char * password = NULL;
    /* Password read from user for EFM-INDEX.*. */

int main ( int argc, char ** argv )
{
    line_buffer buffer;
    int tofd;
    struct sockaddr_un sa;
    const char * pass;
    int indexchild;
    int indexfd;
    FILE * indexf;
    int listenfd;
    pid_t childpid;
    FILE * tof;
    char ** argp;

    if ( argc < 2
         ||
	 strncmp ( argv[1], "-doc", 4 ) == 0 )
    {
	printf ( documentation );
	exit (1);
    }

    tofd = socket ( PF_UNIX, SOCK_STREAM, 0 );
    if ( tofd < 0 ) error ( errno );
    sa.sun_family = AF_UNIX;
    strcpy ( sa.sun_path, "EFM-INDEX.sock" );
    if ( connect ( tofd,
                   (const struct sockaddr *) & sa,
		   sizeof ( sa ) ) < 0 )
    {
	if ( errno == ECONNREFUSED )
	{
	    unlink ( sa.sun_path );
	    if ( strcmp ( argv[1], "kill" ) == 0
	         &&
		 argc == 2 )
	    {
	        printf ( "efm background process"
		         " has died on its own\n" );
		exit ( 0 );
	    }
	}
	else if ( errno == ENOENT )
	{
	    if ( strcmp ( argv[1], "kill" ) == 0
	         &&
		 argc == 2 )
	    {
	        printf ( "efm not running\n" );
		exit ( 0 );
	    }
	}
	else
	    error ( errno );

	pass = getpass ( "Password: " );
	if ( pass == NULL ) error ( errno );
	password = strdup ( pass );

	indexfd =
	    crypt ( 1, "EFM-INDEX.gpg", NULL,
	            password,
		    strlen ( password ),
		    & indexchild );
	if ( indexfd < 0 ) exit ( 1 );
	indexf = fdopen ( indexfd, "r" );
	read_index ( indexf );
	fclose ( indexf );
	if ( cwait ( indexchild ) < 0 )
	{
	    printf ( "ERROR: error decypting"
	             " EFM-INDEX.gpg\n" );
	    exit ( 1 );
	}
	listenfd =
	    socket ( PF_UNIX, SOCK_STREAM, 0 );
	if ( bind ( listenfd,
		    (const struct sockaddr *) & sa,
		    sizeof ( sa ) ) < 0 )
	    error ( errno );
	if ( listen ( listenfd, 0 ) < 0 )
	    error ( errno );
	childpid = fork ( );
	if ( childpid < 0 ) error ( errno );
	if ( childpid == 0 )
	{
	    struct stat st;
	    int done;

	    close ( tofd );

	    /* Temporarily reroute stdout to error
	     * descriptor.
	     */
	    fflush ( stdout );
	    close ( 1 );
	    dup2 ( 2, 1 );

	    done = 0;  /* Res
	        /* 0 if done without error.
		   -1 if done with error.
		   1 to kill (without error).
		 */
	    while ( done != 1 )
	    {
		int fromfd;
		FILE * inf;

		fromfd =
		    accept ( listenfd, NULL, NULL );
		if ( fromfd < 0 ) error ( errno );

		/* Reroute stdout to new descriptor.
		 */
		assert ( fromfd != 1 );
		fflush ( stdout );
		close ( 1 );
		dup2 ( fromfd, 1 );
		inf = fdopen ( fromfd, "r" );

		index_modified = 0;
		done = execute_command ( inf );
		if ( index_modified )
		{
		    int indexchild;
		    int indexfd;
		    FILE * indexf;

		    indexfd =
			crypt ( 0,
				NULL, "EFM-INDEX.gpg+",
				password,
				strlen ( password ),
				& indexchild );
		    if ( indexfd < 0 ) exit ( 1 );
		    indexf = fdopen ( indexfd, "w" );
		    if ( trace )
		        printf ( "* writing"
			         " EFM-INDEX.gpg+\n" );
		    write_index ( indexf, 7, -1 );
		    fclose ( indexf );
		    if ( cwait ( indexchild ) < 0 )
		    {
			if ( trace )
			    printf
			        ( "* removing"
			          " EFM-INDEX.gpg+\n" );
		        unlink ( "EFM-INDEX.gpg+" );
			printf ( "ERROR: error"
			         " encypting"
				 " EFM-INDEX.gpg\n" );
			exit ( 1 );
		    }

		    if ( stat ( "EFM-INDEX.gpg-", & st )
		         >= 0 )
		    {
			if ( trace )
			    printf
				( "* removing"
				  " EFM-INDEX.gpg-\n"
			        );
			unlink ( "EFM-INDEX.gpg-" );
		    }
		    if ( trace )
			printf
			    ( "* renaming"
			      " EFM-INDEX.gpg to"
			      " EFM-INDEX.gpg-\n"
			    );
		    if ( link ( "EFM-INDEX.gpg",
		                "EFM-INDEX.gpg-" ) < 0
			 &&
			 errno != ENOENT )
		        error ( errno );
		    unlink ( "EFM-INDEX.gpg" );
		    if ( trace )
			printf
			    ( "* renaming"
			      " EFM-INDEX.gpg+ to"
			      " EFM-INDEX.gpg\n"
			    );
		    if ( link ( "EFM-INDEX.gpg+",
		                "EFM-INDEX.gpg" ) < 0 )
		        error ( errno );
		    unlink ( "EFM-INDEX.gpg+" );
		}

		printf ( "%s%d\n", END_STRING,
		         done == 1 ? 0 : - done );
		fflush ( stdout );
		fclose ( inf );
		close ( 1 );
		dup2 ( 2, 1 );
	    }

	    unlink ( "EFM-INDEX.sock" );
	    exit ( 0 );
	}
	if ( connect ( tofd,
		       (const struct sockaddr *) & sa,
		       sizeof ( sa ) ) < 0 )
	    error ( errno );
    }

    /* To work the parent needs one a+ FILE for some
     * reason.
     */
    tof = fdopen ( tofd, "a+" );

    /* Send arguments to child.  Each argument sent as
     * a lexeme on its own line.  At the end of the
     * argument list, a blank line is written.
     */
    argp = argv + 1;
    for ( ; * argp; ++ argp )
    {
        char * b = buffer;
	const char * p;
	if ( strlen ( * argp ) > MAX_LEXEME_SIZE )
	{
	    printf ( "ERROR: program argument too long:"
	             " %s\n", * argp );
	    exit ( 1 );
	}
	p = * argp;
	while ( * p )
	{
	    if ( * p ++ == '\n')
	    {
		printf ( "ERROR: program argument"
		         " contains line feed: %s\n",
			 * argp );
		exit ( 1 );
	    }
	}
	put_lexeme ( & b , * argp );
	* b ++ = 0;
	fprintf ( tof, "%s\n", buffer );
    }
    fprintf ( tof, "\n" );
    while ( get_line ( buffer, tof ) )
    {
	char ** fp;

        if ( strncmp ( buffer, END_STRING,
		       strlen ( END_STRING) ) == 0 )
	    break;

	/* Filter out unwanted missives from gpg. */

	fp = ofilter;
	for ( ; * fp; ++ fp )
	{
	    if ( strcmp ( * fp, buffer ) == 0 ) break;
	}
	if ( * fp == NULL ) printf ( "%s\n", buffer );
    }
    fclose ( tof );
    exit ( atoi ( buffer + strlen ( END_STRING ) ) );
}
