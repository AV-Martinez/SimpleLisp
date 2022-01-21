#pragma once

#include <stdio.h>
#include "memory.h"

#define MAXLINELENPARSER 180 /** Maximum length of a line to be parsed */

/**
 * This implements a tail recursive parser for sexprs using functions Parse, ParseQuote and ParseListItem.
 * 
 * Any memory cell needed by the parser is created by CreatedCellForParser, which also pushes the cell 
 * into the _GCSAFE_ stack and keeps a count of the created cells. This guarantees that the checks for
 * garbage collection at the begining of Parse do not spoil the parsing tree being built.
 * 
 * When parsing is complete, the same number of memory cells created during the process are popped out 
 * of _GCSAFE_.
 * 
 */

class ParserClass {
public:
	void Init(char *str);
	void Init(FILE *f);
	addr Parse(int level);

	bool Ok;
	bool Trace = false;

private:
	char *TokenString;
	
	addr ParseQuote(int level);
	addr ParseListItem(int level);

	char *NextToken();
	char NextChar();
	bool NextCharRepeat;

	int  CreateCellCount;
	addr CreateCellForParser(char *item);
	addr CreateCellForParser(addr car, addr cdr);

	void FileInputReadLine();
	FILE *FileInput;

	void Blanks(int n);
};

extern ParserClass Parser;
