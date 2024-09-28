# perl linerep.pl
#
# Copies the standard input to the standard output replacing all
# instances of the $from lines by the $to lines.
#
#     lineref.pl, Bob Walton, walton@acm.org,
#     Sat Sep 28 07:24:12 AM EDT 2024

$/ = undef;
$text = <>;
   # read standard input into the $text variable

$from = <<EOF;    # put lines to be replaced after this line
    Creature
  [
    Creature_AvoidWildMagic [ true ]  
  ]
EOF
    # put EOF line just after last line to be replaced

$to = <<EOF;      # put replacement lines after this line
    Creature
  [
    Creature_AvoidWildMagic [ false ]  
  ]
EOF
    # put EOF line just after last replacement line
  
$from =~ s/^\s+|\s+$//g;
    # remove whitespace from ends of $from
$from = quotemeta ( $from );
    # put \ just before all space and [ characters
    # and other regular expression metacharacters
$from =~ s/(\\[ \n\t])+/\\s+/g;
    # replace all whitespace (now with \'s before) by the
    # regular subexpression \s+

$text =~ s/$from/$to/g;
    # replace $from by $to in $text

print "$text";
