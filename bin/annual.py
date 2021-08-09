#!/bin/python3
#
# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Sun Aug  8 21:49:36 EDT 2021
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

import sys
import math


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
