#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "memory.h"

MemoryClass Memory;

void MemoryClass::Init() {
	printf("%d memory cells available (%ld KB)\n", MEMSIZE, MEMSIZE*sizeof(MemoryCell)/1024);
	printf("Type ?<enter> for help.\n");
	for (int i = 0; i < MEMSIZE; i++) {
		Mem[i].available = true;
		Mem[i].mark = false;
	}
	UsedCells      = 0;
	GCNumberDone   = 0;
	GCTimeSpent    = 0;
	GCConsesFreed  = 0;
	GCConsesMarked = -1;
	
	MemIx       = 1; /// address 0 is reserved to represent NIL with a (0,0) cons
	DEFVARS     = _NIL_;
	DEFUNS      = _NIL_;
	GCSAFE      = _NIL_;
	RETURNS     = _NIL_;
	TRACEDFUNCS = _NIL_;
}

addr MemoryClass::CreateCell(addr car, addr cdr) {
	while (!Mem[MemIx].available) MemIx++; CheckEndOfMemory(); UsedCells++;
	Mem[MemIx].available = false;
	Mem[MemIx].type = 'C';
	Mem[MemIx].car = car;
	Mem[MemIx].cdr = cdr;
	MemIx++;
	return MemIx-1;
}

addr MemoryClass::CreateCell(char *t) {
	while (!Mem[MemIx].available) MemIx++; CheckEndOfMemory(); UsedCells++;
	Mem[MemIx].available = false;
	Mem[MemIx].type = IsNumber(t) ? 'N' : 'S';
	if 		(Mem[MemIx].type == 'N') Mem[MemIx].value = atoi(t);
	else if (Mem[MemIx].type == 'S') Mem[MemIx].name  = strdup(t);
	MemIx++;
	return MemIx-1;
}

addr MemoryClass::CreateCell(long v) {
	while (!Mem[MemIx].available) MemIx++; CheckEndOfMemory(); UsedCells++;
	Mem[MemIx].available = false;
	Mem[MemIx].type = 'N';
	Mem[MemIx].value = v;
	MemIx++;
	return MemIx-1;
}

void MemoryClass::Print(addr sexpr) {
	if 		(Mem[sexpr].type == 'S') printf("%s", Mem[sexpr].name);
	else if (Mem[sexpr].type == 'N') printf("%ld", Mem[sexpr].value);
	else if (Mem[sexpr].type == 'C') {
		if (Mem[sexpr].car == 0 && Mem[sexpr].cdr == 0)
			printf("()");
		else if (Mem[Mem[sexpr].cdr].type != 'C') { /// a cons
			printf("("); Print(Mem[sexpr].car); printf(" . "); Print(Mem[sexpr].cdr); printf(")");
		}
		else {
			printf("(");
			PrintList(sexpr);
		}
	}
}

void MemoryClass::PrintList(addr sexpr) {
	Print(Mem[sexpr].car);
	addr cdr = Mem[sexpr].cdr;
	if (Mem[cdr].car == 0 && Mem[cdr].cdr == 0) 
		printf(")");
	else {
		printf(" ");
		PrintList(Mem[sexpr].cdr);
	}
}

void MemoryClass::Dump() {
	const int addrsz = 4;
	addr upperlimit = MEMSIZE-1;
	while (Mem[upperlimit].available) upperlimit--;
	
	bool inAvaSpc = false;
	for (int i = 0; i < addrsz*3+4; i++) printf("*"); printf("\n");
	printf("Used %d/%d (%d%%)\n", UsedCells, MEMSIZE, (UsedCells*100)/MEMSIZE);
	for (int i = 0; i < upperlimit+1; i++) {
		if (i == 0) printf("%0*d NIL\n", addrsz, i);
		else {
			if (Mem[i].available) {
				if (!inAvaSpc) inAvaSpc = true;
			}
			else {
				if (inAvaSpc) {
					for (int i = 0; i < addrsz; i++) printf("."); printf("\n");
					inAvaSpc = false;
				}
				printf("%0*d %c ", addrsz, i, Mem[i].type);
				if 		(Mem[i].type == 'S') printf("%s\n", Mem[i].name);
				else if (Mem[i].type == 'N') printf("%ld\n", Mem[i].value);
				else if (Mem[i].type == 'C') {
					printf("%0*d %0*d", addrsz, Mem[i].car, addrsz, Mem[i].cdr);
					if 		(i == DEFVARS) 		printf(" DEFVARS\n");
					else if (i == DEFUNS)  		printf(" DEFUNS\n");
					else if (i == GCSAFE)  		printf(" GCSAFE\n");
					else if (i == RETURNS) 		printf(" RETURNS\n");
					else if (i == TRACEDFUNCS) 	printf(" TRACEDFUNCS\n");
					else 				   		printf("\n");
				}
			}
		}
	}
	for (int i = 0; i < addrsz*3+4; i++) printf("*"); printf("\n");
}

bool MemoryClass::IsNIL(addr c) {
	if (Mem[c].type == 'C' && Mem[c].car == 0 && Mem[c].cdr == 0) return true;
	return false;
}

void MemoryClass::GC(const char *msg) {
	GCNumberDone++;
	long m0 = Millis();
	for (addr i = 0; i < MEMSIZE; i++) Mem[i].mark = false;
	GCConsesMarked = 0;
	Mark(_DEFVARS_);
	Mark(_DEFUNS_);
	Mark(_GCSAFE_);
	Mark(_RETURNS_);
	Mark(_TRACEDFUNCS_);
	printf("[   gc] %s >> Used mem: %d%%\n", msg, USEDMEMPCT);
	long markms = Millis()-m0; if (markms > 0) GCTimeSpent += markms;
	long m1 = Millis();
	Sweep();
	long sweepms = Millis()-m1; if (sweepms > 0) GCTimeSpent += sweepms;
	printf("[   gc]    Mark/Sweep %ld/%ld ms\n", markms, sweepms);
	printf("[   gc] << Used mem: %d%%\n", USEDMEMPCT);
	MemIx = 1; /// Start over when CreateCons is called
}

bool MemoryClass::IsNumber(char *v) {
	if ((*v == '+' || *v == '-') && *(v+1) == '\0') return false;
	
	int inix = (*v == '+' || *v == '-') ? 1 : 0;
	bool alldigits = true;
	for (int i = inix; i < strlen(v); i++) 
		if (v[i] < '0' || v[i] > '9') {
			alldigits = false;
			break;
		}
	return alldigits;
}

long MemoryClass::Millis() {
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
    time_t secs = spec.tv_sec;
    long ms = spec.tv_nsec / 1.0e6; /// Convert nanoseconds to milliseconds
    return ms;
}

void MemoryClass::Mark(addr memaddr) {
	if (Mem[memaddr].mark) return;
	GCConsesMarked++;
	Mem[memaddr].mark = true;
	if (Mem[memaddr].type == 'C') {
		if ((Mem[memaddr].car == 0 && Mem[memaddr].cdr != 0) || (Mem[memaddr].car != 0 && Mem[memaddr].cdr == 0)) {
			printf("[   gc] Internal mem error at %d\n", memaddr);
			return;
		}
		if (IsNIL(memaddr)) return;
		Mark(Mem[memaddr].car);
		Mark(Mem[memaddr].cdr);
	}
}

void MemoryClass::Sweep() {
	addr freed = 0;
	for (int i = 0; i < MEMSIZE; i++) {
		if (!Mem[i].mark && !Mem[i].available) {
			if (Mem[i].type == 'S') free (Mem[i].name);
			Mem[i].available = true;
			freed++;
		}
		Mem[i].mark = false;
	}
	UsedCells -= freed;
	GCConsesFreed += freed;
}

void MemoryClass::CheckEndOfMemory() {
	if (MemIx == MEMSIZE) { 
		printf("\nMemory exhausted.\nIncrease MEMSIZE or decrease PCT_TRIGGER_GC.\nExiting.\n"); 
		exit(0); 
	}
}
