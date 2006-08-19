/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	efm.c
** Date:	Sat Aug 19 16:09:04 EDT 2006
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2006/08/19 21:01:03 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.31 $
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
"efm list\n"
"\n"
"efm start\n"
"efm kill\n"
"\n"
"efm add file ...\n"
"efm sub file ...\n"
"efm del source file ...\n"
"\n"
"efm trace on\n"
"efm trace off\n"
"\n"
"    Each file may have an encrypted version in the\n"
"    target/source directory.  The file can be moved\n"
"    or copied to/from that directory.  It is encryp-\n"
"    ted when moved or copied to the target direc-\n"
"    tory, and decrypted when moved or copied from\n"
"    the source directory.  The directory may be"
                                            " \".\"\n"
"    to encrypt or decrypt in place.\n"
"\n"
"    The \"remove\" command is like \"movefrom\" fol-\n"
"    lowed by discarding the decrypted file.  The\n"
"    \"check\" command is like \"copyfrom\" followed\n"
"    by discarding the decrypted file.  Neither of\n"
"    these two commands affects the decrypted file.\n"
"\n"
"    The \"list\" command lists all encrypted files\n"
"    and for each its MD5sum, modification time, and\n"
"    protection mode.\n"
"\n"
"    File names must be relative to the current di-\n"
"    rectory.  Source and target names can be any di-\n"
"    rectory names acceptable to scp.  Efm makes tem-\n"
"    porary files in the current directory whose base\n"
"    names are the 32 character MD5 sums of the de-\n"
"    crypted files.\n"
"\n"
"    The current directory must contain an encrypted\n"
"    index file named \"EFM-INDEX.gpg\" that is main-\n"
"    tained by efm.  You must create and encrypt an\n"
"    initial empty EFM-INDEX.gpg file by using the\n"
"    gpg program.  You choose the password that pro-\n"
"    tects this index when you create it using gpg.\n"
"\n"
"    The efm program asks for a password to decrypt\n"
"    the index only the first time it is run during\n"
"    a login session.  It then sets up a background\n"
"    program holding the password that is accessible\n"
"    through the socket \"EFM-INDEX.sock\".  This\n"
"    background program dies on a hangup signal when\n"
"    you log out, and may be killed at any time.\n"
"    The \"kill\" command may be use to kill the\n"
"    background process.  The \"start\" command just\n"
"    starts the background process if it is not\n"
"    already running, but this is also done by other\n"
"    commands implicitly.\n"
"\n"
"    The \"trace\" commands turn tracing on/off.\n"
"    When on, actions are annotated on the standard\n"
"    output by lines beginning with \"* \".\n"
"\n"
"    The index file contains three line entries of\n"
"    the form:\n"
"\n"
"	 filename\n"
"            mode mtime\n"
"            MD5sum key\n"
"\n"
"    where the first entry line is not indented and\n"
"    the other lines are.  The filename may be quoted\n"
"    with \"'s if it contains special characters,\n"
"    and a quote in such a filename is represented\n"
"    by a pair of quotes (\"\").  The mtime will\n"
"    always be quoted, and is Greenwich Mean Time\n"
"    (GMT).\n"
"\n"
"    Lines at the beginning of the index file whose\n"
"    first character is # are comment lines, and are\n"
"    preserved.  Blank lines are forbidden.  Comment\n"
"    lines must be inserted in the initial file made\n"
"    with gpg, or changed by using gpg to decrypt\n"
"    and re-encrypt the file.\n"
"\n"
"    An entry for a file is created when the file is\n"
"    encrypted and deleted when the encrypted file\n"
"    is deleted by \"movefrom\" or \"remove\".  If\n"
"    a file value is changed it may not be reencryp-\n"
"    ted until it has been removed.  No two files in\n"
"    the index are allowed to have the same MD5sum.\n"
"\n"
"    The mtime and mode are used to set the file mod-\n"
"    ification time and mode of the file when it is\n"
"    decrypted.  The MD5sum is the MD5 sum of the de-\n"
"    crypted file, and is used to check the integri-\n"
"    ty of the encryption/decryption process.  The\n"
"    mode is 4 octal digits and the MD5sum is 32\n"
"    hexadecimal digits.  The key is the symmetric\n"
"    encryption/decryption password for the file, and\n"
"    is the uppercase 32 digit hexadecimal represen-\n"
"    tation of a 128 bit random number.  However, it\n"
"    is this 32 character representation, and NOT\n"
"    the number, that is the key.\n"
"\n"
"    The \"add\" command creates a file entry without\n"
"    encrypting or moving a file.  The \"sub\" com-\n"
"    mand deletes a file entry without decrypting or\n"
"    moving a file.  The \"del\" command deletes the\n"
"    encrypted file without removing the file from\n"
"    the index.  All these commands can cause this\n"
"    index to disagree with the set of existing\n"
"    encrypted files.  The \"list\" command actually\n"
"    lists the index, which includes files that have\n"
"    been deleted or added but have no actual encryp-\n"
"    ted file, and does not include files that have\n"
"    been subtracted from the index without deleting\n"
"    the encrypted file.\n"
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
"\n"
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
    if ( ! fgets ( buffer, MAX_LINE_SIZE+2, in ) )
        return 0;
    int length = strlen ( buffer );
    if ( length == MAX_LINE_SIZE+1 )
    {
	printf ( "ERROR: line too long:\n" );
	printf ( "%-60.60s...\n", buffer );
	exit ( 1 );
    }
    assert ( buffer[length-1] == '\n' );
    buffer[length-1] = 0;
    return 1;
}

/* The following is set if the index is modified and
 * needs to be rewritten.
 */
int index_modified;

/* Index is a circular list of entries.
 */
struct entry {

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
    char * line;

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
 */
char * get_lexeme ( char ** buffer )
{
    char * p = * buffer;
    while ( isspace ( * p ) ) ++ p;
    char * result = p;
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
 * updated to point after the lexeme.
 */
void put_lexeme ( char ** buffer, const char * lexeme )
{
    const char * p = lexeme;
    int quote = ( * p == 0 );
    while ( * p && ! quote )
    {
        char c = * p ++;
	quote = ( c <= ' ' || c >= 0177 || c == '"'
				        || c == '#' );
    }
    if ( ! quote )
    {
        strcpy ( * buffer, lexeme );
	* buffer += strlen ( * buffer );
    }
    else
    {
        p = lexeme;
	char * q = * buffer;
	* q ++ = '"';
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

/* Read index from file stream.
 */
void read_index ( FILE * f )
{
    line_buffer buffer;
    int begin = 1;
    char * filename;
    while ( get_line ( buffer, f ) )
    {
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

	char * b = buffer;
	if ( * b == 0 || isspace ( * b ) )
	{
	    printf ( "ERROR: index entry first line"
	             " begins with space, or there"
		     " is a blank line in"
		     " index\n    %s\n", buffer );
	    exit ( 1 );
	}
	filename = strdup ( get_lexeme ( & b ) );
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " filename for file %s\n",
		     filename );
	    exit ( 1 );
	}

	if ( ! get_line ( buffer, f ) )
	{
	    printf ( "ERROR: premature end of entry"
		     " for file %s\n", filename );
	    exit ( 1 );
	}
	b = buffer;
	if ( ! isspace ( * b ) )
	{
	    printf ( "ERROR: index entry non-first line"
	             " does not begin with"
		     " space,\n    for %s entry",
		     filename );
	    exit ( 1 );
	}
	char * mode = get_lexeme ( & b );
	char * mtime = get_lexeme ( & b );
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " mode and mtime for file %s\n",
		     filename );
	    exit ( 1 );
	}
	char * q;
	unsigned long m = strtoul ( mode, & q, 8 );
	if ( * q || m >= ( 1 << 16 ) )
	{
	    printf ( "ERROR: bad EFM-INDEX mode (%s)"
	             " for file %s\n",
		     mode, filename );
	    exit ( 1 );
	}
	struct tm td;
	const char * ts =
	    (const char *)
	    strptime ( mtime, time_format, & td );
	time_t d = ( ts == NULL || * ts != 0 ) ? -1 :
	           mktime ( & td );
	if ( d == -1 )
	{
	    printf ( "ERROR: bad EFM-INDEX mtime (%s)"
	             " for file %s\n",
		     mtime, filename );
	    exit ( 1 );
	}

	if ( ! get_line ( buffer, f ) )
	{
	    printf ( "ERROR: premature end of entry"
		     " for file %s\n", filename );
	    exit ( 1 );
	}
	b = buffer;
	if ( ! isspace ( * b ) )
	{
	    printf ( "ERROR: index entry non-first line"
	             " does not begin with"
		     " space,\n    for %s entry",
		     filename );
	    exit ( 1 );
	}
	char * md5sum = get_lexeme ( & b );
	char * key = get_lexeme ( & b );
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " MD5 sum and key for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( strlen ( md5sum ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX md5sum (%s)"
	             " for file %s\n",
		     md5sum, filename );
	    exit ( 1 );
	}
	if ( strlen ( key ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX key (%s)"
	             " for file %s\n",
		     key, filename );
	    exit ( 1 );
	}

	struct entry * e =
	    (struct entry * )
	    malloc ( sizeof ( struct entry ) );
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

/* Write index entry into file stream.
 */
void write_index_entry
	( FILE * f, struct entry * e,
	  const char * prefix )
{
    line_buffer buffer;
    char * b = buffer;
    put_lexeme ( & b, e->filename );
    * b = 0;
    fprintf ( f, "%s%s\n", prefix, buffer );
    b = buffer;
    strcpy ( b, "    " );
    b += 4;
    sprintf ( b, "%04o", e->mode );
    b += 4;
    * b ++ = ' ';
    char tbuffer [100];
    strftime ( tbuffer, 100, time_format, 
	       gmtime ( & e->mtime ) );
    put_lexeme ( & b, tbuffer );
    * b = 0;
    fprintf ( f, "%s%s\n", prefix, buffer );
    b = buffer;
    strcpy ( b, "    " );
    b += 4;
    put_lexeme ( & b, e->md5sum );
    * b ++ = ' ';
    put_lexeme ( & b, e->key );
    * b = 0;
    fprintf ( f, "%s%s\n", prefix, buffer );
}

/* Write index into file stream.
 */
void write_index ( FILE * f )
{
    struct comment * c = first_comment;
    if ( c ) do
    {
        fprintf ( f, "%s\n", c->line );
    } while ( ( c = c->next ) != first_comment );
    struct entry * e = first_entry;
    if ( e ) do write_index_entry ( f, e, "" );
    while ( ( e = e->next ) != first_entry );
}

/* Check if index has existing file with given filename.
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

/* Check if index has existing file with given MD5sum.
 * Return entry if yes, NULL if no.
 */
struct entry * find_md5sum ( const char * md5sum )
{
    struct entry * e = first_entry;
    if ( e ) do
    {
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
    const char * s = strerror ( errno );
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
    int fd = open ( "/dev/random", O_RDONLY );
    if ( fd < 0 ) error ( errno );
    unsigned char b[16];
    if ( read ( fd, b, 16 ) < 0 ) error ( errno );
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
 * and given user only read/write mode.  Password is
 * plength string of bytes.  If error, return -1.
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
	    if ( dup2 ( passfd, 3 ) < 0 )
		error ( errno );
	    close ( passfd );
	}
	int fd = getdtablesize();
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

/* Compute the MD5 sum of a file.  The 32 character
 * md5sum followed by a NUL is returned in the buffer,
 * which must be at least 33 characters long.  0 is
 * returned on success, -1 on error.  Error messages
 * are written on stdout.
 */
int md5sum ( char * buffer,
             const char * filename )
{
    int fd[2];
    if ( pipe ( fd ) < 0 ) error ( errno );

    fflush ( stdout );

    int child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
        close ( fd[0] );

	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> fd[1]
	 *	2 -> parent's 1
	 */
	int newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	close ( 1 );
	if ( dup2 ( fd[1], 1 ) < 0 ) error ( errno );
	close ( fd[1] );
	int d = getdtablesize();
	while ( d > 2 ) close ( d -- );

	if ( execlp ( "md5sum", "md5sum",
	              filename, NULL ) < 0 )
	    error ( errno );
    }

    close ( fd[1] );
    FILE * inf = fdopen ( fd[0], "r" );

    line_buffer line;
    int e = -1;

    if ( fgets ( line, sizeof ( line ), inf ) )
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
 * may have the format acceptable to scp.
 */
int copyfile
	( const char * source, const char * target )
{
    fflush ( stdout );

    int child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> parent's 1
	 *	2 -> parent's 1
	 */
	int newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	int d = getdtablesize();
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
    fflush ( stdout );
    line_buffer buffer;
    strcpy ( buffer, filename );
    char * p = buffer;
    int at_found = 0;
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
	    printf ( "ERROR: could not delete %s\n",
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

    int child = fork();
    if ( child < 0 ) error ( errno );

    if ( child == 0 )
    {
	/* Set fd's as follows:
	 * 	0 -> /dev/null
	 *	1 -> parent's 1
	 *	2 -> parent's 1
	 */
	int newfd = open ( "/dev/null", O_RDONLY );
	if ( newfd < 0 ) error ( errno );
	close ( 0 );
	if ( dup2 ( newfd, 0 ) < 0 ) error ( errno );
	close ( newfd );
	close ( 2 );
	if ( dup2 ( 1, 2 ) < 0 ) error ( errno );
	int d = getdtablesize();
	while ( d > 2 ) close ( d -- );

	if ( execlp ( "ssh", "buffer", "rm", "-f",
	              p, NULL ) < 0 )
	    error ( errno );
    }

    return cwait ( child );
}

/* Add file entry to index.  Return -1 on error, 0 on
 * success.
 */
int add ( const char * filename )
{
    if ( find_filename ( filename ) )
    {
        printf ( "ERROR: index entry already exists"
	         " for %s\n", filename );
	return -1;
    }

    char sum [33];
    if ( md5sum ( sum, filename ) < 0 ) return -1;
    struct entry * e = find_md5sum ( sum );
    if ( e != NULL )
    {
        printf ( "ERROR: cannot make index entry for"
	         " %s\n    as it has the same MD5 sum"
		 " as existing entry for %s\n",
		 filename, e->filename );
	return -1;
    }

    struct stat s;
    if ( stat ( filename, & s ) < 0 )
    {
        printf ( "ERROR: cannot stat %s\n", filename );
	return -1;
    }

    char key[33];
    newkey ( key );

    e = (struct entry *)
        malloc ( sizeof ( struct entry ) );
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
	write_index_entry ( stdout, e, "* " );
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
	write_index_entry ( stdout, e, "* " );
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
    if ( eol_found ) return NULL;
    if ( ! get_line ( buffer, in ) )
    {
        eol_found = 1;
	return NULL;
    }
    char * b = buffer;
    char * r = get_lexeme ( & b );
    char * j = get_lexeme ( & b );
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
 * ten to stdout.  Return 1 to if kill command processed
 * (do nothing else for kill), and 0 otherwise.
 */
int execute_command ( FILE * in )
{
    int result = 0;
    int error_found = 0;
    line_buffer buffer, directory;
    eol_found = 0;
    char * arg = get_argument ( buffer, in );

    if ( arg == NULL ) return 0;
    else if ( strcmp ( arg, "start" ) == 0 )
        /* Do Nothing */;
    else if ( strcmp ( arg, "kill" ) == 0 )
    {
	printf ( "efm killed\n" );
        result = 1;
    }
    else if ( strcmp ( arg, "list" ) == 0 )
	write_index ( stdout );
    else if ( strcmp ( arg, "add" ) == 0 )
    {
        while ( arg = get_argument ( buffer, in ) )
	{
	    if ( add ( arg ) < 0 ) error_found = 1;
	}
    }
    else if ( strcmp ( arg, "sub" ) == 0 )
    {
        while ( arg = get_argument ( buffer, in ) )
	{
	    if ( sub ( arg ) < 0 ) error_found = 1;
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
	    error_found = 1;
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
		if ( e == NULL )
		{
		    if ( direction != 't' )
		    {
		        printf ( "ERROR: no index entry"
			         " exists for ", arg );
			error_found = 1;
			continue;
		    }
		    if ( add ( arg ) < 0 )
		    {
			error_found = 1;
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
			error_found = 1;
			continue;
		    }
		    if ( strcmp ( sum, e->md5sum )
		         != 0 )
		    {
		        printf ( "ERROR: M5 sum %s\n"
			         "    of existing file"
				 " %s\n"
				 "    does not match"
				 " the MD5 sum %s in"
				 " the index\n",
				 sum, arg, e->md5sum );
			error_found = 1;
			continue;
		    }
		} else if ( direction == 't' ) {
		    printf ( "ERROR: file not found:"
		             " %s\n", arg );
		    error_found = 1;
		    continue;
		}

		/* Perform Copying */

		char efile [40];
		strcpy ( efile, e->md5sum );
		strcpy ( efile + 32, ".gpg" );
		pid_t child;
		strcpy ( dend, efile );
		if ( direction == 't' )
		{
		    if ( trace )
		        printf ( "* encrypting %s to"
			         " make %s\n",
				 arg, efile );
		    unlink ( efile );
		    if ( crypt ( 0, arg, efile,
		                 e->key, 32,
				 & child ) < 0 )
		    {
		        printf ( "ERROR: could not"
			         " encrypt %s\n", arg );
			error_found = 1;
			continue;
		    }
		    if ( trace )
		        printf ( "* changing mode of %s"
			         " to user-only"
				 " read-only\n",
				 efile );
		    if ( chmod ( efile, S_IRUSR ) < 0 )
		    {
		        printf ( "ERROR: cannot chmod"
			         " %s\n", efile );
			error_found = 1;
			continue;
		    }
		    if ( ! current_directory )
		    {
			if ( trace )
			    printf ( "* deleting %s\n",
			             dbegin );
			if ( delfile ( dbegin ) < 0 )
			{
			    error_found = 1;
			    continue;
			}
			if ( trace )
			    printf ( "* copying %s to"
			             " %s\n",
			             efile, dbegin );
			if ( copyfile ( efile, dbegin )
			     < 0 )
			{
			    error_found = 1;
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
		    if ( ! current_directory )
		    {
			if ( trace )
			    printf ( "* copying %s to"
			             " %s\n",
			             dbegin, efile );
			unlink ( efile );
			if ( copyfile ( dbegin, efile )
			     < 0 )
			{
			    error_found = 1;
			    continue;
			}
		    }
		    else if ( stat ( efile, & st ) < 0 )
		    {
		        printf ( "ERROR: encrypted %s\n"
			         "    (%s) does not"
				 " exist\n",
				 arg, efile );
			error_found = 1;
			continue;
		    }
		    if ( trace )
		        printf ( "* decrypting %s to"
			         " make %s\n",
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
			error_found = 1;
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
		    char sum [33];
		    if ( md5sum ( sum, e->md5sum ) < 0 )
		    {
		        printf ( "ERROR: cannot compute"
			         " MD5 sum of %s\n    "
				 "which is the"
				 " retrieval of %s\n",
				 e->md5sum, arg );
			error_found = 1;
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
			error_found = 1;
			continue;
		    }
		    if ( op != 'k' )
		    {
			if ( trace )
			    printf
				( "* linking %s to"
				  " %s\n",
				  e->md5sum, arg );
		        unlink ( arg );
			if ( link ( e->md5sum, arg )
			     < 0 )
			{
			    printf ( "ERROR: cannot"
			             " rename %s to"
				     " %s\n",
				     e->md5sum, arg );
			    error_found = 1;
			    continue;
			}
			if ( trace )
			    printf
				( "* changing mode and"
				  " modification time"
				  " of %s\n", arg );
			if ( chmod ( arg, e->mode )
			     < 0 )
			    printf ( "ERROR: cannot"
			             " chmod %s\n",
				     arg );
			struct utimbuf ut;
			ut.actime = time ( NULL );
			ut.modtime = e->mtime;
			if ( utime ( arg, & ut ) < 0 )
			    printf ( "ERROR: cannot set"
				     " modification"
				     " time of %s\n",
				     arg );
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
			error_found = 1;
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
			error_found = 1;
			continue;
		    }
		}

		/* Delete entry if necessary. */

		if ( ( op == 'm' && direction == 'f' )
		     || op == 'r' )
		{
		    int r = sub ( arg );
		    assert ( r >= 0 );
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
	    error_found = 1;
	}
    }
    else
    {
        printf ( "ERROR: bad command (ignored):"
	         " %s\n", arg );
	error_found = 1;
    }

    arg = get_argument ( buffer, in );
    if ( arg != NULL && ! error_found )
	printf ( "ERROR: extra argument (ignored):"
		 " %s\n", arg );

    while ( arg != NULL )
        arg = get_argument ( buffer, in );

    return result;
}

char * password = NULL;
    /* Password read from user for EFM-INDEX.*. */

int main ( int argc, char ** argv )
{
    line_buffer buffer;

    if ( argc < 2
         ||
	 strncmp ( argv[1], "-doc", 4 ) == 0 )
    {
	printf ( documentation );
	exit (1);
    }

    int tofd = socket ( PF_UNIX, SOCK_STREAM, 0 );
    if ( tofd < 0 ) error ( errno );
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    strcpy ( sa.sun_path, "EFM-INDEX.sock" );
    if ( connect ( tofd,
                   (const struct sockaddr *) & sa,
		   sizeof ( sa ) ) < 0 )
    {
	if ( errno == ECONNREFUSED )
	    unlink ( sa.sun_path );
	else if ( errno != ENOENT )
	    error ( errno );

	const char * pass = getpass( "Password: " );
	if ( pass == NULL ) error ( errno );
	password = strdup ( pass );

	int indexchild;
	int indexfd =
	    crypt ( 1, "EFM-INDEX.gpg", NULL,
	            password,
		    strlen ( password ),
		    & indexchild );
	if ( indexfd < 0 ) exit ( 1 );
	FILE * indexf = fdopen ( indexfd, "r" );
	read_index ( indexf );
	fclose ( indexf );
	if ( cwait ( indexchild ) < 0 )
	{
	    printf ( "ERROR: error decypting"
	             " EFM-INDEX.gpg\n" );
	    exit ( 1 );
	}
	int listenfd =
	    socket ( PF_UNIX, SOCK_STREAM, 0 );
	if ( bind ( listenfd,
		    (const struct sockaddr *) & sa,
		    sizeof ( sa ) ) < 0 )
	    error ( errno );
	if ( listen ( listenfd, 0 ) ) error ( errno );
	pid_t childpid = fork ( );
	if ( childpid < 0 ) error ( errno );
	if ( childpid == 0 )
	{
	    struct stat st;
	    close ( tofd );

	    /* Temporarily reroute stdout to error
	     * descriptor.
	     */
	    fflush ( stdout );
	    close ( 1 );
	    dup2 ( 2, 1 );

	    int done = 0;
	    while ( ! done )
	    {
		int fromfd =
		    accept ( listenfd, NULL, NULL );
		if ( fromfd < 0 ) error ( errno );

		/* Reroute stdout to new descriptor.
		 */
		assert ( fromfd != 1 );
		fflush ( stdout );
		close ( 1 );
		dup2 ( fromfd, 1 );
		FILE * inf = fdopen ( fromfd, "r" );

		index_modified = 0;
		done = execute_command ( inf );
		if ( index_modified )
		{
		    int indexchild;
		    int indexfd =
			crypt ( 0,
				NULL, "EFM-INDEX.gpg+",
				password,
				strlen ( password ),
				& indexchild );
		    if ( indexfd < 0 ) exit ( 1 );
		    FILE * indexf =
		        fdopen ( indexfd, "w" );
		    if ( trace )
		        printf ( "* writing"
			         " EFM-INDEX.gpg+\n" );
		    write_index ( indexf );
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

		printf ( "%s\n", END_STRING );
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
    FILE * tof = fdopen ( tofd, "a+" );

    /* Send arguments to child.  Each argument sent as
     * a lexeme on its own line.  At the end of the
     * argument list, a blank line is written.
     */
    char ** argp = argv + 1;
    for ( ; * argp; ++ argp )
    {
        char * b = buffer;
	if ( strlen ( * argp ) > MAX_LEXEME_SIZE )
	{
	    printf ( "ERROR: program argument too long:"
	             " %s\n", * argp );
	    exit ( 1 );
	}
	put_lexeme ( & b , * argp );
	* b ++ = 0;
	fprintf ( tof, "%s\n", buffer );
    }
    fprintf ( tof, "\n" );
    while ( get_line ( buffer, tof ) )
    {
        if ( strcmp ( buffer, END_STRING ) == 0 )
	    break;

	/* Filter out unwanted missives from gpg. */

	char ** fp = ofilter;
	for ( ; * fp; ++ fp )
	{
	    if ( strcmp ( * fp, buffer ) == 0 ) break;
	}
	if ( * fp == NULL ) printf ( "%s\n", buffer );
    }
    fclose ( tof );
    exit (0);
}
