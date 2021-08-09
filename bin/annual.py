#!/bin/python3
#
# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Mon Aug  9 13:31:29 EDT 2021
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
if len ( sys.argv ) > 3:
    print ( "too many arguments" )
    exit ( 1 )

year_re = re.compile ( r"^\d\d\d\d$" )

description = []
values = {}
year_line = {}

filename = sys.argv[1]
line = ""
line_number = 0
input_done = False

def fail_message ( message ):
    if input_done:
        print ( 'FATAL ERROR: ' + message )
    else:
        print ( 'FATAL ERROR: ' + message + 
                ' in line number ' +
                str ( line_number ) +
                ':' + "\n" + line,
                file = sys.stderr );

def Fail ( message ):
    fail_message ( message )
    sys.exit ( 1 )

# Compute the average annual return from buy year to
# sell year, and return the resulting percentage as a
# string with 2 decimal places.  If buy year >= sell
# year, return ''.  Otherwise if no return can be
# computed, return 'X.XX'.
#
def compute_return ( buy, sell ):
    if buy >= sell: return ''
    bval = values.get ( buy )
    sval = values.get ( sell )
    if bval == None: return 'X.XX'
    if sval == None: return 'X.XX'
    r = sval / bval
    i = sell - buy
    r = math.exp ( math.log ( r ) / i )
    p = 100 * ( r - 1 )
    return format ( p, '.2f' )

# In the following functions, left is the leftmost
# sell year, right is the rightmost sell year,
# current is the current buy year.

def print_header ( left, right ):
    l1 = ' BUY   '
    l2 = ' YEAR |'
    col = 0
    sign = +1
    if left > right: sign = -1
    while True:
        l2 += format ( left, "8d" )
        col += 8
        if left == right: break
        if ( sign == +1 and right % 5 == 0 ) \
           or \
           ( sign == -1 and left % 5 == 0 ):
            l2 += ' |'
            col += 2
        if left > right: left += -1
        else:            left += +1

    l2 = l2.replace ( ' ', '_' )
    l1 += 'SELL YEAR'.center ( col )

    print ( l1 )
    print ( l2 )

def print_data ( current, left, right ):
    l = format ( current, "5d" )
    l += ' |'
    sign = +1
    if left > right: sign = -1
    while True:
        r = compute_return ( current, left )
        l += format ( r, ">8" )
        if left == right: break
        if ( sign == +1 and right % 5 == 0 ) \
           or \
           ( sign == -1 and left % 5 == 0 ):
            l += ' |'
        if left > right: left += -1
        else:            left += +1

    if current % 5 == 0:
        l = l.replace ( ' ', '_' )
    print ( l )

def print_page ( first_buy, last_buy, left, right ):
    for d in description:
        print ( d )
    print_header ( left, right )
    while True:
        print_data ( first_buy, left, right )
        if first_buy == last_buy: break
        if first_buy > last_buy: first_buy += -1
        else                   : first_buy += -1

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
        if not year_re.match ( lyear ):
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

    input_done = True
    years = list ( values )
    if len ( years ) == 0:
        Fail ( "there are no data lines" )
    if len ( years ) == 1:
        Fail ( "there is only one data lines" )
    years.sort()
    first_buy_year = years[0]
    last_buy_year = years[-1]

    if len ( sys.argv ) == 2:
        first_sell_year = first_buy_year + 1
        last_sell_year = last_buy_year
    else:
        lsell = sys.argv[2].strip()
        sell = lsell.split ( "-" )
        if len ( sell ) != 2:
            Fail ( sell + " argument is badly" +
                          " formatted" )
        if sell[0] == '':
            first_sell_year = first_buy_year + 1
        elif not year_re.match ( sell[0] ):
            Fail ( sell[0] + " in argument is not a" +
                             " 4-digit number" )
        else:
            first_sell_year = int ( sell[0] )
        if sell[1] == '':
            last_sell_year = last_buy_year
        elif not year_re.match ( sell[1] ):
            Fail ( sell[1] + " in argument is not a" +
                             " 4-digit number" )
        else:
            last_sell_year = int ( sell[1] )
        if first_sell_year > last_sell_year:
            Fail ( "sell year range " +
                   str ( first_sell_year ) +
                   "-" +
                   str ( last_sell_year ) +
                   " is empty" )

    print_page ( last_buy_year, first_buy_year,
                 last_sell_year, first_sell_year )


except FileNotFoundError:
    print ( "FATAL ERROR: could not open " +
            filename + " for reading" );
    sys.exit ( 1 )
except SystemExit:
    raise
except:
    fail_message ( "exception raised" )
    raise
