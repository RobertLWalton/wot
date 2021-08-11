#!/bin/python3
#
# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Mon Aug  9 17:24:00 EDT 2021
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
  Without options, one big page.  If you are going to
  print portrait, you can have up to 50 buy years but
  just 5 sell years.  If you are going to print
  landscape, you can have 25 buy years and 10 sell
  years.  Otherwise you can view any size page in a
  window.

  The -P option splits the output into multiple portrait
  sized pages.  The -L option splits the output into
  multiple landscape sized pages.  These pages can be
  put side-by-side to get the big picture.

  Output values that cannot be computed because data
  for the buy or sell year is missing is represented
  as X.XXX.  The value for a buy year the equals or
  is greater than the sell year is blank.  Pages all
  of whose values would be blank are not output.

  The output can be redirected to a file by the command:

    python3 annual.py [-P|-L] INPUT-FILE-NAME \\
                      [SELL-YEARS] > OUTPUT-FILE-NAME
""";


max_buy_per_page = 1000000
max_sell_per_page = 1000000
filename = None
sell_years = None

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
if i < argc:
    sell_years = sys.argv[i]
    i += 1
if i < argc:
    print ( "too many arguments" )
    exit ( 1 )

year_re = re.compile ( r"^\d\d\d\d$" )

description = []
values = {}
year_line = {}

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
        if r == '': l += '        '
        else: l += format ( r, ">7" ) + '%'
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

    if sell_years == None:
        first_sell_year = first_buy_year + 1
        last_sell_year = last_buy_year
    else:
        sell_years = sell_years.strip()
        sell = sell_years.split ( "-" )
        if len ( sell ) != 2:
            Fail ( sell_years + " argument is badly" +
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

    s = last_sell_year
    while s >= first_sell_year:
        sy = min ( s - first_sell_year + 1,
                   max_sell_per_page )
        sy = s - sy + 1
        b = last_buy_year - 1
        while b >= first_buy_year:
            by = min ( b - first_buy_year + 1,
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
