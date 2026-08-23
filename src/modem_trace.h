/*
 * Copyright (c) 2026 bmarty
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Trace de debug du firmware modem PicoWiFiModemUSB.
 *
 * IMPORTANT : la sortie part sur la console stdio = UART0 (GP0/GP1, 115200),
 * PAS sur le CDC modem. Elle ne perturbe donc pas le flux serie vers l'Oric.
 * (Rappel : pico_enable_stdio_uart(1) => printf -> UART0 ; ser_puts(ser0,...)
 * -> CDC.)
 *
 * Deux contextes empilent des evenements :
 *   - contexte principal : loop(), doAtCmds(), sendResult(), tcpConnect()...
 *   - contexte IRQ des callbacks lwIP (pico_cyw43_arch threadsafe_background) :
 *     tcpRecv(), tcpHasConnected(), tcpClientErr()...
 * => ring buffer a DEUX producteurs. L'ecriture de l'index est donc protegee
 *    par save_and_disable_interrupts(). La sortie reelle (printf, bloquante sur
 *    l'UART) se fait UNIQUEMENT depuis loop() via mtrace_flush(), jamais en IRQ
 *    ni dans un chemin chaud : on evite ainsi la famine CDC (meme classe de bug
 *    que startupWait/ATC1/AT$SCAN deja rencontree sur ce projet).
 *
 * Tags :
 *   'A' commande AT recue          (chaine, via mtrace_str, contexte principal)
 *   'R' result code envoye         (ResultCodes : 0=OK 1=CONNECT 2=RING
 *                                    3=NO_CARRIER 4=ERROR 5=NO_ANSWER 6=RING_IP)
 *   'D' tentative connexion TCP    (val = port)
 *   'L' tentative connexion TLS    (val = port)
 *   'C' TCP connecte (callback)    (val = err lwIP)
 *   'X' TCP erreur/ferme (callback)(val = err lwIP)
 *   'S' changement d'etat machine  (val : 0=CMD_NOT_IN_CALL 1=CMD_IN_CALL
 *                                    2=ONLINE 3=PASSWORD)
 *
 * Mettre MODEM_TRACE a 0 pour retirer entierement la trace du binaire.
 */

#ifndef _MODEM_TRACE_H_
#define _MODEM_TRACE_H_

#include <stdint.h>
#include <stdio.h>
#include "hardware/sync.h"

#ifndef MODEM_TRACE
#define MODEM_TRACE 1
#endif

#if MODEM_TRACE

#define MTRACE_SIZE 128            /* puissance de 2 */
#define MTRACE_MASK (MTRACE_SIZE - 1)

typedef struct { char tag; uint32_t val; } mtrace_ev_t;
static volatile mtrace_ev_t mtrace_buf[MTRACE_SIZE];
static volatile uint8_t mtrace_head;
static volatile uint8_t mtrace_tail;

/* Producteur : appelable depuis le contexte principal ET les callbacks lwIP
 * (IRQ). Non bloquant. */
static inline void mtrace(char tag, uint32_t val)
{
    uint32_t save = save_and_disable_interrupts();
    uint8_t h = mtrace_head;
    mtrace_buf[h].tag = tag;
    mtrace_buf[h].val = val;
    mtrace_head = (h + 1) & MTRACE_MASK;
    restore_interrupts(save);
}

/* Consommateur : UNIQUEMENT depuis loop() (contexte principal). Bloquant
 * (printf sur UART) : ne jamais l'appeler en IRQ ni dans un chemin chaud. */
static inline void mtrace_flush(void)
{
    while (mtrace_tail != mtrace_head)
    {
        /* Lecture champ par champ : en C++ la copie d'une struct volatile
         * entiere n'est pas permise (le constructeur de copie n'accepte pas
         * un operande volatile). */
        char tag = mtrace_buf[mtrace_tail].tag;
        uint32_t val = mtrace_buf[mtrace_tail].val;
        printf("MODEM %c %lu\r\n", tag, (unsigned long)val);
        mtrace_tail = (mtrace_tail + 1) & MTRACE_MASK;
    }
}

/* Trace d'une chaine (commande AT). Contexte principal uniquement (cold path).
 * Vide d'abord les evenements en attente pour rester chronologique. */
static inline void mtrace_str(char tag, const char *s)
{
    mtrace_flush();
    printf("MODEM %c %s\r\n", tag, s);
}

#else /* MODEM_TRACE == 0 */

#define mtrace(tag, val)   ((void)0)
#define mtrace_flush()     ((void)0)
#define mtrace_str(tag, s) ((void)0)

#endif /* MODEM_TRACE */

#endif /* _MODEM_TRACE_H_ */
