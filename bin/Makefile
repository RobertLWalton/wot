all:	chkpage efm

chkpage:	chkpage.c
	rm -f chkpage
	gcc -o chkpage chkpage.c
	chmod 555 chkpage

efm:	efm.c
	rm -f efm
	gcc -o efm efm.c
	chmod 555 efm

clean:
	rm -f chkpage efm
