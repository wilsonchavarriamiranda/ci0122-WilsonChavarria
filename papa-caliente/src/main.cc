/**
  *  Tarea programada I - Juego de la "papa" caliente
  *  CI-0122 Sistemas Operativos - UCR ECCI 2026-II
  *
  *  Uso: ./papa n v [sentido]
  *       sentido: 0 = horario (default), 1 = antihorario
 **/

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>

#include "buzon.h"

int pasoCollatz( int v ) {
    return ( v % 2 == 0 ) ? v / 2 : 3 * v + 1;
}

void correrParticipante( int id, int n, int sentido ) {

    setbuf( stdout, NULL );
    srand( (unsigned) time(NULL) ^ getpid() );

    Buzon miBuzon( id, false ); // mi propio buzon, no lo destruyo yo
    int siguiente = ( sentido == 0 ) ? ( id + 1 ) % n : ( id - 1 + n ) % n; // horario o antihorario
    Buzon vecino( siguiente, false ); // buzon del siguiente, solo para enviarle

    bool activo = true; // false = ya explote, solo reenvio sin tocar el valor

    while ( true ) {
        MensajePapa m;
        miBuzon.Recibir( &m, sizeof(m), 1 );

        if ( m.valor < 0 ) {
            vecino.Enviar( &m, sizeof(m), m.mtype ); // reenvio la señal de fin
            break;
        }

        if ( activo ) {
            int nuevo = pasoCollatz( m.valor );
            printf( "[Proceso %d] recibe papa = %d -> Collatz -> %d\n", id, m.valor, nuevo );
            m.valor = nuevo;

            if ( m.valor == 1 ) {
                activo = false;
                m.salidos++;
                printf( "[Proceso %d] la papa exploto, salgo del juego (salidos = %d)\n", id, m.salidos );

                if ( m.salidos == n - 1 ) { // todos los demas ya explotaron: soy el ultimo, gano
                    printf( "[Proceso %d] GANADOR, valor final = %d\n", id, m.valor );
                    m.valor = -1; // valor negativo = señal de fin de juego
                    vecino.Enviar( &m, sizeof(m), m.mtype );
                    break;
                }

                m.valor = rand() % 100 + 1;
                printf( "[Proceso %d] nuevo valor al azar = %d\n", id, m.valor );
            }
        } else {
            printf( "[Proceso %d] reenvia papa = %d\n", id, m.valor );
        }

        vecino.Enviar( &m, sizeof(m), m.mtype );
    }

    printf( "[Proceso %d] termina.\n", id );
}

int main( int argc, char ** argv ) {

    if ( argc < 3 ) {
        fprintf( stderr, "Uso: %s n v [sentido]\n", argv[0] );
        return 1;
    }

    int n       = atoi( argv[1] );
    int v       = atoi( argv[2] );
    int sentido = ( argc >= 4 ) ? atoi( argv[3] ) : 0;

    if ( n < 2 ) {
        fprintf( stderr, "n debe ser >= 2\n" );
        return 1;
    }

    setbuf( stdout, NULL );

    // main crea los n buzones antes de generar los procesos
    for ( int i = 0; i < n; i++ ) {
        Buzon crear( i, false ); // false: main no los destruye aqui todavia
    }

    srand( (unsigned) time(NULL) );
    int idInicial = rand() % n; // participante que arranca el juego

    printf( "[main] n=%d v=%d sentido=%d, arranca el proceso %d\n", n, v, sentido, idInicial );

    for ( int i = 0; i < n; i++ ) {
        pid_t pid = fork();

        if ( pid < 0 ) {
            perror( "fork" );
            return 1;
        }

        if ( pid == 0 ) { // codigo del hijo
            correrParticipante( i, n, sentido );
            exit( 0 ); // el hijo no debe seguir ejecutando el for del padre
        }
    }

    {
        Buzon inicial( idInicial, false ); // solo para enviar el primer mensaje
        MensajePapa m;
        m.mtype   = 1;
        m.valor   = v; // valor inicial de la papa, dado por parametro
        m.salidos = 0;
        inicial.Enviar( &m, sizeof(m), m.mtype ); // unica intervencion de main en la ronda
    }

    for ( int i = 0; i < n; i++ ) {
        wait( NULL ); // espera a que cada hijo termine, sin participar en el juego
    }

    // main elimina los buzones ya que todos los participantes terminaron
    for ( int i = 0; i < n; i++ ) {
        Buzon destruir( i, true ); // true: ahora si se destruye cada buzon
    }

    printf( "[main] todos los procesos terminaron.\n" );
    return 0;
}