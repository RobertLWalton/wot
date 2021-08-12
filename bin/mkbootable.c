/* Program to make a non-bootable ISO file into a USB
** bootable file.
**
** Author:	Bob Walton (walton@deas.harvard.edu)
** File:	mkbootaboe.c
** Date:	Thu Aug 12 14:52:25 EDT 2021
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
};

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
};

int main ( int argc, char ** argv )
{
    int input, efi, output;
    off_t input_size, efi_size;
    uint32_t input_sectors, efi_sectors;
    struct stat statbuf;
    struct partition partition;
    struct mbr mbr;
    FILE * md5;
    char command[1 << 16];
    char md5sum[1 << 16];
        /* In UNIX max total pathname size is 4096. */

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



    exit (0);
}
