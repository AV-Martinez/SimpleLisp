#include "memory.h"
#include "lisp.h"

/**
 * A simple, well documented Lisp interpreter written in C++.
 * 
 * Follows as much as possible the Common Lisp standard as specified in:
 * 
 * 		Common Lisp the Language, 2nd Edition
 * 		Guy L. Steele
 * 		https://www.cs.cmu.edu/Groups/AI/html/cltl/cltl2.html
 * 
 * Code is structured along the following single-instance classes:
 * 
 * 		MemoryClass - The memory model
 * 		ParserClass - The parser
 * 		LispClass	- The interpreter
 * 
 * The supported types are symbol (character string with no blanks), number
 * (long integer) and cons (list).
 * The implemented built-in functions are documented in struct Func of LispClass.
 * 
 * To-do list:
 * 
 * 		Support string data type (which would allow using Lisp "format" and "read-from-string")
 * 
 * Not-to-do list:
 * 
 * 		This is experimental and educational code, no major Lisp applications are expected to be built 
 * 		using this interpreter. Code cleaness is prefered to code efficiency. Features that would 
 *  	lead to becoming more compliant to the CL standard or facilitate the creation of large 
 * 		Lisp applications at the expense of making the code more complex are disregarded.
*/

int main(int argc, char **argv) {
	Memory.Init();
	Lisp.REPL();
}
