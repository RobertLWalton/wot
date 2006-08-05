/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	efm.c
** Date:	Sat Aug  5 08:13:27 EDT 2006
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2006/08/05 12:48:33 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.6 $
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>

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
"    crypted the lines of this file have the form:\n"
"\n"
"        filename MD5sum mode \"date\" key\n"
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
"    grity of the decryption.  The key is the sym-\n"
"    metric encryption/decryption key, and is 128\n"
"    bits in 16 hexadecimal digits.\n"
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
int crypt ( const char * input,
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

	if ( execlp ( "gpg", "gpg",
		      /* "--cipher-alog", "BLOWFISH", */
	              "--passphrase-fd", "3",
		      "--batch", "-q",
		      NULL ) < 0 )
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

	int indexchild;
	int indexfd =
	    crypt ( "EFM-INDEX.gpg", NULL,
	            "fum", 3, & indexchild );
	if ( indexfd < 0 ) exit ( 1 );
	FILE * indexf = fdopen ( indexfd, "r" );
	char buffer[1000];
	while ( fgets ( buffer, 1000, indexf ) )
	    printf ( "%s", buffer );
	fclose ( indexf );
	if ( cwait ( indexchild ) < 0 )
	{
	    printf ( "ERROR: error decypting"
	             " EFM-INDEX.gpg\n" );
	    exit ( 1 );
	}

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
