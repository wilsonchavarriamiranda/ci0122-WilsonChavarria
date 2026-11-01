// proctable.cc
//	Table of user processes, see proctable.h
//
// Copyright (c) -2026 Universidad de Costa Rica

#include "copyright.h"
#include "proctable.h"

ProcessTable::ProcessTable()
{
    for (int i = 0; i < MAX_PROCESSES; i++)
        table[i].used = false;
    lock = new Lock("process table");
}

bool
ProcessTable::isValid(int id)
{
    return id >= 0 && id < MAX_PROCESSES && table[id].used;
}

int
ProcessTable::Create()
{
    int id = -1;

    lock->Acquire();
    for (int i = 0; i < MAX_PROCESSES && id < 0; i++)
        if (!table[i].used)
            id = i;

    if (id >= 0) {
        table[id].used = true;
        table[id].joining = false;
        table[id].threads = 1;
        table[id].exitStatus = 0;
        table[id].done = new Semaphore("process done", 0);
        table[id].openFiles = new NachosOpenFilesTable();
    }
    lock->Release();

    return id;
}

void
ProcessTable::AddThread(int id)
{
    lock->Acquire();
    table[id].threads++;
    lock->Release();
}

bool
ProcessTable::RemoveThread(int id, int status, bool isMainThread)
{
    lock->Acquire();
    Process *p = &table[id];

    if (isMainThread)
        p->exitStatus = status;
    bool last = --p->threads == 0;

    if (last) {
        delete p->openFiles;
        p->openFiles = NULL;
        p->done->V();
    }
    lock->Release();

    return last;
}

int
ProcessTable::Join(int id)
{
    lock->Acquire();
    if (!isValid(id) || table[id].joining) {
        lock->Release();
        return -1;
    }
    table[id].joining = true;
    lock->Release();

    table[id].done->P();

    lock->Acquire();
    int status = table[id].exitStatus;
    delete table[id].done;
    table[id].used = false;
    lock->Release();

    return status;
}

NachosOpenFilesTable *
ProcessTable::getOpenFiles(int id)
{
    return isValid(id) ? table[id].openFiles : NULL;
}
