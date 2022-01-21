#pragma once

/**
 * Lisp interpreter. Implementation of the Read-Eval-Print loop (REPL).
 * 
 * Private functions starting with lower case are implementations of the corresponding Lisp functions, 
 * as defined in the Func struct.
 * 
 * Private functions starting with upper case support the Lips interpreter operations. The interpreter relies on
 * some basic functions:
 * 
 * 		Evaluation: Eval, EvalLambda and EvalSequence
 * 
 * 			Eval sits at the core of the interpreter. It takes as parameters:
 * 				The sexpr to be evaluated
 * 				The binding (assoc list) to be used for such purposes
 * 				The recursion level
 * 			
 * 			Eval tests the type of the sexpr and provides a Lisp evaluation for it. Internal Lisp functions
 * 			are detected by exploring the "Func" private struct and calling the associated function.
 * 
 * 			Notes:
 * 				The first call to Eval in REPL builds an initial environment containing the global variables
 * 				contained in DEFVAR.
 * 				The first call to Eval (detected by level==0) pushes (and pops at the end) the sexpr and the bindings
 * 				into the GCSTACK. This ensures that any subsequent call to Eval using portions of the sexpr or extensions
 * 				of the bindings protects all them from a garbage collection.
 * 
 * 			EvalLambda is used to execute both a defuned function or an inline lambda list.
 * 
 * 			EvalSequence is used throughout the code to evaluate a implicit sequence of sexprs in different Lisp functions.
 * 			Note on the (return) function:
 * 				The series of sexprs may include a (return) in some Lisp functions for which an implicit NIL block
 * 				is assumed. This is the case in do, dolist and dotimes.
 * 				The (return) mechanism is implemented as follows:
 * 					1. The evaluation of (return) checks for a previous existance of an item in the RETURNS stack. 
 * 					   If such is not the case, an error is printed. Otherwise a return value is evaluated and
 * 					   pushed into the RETURNS stack, and the special addr RETURNMARK is returned.
 * 					2. The EvalSequence loop detects the RETURNMARK and stops the normal execution of the sequence.
 * 					3. If the caller to EvalSequence is willing to act on a RETURNMARK, it must:
 * 							3a. Push a _NIL_ item on RETURNS so (return) can verify a caller is waiting for it.
 * 							3b. Check the return addr of EvalSequence, and act if it's RETURNMARK.
 * 							3c. Pop the result from (return) out of the RETURNS stack.
 * 				Check the implementation of "doer" for an example of the above.
 * 
 * 		Getting information from lists: Traverse, Length, Nth
 * 
 * 			Traverse must be used as shown in the code. Uses a memory hack with TRAVERSEMARK.
 * 
 * 		Modifying or creating lists: Push, Pop, Extend, Copy
 * 
 * 			Variable bindings are extended or reduced as execution progresses via a Push/Pop mechanism.
 * 			The GCSAFE stack is managed via Push/Pop.
 * 
 * 		Management of assoc lists: AssocListGet, AssocListSet
 * 
 * 			Association lists are used to represent bindings of symbols to values.
 * 			For example: ((one . 1) (two . 2) (three a b c)) represents these bindings:
 * 				one => 1
 * 				two => 2
 * 				three => (a b c)
 */ 

#include "memory.h"
#include "parser.h"

class LispClass {
friend class ParserClass; /// So that Parser can use Push and Pop
public:
	void REPL();
	bool TraceRead = false;

private:
	addr Read(bool showPrompt=true);
	addr Eval(addr sexpr, addr bindings, int level); /// bindings is a list of assoc lists
	void Print(addr sexpr, bool newline=true);

	/// Eval defuned funtions and lambdas
	bool EvalLambda(char *fname, addr lambdaArgs, addr lambdaBody, addr argValues, addr *result, addr bindings, int level); 
	
	/// Sequential evaluation of the sexpr in list. Returns last result
	addr EvalSequence(addr list, addr bindings, int level);
	
	/// Traverse the nodes of a list. 
	/// Recursion safe thanks to the helper argument (i.e., no static variables are used).
	/// Returns the nodes of the list (the list item is the CAR of the returned value)
	addr Traverse(addr list, addr *helper);	
	
	/// List information
	int  Length(addr list);
	addr Nth(addr list, int n);
	
	/// List manipulation
	/// Push/Pop is used for managing the lists of bindings and the internal stack 
	void Push(addr sexpr, addr list);		/// sexpr becomes the first element in list
	void Pop (addr list);					/// Discard first item in list
	void Extend(addr list, addr sexpr);		/// Set last item in list
	addr Copy(addr sexpr);					/// Create a new copy
	
	/// Assoc lists are lists of conses, each representing a (symbol value) pair. 
	/// The bindings of symbols to values is represented by assoc lists.
	bool AssocListGet(addr assoclist, char *symbol, addr *value);	/// Sets *value to the value of the symbol in assoclist. True if exists
	void AssocListSet(addr assoclist, char *symbol, addr value);	/// Updates an existing pair or creates a new one
	bool AssocListDel(addr assoclist, char *symbol);				/// Deletes symbol from the assoc list. Returns true if found and deleted
	
	/// Utility funcs
	void Blanks(int level, const char *msg);
	
	/// Lisp function implementations
	addr append		(addr sexpr, addr bindings, int level);
	addr apply		(addr sexpr, addr bindings, int level);
	addr atom		(addr sexpr, addr bindings, int level);
	addr bools		(addr sexpr, addr bindings, int level);
	addr bound		(addr sexpr, addr bindings, int level);
	addr carcdr		(addr sexpr, addr bindings, int level);
	addr cond		(addr sexpr, addr bindings, int level);
	addr cons		(addr sexpr, addr bindings, int level);
	addr defun		(addr sexpr, addr bindings, int level);
	addr defvarpar	(addr sexpr, addr bindings, int level);
	addr do_		(addr sexpr, addr bindings, int level);
	addr doer		(addr sexpr, addr bindings, int level);
	addr eq_		(addr sexpr, addr bindings, int level);
	addr eval		(addr sexpr, addr bindings, int level);
	addr ffunc		(addr sexpr, addr bindings, int level);
	addr funcall	(addr sexpr, addr bindings, int level);
	addr if_		(addr sexpr, addr bindings, int level);
	addr length		(addr sexpr, addr bindings, int level);
	addr let		(addr sexpr, addr bindings, int level);
	addr list		(addr sexpr, addr bindings, int level);
	addr load		(addr sexpr, addr bindings, int level);
	addr loop		(addr sexpr, addr bindings, int level);
	addr mapcar		(addr sexpr, addr bindings, int level);
	addr mod		(addr sexpr, addr bindings, int level);
	addr nth		(addr sexpr, addr bindings, int level);
	addr null		(addr sexpr, addr bindings, int level);
	addr pop		(addr sexpr, addr bindings, int level);
	addr print		(addr sexpr, addr bindings, int level);
	addr progn		(addr sexpr, addr bindings, int level);
	addr push		(addr sexpr, addr bindings, int level);
	addr quote		(addr sexpr, addr bindings, int level);
	addr read		(addr sexpr, addr bindings, int level);
	addr return_	(addr sexpr, addr bindings, int level);
	addr room		(addr sexpr, addr bindings, int level);
	addr setf		(addr sexpr, addr bindings, int level);
	addr setq		(addr sexpr, addr bindings, int level);
	addr terpri		(addr sexpr, addr bindings, int level);
	addr time		(addr sexpr, addr bindings, int level);
	addr trace		(addr sexpr, addr bindings, int level);
	addr type_of	(addr sexpr, addr bindings, int level);
	addr zoprs		(addr sexpr, addr bindings, int level);
	addr zcmps		(addr sexpr, addr bindings, int level);

	#define NFUNCS 61
	struct {
		const char *fname;		/// Lisp function
		const char *nargs;		/// Number of arguments condition
		addr (LispClass::*f)(addr sexpr, addr bindings, int level);
		bool traced = false;	/// Tracing flag
	} Func[NFUNCS] = {
		{"append", 			"*",  &LispClass::append	},	/// append {list}* => list
		{"apply", 			"=2", &LispClass::apply		},	/// apply function argument-list => result
		{"atom", 			"=1", &LispClass::atom		},	/// atom object => boolean
		{"boundp", 			"=1", &LispClass::bound		},	/// boundp symbol => boolean
		{"car", 			"=1", &LispClass::carcdr	},	/// car object => object
		{"cdr", 			"=1", &LispClass::carcdr	},	/// cdr object => object
		{"cond",			"*",  &LispClass::cond		},	/// cond {(test-form {form}*)}* => result
		{"cons",			"=2", &LispClass::cons		},	/// cons object1 object2 => cons
		{"defun",			">1", &LispClass::defun		},	/// defun function-name lambda-list {form}* => function-name
		{"defvar",			">0", &LispClass::defvarpar	},	/// defvar name [initial-value] => name
		{"defparameter",	">0", &LispClass::defvarpar	},	/// defparameter name [initial-value] => name
		{"do",				">1", &LispClass::do_		},	/// do ({(var init-form step-form)}*) (end-test-form [result-form]) {form}* => result-form
		{"dolist",			">0", &LispClass::doer		},  /// dolist (var list-form [result-form]) {form}* => result-form
		{"dotimes",			">0", &LispClass::doer		},  /// dotimes (var count-form [result-form]) {form}* => result-form
		{"do-symbols",		">0", &LispClass::doer		},  /// do-symbols (var [result-form]) {form}* => result
		{"dumpm",			"=0", &LispClass::ffunc		},	/// dump memory (non standard CL function)
		{"eq",				"=2", &LispClass::eq_		},	/// eq x y => boolean 	 (true if both objects are at the same address)		
		{"eql",				"=2", &LispClass::eq_		},	/// eql x y => boolean	 (true if eq or numbers/symbols with same representation)
		{"equal",			"=2", &LispClass::eq_		},	/// equal x y => boolean (true if eql or lists with same representation)
		{"eval",			"=1", &LispClass::eval		},	/// eval form => result
		{"fboundp", 		"=1", &LispClass::bound		},	/// fboundp symbol => boolean
		{"funcall",			">0", &LispClass::funcall	},	/// funcall function {args}* => result
		{"gc",				"=0", &LispClass::ffunc		},	/// trigger gc
		{"if",				">1", &LispClass::if_		},	/// if test-form then-form [else-form] => result
		{"length",			"=1", &LispClass::length	},	/// length list => n
		{"let",				">0", &LispClass::let		},	/// let ({var | (var [init-form])}*) {form}* => last evaled form
		{"let*",			">0", &LispClass::let		},	/// let* ({var | (var [init-form])}*) {form}* => last evaled form
		{"list",			">0", &LispClass::list		},	/// list {objects}* => list
		{"load",			"=1", &LispClass::load		},	/// load filespec => boolean
		{"loop",			"*",  &LispClass::loop		},	/// loop {form}* => result of (return)
		{"mapcar",			">1", &LispClass::mapcar	},	/// mapcar function {list}* => list
		{"mod",				"=2", &LispClass::mod		},	/// mod x y => x % y
		{"not",				"=1", &LispClass::null		},	/// not x => boolean
		{"nth",				"=2", &LispClass::nth		},	/// nth n list => object
		{"null",			"=1", &LispClass::null		},	/// null object => boolean
		{"pop", 			"=1", &LispClass::pop		},	/// pop list => result
		{"print", 			"=1", &LispClass::print		},	/// print object => object
		{"prin1", 			"=1", &LispClass::print		},	/// prin1 object => object
		{"progn", 			"*",  &LispClass::progn		},	/// progn {form}* => result
		{"push", 			"=2", &LispClass::push		},	/// push item list => new-list
		{"quote", 			"=1", &LispClass::quote		},	/// quote object => object
		{"read", 			"=0", &LispClass::read		},	/// read
		{"return", 			"<2", &LispClass::return_	},	/// return [result]
		{"room", 			"=0", &LispClass::room		},	/// room
		{"'", 				"=1", &LispClass::quote		},	/// ' object = object
		{"setf", 			"=2", &LispClass::setf		},	/// setf place newvalue => result. Check source for supported places
		{"setq", 			"=2", &LispClass::setq		},	/// setq var form => form
		{"terpri", 			"=0", &LispClass::terpri	},	/// terpri => NIL
		{"time", 			"=1", &LispClass::time		},	/// time sexpr => result
		{"trace", 			"*",  &LispClass::trace		},	/// trace [function-name*] => t or list of traced functions if no arg
		{"type-of", 		"=1", &LispClass::type_of	},	/// type-of x => typespec
		{"untrace", 		"*",  &LispClass::trace		},	/// untrace [function-name*] => t or list of traced functions if no arg
		{"+", 				">0", &LispClass::zoprs		},	/// + number* => number
		{"-", 				">0", &LispClass::zoprs		},	/// - number* => number
		{"*", 				">0", &LispClass::zoprs		},	/// * number* => number
		{"/", 				">0", &LispClass::zoprs		},	/// / number* => number
		{"=", 				"=2", &LispClass::zcmps		},	/// = n1 n2 => boolean
		{">", 				"=2", &LispClass::zcmps		},	/// > n1 n2 => boolean
		{"<", 				"=2", &LispClass::zcmps		},	/// < n1 n2 => boolean
		{"and",				"*",  &LispClass::bools		},	/// and {form}* => boolean
		{"or",				"*",  &LispClass::bools		},	/// or {form}* => boolean
		};
};

extern LispClass Lisp;
