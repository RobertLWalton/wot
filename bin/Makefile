all:	chkpage efm conf_helper remrestore \
        unicode2latin1 lrcs help mkbootable

help:	../help/git.help ../help/wot.help

../help/git.help:	git*
	rm -f ../help/git.help
	githelp > ../help/git.help

../help/wot.help:	*
	rm -f ../help/wot.help
	wothelp > ../help/wot.help

chkpage:	chkpage.c
	rm -f chkpage
	gcc -ansi -pedantic -o chkpage chkpage.c
	chmod 555 chkpage

unicode2latin1:	unicode2latin1.c
	rm -f unicode2latin1
	gcc -ansi -pedantic -o unicode2latin1 \
	    unicode2latin1.c
	chmod 555 unicode2latin1

efm:	efm.c
	rm -f efm
	gcc -std=c99 -pedantic -o efm efm.c
	chmod 555 efm

conf_helper:	conf_helper.c
	rm -f conf_helper
	gcc -ansi -pedantic -o conf_helper conf_helper.c
	chmod 555 conf_helper

lrcs:	lrcs.c
	rm -f lrcs
	gcc -ansi -pedantic -o lrcs lrcs.c
	chmod 555 lrcs

mkbootable:	mkbootable.c
	rm -f mkbootable
	gcc -ansi -pedantic -o mkbootable mkbootable.c
	chmod 555 mkbootable

remrestore:
	ln -s remsave remrestore

sniffbench:	sniffbench.c
	rm -f sniffbench
	gcc -ansi -pedantic -o sniffbench sniffbench.c
	chmod 555 sniffbench


clean:
	rm -f chkpage efm conf_helper remrestore \
	      sniffbench
