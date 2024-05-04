################################################################################

CC := gcc
CFLAGS := -Wall

DIR_BIN := bin
DIR_SRC := src

OBJ := $(shell find ${DIR_SRC} -type f)
BIN := ${DIR_BIN}/mosquitto

SLIDE := apresentacao

ARCHIVE_DIR := ep1-andre_souza_abreu
ARCHIVE := ${ARCHIVE_DIR}.tar.gz

################################################################################

all: ${BIN}

clean:
	@rm ${BIN}

run: ${BIN}
	./${BIN} 1883

${BIN}: ${OBJ}
	if [[ ! -d ${DIR_BIN} ]]; then mkdir ${DIR_BIN}; fi
	${CC} ${CFLAGS} $^ -o $@

tar: ${ARCHIVE}

${ARCHIVE_DIR}: ${DIR_SRC} ${SLIDE}.pdf Makefile README.md
	mkdir $@
	cp -r $^ $@

${ARCHIVE}: ${ARCHIVE_DIR}
	tar czf $@ $^
	rm -rf $^

# slide

slide: ${SLIDE}.pdf

${SLIDE}.pdf: ${SLIDE}.md
	pandoc -t beamer -o $@ $^

# tests

container: 
	docker build --tag mosquitto tests/

test1: container
	psrecord \
		--log tests/logs/log1.txt \
		--plot tests/plots/plot1.png \
		--duration 100 \
		--include-children \
		"./${BIN} 1883"

test2: container
	psrecord \
		--log tests/logs/log2.txt \
		--plot tests/plots/plot2.png \
		--duration 300 \
		--include-children \
		"./${BIN} 1883" &
	(timeout 300s ./tests/test.sh 100); ./tests/kill_containers.sh

test3: container
	psrecord \
		--log tests/logs/log3.txt \
		--plot tests/plots/plot3.png \
		--duration 1100 \
		--include-children \
		"./${BIN} 1883" &
	(timeout 1100s ./tests/test.sh 1000); ./tests/kill_containers.sh

test: test1 test2 test3

# development stuff

watch:
	echo ${SLIDE}.md | entr make ${SIDE}.pdf

tags: ${OBJ}
	ctags $^

.PHONY: all clean run slide tar test test1 test2 test3 watch
