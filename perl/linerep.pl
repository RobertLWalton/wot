# perl linerep.pl
#
# Copies the standard input to the standard output
# replacing all instances of the $from lines by
# the $to lines.
#
#     lineref.pl, Bob Walton, walton@acm.org,
#     Fri Oct  4 07:10:30 AM EDT 2024

use strict;
use warnings;

my $FROM = shift;

if ( not defined $FROM )
{
    print
'perl linerep.pl
perl linerep.pl FROM-FILE TO-FILE LIST-FILE
perl linerep.pl FROM-FILE

When no FROM-FILE is given, this document is output.

LIST-FILE is a list of TEXT-FILE names, one per line.

When LIST-FILE is given, this program modifies each
TEXT-FILE by replacing every occurance of the contents
of FROM-FILE by the contents of TO-FILE.

The contents of FROM-FILE must be a sequence of lines
encoding tokens that do not contain whitespace and
that are separated by whitespace.  Occurences of the
FROM-FILE contents in the input text may have different
whitespace characters than the FROM-FILE contents has;
e.g., a space character may be replaced by a line-feed
or 5 space characters may be replaced by 1 space
character.

Blank lines at the beginning or end of the FROM-FILE
contents are ignored, as if they did not exist.

This program works in part by re-coding the contents of
the FROM-FILE as a regular expression.  If FROM-FILE is
given but TO-FILE is not, no files are modified and
the regular expression is copied to the standard output.
This is only useful for debugging.

Example:

---------- FROM-FILE Contents:
FOOBAR ABC
    [ 99 ]
---------- TO-FILE Contents:
FOOBAR
ABC
  [ 88 ]
---------- LIST-FILE Contents
<TEXT-FILE name>
---------- TEXT-FILE Contents BEFORE Modification:
FOOBAR
[
    ABC [ 22 ]
]
FOOBAR ABC [ 99 ]
FOOBAR ABC [ 33 ]
FOOBAR
   ABC [ 99 ]
THE END
---------- TEXT-FILE Contents AFTER Modification:
FOOBAR
[
    ABC [ 22 ]
]
FOOBAR
ABC
  [ 88 ]
FOOBAR ABC [ 33 ]
FOOBAR
ABC
  [ 88 ]
THE END
';
    exit ( 0 );
}

local $/;
    # setting so that reading from a file reads the
    # entire file all at once, instead of one line
    # at a time
open ( my $from_handle, $FROM )
    or die ( "cannot read $FROM: $!" );
my $from = <$from_handle>;
    # read $FROM file into $from

$from =~ s/^\s+|\s+$//g;
    # remove whitespace from ends of $from
$from = quotemeta ( $from );
    # put \ just before all space and [ characters
    # and other regular expression metacharacters
$from =~ s/(\\[ \r\n\t])+/\\s+/g;
    # replace all whitespace (now with \'s before) by
    # the regular subexpression \s+
$from = '[\ \t]*' . $from . '[\ \t\r]*\n';
    # include any whitespace at the beginning of the
    # first $from instance line and optional whitespace
    # followed by \n at the end of the instance.

my $TO   = shift;
if ( not defined $TO )
{
    print "$from\n";
    exit ( 0 );
}

open ( my $to_handle, $TO )
    or die ( "cannot read $TO: $!" );
my $to = <$to_handle>;
    # read $TO file into $to

my $LIST = shift;
if ( not defined $LIST )
{
    print "TO-FILE read but LIST-FILE not given\n";
    exit ( 1 );
}
open ( my $list_handle, $LIST )
    or die ( "cannot read $LIST $!" );
my $list = <$list_handle>;
    # read $LIST file into $list


my @files = split ( "\n", $list );
foreach ( @files )
{
    my $TEXT = $_;
    $TEXT =~ s/^\s+|\s+$//g;
    open ( my $text_handle, $TEXT )
	or die ( "cannot read $TEXT: $!" );
    my $text = <$text_handle>;
    close $text_handle;

    $text =~ s/$from/$to/g;
	# replace $from by $to in $text

    open ( $text_handle, '>', $TEXT )
	or die ( "cannot write $TEXT: $!" );
    print $text_handle "$text";
    close $text_handle;

    $TEXT = shift;
}
