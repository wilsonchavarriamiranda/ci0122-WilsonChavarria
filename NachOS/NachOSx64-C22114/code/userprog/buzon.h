// buzon.h
//	Mailboxes for user programs, like System V message queues.
//
// Copyright (c) -2026 Universidad de Costa Rica

#ifndef BUZON_H
#define BUZON_H

#include "synch.h"

#define MAX_MENSAJE	256
#define MAX_MENSAJES	64

class Buzon {
  public:
    Buzon(int llave);
    ~Buzon();

    int getLlave() { return llave; }
    bool EnUso() { return usuarios > 0; }

    void Enviar(const char *mensaje, int cantidad, int tipo);
    int Recibir(char *mensaje, int cantidad, int tipo);

  private:
    struct Mensaje {
       int tipo;
       int cantidad;
       char datos[MAX_MENSAJE];
    };

    int Buscar(int tipo);

    int llave;
    int usuarios;
    Mensaje *mensajes;
    int cantidadMensajes;
    Lock *lock;
    Condition *hayMensaje;
    Condition *hayEspacio;
};

#endif // BUZON_H
