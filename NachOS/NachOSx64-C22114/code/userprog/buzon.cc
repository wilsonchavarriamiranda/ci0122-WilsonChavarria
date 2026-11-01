// buzon.cc
//	Mailboxes for user programs, see buzon.h
//
// Copyright (c) -2026 Universidad de Costa Rica

#include <string.h>

#include "copyright.h"
#include "buzon.h"

Buzon::Buzon(int llaveBuzon)
{
    llave = llaveBuzon;
    usuarios = 0;
    mensajes = new Mensaje[MAX_MENSAJES];
    cantidadMensajes = 0;
    lock = new Lock("buzon");
    hayMensaje = new Condition("buzon hay mensaje");
    hayEspacio = new Condition("buzon hay espacio");
}

Buzon::~Buzon()
{
    delete [] mensajes;
    delete lock;
    delete hayMensaje;
    delete hayEspacio;
}

void
Buzon::Enviar(const char *mensaje, int cantidad, int tipo)
{
    usuarios++;
    lock->Acquire();
    while (cantidadMensajes == MAX_MENSAJES)
        hayEspacio->Wait(lock);

    Mensaje *nuevo = &mensajes[cantidadMensajes++];
    nuevo->tipo = tipo;
    nuevo->cantidad = cantidad;
    memcpy(nuevo->datos, mensaje, cantidad);

    hayMensaje->Broadcast(lock);
    lock->Release();
    usuarios--;
}

int
Buzon::Buscar(int tipo)
{
    for (int i = 0; i < cantidadMensajes; i++)
        if (tipo == 0 || mensajes[i].tipo == tipo)
            return i;
    return -1;
}

int
Buzon::Recibir(char *mensaje, int cantidad, int tipo)
{
    int i;

    usuarios++;
    lock->Acquire();
    while ((i = Buscar(tipo)) < 0)
        hayMensaje->Wait(lock);

    if (cantidad > mensajes[i].cantidad)
        cantidad = mensajes[i].cantidad;
    memcpy(mensaje, mensajes[i].datos, cantidad);

    cantidadMensajes--;
    for (; i < cantidadMensajes; i++)
        mensajes[i] = mensajes[i + 1];

    hayEspacio->Signal(lock);
    lock->Release();
    usuarios--;

    return cantidad;
}
