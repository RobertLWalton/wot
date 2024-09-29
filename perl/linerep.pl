# perl linerep.pl
#
# Copies the standard input to the standard output replacing all
# instances of the $from lines by the $to lines.
#
#     lineref.pl, Bob Walton, walton@acm.org,
#     Sat Sep 28 08:31:34 PM EDT 2024


my $FROM = $ARGV[0];
my $TO   = $ARGV[1];

if ( not defined $FROM )
{
    print
'perl linerep.pl
perl linerep.pl FROM-FILE TO-FILE <TEXT-IN >TEXT-OUT
perl linerep.pl FROM-FILE

When no FROM-FILE is given, this document is output.

When TO-FILE is given, this program copies its standard
input to its standard output replacing every instance
of the FROM-FILE contents with the TO-FILE contents.

The contents of FROM-FILE must be a sequence of lines
encoding tokens that do not contain whitespace and
that are separated by whitespace.  Instances of the
FROM-FILE contents in the input text may have different
whitespace characters than the FROM-FILE contents has;
e.g., a space character may be replaced by a line-feed
or 5 space characters may be replaced by 1 space
character.

Blank lines at the beginning or end of the FROM-FILE
contents are ignored, as if they did not exist.

This program works in part by recoding the contents of
the FROM-FILE as a regular expression.  If TO-FILE
is not given, the standard input is ignored and the
regular expression is copied to the standard output.
This is only useful for debugging.

';
    exit ( 0 );
}

$/ = undef;
open ( from_handle, $FROM )
    or die ( "cannot read $FROM" );
$from = <from_handle>;

$from =~ s/^\s+|\s+$//g;
    # remove whitespace from ends of $from
$from = quotemeta ( $from );
    # put \ just before all space and [ characters
    # and other regular expression metacharacters
$from =~ s/(\\[ \n\t])+/\\s+/g;
    # replace all whitespace (now with \'s before) by the
    # regular subexpression \s+
$from = '[ \t]*' . $from . '[ \t]*[\n]';
    # include any whitespace at the beginning of the
    # first $from instance line and optional whitespace
    # followed by \n at the end of the instance.

if ( not defined $TO )
{
    print "$from\n";
    exit ( 0 );
}

open ( to_handle, $TO )
    or die ( "cannot read $TO" );
$to = <to_handle>;
$text = <STDIN>;
   # read standard input into the $text variable
   #
$text =~ s/$from/$to/gm;
    # replace $from by $to in $text
print "$text";
