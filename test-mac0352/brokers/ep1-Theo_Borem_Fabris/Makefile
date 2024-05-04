source=.
texName=apresentacao
texFile=$(source)/$(texName).tex
pdfFile=$(source)/$(texName).pdf
tarFiles=$(textFile) 
tarName=11735293Theo.tar

CC := gcc
pthreadsFLAGS := -pthread
CFLAGS := -g -O0 -Wall -Wextra -Wpedantic -Wno-variadic-macros ${pthreadsFLAGS}

all: mqttbroker

mac0352-servidor-exemplo-ep1: mac0352-servidor-exemplo-ep1.c
	${CC} mac0352-servidor-exemplo-ep1.c -o servidor

mqttbroker: mqttbroker.c queue.c search.c mqtt.c
	${CC} ${CFLAGS} mqttbroker.c queue.c search.c mqtt.c -o mqttbroker

.PHONY: clean debug* dataReport data

data:
	psrecord "./mqttbroker 3521" --log brokerlog.txt --plot brokerplot.png --include-children

debug0:
	valgrind --track-origins=yes --leak-check=full -v ./mqttbroker 3521

debug1:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all --show-reachable=yes -v ./mqttbroker 3521

pdffile: $(texFile)
	pdflatex $(texFile) 

xelatex: $(texFile)
	xelatex $(texFile)

tar:
	rm -rf ep1-Theo_Borem_Fabris
	mkdir ep1-Theo_Borem_Fabris
	cp Dockerfile Makefile README.md apresentacao.pdf \
		mqtt.c mqtt.h mqttbroker.c \
		queue.c queue.h search.c search.h \
		ep1-Theo_Borem_Fabris/
	tar -czvf entrega.tar.gz ep1-Theo_Borem_Fabris/

clean:
	rm -f mqttbroker servidor log/*
