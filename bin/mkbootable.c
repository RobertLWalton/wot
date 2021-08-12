/* Program to make a non-bootable ISO file into a USB
** bootable file.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	mkbootaboe.c
** Date:	Thu Aug 12 17:05:04 EDT 2021
**
** The authors have placed this program in the public
** domain; they make no warranty and accept no liability
** for this program.
*/

#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char * documentation [] = {
"mkbootable INPUT-ISO-FILE EFI-PARTITION-CONTENTS"
                            " OUTPUT-ISO-FILE",
"",
"    Produces OUTPUT-ISO-FILE by concatenating the",
"    INPUT-ISO-FILE and EFI-PARTITION-CONTENTS, and",
"    then modifiying the first 512 bytes of the result",
"    to install a partition table which allows the",
"    OUTPUT-ISO-FILE to be EFI booted from a USB.",
"",
"    The INPUT-ISO-FILE is in ISO 9660 format.  Since",
"    this format makes no use of its first 32,768",
"    bytes, the OUTPUT-ISO-FILE can be used in any",
"    application in which the INPUT-ISO-FILE can be",
"    used.",
"",
"    The EFI-PARTITION-CONTENTS should be copied from",
"    a USB bootable ISO file for the same operating",
"    system at that from which the INPUT-ISO-FILE was",
"    made.",
NULL };

struct partition  /* Partition in MBR */
{
    uint8_t status;
        /* 0x80 for bootable partition; 0 otherwise. */
    uint8_t first_chs[3];
        /* Unused for LINUX USB. */
    uint8_t type;
        /* 0 for empty or undefined.
	 * 0xef for EFI.
	 */
    uint8_t last_chs[3];
        /* Unused for LINUX USB. */
    uint32_t first;
        /* First sector of partition */
    uint32_t size;
        /* Number of sectors in partition */

    /* Sectors are 512 byte units. */
} partition;

struct mbr /* Modern Standard MBR */
{
    uint8_t bootstrap[440];
        /* Unused bootstrap and time stamp area. */
    uint32_t label_id;
        /* Label ID, first part of disk signature */
    uint16_t protect;
        /* 0 if not copy-protected, second part of
	 * disk signature.
	 */
    uint8_t partition[4][16];
        /* partition[i] is a partition struct
	 * but is NOT aligned on a 4 byte boundary.
	 * So we write it with memcpy.
	 */
    uint16_t signature;
        /* Must be 0xAA55.  Boot signature. */
} mbr;

/* Copy from file descriptor in to file descriptor
 * out.  The first 512 bytes to be copied are
 * replaced by mbr.  The names are file names used
 * in error messages.
 */
int first = 1;
char buffer[1<<16];
void copy ( int in, int out,
            const char * in_filename,
            const char * out_filename )
{
    while ( 1 )
    {
        int r;
	r = read ( in, buffer, sizeof ( buffer ) );
	if ( r < 0 )
	{
	    printf ( "error reading %s\n",
	             in_filename );
	    exit ( 1 );
	}
	if ( r == 0 ) break;
	if ( first )
	{
	    memcpy ( buffer, & mbr, 512 );
	    first = 0;
	    printf ( "mbr copied\n" );
	}
	r = write ( out, buffer, r );
	if ( r < 0 )
	{
	    printf ( "error writing %s\n",
	             out_filename );
	    exit ( 1 );
	}
    }
}

int main ( int argc, char ** argv )
{
    int input, efi, output;
    off_t input_size, efi_size;
    uint32_t input_sectors, efi_sectors;
    struct stat statbuf;
    FILE * md5;
    char command[1 << 16];
    char md5sum[1 << 16];
        /* In UNIX max total pathname size is 4096. */
    char * endp;

    if ( argc < 4 )
    {
	const char ** p = documentation;
	while ( * p )
	    printf ( "%s\n", * p ++ );
	exit ( 0 );
    }

    input = open ( argv[1], O_RDONLY );
    if ( input < 0 )
    {
	printf ( "%s: cannot open for reading\n",
	         argv [1] );
	exit ( 1 );
    }
    if ( fstat ( input, & statbuf ) < 0 )
    {
	printf ( "%s: cannot stat\n", argv [1] );
	exit ( 1 );
    }
    input_size = statbuf.st_size;
    input_sectors = (uint32_t) ( input_size >> 9 );
    if ( (off_t) input_sectors * 512 != input_size )
    {
	printf ( "%s: size is not a multiple"
	         " of 512 bytes\n", argv [1] );
	exit ( 1 );
    }
    printf ( "%s has %d sectors\n", argv[1],
             input_sectors );
    sprintf ( command, "md5sum %s", argv[1] );
    md5 = popen ( command, "r" );
    if ( md5 == NULL )
    {
	printf ( "cannot execute %s\n", command );
	exit ( 1 );
    }
    if ( fread ( md5sum, 1, sizeof ( md5sum ), md5 ) < 0
         ||
	 pclose ( md5 ) < 0 )
    {
	printf ( "error executing %s\n", command );
	exit ( 1 );
    }
    md5sum[32] = 0;
    printf ( "md5sum of %s is\n"
             "          %s\n", argv[1], md5sum );

    efi = open ( argv[2], O_RDONLY );
    if ( efi < 0 )
    {
	printf ( "%s: cannot open for reading\n",
	         argv [2] );
	exit ( errno );
    }
    if ( fstat ( efi, & statbuf ) < 0 )
    {
	printf ( "%s: cannot stat\n", argv [2] );
	exit ( 1 );
    }
    efi_size = statbuf.st_size;
    efi_sectors = (uint32_t) ( efi_size >> 9 );
    if ( (off_t) efi_sectors * 512 != efi_size )
    {
	printf ( "%s: size is not a multiple"
	         " of 512 bytes\n", argv [2] );
	exit ( 1 );
    }
    printf ( "%s has %d sectors\n", argv[2],
             efi_sectors );

    /* Construct MBR.  Bot mbr and partition are
     * automatically initialized to zeroes.
     */

    md5sum[8] = 0;
    mbr.label_id =
        (uint32_t) strtol ( md5sum, & endp, 16 );
    if ( endp != md5sum + 8 )
    {
	printf ( "%s has bad md5sum\n", argv[1] );
	exit ( 1 );
    }
    mbr.signature = 0xAA55;
    partition.status = 0x80;
    partition.type = 0;
    partition.first = 0;
    partition.size = input_sectors;
    memcpy ( mbr.partition[0], & partition, 16 );
    partition.status = 0;
    partition.type = 0xef;
    partition.first = input_sectors;
    partition.size = efi_sectors;
    memcpy ( mbr.partition[1], & partition, 16 );

    output = creat ( argv[3], S_IRUSR + S_IRGRP );
    if ( output < 0 )
    {
	printf ( "%s: cannot create\n", argv [3] );
	exit ( errno );
    }
    copy ( input, output, argv[1], argv[3] );
    copy ( efi, output, argv[2], argv[3] );

    close ( input );
    close ( efi );
    close ( output );

    exit (0);
}
