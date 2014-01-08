CC=gcc
INCLUDES=-I/usr/include/libxml2 
LIBS= -lm   -lxml2

all:	svg2ovl demos

demos:
	$(MAKE) -C demo

%.o:	%.c
	${CC} -c $< ${INCLUDES}

%:	%.c
	${CC} -o $@ $< ${INCLUDES} ${LIBS}

clean:
	$(MAKE) -C demo clean
	
