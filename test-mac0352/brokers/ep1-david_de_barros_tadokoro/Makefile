CC				:= gcc
CFLAGS		:= -Wall -o
BIN				:= ./bin/

all : brokerMQTT

ifndef NO_COLORS
DEF     := \033[0;38m
GREEN   := \033[1;32m
YELLOW  := \033[1;33m
BLUE    := \033[1;34m
RES     := \033[0m
endif

define msg-green
printf "${GREEN}%b${RES}\n" $1
endef

define msg-yellow
printf "${YELLOW}%b${RES}\n" $1
endef

define msg-blue
printf "${BLUE}%b${RES}\n" $1
endef

brokerMQTT : brokerMQTT.c
	@$(call msg-yellow, "Compilando $<")
	@$(CC) $(CFLAGS) $@ $<
	@$(call msg-green, "Executável $@ gerado com sucesso")
	@$(call msg-blue, "Uso:\t./brokerMQTT <Porta>")

.PHONY : clean
clean :
	@$(call msg-green, "Removendo executável brokerMQTT")
	@rm -rf brokerMQTT 2> /dev/null
