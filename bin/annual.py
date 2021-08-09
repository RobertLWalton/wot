#!/bin/python3
#
# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Mon Aug  9 05:08:35 EDT 2021
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

import sys
import math
import re

document = """
Program to Compute Annual Returns of an Investment

Command: python3 annual.py INPUT-FILE-NAME [SELL-YEARS]

  INPUT-FILE-NAME is the name of the input file.
  SELL-YEARS is a range of years: e.g., 1970-2020.
  If not given the maximum possible range is used.
  If neither INPUT-FILE-NAME nor SELL-YEARS are given,
  this document is output.

Input File Format:
  First: Zero or more `description lines' each beginning
    with `*'.  These describe the investment.
  Second: Two or more lines each of the form:
             YEAR VALUE
    Each of these means that the investment had the
    given VALUE at a given time in the given YEAR.
    The within-year time must be the same for all
    YEAR's: e.g., it might be midnight December 31,
    or more specifically, the end of the last trading
    session in the YEAR.

    The YEAR VALUE lines can be in no particular order.

Output Format:
  There are typically 50 buy years and 5 sell years on
  a page, and pages with different sell years can be
  placed side-by-side to get a better picture of the
  annual returns.  Space is reserved in the output for
  values that cannot be computed because years are
  missing in the input, with `---' indicating such
  space.  Such missing data has no effect on the
  values that can be computed.

  The output can be redirected to a file by the command:

    python3 annual.py INPUT-FILE-NAME [SELL-YEARS] \\
                    > OUTPUT-FILE-NAME
""";


if len ( sys.argv ) <= 1:
    print ( document )
    exit ( 1 )

year_re = re.compile ( r"^\d\d\d\d$" )

description = []
values = {}
year_line = {}

filename = sys.argv[1]
line = ""
line_number = 0

def fail_message ( message ):
    print ( 'FATAL ERROR: ' + message + 
            ' in line number ' + str ( line_number ) +
            ':' + "\n" + line,
            file = sys.stderr );

def Fail ( message ):
    fail_message ( message )
    sys.exit ( 1 )

try:
    f = open ( filename )
    description_done = False
    while True:
        line = f.readline()
        if line == '': break
        line_number += 1
        line = line.rstrip()
        if line == '': continue
        if ( not description_done ):
            if line[0] == '*':
                description.append ( line )
                continue
            else:
                description_done = True
        line = line.strip()
        pair = line.split()
        if len ( pair ) != 2:
            Fail ( "badly formatted data line" )
        lyear = pair[0]
        lvalue = pair[1]
        match = year_re.match ( lyear )
        if not match:
            Fail ( lyear + " is not a 4-digit" +
                          " year number" )
        year = int ( lyear )
        try:
            value = float ( lvalue )
        except:
            Fail ( lvalue + " is not a floating" +
                            " point number" )
        if value <= 0:
            Fail ( lvalue + " is negative or zero" )
        if year_line.get ( year ):
            Fail ( lyear + " is was in line " +
                           str ( year_line[year] ) )
        year_line[year] = line_number
        values[year] = value
        


except FileNotFoundError:
    print ( "FATAL ERROR: could not open " +
            filename + " for reading" );
    sys.exit ( 1 )
except SystemExit:
    raise
except:
    fail_message ( "exception raised" )
    raise
