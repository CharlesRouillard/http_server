#			Programmation Repartie (4I400)
#		Makefile pour projet - Un serveur HTTP


# Documentation: http://www.gnu.org/software/make/manual/make.html
CC = gcc
LDFLAGS = -lpthread
CFLAGS =-W -Wall -ansi -pedantic -Iinclude

DIR=.
BIN=$(DIR)/bin/
OBJ=$(DIR)/obj/
INCLUDE=$(DIR)/include/
LIB=$(DIR)/lib/
SRC=$(DIR)/src/

HC=

.SUFFIXES:
.PHONY: all clean http_server test-client
all: $(BIN)http_server $(BIN)client_test

http_server: $(BIN)http_server
	-$$PWD/bin/http_server 8080 5 0

test-client: $(BIN)client_test
	-$$PWD/bin/client_test

$(BIN)%: $(OBJ)%.o
	@if [ -d $(BIN) ]; then : ; else mkdir $(BIN); fi
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ)%.o: $(SRC)%.c $(HC)
	@if [ -d $(OBJ) ]; then : ; else mkdir $(OBJ); fi
	$(CC) $(CFLAGS) -o $@ -c $<

$(INCLUDE)%.h:
	@if [ -d $(INCLUDE) ]; then : ; else mkdir $(INCLUDE); fi

clean: 
	rm -rf $(OBJ)*.o $(BIN)*
