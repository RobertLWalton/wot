all:	chkpage efm conf_helper remrestore \
        unicode2latin1

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

remrestore:
	ln -s remsave remrestore

sniffbench:	sniffbench.c
	rm -f sniffbench
	gcc -ansi -pedantic -o sniffbench sniffbench.c
	chmod 555 sniffbench


clean:
	rm -f chkpage efm conf_helper remrestore \
	      sniffbench
