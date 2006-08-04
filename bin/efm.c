/* Encrypted File Management (EFM) Program.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	efm.c
** Date:	Sat Nov 16 05:25:15 EST 2002
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
**
** RCS Info (may not be true date or author):
**
**   $Author: walton $
**   $Date: 2006/08/04 18:39:36 $
**   $RCSfile: efm.c,v $
**   $Revision: 1.1 $
*/

#include <stdio.h>

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
"    You must create and encrypt an initial empty\n"
"    EFM-INDEX.gpg file by using the gpg program.\n"
"\n"
"    The date and mode are used to set the file modi-\n"
"    fication date and mode of the file when it is\n"
"    decrypted.  The MD5sum is used to check the in-\n"
"    grity of the decryption.  The key is the sym-\n"
"    metric encryption/decryption key, and is 128\n"
"    bits in 16 hexadecimal digits.\n"
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

int main ( int argc, char ** argv )
{
    if ( argc < 2 )
    {
	printf ( documentation );
	exit (1);
    }
    exit (0);
}
