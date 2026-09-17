/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class interface
  *
 **/

#ifndef BUZON_H
#define BUZON_H

#include <sys/types.h> // pid_t definition

#define KEY_BASE 0x1A2B00 // Valor de la llave del recurso

struct MensajePapa {
    long mtype;
    int  valor;
    int  salidos;
};

class Buzon {
   public:
      Buzon( int idParticipante, bool esPropio = true );
      ~Buzon();
      int Enviar( const void *mensaje, int cantidad, long tipo = 1 );
      int Recibir( void *mensaje, int cantidad, long tipo = 1 );

   private:
      int id; // Identificador del buzon
      key_t llave; // Llave del buzon
      bool propio; // Si debe destruirlo al terminar

};

#endif