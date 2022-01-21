#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "lisp.h"

#define MAXTOKENSIZE 100 /** maximum token size */

ParserClass Parser;

void ParserClass::Init(char *str) {
	NextCharRepeat = false;
	FileInput = NULL;
	TokenString = str;
}

void ParserClass::Init(FILE *f) {
	NextCharRepeat = false;
	FileInput = f;
	FileInputReadLine();
}

addr ParserClass::Parse(int level) {
	if (level == 0) {
		Ok = true;
		CreateCellCount = 0;
	}
	if (USEDMEMPCT > PCT_TRIGGER_GC) Memory.GC("At parser");

	char *token = NextToken();
	if (Trace) { Blanks(level); printf("> \"%s\"\n", token); }
	addr result; 
	if 		(token == NULL)  result =  ENDOFSEXPR;
	else if (*token == '(')  result =  ParseListItem(level+1); 
	else if (*token == ')')  { if (level == 0) { printf("[parse] Unexpected )\n"); Ok = false; } result =  ENDOFLIST; }
	else if (*token == '\'') result =  ParseQuote(level+1);
	else 		             result =  CreateCellForParser(token); // result =  Memory.CreateCell(token);
	if (Trace) { Blanks(level); printf("< @%d\n", result); }
	if (level == 0) 
		for (int i = 0; i < CreateCellCount; i++) Lisp.Pop(_GCSAFE_);
	return result;
}

addr ParserClass::ParseQuote(int level) {
	addr result = _NIL_;
	CAR(result) = CreateCellForParser((char *)"'"); // CAR(result) = Memory.CreateCell((char *)"'");
	addr quoted = _NIL_;
	addr q = Parse(level);
	if (q == ENDOFSEXPR) {
		printf("[parse] Bad quote\n");
		Ok = false;
		return _NIL_;
	}
	CAR(quoted) = q;
	CDR(quoted) = _NIL_;
	CDR(result) = quoted;
	return result;
}

addr ParserClass::ParseListItem(int level) {
	addr item = Parse(level);
	if (item == ENDOFSEXPR) {
		printf("[parse] Bad list\n");
		Ok = false;
		return _NIL_;
	}
	if (item == ENDOFLIST) return _NIL_;
	addr node = CreateCellForParser(item,ParseListItem(level)); // addr node = Memory.CreateCell(item,ParseListItem(level));
	return node;
}

char *ParserClass::NextToken() {
	static char token[MAXTOKENSIZE]; /// collected token (return value)
	int 		tokenIx = 0; 		 /// index on collected token
	
	char c = NextChar();
	while (true) {
		if (c == '\0') {
			if (tokenIx == 0)
				return NULL;
			else {
				token[tokenIx] = '\0';
				if (strlen(token) > 0) return token; else return NULL;
			}
		 }
		else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (tokenIx == 0)
				c = NextChar();
			else {
				token[tokenIx] = '\0'; 
				return token;
			}
		}
		else if (c == '(' || c == ')' || c == '\'') {
			if (tokenIx == 0) {
				token[0] = c; token[1] = '\0';
				return token;
			}
			else {
				token[tokenIx] = '\0';
				NextCharRepeat = true;
				return token;
			}
		}
		else {
			token[tokenIx++] = c;
			if (tokenIx == MAXTOKENSIZE-1) return NULL;
			c = NextChar();
		}
	}
}

char ParserClass::NextChar() {
	static char lastChar;
	if (NextCharRepeat) {
		NextCharRepeat = false;
		return lastChar;
	}
	char c = *(TokenString++);
	if (c == '\0' && FileInput) {
		FileInputReadLine();
		c = *(TokenString++);
	}
	lastChar = c;
	return c;
}

addr ParserClass::CreateCellForParser(char *item) {
	addr mc = Memory.CreateCell(item);
	Lisp.Push(mc, _GCSAFE_);	
	CreateCellCount++;
	return mc;
}

addr ParserClass::CreateCellForParser(addr car, addr cdr) {
	addr mc = Memory.CreateCell(car,cdr);
	Lisp.Push(mc, _GCSAFE_);
	CreateCellCount++;
	return mc;
}

void ParserClass::FileInputReadLine() {
	static char line[MAXLINELENPARSER];
	if (!fgets(line, sizeof(line), FileInput)) { 
		Ok = false; line[0] = '\0'; 
	}
	else {
		/// Skip comments
		if (line[0] == ';') { FileInputReadLine(); return; }
		char *c = line; 
		while (*c != '\0') {
			if (*c == ';') { *c = '\0'; break; }
			c++;
		}
		
		/// Provide some load verbosity
		strtok(line, "\r\n");
		if (strstr(line, "defun") || strstr(line,"defvar") || strstr(line,"defparameter")) {
			printf("[ load] %s", line);
			int popen = 0, pclose = 0;
			char *c = line; while (*c) { if (*c == '(') popen++; if (*c == ')') pclose++; c++; }
			if (popen != pclose) printf(" ...\n"); else printf("\n");
		}
	}
	TokenString = line;
}

void ParserClass::Blanks(int n) {
	printf("[parse] %02d", n); 
	for (int i = 0; i < (n+1)*2; i++) printf(" "); 
}

