// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create several threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//
//
// Copyright (c) -2026 Universidad de Costa Rica

#include <unistd.h>
#include "copyright.h"
#include "system.h"
#include "diningph.h"

DiningPh * dp;
Semaphore * fin; // main espera aqui a que terminen los demas hilos

// Avisa a main que este hilo terminó y lo finaliza, sin que el timer
// pueda interrumpir entre ambas cosas: asi main siempre es el último
// hilo vivo cuando NachOS se apaga
void Terminar() {
    interrupt->SetLevel( IntOff );
    fin->V();
    currentThread->Finish();
}

void Philo( void * p ) {

    int eats, thinks;
    long who = (long) p;

    currentThread->Yield();

    for ( int i = 0; i < 10; i++ ) {

        printf(" Philosopher %ld will try to pickup sticks\n", who + 1);

        dp->pickup( who );
        dp->print();
        eats = Random() % 6;

        currentThread->Yield();
        sleep( eats );

        dp->putdown( who );

        thinks = Random() % 6;
        currentThread->Yield();
        sleep( thinks );
    }

    Terminar();

}

//----------------------------------------------------------------------
// Problema del agua (H2O), resuelto con un monitor (Lock + Condition)
//	cH, cO: atomos que llegaron y todavia no estan en una molecula
//	pH, pO: "pases" de salida, atomos ya enlazados que pueden seguir
//	El atomo que completa una molecula (2 H y 1 O) la forma: pasa esos
//	atomos de cH/cO a pH/pO y despierta a los que esperan.
//----------------------------------------------------------------------

int cO = 0;
int cH = 0;
int pO = 0;
int pH = 0;
int moleculas = 0;
Lock * agua; // protege todas las variables del agua
Condition * esperaH; // aqui esperan los H
Condition * esperaO; // aqui esperan los O

// Debe llamarse con el lock "agua" tomado
void FormarMolecula( const char * quien, long id ) {

    if ( cH >= 2 && cO >= 1 ) {
        cH -= 2;
        cO -= 1;
        pH += 2;
        pO += 1;
        moleculas++;
        printf( "Molecula #%d de agua formada por el %s %ld\n", moleculas, quien, id );
        esperaH->Broadcast( agua );
        esperaO->Broadcast( agua );
    }

}

void H( void * p ) {

    long id = (long) p;

    agua->Acquire();
    cH++;
    FormarMolecula( "H", id );
    while ( pH == 0 )
        esperaH->Wait( agua );
    pH--;
    agua->Release();

    Terminar();

}

void O( void * p ) {

    long id = (long) p;

    agua->Acquire();
    cO++;
    FormarMolecula( "O", id );
    while ( pO == 0 )
        esperaO->Wait( agua );
    pO--;
    agua->Release();

    Terminar();

}

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 10 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"name" points to a string with a thread name, just for
//      debugging purposes.
//----------------------------------------------------------------------

void
SimpleThread(void* name)
{
    // Reinterpret arg "name" as a string
    char* threadName = (char*)name;
    
    // If the lines dealing with interrupts are commented,
    // the code will behave incorrectly, because
    // printf execution may cause race conditions.
    for (int num = 0; num < 3; num++) {
        //IntStatus oldLevel = interrupt->SetLevel(IntOff);
	printf( "*** thread %s looped %d times\n", currentThread->getName(), num );
	//interrupt->SetLevel(oldLevel);
        //currentThread->Yield();
    }
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
    printf( ">>> Thread %s has finished\n", threadName );
    //interrupt->SetLevel(oldLevel);

    currentThread->Yield();		// Yielding CPU will delete threads asynchronously

    currentThread->Finish();

}



//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

// Prueba que corre ThreadTest:
//	1 = hilos simples (SimpleThread)   2 = filosofos   3 = agua (H2O)
#define PRUEBA 2

void
ThreadTest()
{
    Thread * Ph;

    DEBUG('t', "Entering SimpleTest");

#if PRUEBA == 2
    dp = new DiningPh();
    fin = new Semaphore( "fin", 0 );

    for ( long k = 0; k < 5; k++ ) {
        char * nombre = new char[ 20 ]; // ~Thread() hace delete[] del nombre
        sprintf( nombre, "Filosofo %ld", k + 1 );
        Ph = new Thread( nombre );
        Ph->Fork( Philo, (void *) k );
    }

    for ( int k = 0; k < 5; k++ ) // main espera a los 5 filosofos
        fin->P();

    return;
#elif PRUEBA == 3
    agua = new Lock( "agua" );
    esperaH = new Condition( "esperaH" );
    esperaO = new Condition( "esperaO" );
    fin = new Semaphore( "fin", 0 );

    const int N = 10; // cantidad de moleculas a formar
    int faltanH = 2 * N, faltanO = N;
    for ( long k = 0; k < 3 * N; k++ ) {
        char * nombre = new char[ 20 ]; // ~Thread() hace delete[] del nombre
        // orden aleatorio, pero exactamente 2N hidrogenos y N oxigenos
        if ( Random() % ( faltanH + faltanO ) < faltanH ) {
            faltanH--;
            sprintf( nombre, "H %ld", k );
            Ph = new Thread( nombre );
            Ph->Fork( H, (void *) k );
        } else {
            faltanO--;
            sprintf( nombre, "O %ld", k );
            Ph = new Thread( nombre );
            Ph->Fork( O, (void *) k );
        }
    }

    for ( int k = 0; k < 3 * N; k++ ) // main espera a los 3N atomos
        fin->P();
    printf( "Total: %d moleculas de agua\n", moleculas );

    return;
#else
    char * threadName;
    Thread * newThread;
    for ( int k=1; k<5; k++ ) {
      threadName = new char[100];
      sprintf( threadName, "Hilo %d", k );
      newThread = new Thread( threadName );
      newThread->Fork ( SimpleThread, (void*) threadName );
    }

    currentThread->Yield();
 
    SimpleThread( (void*) "Hilo 0" );

    currentThread->Finish();
#endif

}

