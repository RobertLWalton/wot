/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	efm.c
** Date:	Sun Aug  6 08:24:51 EDT 2006
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2006/08/06 12:24:42 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.9 $
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
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
"efm moveto target file ...\n"
"efm movefrom source file ...\n"
"efm copyto target file ...\n"
"efm copyfrom source file ...\n"
"efm remove source file ...\n"
"efm check source file ...\n"
"efm list\n"
"\n"
"    Each file has an encrypted version in the target\n"
"    directory or source directory.  The file can be\n"
"    moved or copied to/from that directory.  It is\n"
"    encrypted when moved or copied to the target di-\n"
"    rectory, and decrypted when moved or copied from\n"
"    the directory.  The directory may be \".\" to\n"
"    encrypt or decrypt in place.\n"
"\n"
"    The \"remove\" command is like \"movefrom\" fol-\n"
"    lowed by discarding the decrypted file.  The\n"
"    \"check\" command is like \"copyfrom\" followed\n"
"    by discarding the decrypted file.  The \"list\"\n"
"    command lists all encrypted files and their\n"
"    MD5sums, modification times, and protection\n"
"    modes.\n"
"\n"
"    File names must be relative to the current direc-\n"
"    tory.  Source and target names can be any direc-\n"
"    tory names acceptable to scp.\n"
"\n"
"    The current directory must contain an encrypted\n"
"    index file named \"EFM-INDEX.gpg\".  When de-\n"
"    crypted the lines of this file are in pairs of\n"
"    the form:\n"
"\n"
"	 filename\n"
"            MD5sum mode date key\n"
"\n"
"    where the first line is not indented and the\n"
"    second line is.  The filename may be quoted\n"
"    with \"'s if it contains special characters,\n"
"    and a quote in such a filename is represented\n"
"    by a pair of quotes (\"\").  The date will\n"
"    always be quoted.\n"
"\n"
"    Lines at the beginning of the file whose first\n"
"    character is # are comment lines, and are\n"
"    preserved.  Blank lines are forbidden.\n"
"\n"
"    A line for a file is created when the file is\n"
"    encrypted and deleted when the encrypted file\n"
"    is deleted by \"movefrom\" or \"remove\".  If\n"
"    a file value is changed it may not be reencryp-\n"
"    ted until it has been removed.  No two files\n"
"    are allowed to have the same MD5sum.\n"
"\n"
"    The date and mode are used to set the file modi-\n"
"    fication date and mode of the file when it is\n"
"    decrypted.  The MD5sum is used to check the in-\n"
"    grity of the decryption.  The mode is 4 octal\n"
"    digits and the MD5sum is 16 hexadecimal digits.\n"
"    The key is the symmetric encryption/decryption\n"
"    key for the file, and is the uppercase 16 digit\n"
"    hexadecimal representation of a 128 bit random\n"
"    number.  However, it is this 16 character repre-\n"
"    sentation, and NOT the number, that is the key.\n"
"\n"
"    You must create and encrypt an initial empty\n"
"    EFM-INDEX.gpg file by using the gpg program.\n"
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
"\n"
"    The efm program asks for a password to decrypt\n"
"    the index only the first time it is run during\n"
"    a login session.  It then sets up a background\n"
"    program holding the password that is accessible\n"
"    through the socket \"EFM-INDEX.sock\".  This\n"
"    background program dies on a hangup signal when\n"
"    you log out, and may be killed at any time.\n"
;

#define MAX_LINE_SIZE 4000
const char * time_format = "%Y/%m/%d %H:%M:%S";

/* Data is a list of entries, one entry per line.
 */
struct entry {

    char * filename;
        /* Filename may not contain \0 or \n and may
	 * not be more than (MAX_LINE_SIZE-2)/2
	 * characters long (so its quoted lexeme will
	 * fit on one line).
	 */

    char * md5sum;
    unsigned mode;
    time_t date;
    char * key;

    struct entry * next;
};
struct entry * first_entry = NULL;
struct entry * last_entry = NULL;

/* Comment lines are just a list of lines.
 */
struct comment {
    char * line;

    struct comment * next;
};
struct comment * first_comment = NULL;
struct comment * last_comment = NULL;

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
 * non-graphic characters, #, or ".  The buffer pointer
 * is up dated.
 */
void put_lexeme ( char ** buffer, const char * lexeme )
{
    const char * p = lexeme;
    int quote = 0;
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
    char buffer [MAX_LINE_SIZE+2];
    int begin = 1;
    int even = 1;
    char * filename;
    while ( fgets ( buffer, 4000, f ) )
    {
        int length = strlen ( buffer );
        if ( length == MAX_LINE_SIZE+1 )
	{
	    printf ( "ERROR: EFM-INDEX line too"
	             " long\n" );
	    exit ( 1 );
	}
	assert ( buffer[length-1] == '\n' );
	buffer[length-1] = 0;

	if ( begin && buffer[0] == '#' )
	{
	    struct comment * c =
	        (struct comment *)
		malloc ( sizeof ( struct comment ) );
	    c->line = strdup ( buffer );
	    c->next = NULL;
	    if ( first_comment == NULL )
		last_comment = first_comment = c;
	    else
		last_comment = last_comment->next = c;
	    continue;
	}
	begin = 0;
	char * b = buffer;

	if ( even )
	{
	    filename = strdup ( get_lexeme ( & b ) );
	    if ( get_lexeme ( & b ) )
	    {
		printf ( "ERROR: stuff on line after"
		         " filename for file %s\n",
			 filename );
		exit ( 1 );
	    }
	    even = 0;
	    continue;
	}
	even = 1;

	char * md5sum = get_lexeme ( & b );
	char * mode = get_lexeme ( & b );
	char * date = get_lexeme ( & b );
	char * key = get_lexeme ( & b );

	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " key for file %s\n",
		     filename );
	    exit ( 1 );
	}

	if ( strlen ( md5sum ) != 16 )
	{
	    printf ( "ERROR: bad EFM-INDEX md5sum (%s)"
	             " for file %s\n",
		     md5sum, filename );
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
	char * ts = strptime ( date, time_format, & td );
	time_t d = ( ts == NULL || * ts != 0 ) ? -1 :
	           mktime ( & td );
	if ( d == -1 )
	{
	    printf ( "ERROR: bad EFM-INDEX date (%s)"
	             " for file %s\n",
		     date, filename );
	    exit ( 1 );
	}
	if ( strlen ( key ) != 16 )
	{
	    printf ( "ERROR: bad EFM-INDEX md5sum (%s)"
	             " for file %s\n",
		     key, filename );
	    exit ( 1 );
	}

	struct entry * e =
	    (struct entry * )
	    malloc ( sizeof ( struct entry ) );
	e->filename = filename;
	e->md5sum = strdup ( md5sum );
	e->mode = m;
	e->date = d;
	e->key = strdup ( key );
	e->next = NULL;
	if ( first_entry == NULL )
	    last_entry = first_entry = e;
	else
	    last_entry = last_entry->next = e;
    }
}

/* Write index into file stream.
 */
void write_index ( FILE * f )
{
    struct comment * c = first_comment;
    for ( ; c; c = c->next )
        fprintf ( f, "%s\n", c->line );
    struct entry * e = first_entry;
    char buffer [ MAX_LINE_SIZE + 1 ];
    for ( ; e; e = e->next )
    {
        char * b = buffer;
	put_lexeme ( & b, e->filename );
	* b = 0;
        fprintf ( f, "%s\n", buffer );
	b = buffer;
	strcpy ( b, "    " );
	b += 4;
	put_lexeme ( & b, e->md5sum );
	* b ++ = ' ';
	sprintf ( b, "%04o", e->mode );
	b += 4;
	* b ++ = ' ';
	char tbuffer [100];
	strftime ( tbuffer, 100, time_format, 
	           gmtime ( & e->date ) );
	put_lexeme ( & b, tbuffer );
	* b ++ = ' ';
	put_lexeme ( & b, e->key );
	* b = 0;
        fprintf ( f, "%s\n", buffer );
    }
}

/* Check if index has existing file with given MD5sum.
 * Return entry if yes, NULL if no.
 */
struct entry * find_md5sum ( const char * md5sum )
{
    struct entry * e = first_entry;
    for ( ; e; e = e->next )
    {
        if ( strcmp ( md5sum, e->md5sum ) == 0 )
	    return e;
    }
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
		        "--batch", "-q",
		        NULL ) :
	       execlp ( "gpg", "gpg", "-c"
		        "--cipher-alog", "BLOWFISH",
	                "--passphrase-fd", "3",
		        "--batch", "-q",
		        NULL )
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

char * password = NULL;
    /* Password read from user for EFM-INDEX.*. */

int main ( int argc, char ** argv )
{

    if ( argc < 2 )
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
	            password, 3, & indexchild );
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

	write_index ( stdout );

	int listenfd = socket ( PF_UNIX, SOCK_STREAM, 0 );
	if ( bind ( listenfd,
		    (const struct sockaddr *) & sa,
		    sizeof ( sa ) ) < 0 )
	    error ( errno );
	if ( listen ( listenfd, 0 ) ) error ( errno );
	pid_t childpid = fork ( );
	if ( childpid < 0 ) error ( errno );
	if ( childpid == 0 )
	{
	    char buffer [1000];
	    close ( tofd );
	    int fromfd =
	        accept ( listenfd, NULL, NULL );
	    if ( fromfd < 0 ) error ( errno );
	    FILE * fromf = fdopen ( fromfd, "r" );
	    fgets ( buffer, 1000, fromf );
	    printf ( "%s\n", buffer );
	    printf ( "CHILD DONE\n" );
	    exit ( 0 );
	}
	if ( connect ( tofd,
		       (const struct sockaddr *) & sa,
		       sizeof ( sa ) ) < 0 )
	    error ( errno );
    }

    sleep ( 2 );
    write ( tofd, "MESSAGE\n", 8 );
    sleep ( 2 );
    printf ( "PARENT DONE\n" );
    exit (0);
}
