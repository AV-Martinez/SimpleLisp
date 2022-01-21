O = ./obj/
S = ./src/

OBJS = $(O)main.o $(O)memory.o $(O)parser.o $(O)lisp.o 

# debugger 
# OPTS = -g -O0
OPTS =

./lisp: $(OBJS)
	gcc $(OPTS) $(OBJS) -o ./lisp
	
$(O)main.o: $(S)main.cpp $(S)memory.h $(S)lisp.h
	gcc -c $(OPTS) $(S)main.cpp -o $(O)main.o
	
$(O)memory.o: $(S)memory.cpp $(S)memory.h
	gcc -c $(OPTS) $(S)memory.cpp -o $(O)memory.o

$(O)parser.o: $(S)parser.cpp $(S)parser.h $(S)memory.h
	gcc -c $(OPTS) $(S)parser.cpp -o $(O)parser.o

$(O)lisp.o: $(S)lisp.cpp $(S)lisp.h $(S)parser.h $(S)memory.h
	gcc -c $(OPTS) $(S)lisp.cpp -o $(O)lisp.o

