/* papa.c
 *	Papa caliente con procesos y buzones de NachOS.
 *	Uso (desde el shell):  ../test/papa n v [sentido]
 */
#include "syscall.h"

#define KEY_BASE	0x1A2B00

struct MensajePapa {
    int valor;
    int salidos;
};

int strlen(char *s) {
    int n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

int atoi(char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + *s++ - '0';
    return n;
}

void Copiar(char *destino, char *origen) {
    while ((*destino++ = *origen++) != '\0')
        ;
}

void Print(char *s) {
    Write(s, strlen(s), ConsoleOutput);
}

void PrintInt(int n) {
    char digits[12];
    int i = 11;

    digits[i] = '\0';
    if (n == 0)
        digits[--i] = '0';
    while (n > 0) {
        digits[--i] = '0' + n % 10;
        n /= 10;
    }
    Print(&digits[i]);
}

void Proceso(int id) {
    Print("[Proceso ");
    PrintInt(id);
    Print("] ");
}

int Aleatorio(int *semilla) {
    *semilla = (*semilla * 1103515245 + 12345) & 0x7fffffff;
    return *semilla >> 16;
}

int PasoCollatz(int v) {
    return (v % 2 == 0) ? v / 2 : 3 * v + 1;
}

void Participante(int id, int n, int sentido) {
    struct MensajePapa m;
    int semilla = id + 17;
    int activo = 1;
    int siguiente = (sentido == 0) ? (id + 1) % n : (id - 1 + n) % n;
    Mailbox_t miBuzon = MailboxCreate(KEY_BASE + id);
    Mailbox_t vecino = MailboxCreate(KEY_BASE + siguiente);

    while (1) {
        MailboxReceive(miBuzon, (char *) &m, sizeof(m), 1);

        if (m.valor < 0) {
            MailboxSend(vecino, (char *) &m, sizeof(m), 1);
            break;
        }

        if (activo) {
            Proceso(id);
            Print("recibe papa = ");
            PrintInt(m.valor);
            m.valor = PasoCollatz(m.valor);
            Print(" -> Collatz -> ");
            PrintInt(m.valor);
            Print("\n");

            if (m.valor == 1) {
                activo = 0;
                m.salidos++;
                Proceso(id);
                Print("la papa exploto, salgo del juego (salidos = ");
                PrintInt(m.salidos);
                Print(")\n");

                if (m.salidos == n - 1) {
                    Proceso(id);
                    Print("GANADOR\n");
                    m.valor = -1;
                    MailboxSend(vecino, (char *) &m, sizeof(m), 1);
                    break;
                }

                m.valor = Aleatorio(&semilla) % 100 + 1;
                Proceso(id);
                Print("nuevo valor al azar = ");
                PrintInt(m.valor);
                Print("\n");
            }
        } else {
            Proceso(id);
            Print("reenvia papa = ");
            PrintInt(m.valor);
            Print("\n");
        }

        MailboxSend(vecino, (char *) &m, sizeof(m), 1);
    }

    Proceso(id);
    Print("termina.\n");
}

int main(int argc, char *argv[]) {
    struct MensajePapa m;
    SpaceId jugadores[9];
    char comando[32];
    int i, n, v, sentido, semilla, inicial;

    if (argc == 5) {
        Participante(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        Exit(0);
    }

    n = (argc >= 3) ? atoi(argv[1]) : 0;
    v = (argc >= 3) ? atoi(argv[2]) : 0;
    sentido = (argc == 4 && atoi(argv[3]) != 0) ? 1 : 0;
    if (n < 2 || n > 9 || v < 1) {
        Print("Uso: ../test/papa n v [sentido], con 2 <= n <= 9 y v >= 1\n");
        Exit(1);
    }

    for (i = 0; i < n; i++)
        MailboxCreate(KEY_BASE + i);

    semilla = v;
    inicial = Aleatorio(&semilla) % n;
    Print("[main] arranca el proceso ");
    PrintInt(inicial);
    Print("\n");

    for (i = 0; i < n; i++) {
        Copiar(comando, "../test/papa -j 0 0 0");
        comando[16] = '0' + i;
        comando[18] = '0' + n;
        comando[20] = '0' + sentido;
        jugadores[i] = Exec(comando);
    }

    m.valor = v;
    m.salidos = 0;
    MailboxSend(MailboxCreate(KEY_BASE + inicial), (char *) &m, sizeof(m), 1);

    for (i = 0; i < n; i++)
        Join(jugadores[i]);

    for (i = 0; i < n; i++)
        MailboxDestroy(MailboxCreate(KEY_BASE + i));

    Print("[main] todos los procesos terminaron.\n");
    Exit(0);
}
