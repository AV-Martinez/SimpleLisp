#pragma once

/**
 * The memory model consists of an array of the MemoryCell struct. This is not optimized for storage
 * usage but provides a clear view of the memory model. Memory is consumed solely by calls to the
 * overloaded CreateCell function, which scans the memory space looking for the next available memory
 * cell.
 * 
 * MEMSIZE defines the maximum size of the memory:
 * 		- The first address is at 1 so that address 0 is never reachable and can be used to 
 * 		  represent NIL by a cons(0,0).
 * 		- There are some addresses above MEMSIZE that are used to represent unique addr values
 *   	  to represent special statuses or conditions, facilitating the writing of some functions.
 * 
 * Atoms are represented by memory cells of type S (symbols) or N (numbers). Lists are represented
 * by linked memory cells of type C (cons). The end of a list is marked with a cons(0,0). An empty
 * list item is represented by cons(0,0). In other words, a cons(0,0) in the car of a cons represents
 * an empty list, a cons(0,0) in the cdr of a cons represents the end of the list. Examples (lower case
 * letters represent addresses):
 * 
 * 		List at addr L: (S1 S2 S3)
 * 
 * 		   L ---> cons(a,b) ---> b:cons(c,d) ---> d:cons(e,f) ---> f:cons(0,0)
 *                     V                V                V
 * 					   a:symbol(S1)     c:symbol(S2)     e:symbol(S3)
 * 
 * 		List at addr L: (S1 () S2)
 *
 * 		   L ---> cons(a,b) ---> b:cons(c,d) ---> d:cons(e,f) ---> f:cons(0,0)
 *                     V                V                V
 * 					   a:symbol(S1)     c:cons(0,0)      e:symbol(S2)
 * 
 * 		List at addr L: ()
 * 
 * 		   L ---> cons(0,0)
 * 
 * 		List at addr L: (())
 * 
 * 		   L ---> cons(a,b) ---> f:cons(0,0)
 *                     V        
 * 					   a:cons(0,0)
 * 
 * As a design principle, a pointer to a cons(0,0) could have been substituted by the value 0 as long as
 * memory cells start to be consumed at index 1 (which is the case), but consuming this extra cell facilitates
 * to some extent the writing of some functions across the interpreter.
 * 
 * On memory initialization, some important lists are created:
 * 		DEFVARS, to hold global variables
 *  	DEFUNS, to hold the defuned functions
 * 		GCSAFE, to hold conses that must survive the gc process.
 * 		RETURNS, to hold a stack for the management of the Lisp (return) function
 * 		TRACEDFUNCS, to hold the list of defuned functions which are marked to be traced via (trace)
 * 
 * The garbage collection approach is based on a simple Mark/Seep algorithm. Sexprs that need to be
 * safe from gc should be kept in the _GCSAFE_ list. At Mark time all conses in the above mentioned lists
 * are marked to be kept. At Sweep time, those conses not marked are set to available. MemIx is reset to 1
 * to start exploring available memory space in sequence.
 */

#define MEMSIZE 		1000000		/** Number of memory cells 										*/
#define PCT_TRIGGER_GC	80			/** Percentage of MEMSIZE used to trigger garbage collection	*/

/// Utility defines to access MemoryCells
#define _NIL_			Memory.CreateCell(0,0)
#define _T_				Memory.CreateCell((char *)"T")
#define _DEFVARS_		Memory.DEFVARS
#define _DEFUNS_		Memory.DEFUNS
#define _GCSAFE_		Memory.GCSAFE
#define _RETURNS_   	Memory.RETURNS
#define _TRACEDFUNCS_	Memory.TRACEDFUNCS
#define ISNIL(x)		Memory.IsNIL(x)
#define TYPE(x)			Memory.Mem[x].type
#define VALUE(x)		Memory.Mem[x].value
#define NAME(x)			Memory.Mem[x].name
#define CAR(x)			Memory.Mem[x].car
#define CDR(x)			Memory.Mem[x].cdr
#define USEDMEMPCT		((Memory.UsedCells*100)/MEMSIZE)

typedef unsigned int addr;	/// Index on memory array 

/// These are utility hacks to ease the writing of some functions.
/// They are used by functions using/returning addr to signal special conditions.
/// Using this hacks avoids making the functions more complex by having to add extra parameters.
/// This is better understood by analizing the context in which these defines are used.
#define ENDOFLIST    	MEMSIZE+1
#define ENDOFSEXPR   	MEMSIZE+2
#define TRAVERSEMARK 	MEMSIZE+3
#define RETURNMARK	 	MEMSIZE+4
#define DONTUSEBINDINGS MEMSIZE+5

struct MemoryCell {
	bool available;
	char type; 			/// (N)umber (S)ymbol (C)ons
	bool mark;			/// Used in gc processing
	union {
		long value;		/// Case Number
		char *name;		/// Case Symbol
		struct {		/// Case Cons
			addr car;
			addr cdr;
		};
	 };
};

class MemoryClass {
public:
	void Init();
	addr CreateCell(addr car, addr cdr);
	addr CreateCell(char *t);
	addr CreateCell(long n);
	
	void Print(addr sexpr);
	void Dump();
	bool IsNIL(addr);
	
	void GC(const char *msg);	/// Garbage collection
	
	addr DEFVARS; 				/// Global symbol bindings (an assoc list)
	addr DEFUNS;  				/// Defuns (an assoc list)
	addr GCSAFE;  				/// GC safe stack (a list of sexpr). Sexprs in this list are not subject to gc
	addr RETURNS;				/// Lisp (return) stack management
	addr TRACEDFUNCS;			/// Defuned traced functions (an assoc list). The defuned functions that are marked to be traced
								/// are kept in an assoc list in which the value is useless, but in this way the AssocList*
								/// functions in the Lisp interpreter can be used to manage the traced functions.
	
	MemoryCell Mem[MEMSIZE];
	addr UsedCells;
	int  GCNumberDone;			/// GC stats
	long GCTimeSpent;			/// GC stats
	long GCConsesFreed;			/// GC stats
	long GCConsesMarked;		/// GC stats	

	long Millis();				/// System milliseconds

private:
	addr MemIx;	
	bool IsNumber(char *v);

	void Mark(addr memaddr);	/// Garbage collection
	void Sweep();				/// Garbage collection
	
	void PrintList(addr sexpr);	/// Print companion
	void CheckEndOfMemory();	/// Memory can be exhausted due to unfrequent garbage collections
};

extern MemoryClass Memory;

