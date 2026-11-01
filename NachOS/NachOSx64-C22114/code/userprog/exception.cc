// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
// Copyright (c) -2026 Universidad de Costa Rica


#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "addrspace.h"
#include "nachostabla.h"
#include "synch.h"
#include "buzon.h"

#define SC_NachOS	015177

#define MaxPathLength		256
#define MaxExecArgs		16
#define MaxKernelObjects	128


template <class T>
class KernelTable {
  public:
    KernelTable() { for (int i = 0; i < MaxKernelObjects; i++) items[i] = NULL; }

    int Add(T *item) {
       for (int i = 0; i < MaxKernelObjects; i++)
          if (items[i] == NULL) {
             items[i] = item;
             return i;
          }
       return -1;
    }

    T *Get(int id) {
       return (id >= 0 && id < MaxKernelObjects) ? items[id] : NULL;
    }

    T *Remove(int id) {
       T *item = Get(id);
       if (item != NULL)
          items[id] = NULL;
       return item;
    }

  private:
    T *items[MaxKernelObjects];
};

static KernelTable<Semaphore> semaphores;
static KernelTable<Lock> locks;
static KernelTable<Condition> conditions;
static KernelTable<Buzon> mailboxes;


static bool ValidUserBuffer(int addr, int size, bool writing) {
   int physAddr;

   for (int i = 0; i < size; i++)
      if (machine->Translate(addr + i, &physAddr, 1, writing) != NoException)
         return false;
   return true;
}

static void CopyFromUser(int addr, char *into, int size) {
   int physAddr;

   for (int i = 0; i < size; i++) {
      machine->Translate(addr + i, &physAddr, 1, false);
      into[i] = machine->mainMemory[physAddr];
   }
}

static void CopyToUser(int addr, const char *from, int size) {
   int physAddr;

   for (int i = 0; i < size; i++) {
      machine->Translate(addr + i, &physAddr, 1, true);
      machine->mainMemory[physAddr] = from[i];
   }
}

static bool CopyStringFromUser(int addr, char *into, int size) {
   int physAddr;

   for (int i = 0; i < size; i++) {
      if (machine->Translate(addr + i, &physAddr, 1, false) != NoException)
         return false;
      into[i] = machine->mainMemory[physAddr];
      if (into[i] == '\0')
         return true;
   }
   return false;
}

static void ReturnValue(int value) {
   machine->WriteRegister(2, value);
}

static void AdvancePC() {
   machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
   machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
   machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
}

static NachosOpenFilesTable * CurrentOpenFiles() {
   return processTable->getOpenFiles(currentThread->processId);
}

static int CurrentUnixHandle(int id) {
   return CurrentOpenFiles()->getUnixHandle(id);
}

static char * CopyName(const char *name) {
   char *copy = new char[strlen(name) + 1];
   strcpy(copy, name);
   return copy;
}

static void NachOS_ExitThread(int status) {
   bool isMainThread = currentThread->userStackPage < 0;

   if (processTable->RemoveThread(currentThread->processId, status, isMainThread))
      delete currentThread->space;
   currentThread->space = NULL;
   currentThread->Finish();
}

static void KillUserThread(const char *reason, ExceptionType which) {
   printf("%s exception (%d)\n", reason, which);
   NachOS_ExitThread(-1);
}

static void StartUserProcess(void *dummy) {
   currentThread->space->InitRegisters();
   currentThread->space->RestoreState();
   machine->Run();
}

static void StartUserThread(void *func) {
   currentThread->space->InitThreadRegisters((long) func, currentThread->userStackPage);
   currentThread->space->RestoreState();
   machine->Run();
}


/*
 *  System call interface: Halt()
 */
void NachOS_Halt() {		// System call 0

	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();

}


/*
 *  System call interface: void Exit( int )
 */
void NachOS_Exit() {		// System call 1
   NachOS_ExitThread(machine->ReadRegister(4));
}


/*
 *  System call interface: SpaceId Exec( char * )
 */
void NachOS_Exec() {		// System call 2
   char command[MaxPathLength];
   char *argv[MaxExecArgs];
   int argc = 0;

   if (!CopyStringFromUser(machine->ReadRegister(4), command, MaxPathLength)) {
      ReturnValue(-1);
      return;
   }

   for (char *word = strtok(command, " "); word != NULL && argc < MaxExecArgs;
        word = strtok(NULL, " "))
      argv[argc++] = word;

   OpenFile *executable = argc > 0 ? fileSystem->Open(argv[0]) : NULL;
   if (executable == NULL) {
      ReturnValue(-1);
      return;
   }

   AddrSpace *space = new AddrSpace(executable);
   delete executable;

   int id = space->IsValid() ? processTable->Create() : -1;
   if (id < 0) {
      delete space;
      ReturnValue(-1);
      return;
   }
   space->SetArguments(argc, argv);

   Thread *thread = new Thread(CopyName(argv[0]));
   thread->space = space;
   thread->processId = id;
   thread->Fork(StartUserProcess, NULL);

   ReturnValue(id);
}


/*
 *  System call interface: int Join( SpaceId )
 */
void NachOS_Join() {		// System call 3
   int id = machine->ReadRegister(4);

   if (id == currentThread->processId) {
      ReturnValue(-1);
      return;
   }
   ReturnValue(processTable->Join(id));
}


/*
 *  System call interface: void Create( char * )
 */
void NachOS_Create() {		// System call 4
   char name[MaxPathLength];
   int handle = -1;

   if (CopyStringFromUser(machine->ReadRegister(4), name, MaxPathLength))
      handle = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if (handle >= 0)
      close(handle);
   ReturnValue(handle >= 0 ? 0 : -1);
}


/*
 *  System call interface: OpenFileId Open( char * )
 */
void NachOS_Open() {		// System call 5
   char name[MaxPathLength];
   int handle = -1;

   if (CopyStringFromUser(machine->ReadRegister(4), name, MaxPathLength))
      handle = open(name, O_RDWR);
   if (handle < 0) {
      ReturnValue(-1);
      return;
   }

   int id = CurrentOpenFiles()->Open(handle);
   if (id < 0)
      close(handle);
   ReturnValue(id);
}


/*
 *  System call interface: OpenFileId Write( char *, int, OpenFileId )
 */
void NachOS_Write() {		// System call 6
   int buffer = machine->ReadRegister(4);
   int size = machine->ReadRegister(5);
   int handle = CurrentUnixHandle(machine->ReadRegister(6));

   if (handle < 0 || size < 0 || !ValidUserBuffer(buffer, size, false)) {
      ReturnValue(-1);
      return;
   }

   char *data = new char[size];
   CopyFromUser(buffer, data, size);
   fflush(stdout);
   int written = write(handle, data, size);
   delete [] data;

   ReturnValue(written);
}


/*
 *  System call interface: OpenFileId Read( char *, int, OpenFileId )
 */
void NachOS_Read() {		// System call 7
   int buffer = machine->ReadRegister(4);
   int size = machine->ReadRegister(5);
   int handle = CurrentUnixHandle(machine->ReadRegister(6));

   if (handle < 0 || size < 0 || !ValidUserBuffer(buffer, size, true)) {
      ReturnValue(-1);
      return;
   }

   char *data = new char[size];
   int count = read(handle, data, size);
   if (count > 0)
      CopyToUser(buffer, data, count);
   delete [] data;

   ReturnValue(count);
}


/*
 *  System call interface: void Close( OpenFileId )
 */
void NachOS_Close() {		// System call 8
   int handle = CurrentOpenFiles()->Close(machine->ReadRegister(4));

   if (handle > ConsoleError)
      close(handle);
   ReturnValue(handle >= 0 ? 0 : -1);
}


/*
 *  System call interface: void Fork( void (*func)() )
 */
void NachOS_Fork() {		// System call 9
   long func = machine->ReadRegister(4);
   AddrSpace *space = currentThread->space;

   int stackPage = space->AllocateStack();
   if (stackPage < 0) {
      ReturnValue(-1);
      return;
   }

   processTable->AddThread(currentThread->processId);

   Thread *thread = new Thread(CopyName("forked thread"));
   thread->space = space;
   thread->processId = currentThread->processId;
   thread->userStackPage = stackPage;
   thread->Fork(StartUserThread, (void *) func);

   ReturnValue(0);
}


/*
 *  System call interface: void Yield()
 */
void NachOS_Yield() {		// System call 10
   currentThread->Yield();
   ReturnValue(0);
}


/*
 *  System call interface: Sem_t SemCreate( int )
 */
void NachOS_SemCreate() {		// System call 11
   int initVal = machine->ReadRegister(4);

   if (initVal < 0) {
      ReturnValue(-1);
      return;
   }

   Semaphore *semaphore = new Semaphore("user semaphore", initVal);
   int id = semaphores.Add(semaphore);
   if (id < 0)
      delete semaphore;
   ReturnValue(id);
}


/*
 *  System call interface: int SemDestroy( Sem_t )
 */
void NachOS_SemDestroy() {		// System call 12
   Semaphore *semaphore = semaphores.Remove(machine->ReadRegister(4));

   delete semaphore;
   ReturnValue(semaphore != NULL ? 0 : -1);
}


/*
 *  System call interface: int SemSignal( Sem_t )
 */
void NachOS_SemSignal() {		// System call 13
   Semaphore *semaphore = semaphores.Get(machine->ReadRegister(4));

   if (semaphore != NULL)
      semaphore->V();
   ReturnValue(semaphore != NULL ? 0 : -1);
}


/*
 *  System call interface: int SemWait( Sem_t )
 */
void NachOS_SemWait() {		// System call 14
   Semaphore *semaphore = semaphores.Get(machine->ReadRegister(4));

   if (semaphore != NULL)
      semaphore->P();
   ReturnValue(semaphore != NULL ? 0 : -1);
}


/*
 *  System call interface: Lock_t LockCreate( int )
 */
void NachOS_LockCreate() {		// System call 15
   Lock *lock = new Lock("user lock");
   int id = locks.Add(lock);

   if (id < 0)
      delete lock;
   ReturnValue(id);
}


/*
 *  System call interface: int LockDestroy( Lock_t )
 */
void NachOS_LockDestroy() {		// System call 16
   Lock *lock = locks.Remove(machine->ReadRegister(4));

   delete lock;
   ReturnValue(lock != NULL ? 0 : -1);
}


/*
 *  System call interface: int LockAcquire( Lock_t )
 */
void NachOS_LockAcquire() {		// System call 17
   Lock *lock = locks.Get(machine->ReadRegister(4));

   if (lock == NULL || lock->isHeldByCurrentThread()) {
      ReturnValue(-1);
      return;
   }
   lock->Acquire();
   ReturnValue(0);
}


/*
 *  System call interface: int LockRelease( Lock_t )
 */
void NachOS_LockRelease() {		// System call 18
   Lock *lock = locks.Get(machine->ReadRegister(4));

   if (lock == NULL || !lock->isHeldByCurrentThread()) {
      ReturnValue(-1);
      return;
   }
   lock->Release();
   ReturnValue(0);
}


/*
 *  System call interface: Cond_t LockCreate( int )
 */
void NachOS_CondCreate() {		// System call 19
   Condition *condition = new Condition("user condition");
   int id = conditions.Add(condition);

   if (id < 0)
      delete condition;
   ReturnValue(id);
}


/*
 *  System call interface: int CondDestroy( Cond_t )
 */
void NachOS_CondDestroy() {		// System call 20
   Condition *condition = conditions.Remove(machine->ReadRegister(4));

   delete condition;
   ReturnValue(condition != NULL ? 0 : -1);
}


static Condition * CondArguments(Lock **lock) {
   Condition *condition = conditions.Get(machine->ReadRegister(4));
   *lock = locks.Get(machine->ReadRegister(5));

   if (*lock == NULL || !(*lock)->isHeldByCurrentThread())
      return NULL;
   return condition;
}


/*
 *  System call interface: int CondSignal( Cond_t )
 */
void NachOS_CondSignal() {		// System call 21
   Lock *lock;
   Condition *condition = CondArguments(&lock);

   if (condition != NULL)
      condition->Signal(lock);
   ReturnValue(condition != NULL ? 0 : -1);
}


/*
 *  System call interface: int CondWait( Cond_t )
 */
void NachOS_CondWait() {		// System call 22
   Lock *lock;
   Condition *condition = CondArguments(&lock);

   if (condition != NULL)
      condition->Wait(lock);
   ReturnValue(condition != NULL ? 0 : -1);
}


/*
 *  System call interface: int CondBroadcast( Cond_t )
 */
void NachOS_CondBroadcast() {		// System call 23
   Lock *lock;
   Condition *condition = CondArguments(&lock);

   if (condition != NULL)
      condition->Broadcast(lock);
   ReturnValue(condition != NULL ? 0 : -1);
}


/*
 *  System call interface: Mailbox_t MailboxCreate( int )
 */
void NachOS_MailboxCreate() {		// System call 24
   int key = machine->ReadRegister(4);

   for (int i = 0; i < MaxKernelObjects; i++)
      if (mailboxes.Get(i) != NULL && mailboxes.Get(i)->getLlave() == key) {
         ReturnValue(i);
         return;
      }

   Buzon *mailbox = new Buzon(key);
   int id = mailboxes.Add(mailbox);
   if (id < 0)
      delete mailbox;
   ReturnValue(id);
}


/*
 *  System call interface: int MailboxDestroy( Mailbox_t )
 */
void NachOS_MailboxDestroy() {		// System call 25
   int id = machine->ReadRegister(4);
   Buzon *mailbox = mailboxes.Get(id);

   if (mailbox == NULL || mailbox->EnUso()) {
      ReturnValue(-1);
      return;
   }
   mailboxes.Remove(id);
   delete mailbox;
   ReturnValue(0);
}


/*
 *  System call interface: int MailboxSend( Mailbox_t, char *, int, int )
 */
void NachOS_MailboxSend() {		// System call 26
   Buzon *mailbox = mailboxes.Get(machine->ReadRegister(4));
   int addr = machine->ReadRegister(5);
   int size = machine->ReadRegister(6);
   int type = machine->ReadRegister(7);
   char message[MAX_MENSAJE];

   if (mailbox == NULL || size < 0 || size > MAX_MENSAJE || type <= 0
       || !ValidUserBuffer(addr, size, false)) {
      ReturnValue(-1);
      return;
   }

   CopyFromUser(addr, message, size);
   mailbox->Enviar(message, size, type);
   ReturnValue(0);
}


/*
 *  System call interface: int MailboxReceive( Mailbox_t, char *, int, int )
 */
void NachOS_MailboxReceive() {		// System call 27
   Buzon *mailbox = mailboxes.Get(machine->ReadRegister(4));
   int addr = machine->ReadRegister(5);
   int size = machine->ReadRegister(6);
   int type = machine->ReadRegister(7);
   char message[MAX_MENSAJE];

   if (mailbox == NULL || size < 0 || type < 0 || !ValidUserBuffer(addr, size, true)) {
      ReturnValue(-1);
      return;
   }
   if (size > MAX_MENSAJE)
      size = MAX_MENSAJE;

   size = mailbox->Recibir(message, size, type);
   CopyToUser(addr, message, size);
   ReturnValue(size);
}


/*
 *  System call interface: Socket_t Socket( int, int )
 */
void NachOS_Socket() {			// System call 30
   int family = machine->ReadRegister(4);
   int type = machine->ReadRegister(5);

   if (family != AF_INET_NachOS || (type != SOCK_STREAM_NachOS && type != SOCK_DGRAM_NachOS)) {
      ReturnValue(-1);
      return;
   }

   int handle = socket(AF_INET, type == SOCK_STREAM_NachOS ? SOCK_STREAM : SOCK_DGRAM, 0);
   if (handle < 0) {
      ReturnValue(-1);
      return;
   }

   int id = CurrentOpenFiles()->Open(handle);
   if (id < 0)
      close(handle);
   ReturnValue(id);
}


/*
 *  System call interface: Socket_t Connect( char *, int )
 */
void NachOS_Connect() {		// System call 31
   int handle = CurrentUnixHandle(machine->ReadRegister(4));
   int port = machine->ReadRegister(6);
   char address[MaxPathLength];
   int result = -1;

   if (handle >= 0 && CopyStringFromUser(machine->ReadRegister(5), address, MaxPathLength)) {
      struct sockaddr_in host;

      memset(&host, 0, sizeof(host));
      host.sin_family = AF_INET;
      host.sin_port = htons(port);
      if (inet_pton(AF_INET, address, &host.sin_addr) == 1)
         result = connect(handle, (struct sockaddr *) &host, sizeof(host));
   }
   ReturnValue(result < 0 ? -1 : 0);
}


/*
 *  System call interface: int Bind( Socket_t, int )
 */
void NachOS_Bind() {		// System call 32
   int handle = CurrentUnixHandle(machine->ReadRegister(4));
   int port = machine->ReadRegister(5);
   int result = -1;

   if (handle >= 0) {
      struct sockaddr_in host;

      memset(&host, 0, sizeof(host));
      host.sin_family = AF_INET;
      host.sin_addr.s_addr = htonl(INADDR_ANY);
      host.sin_port = htons(port);
      result = bind(handle, (struct sockaddr *) &host, sizeof(host));
   }
   ReturnValue(result < 0 ? -1 : 0);
}


/*
 *  System call interface: int Listen( Socket_t, int )
 */
void NachOS_Listen() {		// System call 33
   int handle = CurrentUnixHandle(machine->ReadRegister(4));
   int backlog = machine->ReadRegister(5);

   ReturnValue(handle >= 0 && listen(handle, backlog) == 0 ? 0 : -1);
}


/*
 *  System call interface: int Accept( Socket_t )
 */
void NachOS_Accept() {		// System call 34
   int handle = CurrentUnixHandle(machine->ReadRegister(4));
   int client = handle < 0 ? -1 : accept(handle, NULL, NULL);

   if (client < 0) {
      ReturnValue(-1);
      return;
   }

   int id = CurrentOpenFiles()->Open(client);
   if (id < 0)
      close(client);
   ReturnValue(id);
}


/*
 *  System call interface: int Shutdown( Socket_t, int )
 */
void NachOS_Shutdown() {	// System call 25
   int handle = CurrentUnixHandle(machine->ReadRegister(4));
   int mode = machine->ReadRegister(5);
   int how[] = { SHUT_RD, SHUT_WR, SHUT_RDWR };

   if (handle < 0 || mode < 0 || mode > 2) {
      ReturnValue(-1);
      return;
   }
   ReturnValue(shutdown(handle, how[mode]) == 0 ? 0 : -1);
}


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2) - SC_User;

    switch ( which ) {

       case SyscallException:
          switch ( type ) {
             case SC_Halt:		// System call # 0
                NachOS_Halt();
                break;
             case SC_Exit:		// System call # 1
                NachOS_Exit();
                break;
             case SC_Exec:		// System call # 2
                NachOS_Exec();
                break;
             case SC_Join:		// System call # 3
                NachOS_Join();
                break;

             case SC_Create:		// System call # 4
                NachOS_Create();
                break;
             case SC_Open:		// System call # 5
                NachOS_Open();
                break;
             case SC_Read:		// System call # 6
                NachOS_Read();
                break;
             case SC_Write:		// System call # 7
                NachOS_Write();
                break;
             case SC_Close:		// System call # 8
                NachOS_Close();
                break;

             case SC_Fork:		// System call # 9
                NachOS_Fork();
                break;
             case SC_Yield:		// System call # 10
                NachOS_Yield();
                break;

             case SC_SemCreate:         // System call # 11
                NachOS_SemCreate();
                break;
             case SC_SemDestroy:        // System call # 12
                NachOS_SemDestroy();
                break;
             case SC_SemSignal:         // System call # 13
                NachOS_SemSignal();
                break;
             case SC_SemWait:           // System call # 14
                NachOS_SemWait();
                break;

             case SC_LckCreate:         // System call # 15
                NachOS_LockCreate();
                break;
             case SC_LckDestroy:        // System call # 16
                NachOS_LockDestroy();
                break;
             case SC_LckAcquire:         // System call # 17
                NachOS_LockAcquire();
                break;
             case SC_LckRelease:           // System call # 18
                NachOS_LockRelease();
                break;

             case SC_CondCreate:         // System call # 19
                NachOS_CondCreate();
                break;
             case SC_CondDestroy:        // System call # 20
                NachOS_CondDestroy();
                break;
             case SC_CondSignal:         // System call # 21
                NachOS_CondSignal();
                break;
             case SC_CondWait:           // System call # 22
                NachOS_CondWait();
                break;
             case SC_CondBroadcast:           // System call # 23
                NachOS_CondBroadcast();
                break;

             case SC_MailboxCreate:      // System call # 24
                NachOS_MailboxCreate();
                break;
             case SC_MailboxDestroy:     // System call # 25
                NachOS_MailboxDestroy();
                break;
             case SC_MailboxSend:        // System call # 26
                NachOS_MailboxSend();
                break;
             case SC_MailboxReceive:     // System call # 27
                NachOS_MailboxReceive();
                break;

             case SC_Socket:	// System call # 30
		NachOS_Socket();
               break;
             case SC_Connect:	// System call # 31
		NachOS_Connect();
               break;
             case SC_Bind:	// System call # 32
		NachOS_Bind();
               break;
             case SC_Listen:	// System call # 33
		NachOS_Listen();
               break;
             case SC_Accept:	// System call # 32
		NachOS_Accept();
               break;
             case SC_Shutdown:	// System call # 33
		NachOS_Shutdown();
               break;

             default:
                printf("NachOS version: %d-%d\n", (SC_User + SC_NachOS)/10, (SC_Base + SC_NachOS)%10 );
                printf("Unexpected syscall exception %d\n", type );
                ReturnValue( -1 );
                break;
          }
          AdvancePC();
          break;

       case PageFaultException:
          KillUserThread( "Page fault", which );
          break;

       case ReadOnlyException:
          KillUserThread( "Read Only", which );
          break;

       case BusErrorException:
          KillUserThread( "Bus error", which );
          break;

       case AddressErrorException:
          KillUserThread( "Address error", which );
          break;

       case OverflowException:
          KillUserThread( "Overflow", which );
          break;

       case IllegalInstrException:
          KillUserThread( "Ilegal instruction", which );
          break;

       default:
          KillUserThread( "Unexpected", which );
          break;
    }

}
