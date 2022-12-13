/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

#include <cstdio>
#include <cassert>
#include <iostream>
#include <fstream>
#include <map>
#include <list>

#include "pin.H"
#include "instlib.H"

using namespace std;

INSTLIB::FILTER filter;

/* ===================================================================== */
/* Structs                                                               */
/* ===================================================================== */

// Struct to represent Unique Instruction
struct UniqueInstr {
    ADDRINT addr;
    USIZE size;
    USIZE exec_count;

    UniqueInstr(ADDRINT addr = 0, USIZE size = 0)
	: addr(addr), size(size), exec_count(0) {}
};

// Representation of a group of unique instructions
struct InstrGroup {
    USIZE exec_count;
    std::list<UniqueInstr*> instrs;

    InstrGroup(USIZE size) : exec_count(0) {}
};

/* ===================================================================== */
/* Way Pin handles command line parameters                               */
/* ===================================================================== */

KNOB<string> KnobInputFile(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "i",
    "default",
    "specify input file name"
);

KNOB<string> KnobOutputFile(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "o",
    "default",
    "specify output file name"
);

// Instructions HashMap
std::map<ADDRINT, UniqueInstr*> instructions;
// Instruction Group as list
std::list<InstrGroup*> groups;

/* ===================================================================== */
/*  Form of fetching the instruction or creating a new one                */
/* ===================================================================== */

UniqueInstr* fetch_instr(ADDRINT addr, USIZE size = 0) {
    UniqueInstr* instr = instructions[addr];
    if (instr == 0) {
        instr = new UniqueInstr(addr, size);
        instructions[addr] = instr;
    } else {
        assert(instr->size == size);
    }

    return instr;
}

/* ===================================================================== */
/* Read Instructions of another filename                                 */
/* ===================================================================== */

void read_instrs(const char* filename) {
    FILE* f;
    char buffer[255];
    static char sep[] = ":\r\n";

    f = fopen(filename, "r");
    if (f != 0) {
        while (fgets(buffer, sizeof(buffer), f)) {
            ADDRINT addr = strtoull(strtok(buffer+2, sep), 0, 16);
            USIZE size = atoi(strtok(0, sep));

            UniqueInstr* instr = fetch_instr(addr, size);
            instr->exec_count += strtoull(strtok(0, sep), 0, 10);
        }

        fclose(f);
    }
}

/* ===================================================================== */
/* Increases exec count of Instrucution group                            */
/* ===================================================================== */

VOID counter(InstrGroup* group) {
    ++group->exec_count;
}

/* ===================================================================== */
/* Creates the Trace that fetches the Basic blocks                       */
/* ===================================================================== */

VOID Trace(TRACE trace, VOID* v) {
    if (!filter.SelectTrace(trace)) return;
    // Visit every basic block in the trace.
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        USIZE num_instrs = BBL_NumIns(bbl);
        InstrGroup* group = new InstrGroup(num_instrs);
        groups.push_back(group);

        // for each instruction in a bbl setup a memory address for it in the hash
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            ADDRINT addr = INS_Address(ins);
            USIZE size = INS_Size(ins);

            UniqueInstr* instr = fetch_instr(addr, size);
            group->instrs.push_back(instr);
        }

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) counter, IARG_PTR, group, IARG_END);
    }
}

/* ===================================================================== */
/* Counts groups into the individual instructions in the Hash.           */
/* ===================================================================== */

void flush_groups() {
    for (std::list<InstrGroup*>::iterator it = groups.begin(), ed = groups.end();
            it != ed; it++) {
        InstrGroup* group = *it;

        for (std::list<UniqueInstr*>::iterator it2 = group->instrs.begin(), ed2 = group->instrs.end();
                it2 != ed2; it2++) {
            UniqueInstr* instr = *it2;

            instr->exec_count += group->exec_count;
        }
    }
}

/* ===================================================================== */
/* Creates stream of instr for print on file or screen                   */
/* ===================================================================== */

void dump_instrs(FILE* fp) {
    FILE* output = fp != 0 ? fp : stdout;
    for (std::map<ADDRINT, UniqueInstr*>::iterator it = instructions.begin(), ed = instructions.end();
            it != ed; it++) {
        UniqueInstr* instr = it->second;
        fprintf(output ,"0x%lx:%lu:%lu\n", instr->addr, instr->size, instr->exec_count);
    }

    if (fp != 0)
        fclose(fp);
}

/* ===================================================================== */
/* Ends Application , Prints Data                                        */
/* This function is called when the application exits                    */
/* ===================================================================== */

VOID Fini(INT32 code, VOID *v) {
    flush_groups();
    dump_instrs((FILE*) v);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage() {
    PIN_ERROR("This Pintool counts every instruction executed\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[]) {
    // Initialize pin
    if (PIN_Init(argc, argv))
      return Usage();

    if (KnobInputFile.Value() != "default") {
       read_instrs(KnobInputFile.Value().c_str());
    }

    FILE* fp = 0;
    if (KnobOutputFile.Value() != "default") {
        fp = fopen(KnobOutputFile.Value().c_str(), "w");
    }

    // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    filter.Activate();

    // Register fini function.
    PIN_AddFiniFunction(Fini, fp);

    // Start the program, never returns.
    PIN_StartProgram();

    return 0;
}