// nachostabla.h
//	Open files of a process: NachOS handles to Unix file descriptors.
//	Handles 0, 1 and 2 are the console.
//
// Copyright (c) -2026 Universidad de Costa Rica

#ifndef NACHOSTABLA_H
#define NACHOSTABLA_H

#include "bitmap.h"

#define MAX_OPEN_FILES	32

class NachosOpenFilesTable {
  public:
    NachosOpenFilesTable();
    ~NachosOpenFilesTable();

    int Open(int UnixHandle);
    int Close(int NachosHandle);
    bool isOpened(int NachosHandle);
    int getUnixHandle(int NachosHandle);

  private:
    int *openFiles;
    BitMap *openFilesMap;
};

#endif // NACHOSTABLA_H
