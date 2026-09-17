/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class implementation
  *
 **/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstring>
#include <stdexcept>

#include "buzon.h"

#define MAX_PAYLOAD 256

struct PaqueteIPC {
    long mtype; // exigido por msgsnd/msgrcv como primer campo
    char datos[ MAX_PAYLOAD ]; // aqui se copia el MensajePapa real
};


/**
  *  Class constructor
  *
 **/
Buzon::Buzon( int idParticipante, bool esPropio ) {
    llave  = KEY_BASE + idParticipante; // misma llave = mismo buzon para todos
    propio = esPropio;

    id = msgget( llave, IPC_CREAT | 0666 ); // crea el buzon, o lo abre si ya existe

    if ( -1 == id ) {
        throw std::runtime_error( "Buzon::Buzon( int )" );
    }
}


/**
  * Class destructor
  *
 **/
Buzon::~Buzon() {
    if ( propio ) { // solo el dueno real destruye el buzon
        if ( -1 == msgctl( id, IPC_RMID, NULL ) ) {
            perror( "Buzon::~Buzon" );
        }
    }
}


/**
  *  Send method
  *
 **/
int Buzon::Enviar( const void * mensaje, int cantidad, long tipo ) {
    int st = -1;

    PaqueteIPC paquete;
    paquete.mtype = tipo;
    memcpy( paquete.datos, mensaje, cantidad ); // empaca el mensaje real dentro del paquete IPC

    st = msgsnd( id, &paquete, cantidad, 0 ); // 0 = bloqueante, espera si la cola esta llena

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Enviar( const void *, int, long )" );
    }

    return st;
}


/**
  *  Receive method
  *
 **/
int Buzon::Recibir( void * mensaje, int cantidad, long tipo ) {
    int st = -1;

    PaqueteIPC paquete;

    st = msgrcv( id, &paquete, cantidad, tipo, 0 ); // 0 = bloqueante, espera hasta que llegue algo

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Recibir( void *, int, long )" );
    }

    memcpy( mensaje, paquete.datos, cantidad ); // saca el mensaje real del paquete IPC

    return st;
}