// nachostabla.cc
//	Open files of a process, see nachostabla.h
//
// Copyright (c) -2026 Universidad de Costa Rica

#include <unistd.h>

#include "copyright.h"
#include "nachostabla.h"

NachosOpenFilesTable::NachosOpenFilesTable()
{
    openFiles = new int[MAX_OPEN_FILES];
    openFilesMap = new BitMap(MAX_OPEN_FILES);

    for (int i = 0; i < 3; i++) {	// stdin, stdout, stderr
        openFiles[i] = i;
        openFilesMap->Mark(i);
    }
}

NachosOpenFilesTable::~NachosOpenFilesTable()
{
    for (int i = 3; i < MAX_OPEN_FILES; i++)
        if (openFilesMap->Test(i))
            close(openFiles[i]);

    delete [] openFiles;
    delete openFilesMap;
}

int
NachosOpenFilesTable::Open(int UnixHandle)
{
    int handle = openFilesMap->Find();

    if (handle >= 0)
        openFiles[handle] = UnixHandle;
    return handle;
}

int
NachosOpenFilesTable::Close(int NachosHandle)
{
    if (!isOpened(NachosHandle))
        return -1;

    openFilesMap->Clear(NachosHandle);
    return openFiles[NachosHandle];
}

bool
NachosOpenFilesTable::isOpened(int NachosHandle)
{
    return NachosHandle >= 0 && NachosHandle < MAX_OPEN_FILES
           && openFilesMap->Test(NachosHandle);
}

int
NachosOpenFilesTable::getUnixHandle(int NachosHandle)
{
    return isOpened(NachosHandle) ? openFiles[NachosHandle] : -1;
}
