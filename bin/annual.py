# Annual Return Calculator
#
# File:         annual.py
# Authors:      Bob Walton (walton@acm.org)
# Date:         Sun Aug  8 17:46:41 EDT 2021
#
# The authors have placed this program in the public
# domain; they make no warranty and accept no liability
# for this program.

import sys
import math


document = \
    "Program to Compute Annual Returns of an" + \
        " Investment\n" + \
    "\n" + \
    "Command: python3 annual.py FILE-NAME [YEAR]" + \
    "\n" + \
    "  FILE-NAME is the name of the input file.\n" + \
    "  YEAR is the first SELL year to be output.\n" + \
    "  If neither are given, this document is" + \
        " output.\n" + \
    "\n" + \
    "Input File Format:\n" + \
    "  First: Zero or more lines beginning" + \
        " with `#'.\n" + \
    "    These describe the investment.\n" + \
    "  Second: Two or more lines each of the" + \
        " form:\n" + \
    "             YEAR VALUE\n" + \
    "    Each of these means that the investment had\m\n" + \
    "    the given VALUE in the given YEAR.";


if len ( sys.argv ) <= 1:
    print ( document )
    exit ( 1 )
