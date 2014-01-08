FIGURES=$(addsuffix .log,$(basename $(shell ls *.svg)))

all: $(FIGURES)

%.log: %.svg
	../svg2ovl $<  `basename $< .svg`-#.pdf 
	echo " "> $@ 

clean:
	rm -fv *.pdf *.log
