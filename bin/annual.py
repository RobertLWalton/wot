#!/bin/python3
#
# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Mon Aug 30 16:23:59 EDT 2021
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

import sys
import math
import re

document = """
Program to Compute Annual Returns of an Investment

Command: python3 annual.py [-L|-P] \\
                 INPUT-FILE-NAME [SELL-YEARS]

  INPUT-FILE-NAME is the name of the input file.

  SELL-YEARS is a range of years: e.g., 1970-2020,
  1970-, -2020.  Years not given are given the
  maximum or minimum values possible.  It is also
  possible to just give a number, e.g., 20, in which
  case the first sell year will be the last data year
  minus the number plus 1.  E.g., if the last data year
  is 2020 and SELL-YEARS is 15, the first sell year will
  be 2006, and the number of sell years will be 15.
  
  If neither INPUT-FILE-NAME nor SELL-YEARS are given,
  this document is output.

Input File Format:

  Zero or more `description lines' each beginning
    with `*'.  These describe the investment.

  Optional sales charge lines of the form:
             pc PURCHASE-CHARGE
             rc REDEMPTION-CHARGE
    The charges are percentages (without the %).
    To buy shares with value V you pay:
        V / ( 1 - PURCHASE-CHARGE/100 )
    Upon redeeming shares with value V you receive:
        V * ( 1 - REDEMPTION-CHARGE/100 )
    These input lines also generate description lines
    of the form:
        * Purchase Charge = X.XX%
        * Redemption Charge = X.XX%

  Two or more data lines each of the form:
             YEAR VALUE
    Each of these means that the investment had the
    given VALUE at a given time in the given YEAR.
    The within-year time must be the same for all
    YEAR's: e.g., it might be midnight December 31,
    or more specifically, the end of the last trading
    session in the YEAR.

    The YEAR VALUE lines can be in no particular order.
    YEAR VALUE lines for some intermediate years may be
    missing.

  The input lines may be in any order, but the output
  of description lines will be in their input order.

Output Format:

  Without options, the output is one big page.  If there
  are at most 51 data lines and 5 sell years, it can be
  printed as a portrait page.  If there are at most 26
  data lines and 10 sell years, it can be printed as a
  landscaped page.  Otherwise is can be viewed in a
  computer window.

  The -L option splits the output into multiple land-
  scape sized pages.  Landscape pages can have at most
  25 buy years but can have up to 10 sell years.

  The -P option splits the output into multiple portrait
  sized pages.  Portrait pages can have up to 50 buy
  years but at most 5 sell years.
  
  -L or -P Pages can be put side-by-side to get the
  big picture.

  Output values that cannot be computed because data
  for the buy or sell year is missing are represented
  as X.XXX.  The value whose buy year equals or is
  greater than its sell year is blank.  Pages all of
  whose values would be blank are not output.

  Input data VALUEs are printed for the buy and sell
  years as they appear in the input.  Missing data
  VALUEs print as XXX.

  The output can be redirected to a file by the command:

    python3 annual.py [-P|-L] INPUT-FILE-NAME \\
                      [SELL-YEARS] > OUTPUT-FILE-NAME

Example:

  If abc.in is an input file with at most 26 data lines,
  then

    python3 annual.py -L abc.in 20 > abc.out

  will output one or two landscape pages in abc.out.

     
""";

# Data

max_buy_per_page = 1000000
max_sell_per_page = 1000000
filename = None
sell_years = None
pc = 0
rc = 0

argc = len ( sys.argv )
i = 1

if i >= argc:
    print ( document )
    exit ( 1 )

n = sys.argv[i]
if n == '-L':
    max_buy_per_page = 25
    max_sell_per_page = 10
    i += 1
elif n == '-P':
    max_buy_per_page = 50
    max_sell_per_page = 5
    i += 1

if i < argc:
    filename = sys.argv[i]
    i += 1
else:
    print ( document )
    exit ( 1 )

if i < argc:
    sell_years = sys.argv[i]
    i += 1

if i < argc:
    print ( "too many arguments" )
    exit ( 1 )

year_re = re.compile ( r"^\d\d\d\d$" )
non_zero_re = re.compile ( r"^[1-9]\d*$" )

description = []
    # Lines of the form `* ...' in order.

# In the following, given data line `YEAR VALUE', then
# year = int ( 'YEAR' ) and value = float ( 'VALUE' )
#
values = {}
    # values[year] = value
year_line = {}
    # year_line[year] is line number of `YEAR VALUE'
lvalues = {}
    # lvalues[year] = 'VALUE'

line = ""
line_number = 0
input_done = False

# Functions

def fail_message ( message ):
    if input_done:
        print ( 'FATAL ERROR: ' + message,
                file = sys.stderr );
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
    bval = bval / ( 1 - pc/100 )
    sval = sval * ( 1 - rc/100 )
    r = sval / bval
    i = sell - buy
    r = math.exp ( math.log ( r ) / i )
    p = 100 * ( r - 1 )
    return format ( p, '.2f' )

# In the following functions, left is the leftmost
# sell year, right is the rightmost sell year,
# current is the current buy year.

def print_header ( left, right ):
    l1 = '               '
    l2 = ' BUY          |'
    l3 = ' YEAR   VALUE |'
    col = 0
    sign = +1
    if left > right: sign = -1
    while True:
        l2 += format ( left, "8d" )
        col += 8
        lvalue = lvalues.get ( left )
        if lvalue == None:
            lvalue = 'XXX'
        l3 += format ( lvalue, ">8" )
        if left == right: break
        if ( sign == +1 and (left + sign) % 5 == 0 ) \
           or \
           ( sign == -1 and left % 5 == 0 ):
            l2 += ' |'
            l3 += ' |'
            col += 2
        left += sign

    l3 = l3.replace ( ' ', '_' )
    l1 += 'SELL YEAR'.center ( col )

    print ( l1 )
    print ( l2 )
    print ( l3 )

def print_data ( current, left, right ):
    l = format ( current, "5d" )
    lvalue = lvalues.get ( current )
    if lvalue == None:
        lvalue = 'XXX'
    l += format ( lvalue, ">8" )
    l += ' |'
    sign = +1
    if left > right: sign = -1
    while True:
        r = compute_return ( current, left )
        if r == '': l += '        '
        else:
            c = format ( r, ">7" ) + '%'
            if ( left - current ) % 5 == 0:
                c = c.replace ( ' ', '.' )
            l += c
        if left == right: break
        if ( sign == +1 and (left + sign) % 5 == 0 ) \
           or \
           ( sign == -1 and left % 5 == 0 ):
            l += ' |'
        left += sign
    if current % 5 == 0:
        l = l.replace ( ' ', '_' )

    print ( l )

first_page = True
def print_page ( first_buy, last_buy, left, right ):
    b = min ( first_buy, last_buy )
    s = max ( left, right )
    if b >= s: return # blank page

    global first_page
    if not first_page:
        print ( "" )
    else:
        first_page = False

    print ( '* Average Annual Return in Percent' +
            ' for Various Buy and Sell Years' )
    for d in description:
        print ( d )
    print_header ( left, right )
    while True:
        print_data ( first_buy, left, right )
        if first_buy == last_buy: break
        if first_buy > last_buy: first_buy += -1
        else                   : first_buy += -1

# Main Program
#
try:

    # Read input file
    #
    f = open ( filename )
    while True:
        line = f.readline()
        if line == '': break
        line_number += 1
        line = line.rstrip()
        if line == '': continue
        if line[0] == '*':
            description.append ( line )
            continue
        line = line.strip()
        pair = line.split()
        if len ( pair ) != 2:
            Fail ( "badly formatted data line" )
        if pair[0] == 'pc':
            try:
                pc = float ( pair[1] )
            except:
                Fail ( pair[1] + " is not a floating" +
                                 " point number" )
            description.append \
                ( '* Purchase Charge = ' +
                  format ( pc, '.2f' ) + '%' );
            continue
        if pair[0] == 'rc':
            try:
                rc = float ( pair[1] )
            except:
                Fail ( pair[1] + " is not a floating" +
                                 " point number" )
            description.append \
                ( '* Redemption Charge = ' +
                  format ( rc, '.2f' ) + '%' );
            continue
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
            Fail ( lyear + " was also in line " +
                           str ( year_line[year] ) )
        year_line[year] = line_number
        values[year] = value
        lvalues[year] = lvalue
    input_done = True  # For fail_message

    # Compute data years
    #
    years = list ( values )
    if len ( years ) == 0:
        Fail ( "there are no data lines" )
    if len ( years ) == 1:
        Fail ( "there is only one data lines" )
    years.sort()
    first_data_year = years[0]
    last_data_year = years[-1]

    # Compute sell years
    #
    if sell_years == None:
        first_sell_year = first_data_year + 1
        last_sell_year = last_data_year
    else:
        sell_years = sell_years.strip()
        sell = sell_years.split ( "-" )
        if len ( sell ) == 1:
            if not non_zero_re.match ( sell[0] ):
                Fail ( sell[0] + " argument is not" +
                                 " a non-zero number" )
            first_sell_year = \
                last_data_year - int ( sell[0] ) + 1
            last_sell_year = last_data_year
        elif len ( sell ) != 2:
            Fail ( sell_years + " argument is badly" +
                                " formatted" )
        else:
            if sell[0] == '':
                first_sell_year = first_data_year + 1
            elif not year_re.match ( sell[0] ):
                Fail ( sell[0] + " in argument is not" +
                                 " a 4-digit number" )
            else:
                first_sell_year = int ( sell[0] )

            if sell[1] == '':
                last_sell_year = last_data_year
            elif not year_re.match ( sell[1] ):
                Fail ( sell[1] + " in argument is not" +
                                 " a 4-digit number" )
            else:
                last_sell_year = int ( sell[1] )

        if first_sell_year > last_sell_year:
            Fail ( "sell year range " +
                   str ( first_sell_year ) +
                   "-" +
                   str ( last_sell_year ) +
                   " is empty" )

    # Check buy/sell overlap
    #
    if first_sell_year > last_data_year:
        Fail ( "First sell year = " +
               str ( first_sell_year ) +
               " > " + str ( last_data_year ) +
               " = last data year" )
    if last_sell_year <= first_data_year:
        Fail ( "Last sell year = " +
               str ( last_sell_year ) +
               " <= " + str ( first_data_year ) +
               " = first data year" )

    # Output pages
    #
    if first_sell_year < first_data_year + 1:
        first_sell_year = first_data_year + 1
    if last_sell_year > last_data_year:
        last_sell_year = last_data_year

    s = last_sell_year
    while s >= first_sell_year:
        sy = min ( s - first_sell_year + 1,
                   max_sell_per_page )
        sy = s - sy + 1
        b = last_data_year - 1
        while b >= first_data_year:
            by = min ( b - first_data_year + 1,
                       max_buy_per_page )
            by = b - by + 1
            print_page ( b, by, s, sy )
            b = by - 1
        s = sy - 1


except FileNotFoundError:
    print ( "FATAL ERROR: could not open " +
            filename + " for reading" );
    sys.exit ( 1 )
except SystemExit:
    raise
except:
    fail_message ( "exception raised" )
    raise
