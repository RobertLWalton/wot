/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@acm.org)
** File:	efm.c
** Date:	Wed Jul 24 04:40:11 AM EDT 2024
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2009/03/21 19:37:48 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.71 $
*/

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED
#define crypt CRYPT
    /* We redefine crypt */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#undef crypt
    /* We redefine crypt */

const char * documentation [] = {
"efm -doc",
"",
"efm moveto target file ...",
"efm movefrom source file ...",
"efm copyto target file ...",
"efm copyfrom source file ...",
"efm check source file ...",
"efm md5check source file ...",
"efm remove target file ...",
"",
"efm list [file ...]",
"efm listkeys [file ...]",
"efm listfiles [file ...]",
"",
"efm start",
"efm kill",
"",
"efm trace on",
"efm trace off",
"efm trace",
"",
"efm listall [file ...]",
"efm listallkeys [file ...]",
"efm listcurfiles [file ...]",
"efm listobsfiles [file ...]",
"efm listallfiles [file ...]",
"",
"efm cur file ...",
"efm obs file ...",
"efm add file ...",
"efm sub file ...",
"efm del source file ...",
"",
"efm s3cmd ...",
"\f",
"    A file in the current directory may have an en-",
"    crypted version in the target/source directory.",
"    The file can be moved or copied to/from that",
"    directory.  It is encrypted when moved or copied",
"    to the target directory, and decrypted when",
"    moved or copied from the source directory.  The",
"    target/source directory may be \".\" to encrypt",
"    or decrypt in place.",
"",
"    Move is like copy except that the source file",
"    copied is also deleted.",
"",
"    The \"remove\" command is like \"movefrom\" fol-",
"    lowed by discarding the decrypted file.  The",
"    \"check\" command is like \"copyfrom\" followed",
"    by discarding the decrypted file.  If there is",
"    an existing decrypted file, \"check\" also tests",
"    that its MD5 sum matches that of the retrieved",
"    decrypted file.  Neither of these two commands",
"    deletes or alters any existing decrypted file.",
"",
"    The \"md5check\" command checks the MD5 sums of",
"    any existing encrypted and/or decrypted files.",
"",
"    File names must not contain any '/'s (files must",
"    be in the current directory).  Source and target",
"    names can be any directory names acceptable to",
"    scp or s3cmd.  Efm makes temporary files in the",
"    current directory whose base names are the 32",
"    character MD5 sums of the decrypted files.  No",
"    two encrypted files may have the same MD5 sum.",
"    It is expected that files will be tar files of",
"    directories.",
"\f",
"    Efm maintains an index of encrypted files.  This",
"    index is itself encrypted, and is stored in the",
"    file named \"EFM-INDEX.gpg\".  You must create",
"    and encrypt an initial empty index by executing:",
"",
"               echo > EFM-INDEX",
"               gpg -c EFM-INDEX",
"",
"    You choose the password that protects the index",
"    when you do this.  You may put comments at the",
"    beginning of EFM-INDEX before you encrypt.  All",
"    comment lines must have `#' as their first char-",
"    acter, and there can be no blank lines.",
"",
"    If S3 is used, there must also be an encrypted",
"    file \"EFM-KEYS.gpg\" whose decrypted version has",
"    lines of the form:",
"",
"         s3_access_key:<S3-ACCESS-KEY>",
"         s3_secret_key:<S3-SECRET-ACCESS-KEY>",
"",
"    This decrypted file may also contain comment",
"    lines that begin with `#'.  This file is",
"    encrypted with the same password as",
"    \"EFM-INDEX.gpg\".",
"",
"    The index is REQUIRED to decrypt files, as each",
"    encrypted file has its own unique random encryp-",
"    tion key listed in the index.  If a user wants",
"    to change the password used to protect the en-",
"    crypted files, the user changes just the pass-",
"    word of the index, and not the keys encrypting",
"    the files.",
"\f",
"    Once listed in the index, files are not normally",
"    removed from the index.  Instead index entries",
"    are marked as being either current or obsolete.",
"    The \"moveto\" and \"copyto\" commands make a",
"    file's entry current.  The \"movefrom\" and"
					" \"re-",
"    move\" commands make a file's entry obsolete.",
"    The \"copyfrom\" command does not change index.",
"    The \"del\" command is like \"remove\" but does",
"    not change the index.",
"",
"    The \"list\" command lists for current index",
"    entries the file name, protection mode, modifi-",
"    cation time, and size.  The \"listkeys\" command",
"    does the same but includes the decrypted file MD5",
"    sum, encrypted file size, encrypted file MD5 sum,",
"    and encryption key.  The \"listfiles\" command",
"    only lists file names, and produces no error mes-",
"    sages if the named files are not current in the",
"    index.  If no file arguments are given to these",
"    commands, all current index entries are listed.",
"",
"    The list commands can also be given encrypted",
"    file names (that consist of MD sum basenames",
"    plus .gpg extension).",
"",
"    This program returns exit status 0 if there is",
"    no error and if there is an error, returns exit",
"    status 1 and prints the error message to the",
"    standard output.",
"\f",
"    The efm program asks for a password to decrypt",
"    the index only the first time it is run during",
"    a login session.  It then sets up a background",
"    program holding the password that is accessible",
"    through the socket \"EFM-INDEX.sock\".  The",
"    \"kill\" command may be use to kill the back-",
"    ground process if it exists.  The \"start\" com-",
"    mand just starts the background process if it is",
"    not already running, but this is also done by",
"    other commands implicitly.",
"",
"    It is recommended that you put",
"",
"            ( cd backup-directory; efm kill )",
"",
"    in your .logout or .bash_logout file to kill any",
"    background process on logout.",
"",
"    The \"trace\" commands turn tracing on/off.",
"    When on, actions are annotated on the standard",
"    output by lines beginning with \"* \".  The",
"    \"trace\" command without any \"on\" or \"off\"",
"    argument just prints the current trace status.",
"",
"    The index file contains four line entries of",
"    the form:",
"",
"	 indicator filename",
"            mode mtime size md5sum",
"            esize emd5sum",
"            key",
"\f",
"    where the first entry line is not indented and",
"    the other lines are.  The filename may be quoted",
"    with \"'s if it contains special characters,",
"    and a quote in such a filename is represented",
"    by a pair of quotes (\"\").  The mtime (file",
"    modification time) will always be quoted, and",
"    is Greenwich Mean Time (GMT).  Esize may be 0",
"    to indicate its value is not known, and emd5sum",
"    may be \"\" to indicate its value is not known.",
"",
"    Lines at the beginning of the index file whose",
"    first character is # are comment lines, and are",
"    preserved.  Blank lines are forbidden.  Comment",
"    lines must be inserted in the initial file made",
"    with gpg, or changed by using gpg to decrypt",
"    and re-encrypt the file.",
"",
"    The indicator is + if the entry is current, and",
"    - if the entry is obsolete.  The mode is 4 octal",
"    digits, and is used to set the file mode when",
"    the file is decrypted.  The mtime is quoted GMT",
"    time and is used to set the file modification",
"    time when the file is decrypted.  The size is",
"    the size of the decrypted file.  The md5sum is",
"    the 32 hexadecimal digit MD5 sum of the decryp-",
"    ted file, and is used as the basename of the en-",
"    crypted file, as the name of a temporary decryp-",
"    ted file, and to check the integrity of decryp-",
"    tion.  The esize is the size of the encrypted",
"    file, and the emd5sum is the 32 hexadecimal digit",
"    MD5 sum of the encrypted file.  The key is the",
"    symmetric encryption/decryption password for the",
"    file, and is the upper-case 32 digit hexadecimal",
"    representation of a 128 bit random number.  How-",
"    ever, it is this 32 character representation,",
"    and NOT the random number, that is the key.",
"\f",
"    No two current files in the index are allowed to",
"    have the same MD5 sum.  Two files (not both cur-",
"    rent) with the same MD5 sum will have the same",
"    key.",
"",
"    The \"listall\" and \"listallkeys\" commands are",
"    like \"list\" and \"listkeys\" respectively, but",
"    list both obsolete and current entries and also",
"    include indicators (+ or -).  The"
				" \"listobsfiles\"",
"    command is like \"listfiles\", but only lists",
"    obsolete entries, instead of only current en-",
"    tries.  The \"listallfiles\" command is like",
"    \"listfiles\" but lists both obsolete and cur-",
"    rent entries.  The \"listcurfiles\" command",
"    lists only current entries, and is just another",
"    name for \"listfiles\".",
"",
"    The following commands may be used to edit the",
"    index in ways that may get the index out of sync",
"    with existing files.  So be careful if you use",
"    these commands.",
"",
"    The \"cur\" command makes index entries cur-",
"    rent.  The \"obs\" command makes index entries",
"    obsolete.  The \"add\" command creates index en-",
"    tries without encrypting or moving files.  The",
"    \"sub\" command deletes index entries without",
"    decrypting or moving files.  The \"del\" command",
"    deletes encrypted files like \"remove\" but does",
"    not change index entries.",
"",
"    An external program is used to encrypt/decrypt",
"    files.  By default this is gpg.  The encrypted",
"    file name is MD5SUM.gpg with this default.  In",
"    general the encrypted file basename is the",
"    MD5sum of the file contents and the extension",
"    denotes the encrypting program.",
"\f",
"    Similarly the extension of the index indicates",
"    the program used to encrypt the index.",
"",
"    The \"s3cmd\" command simply executes a s3cmd(1)",
"    command with the given arguments and with the",
"    --access_key=... and --secret_key=... arguments",
"    added with values from EFM-KEYS.gpg.",
"",
"    Currently only gpg is supported as an encryp-",
"    ing program.",
NULL
};

int trace = 0;		/* 1 if trace on, 0 if off. */

int RETRIES = 3;	/* Number of retries. */

#define MAX_LEXEME_SIZE 2000
#define MAX_LINE_SIZE ( 2 * MAX_LEXEME_SIZE + 10 )
#define MAX_KEY_SIZE 200

typedef char line_buffer[MAX_LINE_SIZE+2];

const char * time_format = "%Y/%m/%d %H:%M:%S";
#define BEGIN_STRING "\001\002\003\004"
#define END_STRING "\001\002\003\005"

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
    if ( buffer[length-1] != '\n' )
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
    off_t size;
    char * md5sum;

    char * emd5sum;
    off_t esize;

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

/* Print error message from errno value and then call
 * exit ( 1 );
 */
void error ( int err_no )
{
    const char * s = strerror ( err_no );
    fprintf ( stderr, "ERROR: %s\n", s );
    exit ( 1 );
}

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
	return result;
    }
    else if ( * p )
    {
        while ( * p && ! isspace ( * p ) ) ++ p;
	if ( isspace ( * p ) ) * p ++ = 0;
	* buffer = p;
	return result;
    }
    else
    {
        * buffer = p;
    	return NULL;
    }
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

/* Read password from input terminal only.  On error
 * print error message to stdout and exit ( 1 ).
 *
 * Note: getpass(3) has been marked OBSOLETE.
 */
char password [MAX_KEY_SIZE+2];
void read_password ( void )
{
    struct termios tios;
    size_t len;
    if ( tcgetattr ( 0, & tios ) < 0 )
    {
	printf ( "ERROR: password required"
		 " and input is not"
		 " terminal\n" );
	exit ( 1 );
    }
    if ( ( tios.c_lflag & ICANON ) == 0
	 ||
	 ( tios.c_lflag & ECHO ) == 0 )
    {
	printf ( "ERROR: password required and"
		 " terminal is non-canonical or"
		 " not echoing\n" );
	exit ( 1 );
    }
    tios.c_lflag &= ~ ECHO;
    if ( tcsetattr ( 0, TCSANOW, & tios ) < 0 )
	error ( errno );
    printf ( "Password: " );
    fflush ( stdout );
    if ( fgets ( password, sizeof ( password ),
		 stdin ) == NULL )
	error ( errno );
    tios.c_lflag |= ECHO;
    if ( tcsetattr ( 0, TCSANOW, & tios ) < 0 )
	error ( errno );
    printf ( "\n" );

    len = strlen ( password );
    if ( password[len-1] != '\n' )
    {
	printf ( "ERROR: password too long\n" );
	exit ( 1 );
    }
    password[len-1] = 0;
}

/* Save of s3 configuration file.  Zero length if none
 * (yet).
 */
char s3_config[100000];

/* Create EFM-S3CONFIG.pipe but no NOT write into it.
 * Execute in PARENT process if child is going to
 * execute s3cmd.
 *
 * Does return -1 if EFM-S3CONFIG.gpg does not exist,
 * return 0 if no errors, and exit on system error.
 */
int setup_s3_pipe ( void )
{
    if ( trace )
    {
	fprintf ( stderr,
		  "Making and loading"
		  " EFM-S3CONFIG.pipe\n" );
	fflush ( stderr );
    }
    if ( s3_config[0] == 0 )
    {
	printf ( "ERROR: EFM-S3CONFIG.gpg missing\n" );
	return -1;
    }
    unlink ( "EFM-S3CONFIG.pipe" );
    if ( mkfifo ( "EFM-S3CONFIG.pipe", 0600 ) < 0 )
        error ( errno );
    return 0;
}

/* Write EFM-S3CONFIG into EFM-S3CONFIG.pipe.  Exits
 * if error.  Execute in parent AFTER STARTING child.
 */
void write_s3_pipe ( void )
{
    int fd = open ( "EFM-S3CONFIG.pipe", O_WRONLY );
    if ( fd < 0 ) error ( errno );
    if ( write ( fd, s3_config, strlen ( s3_config ) )
         < 0 )
        error ( errno );
    close ( fd );
}

/* Read index from file stream.  On error print error
 * message to stdout and exit ( 1 );
 */
int index_read = 0;
void read_index ( FILE * f )
{
    line_buffer buffer;
    int begin = 1;
    char * filename;

    index_read = 1;

    while ( get_line ( buffer, f ) )
    {
	struct tm td;
	char * b, * c, * q,
	     * mode, * mtime, * size, * md5sum,
	     * emd5sum, * esize,
	     * key;
	int current;
	unsigned long m, s, es;
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
	filename = get_lexeme ( & b );
	if ( filename == NULL )
	{
	    printf ( "ERROR: index entry begins badly"
		     "\n    %s\n", buffer );
	    exit ( 1 );
	}
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " filename,\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	filename = strdup ( filename );

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
	    printf ( "ERROR: index entry mtime"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	size = get_lexeme ( & b );
	if ( size == NULL )
	{
	    printf ( "ERROR: index entry size"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	md5sum = get_lexeme ( & b );
	if ( md5sum == NULL )
	{
	    printf ( "ERROR: index entry md5sum"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " md5sum,\n    for file %s\n",
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
	s = strtoul ( size, & q, 10 );
	if ( * q )
	{
	    printf ( "ERROR: bad EFM-INDEX size"
		     " (%s),\n    for file %s\n",
		     size, filename );
	    exit ( 1 );
	}
	if ( strlen ( md5sum ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX md5sum (%s),"
	             "\n    for file %s\n",
		     md5sum, filename );
	    exit ( 1 );
	}
	md5sum = strdup ( md5sum );

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
	esize = get_lexeme ( & b );
	if ( esize == NULL )
	{
	    printf ( "ERROR: index entry esize"
		     " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	emd5sum = get_lexeme ( & b );
	if ( emd5sum == NULL )
	{
	    printf ( "ERROR: index entry emd5sum"
	             " missing\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( get_lexeme ( & b ) )
	{
	    printf ( "ERROR: stuff on line after"
		     " emd5sum\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	es = strtoul ( esize, & q, 10 );
	if ( * q )
	{
	    printf ( "ERROR: bad EFM-INDEX esize"
		     " (%s),\n    for file %s\n",
		     esize, filename );
	    exit ( 1 );
	}
	if ( emd5sum[0] != 0
	     &&
	     strlen ( emd5sum ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX emd5sum"
	             " (%s),\n    for file %s\n",
		     emd5sum, filename );
	    exit ( 1 );
	}
	emd5sum = strdup ( emd5sum );

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
		     " key,\n    for file %s\n",
		     filename );
	    exit ( 1 );
	}
	if ( strlen ( key ) != 32 )
	{
	    printf ( "ERROR: bad EFM-INDEX key (%s),\n"
	             "    for file %s\n",
		     key, filename );
	    exit ( 1 );
	}
	key = strdup ( key );

	e = (struct entry * )
	    malloc ( sizeof ( struct entry ) );
	e->current  = current;
	e->filename = filename;
	e->mode     = m;
	e->mtime    = d;
	e->size     = s;
	e->md5sum   = md5sum;
	e->emd5sum  = emd5sum;
	e->esize    = es;
	e->key      = key;
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

/* Return 1 if EFM-INDEX.gpg read, otherwise print
 * error message and return 0.
 */
int check_index ( void )
{
    if ( index_read ) return 1;
    else
    {
    	printf ( "ERROR: EFM-INDEX.gpg not read\n" );
	return 0;
    }
}

/* Write index entry into file stream.  The mode
 * is a sum of flags that indicate what to list:
 *
 *	+0	List filename (always listed)
 *	+1	List current/obsolete indicator.
 *	+2	List mode, date, size.
 *	+6	List mode, date, size as for +2 and also
 *		list md5sum, esize, emd5sum, key.
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

    if ( ( mode & 2 ) != 0 )
    {
	char tbuffer [100];

	b = buffer;
	strcpy ( b, "    " );
	b += 4;

	sprintf ( b, "%04o", e->mode );
	b += 4;
	* b ++ = ' ';
	strftime ( tbuffer, 100, time_format, 
		   gmtime ( & e->mtime ) );
	put_lexeme ( & b, tbuffer );
	sprintf ( b, " %llu",
	          (unsigned long long) e->size );
	b += strlen ( b );
	if ( ( mode & 4 ) != 0 )
	{
	    * b ++ = ' ';
	    put_lexeme ( & b, e->md5sum );
	}
	* b = 0;
	fprintf ( f, "%s%s\n", prefix, buffer );
    }

    if ( ( mode & 4 ) != 0 )
    {
	b = buffer;
	strcpy ( b, "    " );
	b += 4;
	sprintf ( b, "%llu",
	          (unsigned long long) e->esize );
	b += strlen ( b );
	* b ++ = ' ';
	put_lexeme ( & b, e->emd5sum );
	* b = 0;
	fprintf ( f, "%s%s\n", prefix, buffer );

	b = buffer;
	strcpy ( b, "    " );
	b += 4;
	put_lexeme ( & b, e->key );
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

/* The background process ignores signals.  Its
 * children have default settings, and terminate.
 * The foreground process receives a BEGIN_STRING
 * line from the background process on connection,
 * uses it to set siggroup, and thereafter routes
 * signals to the process group named in the
 * BEGIN_STRING line.
 */

pid_t siggroup = 0;
void handler ( int signum )
{
    if ( siggroup > 0 ) killpg ( siggroup, signum );
}

void setcatch ( void )
{
    struct sigaction act;
    act.sa_flags = 0;
    sigfillset ( & act.sa_mask );
    act.sa_handler = handler;
    if ( sigaction ( SIGHUP, & act, NULL ) < 0 )
        error ( errno );
    if ( sigaction ( SIGINT, & act, NULL ) < 0 )
        error ( errno );
    if ( sigaction ( SIGQUIT, & act, NULL ) < 0 )
        error ( errno );
}

/* Wait for a child to terminate.  Return -1 if child
 * suffered error and 0 otherwise.
 */
int cwait ( pid_t child )
{
    int status;

    while ( waitpid ( child, & status, 0 ) < 0 )
    {
        if ( errno != EINTR ) error ( errno );
    }

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
    int fd, size, result, delays;
    unsigned char b[16];

    fd = open ( "/dev/random", O_RDONLY );
    if ( fd < 0 ) error ( errno );
    unsigned char * p = b;
    size = 0;
    delays = 0;
    while ( size < 16 )
    {
	result = read ( fd, b + size, 16 - size );
	if ( result < 0 ) error ( errno );
	if ( result == 0 )
	{
	    if ( delays == 2 )
	        printf ( "WAITING FOR RANDOM NUMBER"
		         " GENERATOR\n" );
	    sleep ( 1 );
	    ++ delays;
	}
	else size += result;
    }
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
 * Either the input file or the output file or both must
 * be non-NULL.
 *
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
    fflush ( stderr );

    * child = fork();
    if ( * child < 0 ) error ( errno );

    if ( * child == 0 )
    {
        int fd, nullfd;

	/* Set fd's as follows:
	 * 	0 -> infd
	 *	1 -> outfd
	 *	2 -> /dev/null
	 *	3 -> passfd
	 */
	close ( 0 );
	if ( dup2 ( infd, 0 ) < 0 ) error ( errno );
	close ( infd );
	close ( 1 );
	if ( dup2 ( outfd, 1 ) < 0 ) error ( errno );
	close ( outfd );

	/*
	nullfd = open ( "/dev/null", O_RDONLY );
	if ( nullfd < 0 ) error ( errno );
	close ( 2 );
	if ( dup2 ( nullfd, 2 ) < 0 ) error ( errno );
	close ( nullfd );
	*/

	if ( passfd != 3 )
	{
	    close ( 3 );
	    if ( dup2 ( passfd, 3 ) < 0 )
		error ( errno );
	    close ( passfd );
	}
	fd = getdtablesize() - 1;
	while ( fd > 3 ) close ( fd -- );

	if ( decrypt )
	{
	    if ( trace )
	    {
		fprintf ( stderr,
		          "* executing gpg --batch -q"
		          " --no-tty"
			  " --ignore-mdc-error" );
		if ( input != NULL )
		{
		    fprintf ( stderr, " \\\n"
			  "            < %s",
			  input );
		    if ( output != NULL )
			fprintf ( stderr, " \\\n"
			  "            > %s",
			  output );
		}
		else if ( output != NULL )
		    fprintf ( stderr, " \\\n"
		              "            > %s",
			      output );
		fprintf ( stderr, "\n" );
		fflush ( stderr );
	    }
	    if ( execlp ( "gpg", "gpg",
	                  "--passphrase-fd", "3",
		          "--batch", "-q", "--no-tty",
			  "--ignore-mdc-error",
		          NULL ) < 0 )
		error ( errno );
	}
	else
	{
	    if ( trace )
	    {
		fprintf ( stderr,
		          "* executing gpg"
			  " --batch -q --no-tty -c" );
		if ( input != NULL )
		{
		    fprintf ( stderr, " \\\n"
			  "            < %s",
			  input );
		    if ( output != NULL )
			fprintf ( stderr, " \\\n"
			  "            > %s",
			  output );
		}
		else if ( output != NULL )
		    fprintf ( stderr, " \\\n"
		              "            > %s",
			      output );
		fprintf ( stderr, "\n" );
		fflush ( stderr );
	    }
	    if ( execlp ( "gpg", "gpg",
	                  "--passphrase-fd", "3",
		          "--batch", "-q", "--no-tty",
			  "-c", NULL ) < 0 )
	    error ( errno );
	}
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

/* Return true iff filename begins with `s3:'.  Also,
 * if true is returned, checks that s3_config read,
 * and if not, prints an error message and exits
 * program.
 */
int is_s3 ( const char * filename )
{
    if ( strncmp ( "s3:", filename, 3 ) == 0 )
    {
        if ( s3_config[0] == 0 )
	{
	    printf ( "ERROR: S3 used in %s\n"
		     "       but EFM-S3CONFIG.gpg"
		     " missing\n",
		     filename );
	    exit ( 1 );
	}
	return 1;
    }
    else
        return 0;
}

/* Return a pointer to the first `:' in the file name
 * if there is a `:' and the first `:' is preceeded
 * by exactly one `@'.
 */
const char * is_remote ( const char * filename )
{
    int at_found = 0;
    const char * p = filename;
    for ( ; * p; ++ p )
    {
	if ( * p == '@' )
	{
	    if ( at_found ) break;
	    at_found = 1;
	}
	else if ( * p == ':' ) break;
    }
    return * p == ':' && at_found ? p : NULL;
}

/* Compute the MD5 sum of a file.  The filename may have
 * any format acceptable to scp, and must not be longer
 * than MAX_LEXEME_SIZE.  The 32 character md5sum
 * followed by a NUL is returned in the buffer, which
 * must be at least 33 characters long.  0 is returned
 * on success, -1 on error.  Error messages are written
 * on stdout.  If filename is remote (has @ and :) then
 * RETRIES retries are done on failure.
 */
int md5sum ( char * buffer,
             const char * filename )
{
    line_buffer name, line;
    int remote = 0;
    int retries = RETRIES;
    int s3_name, at_found, error_found;
    char * p;

    strcpy ( name, filename );
    p = (char *) is_remote ( name );
    s3_name = is_s3 ( name );
    if ( s3_name )
        remote = 1;
    else if ( p != NULL )
    {
        remote = 1;
	* p ++ = 0;
    }
    else retries = 0;

    while ( 1 )
    {
	int fd[2];
	int child;
	FILE * inf;
	if ( pipe ( fd ) < 0 ) error ( errno );

	fflush ( stdout );
	fflush ( stderr );

	if ( s3_name && setup_s3_pipe() < 0 )
	    return -1;

	child = fork();
	if ( child < 0 )
	{
	    int saved_errno = errno;
	    if ( s3_name )
	        unlink ( "EFM-S3CONFIG.pipe" );
	    error ( saved_errno );
	}

	if ( child == 0 )
	{
	    int newfd, d;

	    close ( fd[0] );

	    /* Set fd's as follows:
	     * 	0 -> /dev/null
	     *	1 -> fd[1]
	     *	2 -> parent's fd 1
	     */
	    newfd = open ( "/dev/null", O_RDONLY );
	    if ( newfd < 0 ) error ( errno );
	    close ( 0 );
	    if ( dup2 ( newfd, 0 ) < 0 )
	    	error ( errno );
	    close ( newfd );
	    close ( 2 );
	    if ( dup2 ( 1, 2 ) < 0 )
	    	error ( errno );
	    close ( 1 );
	    if ( dup2 ( fd[1], 1 ) < 0 )
	    	error ( errno );
	    close ( fd[1] );
	    d = getdtablesize() - 1;
	    while ( d > 2 ) close ( d -- );

	    if ( ! remote )
	    {
		/* Not a remote file. */

		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing md5sum %s\n",
			      filename );
		    fflush ( stderr );
		}
		if ( execlp ( "md5sum", "md5sum",
			      filename, NULL ) < 0 )
		    error ( errno );
	    }
	    else if ( s3_name )
	    {
		/* Remote s3cmd file. */

		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing s3cmd"
			      " info \\\n"
			      "    %s\n",
			      name );
		    fflush ( stderr );
		}
		execlp ( "s3cmd", "s3cmd",
		         "-c", "EFM-S3CONFIG.pipe",
		         "info", name, NULL );
		int saved_errno = errno;
		unlink ( "EFM-S3CONFIG.pipe" );
		error ( saved_errno );
	    }
	    else
	    {
		/* Remote scp/ssh file. */

		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing ssh %s \\\n"
			      "            md5sum %s\n",
			      name, p );
		    fflush ( stderr );
		}
		if ( execlp ( "ssh", "ssh", name,
			      "md5sum", p, NULL ) < 0 )
		    error ( errno );
	    }
	}

	close ( fd[1] );
	inf = fdopen ( fd[0], "r" );

	error_found = 0;

	if ( s3_name )
	{
	    write_s3_pipe();

	    int first = 1;
	    while ( get_line ( line, inf ) )
	    {
	        const char * p = line;
	        if ( first )
		{
		    /* First line is object name */
		    if ( ! is_s3 ( line ) )
		    {
			do {
			    printf ( "%s\n", line );
			} while
			    ( get_line ( line, inf ) );
			error_found = 1;
		    }
		    first = 0;
		    continue;
		}
		while ( isspace ( * p ) ) ++ p;
		if ( strncmp ( "MD5 sum:", p, 8 ) == 0 )
		{
		    p += 8;
		    while ( isspace ( * p ) ) ++ p;
		    if ( strlen ( p ) == 32 )
			strcpy ( buffer, p );
		    else
		    {
		        printf ( "ERROR: wrong size MD5"
			         " sum: %s\n", p );
			error_found = 1;
		    }
		}
		else
		{
		    if ( p != line )
		        while ( * p && * p != ':' )
			    ++ p;
		    if ( p == line || * p == 0 ) do {
			/* Likely error messages */
			printf ( "%s\n", line );
			error_found = 1;
		    } while ( get_line ( line, inf ) );
		}
	    }
	}
	else if ( get_line ( line, inf ) )
	{
	    char * p = line;
	    while ( * p != 0 && ! isspace ( * p ) )
	        ++ p;
	    if ( p - line == 32 )
	    {
	        strncpy ( buffer, line, 32 );
		buffer[32] = 0;
	    }
	    else
	    {
		* p = 0;
		printf ( "ERROR: wrong size MD5"
			 " sum: %s\n", line );
		error_found = 1;
	    }
	    while ( get_line ( line, inf ) )
	    {
		/* Extra lines indicate an error */
	        printf ( "%s\n", line );
		error_found = 1;
	    }
	}
	else
	{
	    printf ( "ERROR: md5sum produced no"
	             " output\n" );
	    error_found = 1;
	}

	if ( ! error_found )
	{
	    char * p = buffer;
	    for ( ; * p; ++ p )
	    {
		char c = * p;
		if ( '0' <= c && c <= '9' ) continue;
		if ( 'a' <= c && c <= 'f' ) continue;
		if ( 'A' <= c && c <= 'F' ) continue;
		break;
	    }
	    if ( * p )
	    {
	        printf ( "ERROR: bad digits in MD5 sum:"
		         " %s\n", buffer );
		error_found = 1;
	    }
	}
	fclose ( inf );
	if ( cwait ( child ) < 0 ) error_found = 1;
	if ( s3_name ) unlink ( "EFM-S3CONFIG.pipe" );

	if ( error_found && retries > 0 )
	{
	    printf ( "RETRYING md5sum %s\n", filename );
	    -- retries;
	    continue;
	}
	else if ( error_found != 0 )
	{
	    printf ( "ERROR: cannot compute MD5 sum of"
		     " %s\n", filename );
	    return -1;
	}
	else return 0;
    }
}

/* Copy file.  0 is returned on success, -1 on error.
 * Error messages are written on stdout.  The mode
 * and mtime of the file are preserved if this is
 * easy to do, but not for files stored in S3 (it
 * would be possible using the MIME type to do so,
 * but we do not).  The filenames may have any format
 * acceptable to scp or s3cmd.  If a filename has
 * S3 form (begins with `s3:'), the other file name
 * must be local (not have `@' before a `:' or begin
 * with `s3:').  RETRIES retries are done on failure
 * (it is assumed that one of the file names is
 * probably remote).
 */
int copyfile
	( const char * source, const char * target )
{
    int child;
    int retries = RETRIES;
    int s3_source = is_s3 ( source );
    int s3_target = is_s3 ( target );

    while ( 1 )
    {
	fflush ( stdout );
	fflush ( stderr );

	if ( ( s3_source || s3_target )
	      && setup_s3_pipe() < 0 )
	    return -1;

	child = fork();
	if ( child < 0 )
	{
	    int saved_errno = errno;
	    if ( s3_source || s3_target )
	        unlink ( "EFM-S3CONFIG.pipe" );
	    error ( saved_errno );
	}

	if ( child == 0 )
	{
	    int newfd, d;

	    /* Set fd's as follows:
	     * 	0 -> /dev/null
	     *	1 -> parent's fd 1
	     *	2 -> parent's fd 1
	     */
	    newfd = open ( "/dev/null", O_RDONLY );
	    if ( newfd < 0 ) error ( errno );
	    close ( 0 );
	    if ( dup2 ( newfd, 0 ) < 0 )
	    	error ( errno );
	    close ( newfd );
	    close ( 2 );
	    if ( dup2 ( 1, 2 ) < 0 )
	    	error ( errno );
	    d = getdtablesize() - 1;
	    while ( d > 2 ) close ( d -- );

	    if ( s3_source )
	    {
		if ( s3_target)
		{
		    printf ( "ERROR: both source %s\n"
		             "       and target %s\n"
			     "       of file copy are"
			     " S3\n",
			     source, target );
		    exit ( 1 );
		}
		else if ( is_remote ( target ) )
		{
		    printf ( "ERROR: attempt to copy S3"
		             " source %s\n"
		             "       to remote target"
			     " %s\n",
			     source, target );
		    exit ( 1 );
		}
		 
		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing s3cmd"
			      " get %s \\\n"
			      "                 "
			      "     %s\n",
			      source, target );
		    fflush ( stderr );
		}
		execlp ( "s3cmd", "s3cmd",
		         "-c", "EFM-S3CONFIG.pipe",
			 // trace ? "-v" : "-q",
		         "get", source, target,
			 NULL );
		int saved_errno = errno;
		unlink ( "EFM-S3CONFIG.pipe" );
		error ( saved_errno );
	    }
	    else if ( s3_target )
	    {
		if ( is_remote ( target ) )
		{
		    printf ( "ERROR: attempt to copy"
		             " remote source %s\n"
		             "       to S3 target"
			     " %s\n",
			     source, target );
		    exit ( 1 );
		}
		 
		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing s3cmd"
			      " put %s \\\n"
			      "                 "
			      "     %s\n",
			      source, target );
		    fflush ( stderr );
		}
		execlp ( "s3cmd", "s3cmd",
		         "-c", "EFM-S3CONFIG.pipe",
			 // trace ? "-v" : "-q",
		         "put", source, target,
			 NULL );
		int saved_errno = errno;
		unlink ( "EFM-S3CONFIG.pipe" );
		error ( saved_errno );
	    }
	    else
	    {
		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing scp -p"
			      " %s \\\n"
			      "                   %s\n",
			      source, target );
		    fflush ( stderr );
		}
		if ( execlp ( "scp", "scp", "-p",
			      source, target, NULL )
		     < 0 )
		    error ( errno );
	    }
	}

	if ( s3_source || s3_target )
	    write_s3_pipe();

	if ( cwait ( child ) < 0 )
	{
	    if ( s3_source || s3_target )
	        unlink ( "EFM-S3CONFIG.pipe" );
	    if ( retries -- )
	    {
		printf ( "RETRYING scp -p %s \\\n"
			 "                %s\\n",
			 source, target );
	    }
	    else return -1;
	}

	if ( s3_source || s3_target )
	    unlink ( "EFM-S3CONFIG.pipe" );
	return 0;
    }
}

/* Delete file.  0 is returned on success, -1 on error.
 * Error messages are written on stdout.  The filename
 * may have the format acceptable to scp, and must
 * not be longer than MAX_LEXEME_SIZE.  If filename is
 * remote (has @ and :) then RETRIES retries are done on
 * failure.
 */
int delfile ( const char * filename )
{
    int at_found, child;
    char * p;
    line_buffer name;
    int retries = RETRIES;
    int s3_file = is_s3 ( filename );

    strcpy ( name, filename );
    p = (char *) is_remote ( name );
    if ( ! s3_file && ! p )
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

    /* Now name points at account and p at file
     * within account.
     */

    while ( 1 )
    {
	fflush ( stdout );
	fflush ( stderr );

	if ( s3_file && setup_s3_pipe() < 0 )
	    return -1;

	child = fork();
	if ( child < 0 )
	{
	    int saved_errno = errno;
	    if ( s3_file )
	        unlink ( "EFM-S3CONFIG.pipe" );
	    error ( saved_errno );
	}

	if ( child == 0 )
	{
	    int newfd, d;

	    /* Set fd's as follows:
	     * 	0 -> /dev/null
	     *	1 -> parent's fd 1
	     *	2 -> parent's fd 1
	     */
	    newfd = open ( "/dev/null", O_RDONLY );
	    if ( newfd < 0 ) error ( errno );
	    close ( 0 );
	    if ( dup2 ( newfd, 0 ) < 0 )
	        error ( errno );
	    close ( newfd );
	    close ( 2 );
	    if ( dup2 ( 1, 2 ) < 0 )
	        error ( errno );
	    d = getdtablesize() - 1;
	    while ( d > 2 ) close ( d -- );

	    if ( s3_file )
	    {
		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing s3cmd"
			      " del; %s \n",
			      filename );
		    fflush ( stderr );
		}
		execlp ( "s3cmd", "s3cmd",
		         "-c", "EFM-S3CONFIG.pipe",
			 // trace ? "-v" : "-q",
		         "del", filename,
			 NULL );
		int saved_errno = errno;
		unlink ( "EFM-S3CONFIG.pipe" );
		error ( saved_errno );
	    }
	    else
	    {
		* p ++ = 0;

		if ( trace )
		{
		    fprintf ( stderr,
			      "* executing ssh %s \\\n"
			      "            rm -f %s\n",
			      name, p );
		    fflush ( stderr );
		}
		if ( execlp ( "ssh", "ssh", name,
			      "rm", "-f", p, NULL )
		     < 0 )
		    error ( errno );
	    }
	}

	if (s3_file ) write_s3_pipe();

	if ( cwait ( child ) < 0 )
	{
	    if (s3_file )
		unlink ( "EFM-S3CONFIG.pipe" );
	    if ( retries -- )
	    {
		printf ( "RETRYING deletion of %s\n",
		         filename );
	    }
	    else return -1;
	}

	if (s3_file )
	    unlink ( "EFM-S3CONFIG.pipe" );
	return 0;
    }
}

/* Add file entry to index.  Return -1 on error, 0 on
 * success.
 */
int add ( const char * filename )
{
    const char * p;
    struct entry * e;
    char sum [33];
    struct stat st;
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

    if ( stat ( filename, & st ) < 0 )
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
    e->current  = 1;
    e->filename = strdup ( filename );
    e->mode     = st.st_mode & 07777;
    e->mtime    = st.st_mtime;
    e->size     = st.st_size;
    e->md5sum   = strdup ( sum );

    e->emd5sum  = strdup ( "" );
    e->esize    = 0;

    e->key      = strdup ( key );

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
    free ( e->emd5sum );
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
 * ten to stdout.  Return 1 if kill command processed
 * (do nothing else for kill), 0 if command processed
 * without error, and -1 if command processed with
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
    else if ( strcmp ( arg, "s3cmd" ) == 0 )
    {
#	define ARG_LIST_SIZE 1000
	char * arg_list[ARG_LIST_SIZE+4];
	int child;
	char ** p = arg_list;
	* p ++ = "s3cmd";
	* p ++ = "-c";
	* p ++ = "EFM-S3CONFIG.pipe";
	while ( arg = get_argument
			  ( buffer, in ) )
	{
	    if (   p - arg_list
		 >= ARG_LIST_SIZE + 3 )
	    {
		printf ( "ERROR: too many"
			 " arguments (more than"
			 " %d)\n",
			 ARG_LIST_SIZE );
		result = -1;
		break;
	    }
	    * p ++ = strdup ( arg );
	}
	* p = NULL;

	fflush ( stdout );
	fflush ( stderr );

	if ( result == -1 )
	    child = -1;
	else if ( setup_s3_pipe() < 0 )
	{
	    result = -1;
	    child = -1;
	}
	else
	{
	    child = fork();
	    if ( child < 0 )
	    {
		int saved_errno = errno;
		unlink ( "EFM-S3CONFIG.pipe" );
		error ( saved_errno );
	    }
	}

	if ( child == 0 )
	{
	    int newfd, d;

	    /* Set fd's as follows:
	     * 	0 -> /dev/null
	     */
	    newfd = open ( "/dev/null", O_RDONLY );
	    if ( newfd < 0 ) error ( errno );
	    close ( 0 );
	    if ( dup2 ( newfd, 0 ) < 0 )
		error ( errno );
	    close ( newfd );
	    d = getdtablesize() - 1;
	    while ( d > 2 ) close ( d -- );

	    if ( trace )
	    {
		fprintf ( stderr,
			  "* executing s3cmd" );
		char ** argp = arg_list;
		while ( * argp != NULL )
		    fprintf ( stderr, " %s",
		              * argp ++ );
		fprintf ( stderr, "\n" );
		fflush ( stderr );
	    }

	    execvp ( "s3cmd", arg_list );
	    int saved_errno = errno;
	    unlink ( "EFM-S3CONFIG.pipe" );
	    error ( saved_errno );
	}

	if ( child >= 0  ) write_s3_pipe();

	p = arg_list + 3;
	while ( * p ) free ( * p ++ );
	if ( child >= 0 && cwait ( child ) < 0 )
	    result = -1;
	unlink ( "EFM-S3CONFIG.pipe" );
    }
    /* Rest of commands require EFM-INDEX.gpg be read.
     */
    else if ( ! check_index() )
    	result = -1;
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
	      				             1
						     );

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
		write_index_entry
		    ( stdout, e, 7, "* " );
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
              || strcmp ( arg, "md5check" ) == 0
              || strcmp ( arg, "del" ) == 0 )
    {
        char op =
	    ( arg[0] == 'c' && arg[1] == 'h' ? 'k' :
	      arg[0] == 'm' && arg[1] == 'd' ? 's' :
	                                       arg[0] );
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

		    if ( access ( arg, R_OK ) < 0 )
		    {
		        printf ( "ERROR: file %s is not"
			         " readable\n", arg );
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    if ( md5sum ( sum, arg ) < 0 )
		    {
			printf ( "    Processing %s"
			         " aborted.\n", arg );
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
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
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
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    if ( add ( arg ) < 0 )
		    {
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    e = find_filename ( arg );
		    assert ( e != NULL );
		}
		else if ( access ( arg, R_OK ) >= 0 )
		{
		    char sum [33];
		    if ( md5sum ( sum, arg ) < 0 )
		    {
			printf ( "    Processing %s"
			         " aborted.\n", arg );
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
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		} else if ( direction == 't' ) {
		    printf ( "ERROR: file not found:"
		             " %s\n", arg );
		    printf ( "    Processing %s"
			     " aborted.\n", arg );
		    result = -1;
		    continue;
		}

		/* Perform Copying and Remote
		   MD5 checking */

		strcpy ( efile, e->md5sum );
		strcpy ( efile + 32, ".gpg" );
		strcpy ( dend, efile );
		if ( direction == 't' )
		{
		    struct stat st;
		    char efile_sum[33];
		    char dbegin_sum[33];

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
			printf ( "    Processing %s"
				 " aborted.\n", arg );
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
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    if ( stat ( efile, & st ) < 0 )
		    {
		        printf ( "ERROR: cannot stat"
			         " %s\n", efile );
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    if ( e->esize == 0 )
		    {
		        e->esize = st.st_size;
			index_modified = 1;
		    }
		    else if ( e->esize != st.st_size )
		    {
		        printf ( "ERROR: esize has"
			         " changed from %llu to"
				 " %llu\n    for %s\n",
				 (unsigned long long)
				 e->esize,
				 (unsigned long long)
				 st.st_size,
				 efile );
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		    if ( md5sum ( efile_sum,
				  efile )
			 < 0 )
		    {
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
			result = -1;
			continue;
		    }
		    if ( e->emd5sum[0] == 0 )
		    {
		        free ( e->emd5sum );
		        e->emd5sum =
			    strdup ( efile_sum );
			index_modified = 1;
		    }
		    else if ( strcmp ( efile_sum,
				       e->emd5sum )
			      != 0 )
		    {
		        printf ( "ERROR: md5sum has"
			         " changed from %s\n"
				 "    to %s\n"
				 "    for %s\n",
				 e->emd5sum, efile_sum,
				 efile );
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
			result = -1;
			continue;
		    }

		    if ( ! current_directory )
		    {

			if ( trace )
			    printf ( "* deleting %s\n",
			             dbegin );
			if ( delfile ( dbegin ) < 0 )
			{
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
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
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
			    result = -1;
			    continue;
			}
			if ( trace )
			    printf ( "* comparing MD5"
			             " sums of %s\n"
				     "*     and %s\n",
			             efile, dbegin );
			if ( md5sum ( dbegin_sum,
				      dbegin )
			     < 0 )
			{
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
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
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
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
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
			    result = -1;
			    continue;
			}
		    }
		    else if ( access ( efile, R_OK )
		              < 0 )
		    {
		        printf ( "ERROR: encrypted %s\n"
			         "    (%s) cannot be"
				 " read\n",
				 arg, efile );
			printf ( "    Processing %s"
				 " aborted.\n", arg );
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
			printf ( "    Processing %s"
				 " aborted.\n", arg );
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
			printf ( "    Processing %s"
				 " aborted.\n", arg );
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
			printf ( "    Processing %s"
				 " aborted.\n", arg );
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
			    printf ( "    Processing %s"
				     " aborted.\n",
				     arg );
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
			    ( "* deleting %s\n",
			      e->md5sum );
		    unlink ( e->md5sum );
		}
		else if ( op == 's' )
		{
		    char dbegin_sum[33];

		    if ( trace )
			printf ( "* comparing MD5"
				 " sums of %s\n"
				 "*     and %s\n",
				 efile, dbegin );
		    if ( md5sum ( dbegin_sum, dbegin )
			 < 0 )
		    {
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
			result = -1;
			continue;
		    }
		    if ( e->emd5sum[0] == 0 )
		    {
			printf ( "ERROR: index has no"
				 " MD5 sum for %s\n",
				 efile );
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
			result = -1;
			continue;
		    }
		    if ( strcmp ( e->emd5sum,
				  dbegin_sum )
			 != 0 )
		    {
			printf ( "ERROR: MD5 sum of"
				 " %s (%s)\n"
				 "    does not"
				 " match that of %s"
				 " (%s)\n",
				 efile, e->emd5sum,
				 dbegin,
				 dbegin_sum );
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
			result = -1;
			continue;
		    }
		}

		/* Perform source file deletion. */

		if ( ( op == 'm' && direction == 'f' )
		     || op == 'r' || op == 'd' )
		{
		    if ( trace )
			printf
			    ( "* deleting %s\n",
			      dbegin );
		    if ( delfile ( dbegin ) < 0 )
		    {
			printf ( "    Processing %s"
			         " aborted.\n", arg );
			result = -1;
			continue;
		    }
		}
		else if (    op == 'm'
		          && direction == 't' )
		{
		    if ( trace )
			printf
			    ( "* deleting %s\n", arg );
		    if ( delfile ( arg ) < 0 )
		    {
			printf ( "    Processing %s"
				 " aborted.\n",
				 arg );
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

		printf ( "%s: %s\n",
		         op == 'm' ? "MOVED" :
		         op == 'c' ? "COPIED" :
		         op == 'r' ? "REMOVED" :
		         op == 'd' ? "DELETED" :
		         op == 'k' ? "OK" :
		         op == 's' ? "OK" :
			             "DONE",
			 arg );
	    }
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

int main ( int argc, char ** argv )
{
    line_buffer buffer;
    int tofd;
    struct sockaddr_un sa;
    int listenfd;
    pid_t childpid;
    FILE * tof;
    char ** argp;

    if ( argc < 2
         ||
	 strncmp ( argv[1], "-doc", 4 ) == 0 )
    {
	const char ** p = documentation;
	if ( access ( "/usr/bin/less", X_OK ) < 0 )
	    while ( * p ) printf ( "%s\n", * p ++ );
	else
	{
	    FILE * out;
	    int fd[2];
	    fflush ( stdout );
	    if ( pipe ( fd ) < 0 ) error ( errno );
	    childpid = fork ( );
	    if ( childpid < 0 ) error ( errno );
	    if ( childpid == 0 )
	    {
		int fdx;
		char buf[100];
		close ( 0 );
		dup2 ( fd[0], 0 );
		fdx = getdtablesize() - 1;
		while ( fdx > 2 ) close ( fdx -- );
		if ( execlp ( "less",
		              "less", "-F", NULL ) < 0 )
		{
		    printf ( "ERROR: executing"
		             " `less -F'\n" );
		    error ( errno );
		}
	    }
	    close ( 1 );
	    close ( 2 );
	    close ( fd[0] );
	    out = fdopen ( fd[1], "w" );
	    while ( * p )
	        fprintf ( out, "%s\n", * p ++ );
	    fclose ( out );

	    /* If you do not wait for `less' it does
	     * not work.
	     */
	    cwait ( childpid );
	}
	exit (1);
    }

    setcatch();

    /* We use UTC for all published times.  We need
     * mktime(3) to use UTC so we reset the TZ
     * environment variable.
     */
    putenv ( "TZ=" );
    tzset();

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

	read_password();

	if ( access ( "EFM-S3CONFIG.gpg", R_OK ) >= 0 )
	{
	    int keyschild;
	    int keysfd;
	    FILE * keysf;
	    keysfd =
		crypt ( 1, "EFM-S3CONFIG.gpg", NULL,
			password,
			strlen ( password ),
			& keyschild );
	    if ( keysfd < 0 ) exit ( 1 );

	    char * p = s3_config;
	    char * endp = p + sizeof ( s3_config );
	    while ( 1 )
	    {
	        size_t s =
		    read ( keysfd, p, endp - p );
		if ( s < 0 )
		    error ( errno );
		if ( s == 0 ) break;
		p += s;
		if ( p == endp )
		{
		    printf ( "ERROR: EFM-S3CONFIG too"
		             " large (>= %llu bytes)\n",
			     (unsigned long long)
			     sizeof ( s3_config) - 1 );
		    exit ( 1 );
		}
	    }
	    * p = 0;

	        
	    close ( keysfd );
	    if ( cwait ( keyschild ) < 0 )
	    {
		printf ( "ERROR: error decypting"
			 " EFM-KEYS.gpg\n" );
		exit ( 1 );
	    }
	}

	if ( access ( "EFM-INDEX.gpg", R_OK ) >= 0 )
	{
	    int indexchild;
	    int indexfd;
	    FILE * indexf;
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

		printf ( "%s%lld\n", BEGIN_STRING,
		         (long long) getpgrp() );
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
			        ( "* deleting"
			          " EFM-INDEX.gpg+\n" );
		        unlink ( "EFM-INDEX.gpg+" );
			printf ( "ERROR: error"
			         " encypting"
				 " EFM-INDEX.gpg\n" );
			exit ( 1 );
		    }

		    if ( access ( "EFM-INDEX.gpg-",
		                  F_OK )
		         >= 0 )
		    {
			if ( trace )
			    printf
				( "* deleting"
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

    if ( ! get_line ( buffer, tof ) )
    {
	printf ( "ERROR: efm background process has"
	         " died\n" );
	exit ( 1 );
    }
    if ( strncmp ( buffer, BEGIN_STRING,
                   strlen ( BEGIN_STRING ) ) != 0 )
    {
	printf ( "ERROR: efm background process sent"
	         " bad hello message\n" );
	exit ( 1 );
    }

    siggroup = (pid_t)
        atoll ( buffer + strlen ( BEGIN_STRING ) );

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
