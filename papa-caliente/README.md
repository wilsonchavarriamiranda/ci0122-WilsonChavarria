# Wilson Chavarría Miranda - C22114

# Papa Caliente

Tarea programada I - CI-0122 Sistemas Operativos, UCR-ECCI 2026-II

## Descripcion

Simulacion del juego de la papa caliente usando procesos (`fork`) y
buzones de mensajes (colas IPC de System V). Se arma un anillo de `n`
procesos que se van pasando un mensaje (la papa) entre vecinos. Cada
participante activo le aplica un paso de las reglas de Collatz al valor
de la papa; si el resultado da 1, la papa "explota" y ese participante
sale del juego (se vuelve pasivo y elige un nuevo valor al azar antes de
seguir pasando la papa). Gana el ultimo participante en explotar.

## Estructura

```
papa-caliente/
├── Makefile
└── src/
    ├── buzon.h
    ├── buzon.cc
    └── main.cc
```

## Compilar

```
make
```

Genera el ejecutable `papa` en la raiz del proyecto.

Para limpiar los `.o` y el ejecutable:

```
make clean
```

## Correr

```
./papa n v [sentido]
```

- `n`: cantidad de participantes del anillo (n >= 2)
- `v`: valor inicial de la papa
- `sentido`: 0 = horario (por defecto), 1 = antihorario

Ejemplos:

```
./papa 6 27
./papa 8 100 1
```

## Notas

- La comunicacion entre procesos usa colas de mensajes de System V
  (`msgget`/`msgsnd`/`msgrcv`/`msgctl`), encapsuladas en la clase `Buzon`.
- Si el programa termina de forma abrupta y quedan colas sin destruir,
  se revisan con `ipcs -q` y se borran a mano con `ipcrm -q <id>`.