/* Helper program for the conf program conf_passwd and
 * conf_shadow functions.
 *
 * File:	conf_passwd_shadow.c
 * Author:	Bob Walton (walton@deas.harvard.edu)
 * Date:	Thu Mar 19 09:33:42 EDT 2009
 *
 * The authors have placed this program in the public
 * domain; they make no warranty and accept no liability
 * for this program.
 *
 * RCS Info (may not be true date or author):
 *
 *   $Author: root $
 *   $Date: 2009/03/19 15:42:00 $
 *   $RCSfile: conf_helper.c,v $
 *   $Revision: 1.1 $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* This program accepts the COMMANDs `put' and `get' and
 * copies the source or target files, repectively, to
 * to the tmpfile, making appropriate field substitu-
 * tions.  The file TYPEs are `passwd' or `shadow'
 */

/* Move a char pointer to the next `:' or NUL.
 */
#define SKIP(p) while ( * p && * p != ':' ) ++ p; \
                if ( * p ) * p ++ = 0

/* Ditto but print field skipped over to tmpf preceeded
 * by a ':'.
 */
#define PRINT(p) print ( tmpf, & p )
void print ( FILE * tmpf, char ** pp )
{
    char * p = * pp;
    char * q = p;
    SKIP ( p );
    fprintf ( tmpf, ":%s", q );
    * pp = p;
}

/* List of file lines that contain information to be
 * inserted into the file being copied, indexed by
 * account name.
 */
struct line
{
    /* Line is malloced.  Account points at beginning
     * and is NUL terminated, the : having been
     * replaced in the line by NUL.  rest points just
     * after the NUL unless there is nothing after
     * the NUL, in which it points to NUL.
     */

    char * account;
    char * rest;		
    struct line * previous;
} * lastline = NULL;

/* Return line with given account or NUL if none.
 */
struct line * find ( const char * account )
{
    struct line * result = lastline;
    while ( result )
    {
        if ( strcmp ( result->account, account ) == 0 )
	    return result;
	result = result->previous;
    }
    return NULL;
}

/* Insert line.
 */
void insert ( const char * line, const char * des )
{
    struct line * newline
        = (struct line *)
	  malloc ( sizeof ( struct line ) );
    char * p = malloc ( strlen ( line ) + 1 );
    strcpy ( p, line );
    newline->account = p;
    SKIP ( p );
    newline->rest = p;
    if ( find ( newline->account ) )
    {
        printf ( "ERROR: lines with duplicate account"
	         " names in %s\n", des );
	exit ( 2 );
    }
    newline->previous = lastline;
    lastline = newline;
}
    
static char buffer [10000];
int main ( int argc, char ** argv )
{
    if ( argc < 9 )
    {
    	printf ( "conf_passwd_shadow VERBOSE HOST"
	         " TYPE COMMAND FILE SOURCEFILE"
		 " TARGETFILE TMPFILE\n" );
	exit ( 2 );
    }

    int verbose          = ( argv[1][0] != 0 );
    const char * HOST    = argv[2];
    const char * TYPE    = argv[3];
    const char * command = argv[4];
    const char * file    = argv[5];
    const char * source  = argv[6];
    const char * target  = argv[7];
    const char * tmpfile = argv[8];
    
#   define vprintf if ( verbose ) printf

    /* Figure out which is the source file for the
     * copy (src) and which is the ultimate
     * destination (des) that the tmpfile is replace.
     */
    const char * src = NULL;
    const char * des = NULL;
    if ( strcmp ( command, "put" ) == 0 )
    {
        src = source;
	des = target;
    }
    else
    {	src = target;
    	des = source;
    }

    /* Read the des file and insert its lines into the
     */ 
    FILE * desf = fopen ( des, "r" );
    if ( desf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 des );
	exit ( 2 );
    }
    while ( fgets ( buffer, sizeof ( buffer ), desf ) )
    {
        buffer[strlen(buffer)-1] = 0;
        insert ( buffer, des );
    }
    fclose ( desf );

    /* Copy src to tmpfile inserting fields as necessary
     * from des.
     */
    FILE * srcf = fopen ( src, "r" );
    if ( srcf == NULL )
    {
        printf ( "ERROR: cannot open %s for reading\n",
		 src );
	exit ( 2 );
    }
    FILE * tmpf = fopen ( tmpfile, "w" );
    if ( tmpf == NULL )
    {
        printf ( "ERROR: cannot open %s for writing\n",
		 tmpfile );
	exit ( 2 );
    }
    int isput = ( strcmp ( command, "put" ) == 0 );
    int ispasswd = ( strcmp ( TYPE, "passwd" ) == 0 );
    while ( fgets ( buffer, sizeof ( buffer ), srcf ) )
    {
        buffer[strlen(buffer)-1] = 0;
        char * p = buffer;
	SKIP ( p );
	struct line * desline = find ( buffer );
	if ( desline == NULL )
	{
	    if ( * p )
	        fprintf ( tmpf, "%s:%s\n", buffer, p );
	    else
		fprintf ( tmpf, "%s\n", buffer );

	    continue;
	}
	if ( isput )
	{
	    /* shell comes from des file.
	     */
	    char * q = desline->rest;
	    fprintf ( tmpf, buffer );
	    PRINT ( p );
	    SKIP  ( q );
	    PRINT ( p );
	    SKIP  ( q );
	    PRINT ( p );
	    SKIP  ( q );
	    PRINT ( p );
	    SKIP  ( q );
	    PRINT ( p );
	    SKIP  ( q );

	    SKIP  ( p );
	    PRINT ( q );

	    fprintf ( tmpf, "%s\n", p );
	}
	else
	{
	    /* shell comes from src file.
	     */
	    char * q = desline->rest;
	    fprintf ( tmpf, buffer );
	    PRINT ( q );
	    SKIP  ( p );
	    PRINT ( q );
	    SKIP  ( p );
	    PRINT ( q );
	    SKIP  ( p );
	    PRINT ( q );
	    SKIP  ( p );
	    PRINT ( q );
	    SKIP  ( p );

	    SKIP  ( q );
	    PRINT ( p );

	    fprintf ( tmpf, "%s\n", q );
	}
    }
    fclose ( srcf );
    fclose ( tmpf );

    return 0;
}


#ifdef FOOBAR

# Read the src file, replace the relevant fields with des file
# fields, and output in tmpfile.
#
rm -f $tmpfile
exec 3<"$src"
exec 4>$tmpfile
while read <&3
do
account=`echo "$REPLY" | cut -d: -f1`
symbol "$account"
local -i i=$?
smiddle=`echo "$REPLY" | cut -s -d: -f2-6`
dmiddle="${amiddle[$i]:-}"
sshell=`echo "$REPLY" | cut -s -d: -f7-`
dshell="${ashell[$i]:-}"
if [ $direction = put ]
then
    if [ -n "$dshell" ]
    then
	sshell="$dshell"
    fi
else
    if [ -n "$dmiddle" ]
    then
	smiddle="$dmiddle"
    fi
fi
echo >&4 "$account:$smiddle:$sshell"
done
exec 3<&-
exec 4>&-

# If tmpfile equals des file return
#
if cmp -s $tmpfile $des
then
rm -f $tmpfile
return
fi

case "$1" in
test)
echo "DIFFERS: $file"
;;
put)
echo "PUTTING $file"
cp -p $tmpfile "$target"
;;
pdiff)
echo "PDIFFING: $file"
diff "$target" $tmpfile
;;
get)
echo "GETTNG $file"
cp -p $tmpfile "$source"
;;
gdiff)
echo "GDIFFING: $file"
diff "$source" $tmpfile
;;
esac

rm -f $tmpfile

#endif
