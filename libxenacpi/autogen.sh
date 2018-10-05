#!/bin/sh
#
# bstrap:
#
# $Id:$
#
# $Log:$
#
#
#
libtoolize -f -c  --automake
aclocal
autoheader
autoconf
automake -a -c 
automake -a -c Makefile
automake -a -c src/Makefile
automake -a -c test/Makefile
