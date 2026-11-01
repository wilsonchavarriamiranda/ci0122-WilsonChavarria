// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Copyright (c) -2026 Universidad de Costa Rica

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

static const int StackPages = divRoundUp(UserStackSize, PageSize);
static const int ExitAddress = 8;	// "jal Exit" in __start (test/start.s)

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    pageTable = NULL;
    numPages = 0;
    valid = false;
    argc = argvAddr = 0;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    if (noffH.noffMagic != NOFFMAGIC)
        return;

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    initialStack = size - 16;

    memoryLock->Acquire();
    if ((int) numPages > memoryMap->NumClear()) {
        memoryLock->Release();
        numPages = 0;
        return;
    }

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = memoryMap->Find();
	pageTable[i].valid = true;
	pageTable[i].use = false;
	pageTable[i].dirty = false;
	pageTable[i].readOnly = false;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
	bzero(&machine->mainMemory[pageTable[i].physicalPage * PageSize], PageSize);
    }
    memoryLock->Release();

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        char *buffer = new char[noffH.code.size];
        executable->ReadAt(buffer, noffH.code.size, noffH.code.inFileAddr);
        CopyIn(noffH.code.virtualAddr, buffer, noffH.code.size);
        delete [] buffer;
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        char *buffer = new char[noffH.initData.size];
        executable->ReadAt(buffer, noffH.initData.size, noffH.initData.inFileAddr);
        CopyIn(noffH.initData.virtualAddr, buffer, noffH.initData.size);
        delete [] buffer;
    }

    valid = true;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   for (unsigned int i = 0; i < numPages; i++)
      memoryMap->Clear(pageTable[i].physicalPage);
   delete [] this->pageTable;
}

void
AddrSpace::CopyIn(int virtAddr, const char *from, int size)
{
    for (int i = 0; i < size; i++) {
        unsigned int vpn = (unsigned int) (virtAddr + i) / PageSize;
        unsigned int offset = (unsigned int) (virtAddr + i) % PageSize;

        if (vpn < numPages)
            machine->mainMemory[pageTable[vpn].physicalPage * PageSize + offset] = from[i];
    }
}

void
AddrSpace::SetArguments(int count, char **args)
{
    int top = numPages * PageSize;
    int *addresses = new int[count + 1];

    for (int i = count - 1; i >= 0; i--) {
        int length = strlen(args[i]) + 1;
        top -= length;
        addresses[i] = top;
        CopyIn(top, args[i], length);
    }
    addresses[count] = 0;

    top &= ~3;
    top -= (count + 1) * 4;
    for (int i = 0; i <= count; i++) {
        int word = WordToMachine((unsigned int) addresses[i]);
        CopyIn(top + i * 4, (char *) &word, 4);
    }
    delete [] addresses;

    argc = count;
    argvAddr = top;
    initialStack = (top - 16) & ~7;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, initialStack);
    DEBUG('a', "Initializing stack register to %d\n", initialStack);

    machine->WriteRegister(4, argc);
    machine->WriteRegister(5, argvAddr);
}

int
AddrSpace::AllocateStack()
{
    unsigned int first = numPages;

    memoryLock->Acquire();
    if (memoryMap->NumClear() < StackPages) {
        memoryLock->Release();
        return -1;
    }

    TranslationEntry *table = new TranslationEntry[numPages + StackPages];
    for (unsigned int i = 0; i < numPages; i++)
        table[i] = pageTable[i];
    delete [] pageTable;
    pageTable = table;
    numPages += StackPages;

    for (unsigned int i = first; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = memoryMap->Find();
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
        bzero(&machine->mainMemory[pageTable[i].physicalPage * PageSize], PageSize);
    }
    memoryLock->Release();

    RestoreState();
    return first;
}

void
AddrSpace::InitThreadRegisters(int func, int firstPage)
{
    for (int i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    machine->WriteRegister(PCReg, func);
    machine->WriteRegister(NextPCReg, func + 4);
    machine->WriteRegister(RetAddrReg, ExitAddress);
    machine->WriteRegister(StackReg, (firstPage + StackPages) * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
