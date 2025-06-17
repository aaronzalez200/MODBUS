#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define ETH_MAX_BUF_SIZE	512

extern uint8_t blink_delay;

unsigned char ethBuf0[ETH_MAX_BUF_SIZE];
unsigned char ethBuf1[ETH_MAX_BUF_SIZE];
unsigned char ethBuf2[ETH_MAX_BUF_SIZE];

void dputstr(char *s);
void dputchar(char x);
char *appendpstr(char *p, PGM_P s);
char *appendhexbyte(char *p, char x);
char *appendhexint(char *p, unsigned int x);
void appendcrlf(char *p);
