#!/bin/bash

find $PWD ! -type d -not \( -name .svn -prune -o -name .hg -prune -o -name .git -prune -o -name ".*" -prune -o -name Makefile -prune \) -a \( -name \*.cpp -o -name \*.hpp -o -name \*.inl \) | uncrustify -c uncrustify.cfg -F - -l CPP --replace --no-backup
find $PWD ! -type d -not \( -name .svn -prune -o -name .hg -prune -o -name .git -prune -o -name ".*" -prune -o -name Makefile -prune \) -a \( -name \*.c -o -name \*.h \) | uncrustify -c uncrustify.cfg -F - -l C --replace --no-backup
}

