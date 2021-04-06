#ifndef UTIL_H
#define UITL_H

#include "cfa.h"
#include "core.h"


extern void PrintRecord(Record *r, int blanks);
extern void AssignRecordID(Record *r);
extern void DumpRecord(Record *r);
extern void InsertHints(Record *r, Module &M);


#endif
