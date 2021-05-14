


all: libsimplefs.a create_format app test

libsimplefs.a: 	simplefs.c
	gcc -c simplefs.c
	ar -cvq libsimplefs.a simplefs.o
	ranlib libsimplefs.a

create_format: create_format.c
	gcc -o create_format create_format.c   -L. -lsimplefs

app: 	app.c
	gcc -o app app.c -L. -lsimplefs

test: test.c
	gcc -o test test.c -L. -lsimplefs

clean: 
	rm -fr *.o *.a *~ a.out app test vdisk create_format
