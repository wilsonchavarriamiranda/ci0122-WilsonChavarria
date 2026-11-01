// proctable.h
//	Table of user processes, the index is the SpaceId returned by Exec.
//
// Copyright (c) -2026 Universidad de Costa Rica

#ifndef PROCTABLE_H
#define PROCTABLE_H

#include "synch.h"
#include "nachostabla.h"

#define MAX_PROCESSES	128

class ProcessTable {
  public:
    ProcessTable();

    int Create();
    void AddThread(int id);
    bool RemoveThread(int id, int status, bool isMainThread);
    int Join(int id);
    NachosOpenFilesTable *getOpenFiles(int id);

  private:
    struct Process {
        bool used;
        bool joining;
        int threads;
        int exitStatus;
        Semaphore *done;
        NachosOpenFilesTable *openFiles;
    };

    bool isValid(int id);

    Process table[MAX_PROCESSES];
    Lock *lock;
};

#endif // PROCTABLE_H
