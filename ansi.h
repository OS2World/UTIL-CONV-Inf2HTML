/*
 * ansi.h:
 *      a few #define's for easy ANSI escape sequence
 *      access.
 *
 *      Part of ANSISCRN.H as contributed to the
 *      public domain 12-26-91 by Matthew J. Glass.
 */

#define   ESC                  27
#define   ANSI_cup(a,b)        printf("%c[%d;%dH",ESC,a,b)
#define   ANSI_up(a)           printf("%c[%dA",ESC,a)
#define   ANSI_down(a)         printf("%c[%dB",ESC,a)
#define   ANSI_right(a)        printf("%c[%dC",ESC,a)
#define   ANSI_left(a)         printf("%c[%dD",ESC,a)
#define   ANSI_locate(a,b)     printf("%c[%d;%df",ESC,a,b)
#define   ANSI_savecurs()      printf("%c[s",ESC)
#define   ANSI_restcurs()      printf("%c[u",ESC)
#define   ANSI_cls()           printf("%c[2J",ESC)
#define   ANSI_cleol()         printf("%c[K",ESC)
#define   ANSI_margins(a,b)    printf("%c[%d;%dr",ESC,a,b)


