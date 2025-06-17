#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <string.h>

#include "ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "ioLibrary_Driver/Application/loopback/loopback.h"
#include "ioLibrary_Driver/Application/modbus/modbus.h"

#define BIT0POS 0x01
#define BIT0NEG 0xFE
#define BIT1POS 0x02
#define BIT1NEG 0xFD
#define BIT2POS 0x04
#define BIT2NEG 0xFB
#define BIT3POS 0x08
#define BIT3NEG 0xF7
#define BIT4POS 0x10
#define BIT4NEG 0xEF
#define BIT5POS 0x20
#define BIT5NEG 0xDF
#define BIT6POS 0x40
#define BIT6NEG 0xBF
#define BIT7POS 0x80
#define BIT7NEG 0x7F

#define MOSI  PB2   
#define SCK   PB1   
#define SS    PB0
#define RST   PC4

/*
    Interrupt Pin - D37 --> [PC0] (INPUT)
    RST Pin ------- D33 --> [PC4] (OUTPUT)

    SPI Interface: 
    MOSI ---------- D51 --> [PB2]
    SCLK ---------- D52 --> [PB1]
    MISO ---------- D50 --> [PB3]
    SCN(ss) ------- D53 --> [PB0]
 */

static int uart_putchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

// printing to UART
static int uart_putchar(char c, FILE *stream);
void baud_setup(void);

// IO & SPI
void io_setup(void);
void spi_setup(void);

/*
DDRn  [1 = output]
DDRn  [0 =  input]
PORTn [1 = output HIGH; input with pull-up enabled]
PORTn [0 = output  LOW; input with pull-up disabled]
PINn  [0 if input is LOW; 1 if input is HIGH]
*/

unsigned char blink_slow[] = "slow";
unsigned char blink_default[] = "default";
unsigned char blink_fast[] = "fast";

unsigned char led_on[] = "ON";
unsigned char led_off[] = "OFF";

uint8_t blink_delay = 0x00;
uint8_t blink_counter = 0;

/////////////////////////////////////////
// SOCKET NUMBER DEFINION for Examples //
/////////////////////////////////////////
#define SOCK_TCPS       0
#define SOCK_UDPS       1
#define SOCK_MODBUS     2
#define PORT_TCPS		5000
#define PORT_UDPS       3000


////////////////////////////////////////////////
// Shared Buffer Definition for LOOPBACK TEST //
////////////////////////////////////////////////

uint8_t gDATABUF[DATA_BUF_SIZE];

///////////////////////////////////
// Default Network Configuration //
///////////////////////////////////
wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x00, 0xab, 0xcd},
                            .ip = {192, 168, 1, 90},
                            .sn = {255,255,255,0},
                            .gw = {192, 168, 1, 1},
                            .dns = {8,8,8,8},
                            .dhcp = NETINFO_STATIC };

// For TCP client loopback examples; destination network info
uint8_t destip[4] = {192, 168, 1, 248};
uint16_t destport = 5000;


//////////////////////////////////////////////////////////////////////////////////////////////
// Call back function for W5500 SPI - Theses used as parameter of reg_wizchip_xxx_cbfunc()  //
// Should be implemented by WIZCHIP users because host is dependent                         //
//////////////////////////////////////////////////////////////////////////////////////////////
void  wizchip_select(void);
void  wizchip_deselect(void);
void  wizchip_write(uint8_t data);
uint8_t wizchip_read(void);

uint8_t buttonHistory;
uint8_t buttonStable;

#define BUTTON_MASK     0x3F

uint8_t delay;

volatile uint8_t increment = 0;
volatile uint8_t timer_ticks = 0; // Variable to count the number of 1ms ticks
#define TICKS_S 100


void timer2_init(void) { // increments from 100 to 256 and sets the TOV2 flag, we clear it manually. Takes about 9.984mS to overflow. 
    TCCR2A = 0x00;  // normal operation
    TCCR2B = 0x07;  // prescaler of 1024
    TIMSK2 = 0x00;  // no interrupts
    TCNT2 = 100;
    // TIFR2 = 1; set to 1 when an overflow occurs, write a 1 to clear it
}

static int uart_putchar(char c, FILE *stream)
{
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

void baud_setup(void) {
    // 9600 buad setup
    UCSR0A = 2;     // normal speed
    UCSR0B = 0x18;  // recv, xmit enable, 8 data bits
    UCSR0C = 6;     // asynch, no parity, 1 stop 8 bits
    UBRR0H = 0;
    UBRR0L = 207;   // 9600 baud: Using 16MHz clock
}

void io_setup(void) {
    // PORTA - Not used
    DDRA = 0x00; 
    PORTA = 0xFF;   // Enable pull-ups on unused pins
    // PORTB
    DDRB = 0x00;    // SPI configured elsewhere
    PORTB = 0xEE;   // Enable pull-ups on unused pins
    // PORTC
    DDRC = 0x10;    // ---1 ---0
    PORTC = 0xEE;   // Enable pull-ups on unused pins
    // PORTD
    DDRD = 0x00;    // unused
    PORTD = 0xFF;   // Enable pull-ups on unused pins
    // PORTE
    DDRE = 0x10;    // ---0 ----
    PORTE = 0xEF;   // Enable pull-ups on unused pins
    // PORTF
    DDRF = 0x00;    // unused
    PORTF = 0xFF;   // Enable pull-ups on unused pins
    // PORTG
    DDRG = 0x00;    // unused
    PORTG = 0xFF;   // Enable pull-ups on unused pins
    // PORTH
    DDRH = 0xE0;    // 111- ----
    PORTH = 0x1F;   // Enable pull-ups on unused pins
    // PORTJ
    DDRJ = 0x00;    // unused
    PORTJ = 0xFF;   // Enable pull-ups on unused pins
    // PORTK
    DDRK = 0x00;    // unused
    PORTK = 0xFF;   // Enable pull-ups on unused pins
    // PORTL
    DDRL = 0x00;    // unused
    PORTL = 0xFF;   // Enable pull-ups on unused pins
}

void spi_setup(void) {
    // Set MOSI, SCK, and SS as output
    DDRB |= (1 << MOSI);
    DDRB |= (1 << SCK);
    DDRB |= (1 << SS);

    // SPI mode 0; MSB sent first; Clk is 16Mhz / 16
    SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);

    // SS high
    PORTB |= (1 << SS);

    // reset HIGH
    PORTC &= ~(1 << RST);  // Assert reset (LOW)
    _delay_ms(10);          // Hold reset for at least 10ms
    PORTC |= (1 << RST);    // Deassert reset (HIGH)
    _delay_ms(100);         // Wait for W5500 to stabilize
}

void wizchip_deselect(void) {
    PORTB |= (1<<SS);
}

void wizchip_select(void) {
    PORTB &= ~(1<<SS);
}

void wizchip_write(uint8_t data) {
    /* start transmission */
    SPDR = data;
    /* wait for transmission complete */
    while (!(SPSR & (1<<SPIF)));
}

void wizchip_burst_write(uint8_t *buffer, uint16_t length) {
    uint16_t i;
    for (i = 0; i < length; i++) {
        wizchip_write(*buffer++);
    }
}

uint8_t wizchip_read(void) {
    uint8_t read;
    SPDR = 0x00;
    while (!(SPSR & (1<<SPIF)));  // Wait for transmission complete
    read = SPDR;
    //dputstr("SPI Read-->");
    //char buffer[3]; 
    //sprintf(buffer, "%02x", read);
    //dputstr(buffer);
    //dputstr("<--END\r\n");
    return read;
}

void wizchip_burst_read(uint8_t *buffer, uint16_t length) {
    uint16_t i;
    for (i = 0; i < length; i++) {
        buffer[i] = wizchip_read();
    }
    // do something w/ buffer ? 
}

/////////////////////////////////////////////////////////////
// Initialize the network information to be used in WIZCHIP //
/////////////////////////////////////////////////////////////
//***************** WIZCHIP INIT: BEGIN
#define SOCK_TCPS           0
#define SOCK_UDPS           1
#define PORT_TCPS		    5000
#define PORT_UDPS           3000

#define ETH_MAX_BUF_SIZE	512

#define SOCK_MODBUS         2
#define PORT_MODBUS         502

wiz_NetInfo netInfo = { 
        .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // Mac address
		.ip   = {192, 168, 10, 199},        // IP address
		.sn   = {255, 255, 255, 0},         // Subnet mask
		.dns =  {8,8,8,8},			  // DNS address (google dns)
		.gw   = {192, 168, 10, 1},     // Gateway address
		.dhcp = NETINFO_STATIC,       //Static IP configuration
    };  

unsigned char ethBuf0[ETH_MAX_BUF_SIZE];
unsigned char ethBuf1[ETH_MAX_BUF_SIZE];
unsigned char ethBuf2[ETH_MAX_BUF_SIZE];


void IO_LIBRARY_Init(void) {
	uint8_t bufSize[] = {2, 2, 2, 2, 2, 2, 2, 2};

	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
	reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);
	reg_wizchip_spiburst_cbfunc(wizchip_burst_read, wizchip_burst_write);

	wizchip_init(bufSize, bufSize);
	wizchip_setnetinfo(&netInfo);
	//wizchip_setinterruptmask(IK_SOCK_0);
}
//***************** WIZCHIP INIT: END

void print_network_information(void)
{
    uint8_t tmpstr[6] = {0,};
	ctlwizchip(CW_GET_ID,(void*)tmpstr); // Get WIZCHIP name
    printf("\r\n=======================================\r\n");
    printf(" WIZnet chip:  %s \r\n", tmpstr);
    printf("=======================================\r\n");

	wiz_NetInfo gWIZNETINFO;
	wizchip_getnetinfo(&gWIZNETINFO);
    printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n\r",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
    printf("IP address : %d.%d.%d.%d\n\r",gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
    printf("SM Mask	   : %d.%d.%d.%d\n\r",gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
    printf("Gate way   : %d.%d.%d.%d\n\r",gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
    printf("DNS Server : %d.%d.%d.%d\n\r",gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
}

unsigned char USART_Receive( void ) {
    if (UCSR0A & (1 << RXC0)) { // Check if data is available
        return UDR0;
    }
    return 0; // No data available
}

unsigned char calculate_checksum(uint8_t *data, uint8_t len) {
    unsigned char checksum = 0;
    uint8_t i;
    for (i = 0; i < len; i++) {
        checksum ^= data[i];  // XOR each byte (simple checksum calculation)
    }
    return checksum;
}

void read_uart_buffer(void) {
    unsigned char receiveByte;
    uint8_t rxBuffer[5];
    uint8_t expected_bytes = 5;
    uint8_t byte_count = 0;

    while(byte_count < expected_bytes) {
        receiveByte = USART_Receive();
        if (receiveByte != 0) {
            rxBuffer[byte_count] = receiveByte;
            byte_count++;
        }
    }

    // Check the CRC (assuming the last byte is CRC)
    unsigned char calculated_crc = calculate_checksum(rxBuffer, expected_bytes - 1);  // Exclude CRC byte
    if (calculated_crc == rxBuffer[expected_bytes - 1]) {
        // CRC is correct, print the data
        printf("Received data with valid CRC:\n");
        uint8_t i;
        for (i = 0; i < expected_bytes; i++) {
            printf("0x%02x ", rxBuffer[i]);
        }
        printf("\n");
    } else {
        // CRC mismatch
        printf("Invalid CRC, discarding data.\n");
    }

    byte_count = 0;
}

void USART_Transmit( unsigned char data )
{
/* Wait for empty transmit buffer */
while (!( UCSR0A & (1<<UDRE0))) {
    // nothing
}
/* Put data into buffer, sends the data */
    UDR0 = data;
}

void request_rcu_status(void) {
    USART_Transmit(0x7A);   // send request byte to remote
    unsigned char firstByte;
    firstByte = USART_Receive();
    if (firstByte == 0xC5) {
        //printf("Got start bit!\n");
    } else {
        //printf("Didn't get start bit\n");
    }
}
    
void check_button_input(void) {
    uint8_t dummy;
    buttonHistory <<= 1;
    if ((PINH & (1<<4))) { buttonHistory++; }
    dummy = buttonHistory & BUTTON_MASK;
    if (dummy == 0) {
        buttonStable = 0;
    } else if (dummy == BUTTON_MASK) {
        if (!buttonStable) {
            // something
         }
        buttonStable = 1;    
    }
    if (buttonStable) {
        PORTH ^= 0x20;
    }
    //printf("Button History: 0x%02x\n", buttonHistory);
}

int main(void) {
    uint8_t phy_status;
    uint8_t previous_state;
    uint8_t monitor_tcps;
    uint8_t monitor_udps;

    int8_t getIP[4];
    memcpy(getIP, netInfo.ip, 4);  // Copy IP address
    
    previous_state = 0;
    baud_setup();
    io_setup();
    spi_setup();
    timer2_init();
    stdout = &mystdout;
    printf("Hello\n");
    /* wiznet section start */
    IO_LIBRARY_Init();
    print_network_information();
    /* wiznet section end */
    test_it();
    while(1) {
        /* Loopback Test: TCP Server and UDP */
		// Test for Ethernet data transfer validation
		{
            if (TIFR2 & (1 << TOV2)) {
                TIFR2 = 0x01;
                TCNT2 = 100;
                timer_ticks++;
            }
            //printf("Timer ticks: %d\n", timer_ticks);
            monitor_tcps = loopback_tcps(SOCK_TCPS,ethBuf0,PORT_TCPS);
		    monitor_udps = loopback_udps(SOCK_UDPS,ethBuf1,PORT_UDPS);
            loopback_modbus(SOCK_MODBUS,ethBuf2,PORT_MODBUS, getIP);
            if (monitor_tcps == 10) {
                printf("TCPS: %s\n", ethBuf0);
                if(strcmp((char *)ethBuf0, (char *)blink_slow) == 0) {
                    blink_delay = 30;
                    printf("delay --> slow\n");
                }
                if(strcmp((char *)ethBuf0, (char *)blink_default) == 0) {
                    blink_delay = 10;
                    printf("delay --> default\n");
                }
                if(strcmp((char *)ethBuf0, (char *)blink_fast) == 0) {
                    blink_delay = 4;
                    printf("delay --> fast\n");
                }
                uint16_t i;
                for (i = 0; i < ETH_MAX_BUF_SIZE; i++) {
                    ethBuf0[i] = 0; // clear buffer
                }
            }
            if (monitor_udps == 10) {
                printf("UPS: %s\n", ethBuf1);
                if(strcmp((char *)ethBuf1, (char *)led_on) == 0) {
                    PORTH |= 0x20;
                    printf("LED --> ON\n");
                }
                if(strcmp((char *)ethBuf1, (char *)led_off) == 0) {
                    PORTH &= ~(0x20);
                    printf("LED --> OFF\n");
                }
                uint16_t i;
                for (i = 0; i < ETH_MAX_BUF_SIZE; i++) {
                    ethBuf1[i] = 0; // clear buffer
                }
            }
            //printf("timer_ticks: %d\n", timer_ticks);
            if ((timer_ticks & 0x0F) == 5) { // every 50mS
                blink_counter++;    // increment every 50mS
                if ((blink_counter & 0x7F) == 40) { 
                    PORTH ^= 0x40;
                }
                #if(0)
                if ((blink_counter & 0x0F) == 2) { // enter every 100 mS
                    delay++;
                    if ((delay & 0x0F) == 1) { 
                        PORTH ^= 0x40;
                    }
                #endif
                    //send_modbus_request(SOCK_MODBUS, ethBuf2, PORT_MODBUS, getIP);
                    
                    /* Add remote RCU new here */
                    //request_rcu_status();
            }
            if ((timer_ticks & 0x02) == 0) {
                check_button_input();
            }
            if (timer_ticks & 0x40) {
                // Run your code here for every 100ms
                phy_status = wizphy_getphylink();
                if(phy_status == PHY_LINK_ON && !previous_state) {
                    printf("|PHY LINK ON|\n");
                    previous_state = 1;
                } else if (phy_status == PHY_LINK_OFF && previous_state){
                    printf("|PHY LINK OFF|\n");
                    previous_state = 0;
                }
            }
        }
    }
}