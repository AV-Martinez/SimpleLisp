#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lisp.h"
#include "memory.h"

LispClass Lisp;

void LispClass::REPL() {
	addr bindings = _NIL_;
	for (;;) {
		CAR(bindings) = _DEFVARS_;
		CDR(bindings) = _NIL_;
		addr result = Eval(Read(),bindings,0);
		Print(result);
	}
}

addr LispClass::Read(bool showPrompt) {
	static char lastcmd[MAXLINELENPARSER] = "";
	
	if (showPrompt) printf("%d%% REPL> ", USEDMEMPCT);
	char line[MAXLINELENPARSER]; int lineIx = 0;
	while (true) {
		char c = fgetc(stdin);
		if (c == '\n') break;
		line[lineIx++] = c;
		if (lineIx == MAXLINELENPARSER-1) break;
	}
	line[lineIx] = '\0';
	
	if (!strcasecmp(line, "?") && showPrompt) {
		printf("   Toplevel REPL. Percentage before prompt shows used memory.\n");
		printf("   Ctrl-C returns to OS.\n");
		printf("   +<enter> repeats last command.\n");
		printf("   sexpr<enter> evals s-expression.\n");
		return _T_;
	}
	if (!strcasecmp(line, "+") && showPrompt) {
		printf("%s\n",lastcmd);
		strcpy(line,lastcmd);
	}
	addr result;
	Parser.Init(line);
	result = Parser.Parse(0);
	if (Parser.Ok && result != ENDOFSEXPR) {
		if (TraceRead) { printf("[ read] @%d ", result); Print(result); }
	}
	else
		result = _NIL_;
	strcpy(lastcmd,line);
	return result;
}

addr LispClass::Eval(addr sexpr, addr bindings, int level) {
	/**
	 * Any memory cell created during the execution of Eval (included called functions)
	 * should be pushed into GCSAFE to prevent them from being gc'ed if USEDMEMPCT is above the given level.
	 * In addition, the sexpr and the bindings will be GCSAFEd in the first level Eval. This means that 
	 * any addr which holds parts of either the sexpr or the bindings will safe before any gc is performed.
	 * AssocListSet may create memory cells in the passed assoc list, so it may be gc'ed. Note that _DEFUNS_ is 
	 * always marked to be kept as part of the call to Memory.GC. Also, note that _DEFVARS_ is already part of the
	 * passed bindings variable, so no need to care for it when used in AssocListSet.
	 */

	bool traceResult = false;	/// To be updated if the result needs to be traced at the end of the function
	
	if (level == 0) {
		Push(sexpr,_GCSAFE_);
		Push(bindings,_GCSAFE_);
	}

	if (USEDMEMPCT > PCT_TRIGGER_GC) Memory.GC("At Eval");
	
	addr result;
	if 	(TYPE(sexpr) == 'N') /// Eval a number
		result = Memory.CreateCell(VALUE(sexpr));
	else if (TYPE(sexpr) == 'S') { /// Eval a symbol
		if 		(!strcasecmp(NAME(sexpr), "t"))     result = _T_;
		else if (!strcasecmp(NAME(sexpr), "nil"))   result = _NIL_;
		else { /// Look for symbol in the bindings (list of assoc list)
			addr value;
			bool found  = false;
			addr helper = TRAVERSEMARK;
			addr node   = Traverse(bindings,&helper);
			while (!ISNIL(node) && !found) {
				addr assoclist = CAR(node);
				if (AssocListGet(assoclist, NAME(sexpr), &value))
					found = true;
				else
					node = Traverse(bindings,&helper); 
			}
			if (found) result = value; 
			else {
				printf("[error] Undefined symbol: %s\n", NAME(sexpr)); result = _NIL_;
			}
		}
	}
	else if (TYPE(sexpr) == 'C') { /// Eval a list
		if (ISNIL(sexpr)) result = _NIL_;
		else {
			addr car = CAR(sexpr);
			if (TYPE(car) == 'S') { /// Potential function call starting with a symbol
				char *fname  = NAME(car);
				addr args    = CDR(sexpr);
				bool builtin = false; 
				for (int i = 0; i < NFUNCS && !builtin; i++) 
					if (!strcasecmp(fname,Func[i].fname)) {
						builtin = true;
						bool condok; /// Check number of arguments condition
						switch (Func[i].nargs[0]) {
							case '<': condok = (Length(args) <  atoi(Func[i].nargs+1)); break;
							case '=': condok = (Length(args) == atoi(Func[i].nargs+1)); break;
							case '>': condok = (Length(args) >  atoi(Func[i].nargs+1)); break;
							case '*': condok = true;
						}
						if (!condok) {
							printf("[error] %s: Got %d args, expected %s at ", NAME(CAR(sexpr)), Length(args), Func[i].nargs); Print(sexpr);
							result = _NIL_;
						}
						else {
							if (Func[i].traced) {
								traceResult = true;
								Blanks(level, ">>> "); Print(sexpr);
							}
							result = (*this.*(Func[i].f))(sexpr,bindings,level+1);
						}
					}
				if (!builtin) { /// Potential defuned function
					addr lambda;
					if (AssocListGet(_DEFUNS_, fname, &lambda)) {
						addr func_args = CAR(lambda);
						addr func_body = CDR(lambda);
						addr vals_args = args;
						traceResult = AssocListGet(_TRACEDFUNCS_, fname, NULL);
						addr r;	result = EvalLambda(fname, func_args, func_body, vals_args, &r, bindings, level) ? r : _NIL_;
					}
					else {
						printf("[error] Undefined function: %s\n", fname); result = _NIL_;
					}
				}
			}
			else if (TYPE(car) == 'C') { /// Potential function call starting with a lambda
				if (ISNIL(car)) {
					printf("[error] Undefined function NIL: "); Print(car); result = _NIL_;
				}
				else {
					if (strcasecmp(NAME(CAR(car)), "lambda")) {
						printf("[error] Expected lambda: "); Print(car); result = _NIL_;
					}
					else {
						if (TYPE(Nth(car,1)) != 'C') {
							printf("[error] Missing argument list: "); Print(car); result = _NIL_;
						}
						else {
							addr func_args = Nth(car, 1);
							addr func_body = CDR(CDR(car));
							addr vals_args = CDR(sexpr);
							addr r;
							result = EvalLambda((char *) "lambda", func_args, func_body, vals_args, &r, bindings, level) ? r : _NIL_;
						}
					}
				}
			}
			else if (TYPE(car) == 'N') { /// Bad function call starting with a number
				printf("[error] Expected symbol or lambda: %ld\n", VALUE(car)); result = _NIL_;
			}
		}
	}
	if (traceResult) { Blanks(level,"<<< "); Print(result); }
	if (level == 0)  { Pop(_GCSAFE_); Pop(_GCSAFE_); }
	return result;
}

void LispClass::Print(addr sexpr, bool newline) {
	if (ISNIL(sexpr)) 
		printf("NIL"); 
	else 
		Memory.Print(sexpr);
	if (newline) printf("\n");
}

bool LispClass::EvalLambda(char *fname, addr lambdaArgs, addr lambdaBody, addr argValues, addr *result, addr bindings, int level) {
	int items = Length(lambdaArgs);
	if (items != Length(argValues)) {
		printf("[error] %s: Arguments mismatch: ", fname); Print(argValues); 
		return false;
	}
	addr bndg = _NIL_; 	 /// New bindings
	Push(bndg,_GCSAFE_); /// bndg may be impacted by a gc on the next Eval call, so save it
	for (int i = 0; i < items; i++)
		AssocListSet(bndg, NAME(Nth(lambdaArgs,i)), Eval(Nth(argValues,i), bindings, level+1));
	Pop(_GCSAFE_);
	
	/// Test if the function is in the traced list. If so, print the evaled arguments which
	/// are contained in the generated bindings
	if (AssocListGet(_TRACEDFUNCS_, fname, NULL)) { 
		Blanks(level,">>> "); printf("%s ",fname); 
		addr helper = TRAVERSEMARK;
		addr node = Traverse(bndg,&helper);
		while (!ISNIL(node)) {
			addr item = CAR(node);
			Print(CDR(item),false); printf(" ");
			node = Traverse(bndg,&helper);
		}
		printf("\n");
	}

	Push(bndg,bindings); /// New bindings
	*result = EvalSequence(lambdaBody,bindings,level+1);
	Pop(bindings); /// Leave bindings as it was before extension
	return true;
}

addr LispClass::EvalSequence(addr list, addr bindings, int level) {
	addr helper = TRAVERSEMARK;
	addr node = Traverse(list,&helper);
	addr result = _NIL_; Push(result,_GCSAFE_);
	while (!ISNIL(node)) {
		result = Eval(CAR(node),bindings,level);
		if (result == RETURNMARK) {
			Pop(_GCSAFE_);
			return result;
		}
		node = Traverse(list,&helper);
	}
	Pop(_GCSAFE_);
	return result;
}

addr LispClass::Traverse(addr list, addr *helper) {
	if (*helper == TRAVERSEMARK) { 
		if (!ISNIL(list)) *helper = CDR(list);
		return list;
	}
	addr result = *helper;
	if (!ISNIL(*helper)) *helper = CDR(*helper);
	return result;
}

int LispClass::Length(addr list) {
	int count = 0;
	addr item = list;
	while (true) {
		if (ISNIL(item)) return count;
		item = CDR(item);
		count++;
	}
}

addr LispClass::Nth(addr list, int n) {
	int count = n;
	addr item = list;
	while (count) {
		item = CDR(item);
		if (ISNIL(item)) return _NIL_;
		count--;
	}
	return CAR(item);
}

bool LispClass::AssocListGet(addr assoclist, char *symbol, addr *value) {
	if (ISNIL(assoclist)) return false;
	addr helper = TRAVERSEMARK;
	addr node = Traverse(assoclist,&helper); 
	bool found = false;
	while (!ISNIL(node) && !found) {
		addr cons = CAR(node);
		if (!strcasecmp(NAME(CAR(cons)), symbol)) {
			if (value) *value = CDR(cons);
			found = true;
		}
		else node = Traverse(assoclist,&helper);
	}
	return found;
}

void LispClass::AssocListSet(addr assoclist, char *symbol, addr value) {
	if (ISNIL(assoclist)) {
		addr newcons = Memory.CreateCell(Memory.CreateCell(symbol),value);
		CAR(assoclist) = newcons;
		CDR(assoclist) = _NIL_;
	}
	else {
		addr helper = TRAVERSEMARK;
		addr node = Traverse(assoclist,&helper);
		while (!ISNIL(node)) {
			addr item = CAR(node);
			if (!strcasecmp(NAME(CAR(item)), symbol)) {
				CDR(item) = value;
				break;
			}
			if (ISNIL(CDR(node))) {
				addr newcons = Memory.CreateCell(Memory.CreateCell(symbol),value);
				addr newnode = Memory.CreateCell(newcons,_NIL_);
				CDR(node) = newnode;
				break;
			}
			node = Traverse(assoclist,&helper);
		}
	}
}

bool LispClass::AssocListDel(addr assoclist, char *symbol) {
	if (ISNIL(assoclist)) return false;
	addr helper = TRAVERSEMARK;
	addr node = Traverse(assoclist,&helper);
	while (!ISNIL(node)) {
		if (!strcasecmp(NAME(CAR(CAR(node))),symbol)) {
			CAR(node) = CAR(CDR(node));
			CDR(node) = CDR(CDR(node));
			return true;
		}
		node = Traverse(assoclist,&helper);
	}
	return false;
}

void LispClass::Push(addr sexpr, addr list) {
	addr newnode = _NIL_;
	if (ISNIL(list)) {
		CAR(list) = sexpr;
		CDR(list) = newnode;
	}
	else {
		CAR(newnode) = CAR(list);
		CDR(newnode) = CDR(list);
		CAR(list) = sexpr;
		CDR(list) = newnode;
	}
}

void LispClass::Pop(addr list) {
	if (ISNIL(list)) return;
	CAR(list) = CAR(CDR(list));
	CDR(list) = CDR(CDR(list));
}

void LispClass::Extend(addr list, addr sexpr) {
	addr newnode;
	if (ISNIL(list)) {
		newnode = _NIL_;
		CAR(list) = sexpr;
		CDR(list) = newnode;
	}
	else {
		addr helper = TRAVERSEMARK;
		addr nix = Traverse(list,&helper);
		while (!ISNIL(nix)) {
			if (ISNIL(CDR(nix))) break;
			nix = Traverse(list,&helper);
		}
		newnode = Memory.CreateCell(sexpr,CDR(nix));
		CDR(nix) = newnode;
	}
}

addr LispClass::Copy(addr sexpr) {
	switch (TYPE(sexpr)) {
		case 'N': return Memory.CreateCell(VALUE(sexpr));
		case 'S': return Memory.CreateCell(NAME(sexpr));
		case 'C':
			if (ISNIL(sexpr)) return _NIL_;
			return Memory.CreateCell(Copy(CAR(sexpr)),Copy(CDR(sexpr)));
	}
	return _NIL_; 
}

void LispClass::Blanks(int level, const char *msg) {
	printf("[trace] "); for (int i = 0; i < level; i++) printf(" "); printf("%s", msg);
}

/// ********************************************************************
/// Lisp function implememtations
/// ********************************************************************

addr LispClass::append(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	if (!ISNIL(args)) {
		addr result = ENDOFSEXPR;
		addr helper = TRAVERSEMARK;
		addr node = Traverse(args,&helper);
		while (!ISNIL(node)) {
			addr itemv = Eval(CAR(node),bindings,level);
			if (TYPE(itemv) != 'C') {
				printf("[error] append: Bad list: "); Print(itemv); break;
			}
			addr cursor;
			if (!ISNIL(itemv)) {
				if (result == ENDOFSEXPR) {
					result = Copy(itemv); Push(result,_GCSAFE_);
					cursor = result;
				}
				else 
					CDR(cursor) = Copy(itemv);
				while (!ISNIL(CDR(cursor))) cursor = CDR(cursor);
			}
			node = Traverse(args,&helper);
		}
		if (result == ENDOFSEXPR) return _NIL_;
		Pop(_GCSAFE_);
		return result;
	}
	return _NIL_;
}

addr LispClass::apply(addr sexpr, addr bindings, int level) {
	addr args  = CDR(sexpr);
	addr fname = Eval(Nth(args,0),bindings,level);
	addr fargs = Eval(Nth(args,1),bindings,level);
	if (TYPE(fargs) != 'C') {
		printf("[error] apply: Bad arguments list: "); Print(fargs); return _NIL_;
	}
	addr callsexpr = Memory.CreateCell(fname,fargs);
	return Eval(callsexpr,bindings,level);
}

addr LispClass::atom(addr sexpr, addr bindings, int level) {
	addr v = Eval(Nth(sexpr,1),bindings,level);
	if (ISNIL(v)) return _T_;
	if (TYPE(v) == 'S' || TYPE(v) == 'N') return _T_;
	return _NIL_;
}

addr LispClass::bools(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr));
	addr args   = CDR(sexpr);
	bool firstItem = true;
	addr helper    = TRAVERSEMARK;
	addr node      = Traverse(args, &helper);
	while (!ISNIL(node)) {
		addr value = Eval(CAR(node),bindings,level);
		if (!strcasecmp(fname, "and")) {
			if (ISNIL(value)) return _NIL_;
		}
		else if (!strcasecmp(fname, "or")) {
			if (!ISNIL(value)) return _T_;
		}
		node = Traverse(args, &helper);
	} 
	if (!strcasecmp(fname, "and")) 
		return _T_;
	else if (!strcasecmp(fname, "or")) 
		return _NIL_;
	return _NIL_;
}

addr LispClass::bound(addr sexpr, addr bindings, int level) {
	char *fname = NAME(Nth(sexpr,0));
	addr symbol = Eval(Nth(sexpr,1),bindings,level);
	if (!strcasecmp(fname,"boundp")) {
		if (TYPE(symbol) != 'S') {
			printf("[error] %s: Bad symbol: ", fname); Print(symbol); return _NIL_;
		}
		if (!strcasecmp(NAME(symbol),"t") || !strcasecmp(NAME(symbol), "nil")) return _T_;
		return AssocListGet(_DEFVARS_, NAME(symbol), NULL) ? _T_ : _NIL_;
	}
	else  /// if (!strcasecmp(fname,"fboundp"))
		return AssocListGet(_DEFUNS_, NAME(symbol), NULL) ? _T_ : _NIL_;
}

addr LispClass::carcdr(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	char *fname = NAME(CAR(sexpr));	
	addr list = Eval(Nth(args,0),bindings,level);
	if (TYPE(list) != 'C') {
		printf("[error] %s: Bad list: ", fname); Print(list); return _NIL_;
	}
	if (ISNIL(list)) return _NIL_;
	if (!strcasecmp(fname, "car")) return CAR(list); else return CDR(list);
}

addr LispClass::cond(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	addr helper = TRAVERSEMARK;
	addr node = Traverse(args,&helper);
	while (!ISNIL(node)) {
		addr clause = CAR(node);
		if (ISNIL(clause)) {
			printf("[error] cond: clause should be non NIL: "); Print(clause); return _NIL_;
		}
		addr test = CAR(clause);
		if (!ISNIL(Eval(test,bindings,level))) return EvalSequence(CDR(clause),bindings,level);
		node = Traverse(args,&helper);
	}
	return _NIL_;
}

addr LispClass::cons(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	return Memory.CreateCell(Eval(Nth(args,0),bindings,level),Eval(Nth(args,1),bindings,level));
}

addr LispClass::defun(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr); addr fname = Nth(args,0);
	if (TYPE(fname) != 'S') {
		printf("[error] defun: Bad function name: "); Print(fname);	return _NIL_;
	}
	addr alist = Nth(args,1);
	if (TYPE(alist) != 'C') {
		printf("[error] defun: Bad argument list: "); Print(alist);	return _NIL_;
	}
	addr helper = TRAVERSEMARK;
	addr node = Traverse(alist,&helper);
	while (!ISNIL(node)) {
		addr var = CAR(node);
		if (TYPE(var) != 'S') {
			printf("[error] defun: Arguments must be symbols: "); Print(alist);	return _NIL_;
		}
		node = Traverse(alist,&helper);
	}
	AssocListSet(_DEFUNS_, NAME(fname), CDR(CDR(sexpr))); 
	return fname;
}

/**
 * http://clhs.lisp.se/Body/m_defpar.htm
 * 
 * defparameter and defvar establish name as a dynamic variable.
 * 		defparameter unconditionally assigns the initial-value to the dynamic variable named name. 
 * 		defvar, by contrast, assigns initial-value (if supplied) to the dynamic variable named name only 
 * 		if name is not already bound.
 */
addr LispClass::defvarpar(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr); addr name = Nth(args,0);
	if (TYPE(name) != 'S') {
		printf("[error] %s: Bad variable name: ", NAME(Nth(sexpr,0))); Print(name);	return _NIL_;
	}
	addr value = Eval(Nth(args,1),bindings,level);
	if (!strcasecmp(NAME(Nth(sexpr,0)), "defvar")) {
		if (!AssocListGet(_DEFVARS_, NAME(name), NULL)) 
			AssocListSet(_DEFVARS_, NAME(name), value);
	}
	else  /// defparameter
		AssocListSet(_DEFVARS_, NAME(name), value);
	return name;
}

addr LispClass::do_(addr sexpr, addr bindings, int level) {
	addr result;
	addr args = CDR(sexpr);
	addr vlist = CAR(args);
	addr test  = CAR(CDR(args));
	addr body  = CDR(CDR(args));
	if (TYPE(vlist) != 'C') {
		printf("[error] do: Bad variable list: "); Print(sexpr); result = _NIL_;
	}
	else {
		addr varvals = _NIL_; 	/// The do variables are held in two assoc lists, one holding the values
		addr varupds = _NIL_; 	/// and the other one holding the update sexpr.
		Push(varvals,_GCSAFE_); /// Both need to be GCSAVEd to protect them from potential gc
		Push(varupds,_GCSAFE_); /// in the upcoming Evals.
		addr helper = TRAVERSEMARK;
		addr node = Traverse(vlist,&helper);
		bool error = false;
		while (!ISNIL(node)) {
			addr vspec = CAR(node); /// Each vspec is (<variable> <initial value> <update sexpr>)
			if (TYPE(vspec) != 'C') 	 { error = true; break; }
			else if (Length(vspec) != 3) { error = true; break; }
			else {
				AssocListSet(varvals,NAME(Nth(vspec,0)),Eval(Nth(vspec,1),bindings,level));
				AssocListSet(varupds,NAME(Nth(vspec,0)),Nth(vspec,2));
			}
			node = Traverse(vlist,&helper);
		}
		if (error) {
			printf("[error] do: Bad variable spec: "); Print(vlist); result = _NIL_;
		}
		else {
			if (TYPE(test) != 'C') {
				printf("[error] do: Bad test spec: "); Print(test); result = _NIL_;
			}
			else {
				Push (varvals,bindings);
				Push(_NIL_,_RETURNS_); /// Get ready for a potential (return) from body
				bool returnFound = false;
				while (ISNIL(Eval(Nth(test,0),bindings,level))) {
					addr r = EvalSequence(body,bindings,level);
					if (r == RETURNMARK) { returnFound = true; break; }
					/// update varvals
					addr helper, node;
					helper = TRAVERSEMARK;
					node = Traverse(varupds,&helper);
					while (!ISNIL(node)) {
						addr varcons = CAR(node);
						addr newvalue = Eval(CDR(varcons),bindings,level);
						AssocListSet(varvals,NAME(CAR(varcons)),newvalue);
						node = Traverse(varupds,&helper);
					}
				}
				if (returnFound) 
					result = CAR(_RETURNS_);
				else 
					result = Eval(Nth(test,1),bindings,level);
				Pop(_RETURNS_);
				Pop(bindings);
			}
		}
		Pop(_GCSAFE_); Pop(_GCSAFE_);
	}
	return result;
}

addr LispClass::doer(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr));
	addr args = CDR(sexpr);
	addr body = CDR(CDR(sexpr));
	addr varspec = Nth(args,0); /// The variable specification: A list starting with the do variable name
	if (TYPE(varspec) != 'C') {	/// Must always be a list
		printf("[error] %s: Expected variable list: ", fname); Print(varspec); return _NIL_;
	}
	
	int n = Length(varspec);		
	if (!strcasecmp(fname, "dolist") || !strcasecmp(fname, "dotimes")) { /// (var iteritem|maxvalue [return-value])
		if (n != 2 && n != 3 /** result-form specified */) {
			printf("[error] %s: Bad variable spec: ", fname); Print(varspec); return _NIL_;
		}
	}
	else if (!strcasecmp(fname,"do-symbols")) {
		if (n != 1 && n != 2 /** result-form specified */) {
			printf("[error] %s: Bad variable spec: ", fname); Print(varspec); return _NIL_;
		}
	}
	
	addr varname = Nth(varspec,0);
	if (TYPE(varname) != 'S') {
		printf("[error] %s: Bad variable name: ", fname); Print(varspec); return _NIL_;
	}
	
	addr iteritem; 
	if (!strcasecmp(fname, "dolist") || !strcasecmp(fname, "dotimes")) {
		iteritem = Eval(Nth(varspec,1),bindings,level);
		if (!strcasecmp(fname, "dolist")) {
			if (TYPE(iteritem) != 'C') {
				printf("[error] dolist: Bad iteration list: "); Print(iteritem); return _NIL_;
			}
		}
		else if (!strcasecmp(fname, "dotimes")) { 
			if (TYPE(iteritem) != 'N') {
				printf("[error] dotimes: Bad max iteration: "); Print(iteritem); return _NIL_;
			}
		}
	}
	else if (!strcasecmp(fname, "do-symbols")) {
		/// The iteration item _DEFVARS_ and _DEFUNS_, will be implemented as if 
		/// doing a dolist over these lists, see below.
		/// Alternatively, iteritem could be set to evaling (mapcar 'car (append _DEFVARS_ _DEFUNS_))
		/// and using the existing dolist code.
	}
	
	addr resultf;
	if (!strcasecmp(fname, "dolist") || !strcasecmp(fname, "dotimes"))
		resultf = Nth(varspec,2);
	else if (!strcasecmp(fname, "do-symbols"))
		resultf = Nth(varspec,1);
		
	addr bndgs = _NIL_; 
	Push(_NIL_,_RETURNS_); /// Get ready for a potential (return) from body
	bool returnFound = false;
	if (!strcasecmp(fname, "dolist")) {
		addr helper = TRAVERSEMARK;
		addr node = Traverse(iteritem,&helper);
		while (!ISNIL(node) && !returnFound) {
			AssocListSet(bndgs, NAME(varname), CAR(node));
			Push(bndgs,bindings);
			addr r = EvalSequence(body,bindings,level);
			Pop(bindings);
			if (r == RETURNMARK) returnFound = true;
			else 				 node = Traverse(iteritem,&helper);
		}
		AssocListSet(bndgs, NAME(varname), _NIL_); /// Proper last value in case it needs to be evaled in resultf
	}
	else if (!strcasecmp(fname, "dotimes")) { 
		long i = 0;
		while (true) {
			AssocListSet(bndgs, NAME(varname), Memory.CreateCell(i));
			Push(bndgs,bindings);
			addr r = EvalSequence(body,bindings,level);
			Pop(bindings);
			if (r == RETURNMARK) { returnFound = true; break; }
			i++; if (i == VALUE(iteritem)) {
				AssocListSet(bndgs, NAME(varname), Memory.CreateCell(i)); /// Proper last value in case it needs to be evaled in resultf
				break;
			}
		}
	}
	else if (!strcasecmp(fname, "do-symbols")) { /// Iterate as if doing a dolist over _DEFVARS_ and _DEFUNS_
		addr helper = TRAVERSEMARK;
		addr node = Traverse(_DEFVARS_,&helper);
		while (!ISNIL(node) && !returnFound) {
			AssocListSet(bndgs, NAME(varname), CAR(CAR(node)));
			Push(bndgs,bindings);
			addr r = EvalSequence(body,bindings,level);
			Pop(bindings);
			if (r == RETURNMARK) returnFound = true;
			else 				 node = Traverse(iteritem,&helper);
		}
		if (!returnFound) {
			helper = TRAVERSEMARK;
			node = Traverse(_DEFUNS_,&helper);
			while (!ISNIL(node) && !returnFound) {
				AssocListSet(bndgs, NAME(varname), CAR(CAR(node)));
				Push(bndgs,bindings);
				addr r = EvalSequence(body,bindings,level);
				Pop(bindings);
				if (r == RETURNMARK) returnFound = true;
				else 				 node = Traverse(iteritem,&helper);
			}
		}
	}
	if (returnFound) {
		addr result = CAR(_RETURNS_);
		Pop(_RETURNS_);
		return result;
	}
	Pop(_RETURNS_);
	Push(bndgs,bindings); /// Eval the result form with the last binding value
	addr result = Eval(resultf,bindings,level);
	Pop(bindings);
	return result;
}

/**
 * http://www.cs.cmu.edu/Groups/AI/html/cltl/clm/node74.html
 * 
 * (eq x y) is true if and only if x and y are the same identical object.
 * 
 * The eql predicate is true if its arguments are eq, or if they are numbers of the same type with the same value, 
 * or if they are character objects that represent the same character.
 * 
 * The equal predicate is true if its arguments are structurally similar (isomorphic) objects. 
 * A rough rule of thumb is that two objects are equal if and only if their printed representations are the same.
*/
addr LispClass::eq_(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr));
	addr args = CDR(sexpr);
	addr o1 = Nth(args,0);
	addr o2 = Nth(args,1);
	if (o1 == o2) return _T_;
	addr o1ev, o2ev;
	if (bindings != DONTUSEBINDINGS) {
		o1ev = Eval(o1,bindings,level);
		o2ev = Eval(o2,bindings,level);
	}
	else {
		o1ev = o1;
		o2ev = o2;
	}
	if (TYPE(o1ev) != TYPE(o2ev)) return _NIL_;
	addr result;
	switch (TYPE(o1ev)) {
		case 'C':
			if (ISNIL(o1ev) && ISNIL(o2ev))
				result = _T_;
			else {
				if (!strcasecmp(fname,"eq") || !strcasecmp(fname,"eql"))
					result = _NIL_;
				else { /// !strcasecmp(fname,"equal")
					/// Check if comparing conses or lists
					bool o1IsCons = TYPE(CDR(o1ev)) != 'C';
					bool o2IsCons = TYPE(CDR(o2ev)) != 'C';
					if (o1IsCons != o2IsCons) 
						result = _NIL_;
					else {
						/**
						 * Lists and conses comparison for equal could be done by building a temporary sexpr calling Eval in two steps
						 * equal on the cars and cdrs of o1ev and o2ev. In order for this approach to work, the
						 * arguments should be quoted to avoid their evaluation:
						 * 
						 * 		temp_sexpr --> cons(*,cell1) --> cell1:cons(cell4,cell2) --> cell2:cons(*,cell3) --> cell3:cons(0,0)
						 * 							V				        V
						 * 							*:symbol("equal")       cell4:cons(*,cell5) --> cell5:cons(*,cell6) --> cell6:cons(0,0)
						 * 												               V                       V
						 * 												               *:symbol("'")           o1ev
						 * 					   
						 * 		CAR(cell2) points to a similar cell4->cell5->cell6 link for o2ev.
						 * 
						 * All in all, this would ask for the creation of 1+6+3 cons cells and 1+1+1 symbol cells (total of 13 cells) and
						 * a cumbersome and lenghy code.
						 * 
						 * Alternatively the comparison can be done by calls to eq_ using a similarly built sexpr. As in the previous 
						 * approach, the arguments should not be evaled at the begining of eq_, and this is the reason for using 
						 * the DONTUSEBINDINGS hack, which avoids quoting them as in the previous approach. All in all, only 3 cons 
						 * cells and 1 symbol cell is needed for this approach.
						 * 
						 * The second approach is preferred to simplify the code at the expense of introducing the DONTUSEBINDINGS hack.
						 */
						addr temp_sexpr = _NIL_; 	
						addr item1 = _NIL_;			/// CAR will be o1ev
						addr item2 = _NIL_;			/// CAR will be o2ev
						addr item3 = _NIL_;			/// End of list marker
						CAR(temp_sexpr) = Memory.CreateCell((char*)"equal");
						CDR(temp_sexpr) = item1;
						CDR(item1) = item2;
						CDR(item2) = item3;
						if (o1IsCons && o2IsCons) { /// Comparing two conses
							CAR(item1) = CAR(o1ev);	CAR(item2) = CAR(o2ev); 
							addr compareCARs = eq_(temp_sexpr, DONTUSEBINDINGS, level);
							CAR(item1) = CDR(o1ev);	CAR(item2) = CDR(o2ev); 
							addr compareCDRs = eq_(temp_sexpr, DONTUSEBINDINGS, level);
							if (ISNIL(compareCARs) || ISNIL(compareCDRs)) result = _NIL_; else result = _T_;
						}
						else { /// Comparing two lists
							if (Length(o1ev) != Length(o2ev)) 
								result = _NIL_;
							else {
								bool allequal = true;
								for (int i = 0; i < Length(o1ev) && allequal; i++) {
									CAR(item1) = Nth(o1ev,i);
									CAR(item2) = Nth(o2ev,i);
									if (ISNIL(eq_(temp_sexpr,DONTUSEBINDINGS,level))) allequal = false;
								}
								result = allequal ? _T_ : _NIL_;
							}
						}
					}
				}
			}
			break;
		case 'N': result = (VALUE(o1ev) == VALUE(o2ev))         ? _T_ : _NIL_; break;
		case 'S': result = (!strcasecmp(NAME(o1ev),NAME(o2ev))) ? _T_ : _NIL_; break;
		default:  result = _NIL_;
	}
	return result;
}

addr LispClass::eval(addr sexpr, addr bindings, int level) {
	addr form = CAR(CDR(sexpr));
	return Eval(Eval(form,bindings,level+1),bindings,level);
}

addr LispClass::ffunc(addr sexpr, addr bindings, int level) {
	char *which = NAME(CAR(sexpr));
	if (!strcasecmp(which, "dumpm")) 
		Memory.Dump();
	else if (!strcasecmp(which, "gc")) 
		Memory.GC("At gc");
	return _T_; 
}

addr LispClass::funcall(addr sexpr, addr bindings, int level) {
	addr args  = CDR(sexpr);
	addr fname = Eval(CAR(args),bindings,level);
	addr callsexpr = Memory.CreateCell(fname,CDR(args));
	return Eval(callsexpr,bindings,level);
}

addr LispClass::if_(addr sexpr, addr bindings, int level) {
	addr args    = CDR(sexpr);
	addr cd      = Nth(args, 0);
	addr iftrue  = Nth(args, 1);
	addr iffalse = Nth(args, 2);
	if (!ISNIL(Eval(cd,bindings,level))) return Eval(iftrue,bindings,level);
	return Eval(iffalse,bindings,level);
}

addr LispClass::length(addr sexpr, addr bindings, int level) {
	addr list = Eval(Nth(sexpr,1), bindings, level);
	if (TYPE(list) != 'C') {
		printf("[error] length: Bad list "); Print(list); return _NIL_;
	}
	return Memory.CreateCell((long)Length(list));
}

addr LispClass::let(addr sexpr, addr bindings, int level) {
	addr result;
	char *fname = NAME(CAR(sexpr));
	addr args   = CDR(sexpr); 
	addr letvars = Nth(args,0);
	if (TYPE(letvars) != 'C') {
		printf("[error] %s: Bad variable spec at ", fname); Print(letvars); result = _NIL_;
	}
	else {
		addr letbody  = CDR(args);
		addr newbinds = _NIL_; Push(newbinds,_GCSAFE_); /// newbinds may be gc'ed in the Eval in loop
		addr varvalue = _NIL_; Push(varvalue,_GCSAFE_);	/// id. varvalue
		addr helper   = TRAVERSEMARK;
		addr nodevars = Traverse(letvars,&helper);
		bool error = false;
		while (!ISNIL(nodevars)) {
			addr variable = CAR(nodevars);
			addr varsymbol;
			if (TYPE(variable) == 'S') {
				varsymbol = variable;
				varvalue  = _NIL_;
			}
			else if (TYPE(variable) == 'C') {
				varsymbol = CAR(variable);
				if (!TYPE(varsymbol) == 'S') {
					printf("[error] %s: Bad variable symbol at ", fname); Print(variable);
					error = true;
					break;
				}
				else {
					addr vexpr = CDR(variable);
					if (ISNIL(vexpr)) varvalue = _NIL_;
					else {
						if (!strcasecmp(fname,"let"))
							varvalue = Eval(CAR(vexpr),bindings,level);
						else { /// For let* the bindings are used as they are built
							Push(newbinds,bindings);
							varvalue = Eval(CAR(vexpr),bindings,level);
							Pop(bindings);
						}
					}
				}
			}
			else if (TYPE(variable) == 'N') {
				printf("[error] %s: Bad variable symbol at ",fname); Print(variable);
				error = true;
				break;
			}
			AssocListSet(newbinds, NAME(varsymbol), varvalue);
			nodevars = Traverse(letvars,&helper);
		}
		Pop(_GCSAFE_); Pop(_GCSAFE_);
		if (error) result = _NIL_;
		else {
			if (Length(letbody) > 0) {
				Push(newbinds,bindings); /// Next Evals are done with the extended bindings
				result = EvalSequence(letbody,bindings,level);
				Pop(bindings);
			}
			else result = _NIL_;
		}
	}
	return result;
}

addr LispClass::list(addr sexpr, addr bindings, int level) {
	addr args   = CDR(sexpr);
	addr result = _NIL_; Push(result,_GCSAFE_);
	addr helper = TRAVERSEMARK;
	addr nargs  = Traverse(args,&helper);
	while (!ISNIL(nargs)) {
		addr item = CAR(nargs);
		addr itemv = Eval(item,bindings,level);
		Extend(result,itemv);
		nargs = Traverse(args,&helper);
	}
	Pop(_GCSAFE_);
	return result;
}

addr LispClass::load(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	addr fname = Eval(Nth(args,0),bindings,level);
	if (TYPE(fname) != 'S') {
		printf("[error] load: Expected symbol at "); Print(fname); return _NIL_;
	}
	FILE *file = fopen(NAME(fname), "r");
	if (!file) {
		printf("[error] load: Bad file %s\n", NAME(fname)); return _NIL_;
	}
	Parser.Init(file);
	addr s = Parser.Parse(0);
	while (Parser.Ok) {
		Eval(s,bindings,0);
		s = Parser.Parse(0);
	}
	return _T_;
}

addr LispClass::loop(addr sexpr, addr bindings, int level) {
	Push(_NIL_,_RETURNS_); /// Get ready for a (return) from body
	while(true) {
		addr r = EvalSequence(CDR(sexpr),bindings,level);
		if (r == RETURNMARK) break;
	}
	addr result = CAR(_RETURNS_);
	Pop(_RETURNS_);
	return result;
}

addr LispClass::mapcar(addr sexpr, addr bindings, int level) {
	char *fname = NAME(Nth(sexpr,0));
	
	/// The sexpr representing the function must either:
	/// 	Eval to an existing funtion name (that's why they are always quoted)
	///		Be a lambda expression
	addr function;
	addr functionSexpr = Nth(sexpr,1);
	if (TYPE(functionSexpr) == 'C') {
		if (!strcasecmp(NAME(CAR(functionSexpr)), "lambda")) 
			function = functionSexpr;
		else 
			function = Eval(functionSexpr,bindings,level);
	}
	else
		function = Eval(functionSexpr,bindings,level);

	addr lists = CDR(CDR(sexpr)); /// The lists to apply the function to
	int nth = 0;
	addr result = _NIL_; Push(result,_GCSAFE_); /// Keep safe from upcoming Evals
	while (true) {
		addr builtlist = _NIL_; Push(builtlist, _GCSAFE_); /// Keep safe from upcoming Eval
		addr helper = TRAVERSEMARK;
		addr node = Traverse(lists,&helper);
		bool noMoreItems = false;
		while (!ISNIL(node)) {
			addr list = Eval(CAR(node),bindings,level);
			if (TYPE(list) != 'C') {
				printf("[error] mapcar: Bad list "); Print(list); 
				Pop(_GCSAFE_); Pop(_GCSAFE_);
				return _NIL_;
			}
			addr item = Nth(list,nth);
			if (ISNIL(item)) { noMoreItems = true; break; }
			Extend(builtlist,item);
			node = Traverse(lists,&helper);
		}
		Pop(_GCSAFE_); /// builtlist
		if (noMoreItems) break;
		/**
		 * "builtlist" contains at this point the list of items to be used as arguments to "function".
		 * The following code takes each item, gets it quoted and assembled into a sexpr
		 * to be evaled.
		 * 
		 * 	Ex. for builtlist (1 2) : builtlist => cons(a,b) --> b:cons(c,d) --> d:cons(0,0)
		 *                                              V               V
		 *                                              a:number(1)     c:number(2)
		 * 
		 * 	Transformation :          builtlist => cons(a,b) --> b:cons(c,d) --> d:cons(0,0)
		 *                                              V               
		 *                                              a:cons(e,f) --> f:cons(g,h) --> h:cons(0,0)
		 *                                                     V               V
		 *                                                     e:symbol(') --> g:number(1) (original a)
		 * 
		 *                                              c:cons(i,j) --> j:cons(k,l) --> l:cons(0,0)
		 *                                                     V               V
		 *                                                     i:symbol(') --> k:number(2) (original c)
		 * 
		 * So, for each item in the original list, four new conses are needed.
		 */
		helper = TRAVERSEMARK;
		node = Traverse(builtlist,&helper);
		while (!ISNIL(node)) {
			addr quoteList1 = _NIL_, quoteList2 = _NIL_, quoteList3 = _NIL_;
			CAR(quoteList1) = Memory.CreateCell((char *)"'");
			CDR(quoteList1) = quoteList2; 
			CAR(quoteList2) = CAR(node);
			CDR(quoteList2) = quoteList3;
			CAR(node) = quoteList1;
			node = Traverse(builtlist,&helper);
		}
		/// Prepend the function name:
		addr sexpr = _NIL_;
		CAR(sexpr) = function;
		CDR(sexpr) = builtlist; 
		
		/// Build the result
		Extend(result,Eval(sexpr,bindings,level));
		nth++;
	}
	Pop(_GCSAFE_); /// result;
	return result;
}

addr LispClass::mod(addr sexpr, addr bindings, int level) {
	addr x = Eval(Nth(sexpr,1),bindings,level);
	addr y = Eval(Nth(sexpr,2),bindings,level);
	if (TYPE(x) != 'N' || TYPE(y) != 'N') {
		printf("[error] mod: Arguments must be integers\n"); return _NIL_;
	}
	return Memory.CreateCell(VALUE(x) % VALUE(y));
}

addr LispClass::nth(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	addr n = Eval(Nth(args,0), bindings, level);
	if (TYPE(n) != 'N') {
		printf("[error] nth: Bad index "); Print(n); return _NIL_;
	}
	if (VALUE(n) < 0) {
		printf("[error] nth: Negative index "); Print(n); return _NIL_;
	}
	addr list = Eval(Nth(args,1), bindings, level);
	if (TYPE(list) != 'C') {
		printf("[error] nth: Bad list "); Print(list); return _NIL_;
	}
	if (VALUE(n) >= Length(list)) {
		printf("[error] nth: List only has %d items: ", Length(list)); Print(list); return _NIL_;
	}
	if (ISNIL(list)) return _NIL_;
	return Nth(list, VALUE(n));
}

addr LispClass::null(addr sexpr, addr bindings, int level) {
	addr v = Eval(Nth(sexpr,1),bindings,level);
	if (ISNIL(v)) return _T_;
	return _NIL_;
}

addr LispClass::pop(addr sexpr, addr bindings, int level) {
	addr list = Eval(Nth(sexpr,1),bindings,level);
	if (TYPE(list) != 'C') {
		printf("[error] pop: Bad list "); Print(list); return _NIL_;
	}
	addr result = CAR(list);
	Pop(list);
	return result;
}

addr LispClass::print(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr));
	addr args = CDR(sexpr);
	addr item = Eval(Nth(args,0),bindings,level); 
	if (!strcasecmp(fname, "print")) printf("\n"); 
	Print(item,false); 
	if (!strcasecmp(fname, "print")) printf(" "); 
	return item;
}

addr LispClass::progn(addr sexpr, addr bindings, int level) {
	return EvalSequence(CDR(sexpr),bindings,level);
}

addr LispClass::push(addr sexpr, addr bindings, int level) {
	addr item = Eval(Nth(sexpr,1),bindings,level);
	addr place = Eval(Nth(sexpr,2),bindings,level);
	if (TYPE(place) != 'C') {
		printf("[error] push: Place must be a list: "); Print(place); return _NIL_;
	}
	Push(item,place);
	return place;
}

addr LispClass::setf(addr sexpr, addr bindings, int level) {
	addr place = Nth(CDR(sexpr),0);
	addr value = Eval(Nth(CDR(sexpr),1),bindings,level);
	
	if (TYPE(place) == 'N') {
		printf("[error] setf: Place can't be a number: "); Print(place); return _NIL_;
	}
	
	/// "place" is a symbol
	if (TYPE(place) == 'S') return setq(sexpr,bindings,level);
	/// "place" is nth
	if (!strcasecmp(NAME(CAR(place)), "nth")) {
		/// Check proper syntax of the nth sexpr. This only prints an error
		/// message if needed, no action is done on the returned item. Note
		/// that NIL may be flagging an error or a correct result.
		nth(place,bindings,level);
		int n = VALUE(Eval(Nth(place,1),bindings,level));
		addr list = Eval(Nth(place,2),bindings,level);
		addr helper = TRAVERSEMARK;
		addr node = Traverse(list,&helper);
		while (!ISNIL(node) && n) {
			node = Traverse(list,&helper);
			n--;
		}
		CAR(node) = value;
		return value;
	}
	/// "place" is car or cdr
	if (!strcasecmp(NAME(CAR(place)), "cdr") || !strcasecmp(NAME(CAR(place)), "car")) {
		addr list = Eval(Nth(place,1),bindings,level);
		if (TYPE(list) != 'C') {
			printf("[error] setf: Bad list to %s place: ", NAME(CAR(place))); Print(list); return _NIL_;
		}
		if (!strcasecmp(NAME(CAR(place)), "cdr")) CDR(list) = value; else CAR(list) = value;
		return value;
	}
	printf("[error] setf: Unsupported place "); Print(place); return _NIL_;
}

addr LispClass::setq(addr sexpr, addr bindings, int level) {
	addr args = CDR(sexpr);
	addr symbol = Nth(CDR(sexpr),0);
	if (TYPE(symbol) != 'S') {
		printf("[error] setq: Expected symbol: "); Print(symbol); return _NIL_;
	}
	/// Look for symbol in bindings and update if found. Else, add symbol to _DEFVARS_
	addr value  = Eval(Nth(args,1),bindings,level);
	bool found  = false;
	addr helper = TRAVERSEMARK;
	addr node   = Traverse(bindings,&helper);
	while (!ISNIL(node) && !found) {
		addr assoclist = CAR(node);
		if (AssocListGet(assoclist, NAME(symbol), NULL)) {
			AssocListSet(assoclist, NAME(symbol), value);
			found = true;
		}
		else
			node = Traverse(bindings,&helper); 
	}
	if (!found) AssocListSet(_DEFVARS_, NAME(symbol), value);
	return value;
}

addr LispClass::terpri(addr sexpr, addr bindings, int level) {
	printf("\n");
	return _NIL_;
}

addr LispClass::time(addr sexpr, addr bindings, int level) {
	long m0  = Memory.Millis();
	addr nuc = Memory.UsedCells;
	int  gcd = Memory.GCNumberDone;
	addr result = Eval(Nth(sexpr,1),bindings,level);
	printf("Run time.........: %ld ms\n", Memory.Millis()-m0);
	if (Memory.GCNumberDone == gcd)
		printf("Cells created........: %d\n", Memory.UsedCells-nuc);
	else 
		printf("GCs..............: %d\n", Memory.GCNumberDone-gcd);
	return result;
}

/**
 * The trace mechanism behaves differently for built-in and for defuned functions:
 * 
 * 		For defuned functions, the evaled arguments are shown in the trace, as
 * 		per the code in EvalLambda.
 * 
 * 		For built-ins, the trace shows the raw function call, with no evaluation
 * 		of the arguments.
 * 
 * Showing the evaled arguments of built-ins would ask for specific management inside
 * each of the implementation functions (which is where the evaluation takes place), 
 * something considered too cumbersome for functions which will be hardly ever traced.
 */
addr LispClass::trace(addr sexpr, addr bindings, int level) {
	char *trfname = NAME(CAR(sexpr));
	
	addr fnames = CDR(sexpr);
	if (ISNIL(fnames)) { /// Return the names of the traced functions
		addr result = _NIL_;
		/// Check built-in functions:
		for (int i = 0; i < NFUNCS; i++) if (Func[i].traced) Push(Memory.CreateCell((char *)Func[i].fname),result);	
		/// Check defuned functions:
		addr helper = TRAVERSEMARK;	
		addr node   = Traverse(_TRACEDFUNCS_,&helper);
		while (!ISNIL(node)) {
			Push(CAR(CAR(node)),result);
			node = Traverse(_TRACEDFUNCS_,&helper);
		}
		return result;
	}
	
	addr helper = TRAVERSEMARK;
	addr node   = Traverse(fnames,&helper);
	while (!ISNIL(node)) {
		addr fname = CAR(node);
		if (TYPE(fname) != 'S') {
			printf("[error] %s: Bad function name ", trfname); Print(fname); return _T_;
		}
		bool found = false;
		for (int i = 0; i < NFUNCS && !found; i++) /// Check if name is a built-in functions
			if (!strcasecmp(NAME(fname), Func[i].fname)) {
				Func[i].traced = !strcasecmp(trfname,"trace") ? true : false;
				found = true;
				break;
			}
		if (!found) { /// Check if name is a defuned functions
			if (AssocListGet(_DEFUNS_, NAME(fname), NULL)) {
				if (!strcasecmp(trfname,"trace"))
					AssocListSet(_TRACEDFUNCS_, NAME(fname), _NIL_);
				else 
					AssocListDel(_TRACEDFUNCS_, NAME(fname));
			}
			else {
				printf("[error] %s: Function does not exist: ", trfname); Print(fname); return _T_;
			}
		}
		node = Traverse(fnames,&helper);
	}
	return _T_;
}

addr LispClass::type_of(addr sexpr, addr bindings, int level) {
	addr obj = Eval(Nth(sexpr,1),bindings,level);
	if (TYPE(obj) == 'C') {
		if (ISNIL(obj)) return Memory.CreateCell((char *)"null");
		return Memory.CreateCell((char *)"cons");
	}
	else if (TYPE(obj) == 'N') return Memory.CreateCell((char *)"integer");
	else if (TYPE(obj) == 'S') return Memory.CreateCell((char *)"symbol");
	else {
		printf("[error] type-of: Unknown object type "); Print(obj);
		return _NIL_;
	}
}

addr LispClass::quote(addr sexpr, addr bindings, int level) {
	return CAR(CDR(sexpr));
}

addr LispClass::read(addr sexpr, addr bindings, int level) {
	return Read(false);
}

addr LispClass::return_(addr sexpr, addr bindings, int level) {
	if (Length(_RETURNS_) == 0) {
		printf("[error] return: No active return point "); Print(sexpr); return _NIL_;
	}
	addr returnValue = Eval(CAR(CDR(sexpr)),bindings,level);
	/// Update the _RETURNS_ stack with a new item containing the result
	Pop(_RETURNS_);
	Push(returnValue,_RETURNS_);
	return RETURNMARK;
}

addr LispClass::room(addr sexpr, addr bindings, int level) {
	printf("Number of garbage collections...: %d\n",  	 Memory.GCNumberDone);
	if (Memory.GCNumberDone > 0) {
		printf("Conses marked by GC (last)......: %ld\n",	 Memory.GCConsesMarked);
		printf("Conses freed by GC (total)......: %ld\n", 	 Memory.GCConsesFreed);
		printf("Time spent in GC (total)........: %ld ms\n", Memory.GCTimeSpent);
	}
	printf("Number of conses................: %d\n",  	 MEMSIZE);
	printf("Bytes per cons..................: %ld\n", 	 sizeof(MemoryCell));
	printf("Conses currently in use.........: %d\n",     Memory.UsedCells);
	return _T_;
}

addr LispClass::zoprs(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr));
	addr args = CDR(sexpr);
	long fresult; bool firstNumber = true;
	bool error = false;
	addr helper = TRAVERSEMARK;
	addr node = Traverse(args, &helper);
	while (!ISNIL(node) && !error) {
		addr value = Eval(CAR(node),bindings,level);
		if (TYPE(value) != 'N') {
			printf("[error] %s: Bad number ", fname); Print(CAR(node));
			error = true;
		}
		else {
			if (firstNumber) { fresult = VALUE(value); firstNumber = false; }
			else switch (*fname) {
				case '+': fresult += VALUE(value); break;
				case '-': fresult -= VALUE(value); break;
				case '*': fresult *= VALUE(value); break;
				case '/': fresult /= VALUE(value); break;
			}
			node = Traverse(args, &helper);
		}
	} 
	if (error) return _NIL_;
	return Memory.CreateCell(fresult);
}

addr LispClass::zcmps(addr sexpr, addr bindings, int level) {
	char *fname = NAME(CAR(sexpr)); addr args = CDR(sexpr);
	addr n1 = Eval(Nth(args,0),bindings,level);
	addr n2 = Eval(Nth(args,1),bindings,level);
	if (TYPE(n1) != 'N' || TYPE(n2) != 'N') {
		printf("[error] %s: Bad numbers ", fname); Print(sexpr); return _NIL_;
	}
	bool cd = false;
	switch (*fname) {
		case '=': if (VALUE(n1) == VALUE(n2)) cd = true; break;
		case '>': if (VALUE(n1) >  VALUE(n2)) cd = true; break;
		case '<': if (VALUE(n1) <  VALUE(n2)) cd = true; break;
	}
	if (cd) return _T_;
	return _NIL_; 
}


