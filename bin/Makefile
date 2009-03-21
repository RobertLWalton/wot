all:	chkpage efm conf_helper

chkpage:	chkpage.c
	rm -f chkpage
	gcc -ansi -pedantic -o chkpage chkpage.c
	chmod 555 chkpage

efm:	efm.c
	rm -f efm
	gcc -ansi -pedantic -o efm efm.c
	chmod 555 efm

conf_helper:	conf_helper.c
	rm -f conf_helper
	gcc -ansi -pedantic -o conf_helper conf_helper.c
	chmod 555 conf_helper

clean:
	rm -f chkpage efm conf_helper
