#include <stdio.h>
#include <avr/io.h>
#include "string.h"
#include "loopback.h"
#include "socket.h"
#include "wizchip_conf.h"
#include "../../main.h"

#define SOCK_MODBUS     2
#define PORT_MODBUS   502

/* Request parsing:
 * 0x00 0x0d - transaction id (word)
 * 0x00 0x00 - protocol
 * 0x00 0x06 - length
 * 0x01      - unit id
 * 0x03      - function code
 * 0xNN 0xNN - start address
 * 0x00 0x02 - no. of registers to read
 * 
 * Response parsing:
 * 0x00 0x0d - transaction id (word) [same as request]
 * 0x00 0x00 - protocol              [same as request]
 * 0x00 0x06 - length                [same as request]
 * 0x01      - unit id               [same as request]
 * 0x03      - function code         [same as request]
 * 0xNN 0xNN - byte count of data    [unique to response]
 * 0xNN      - data bytes            [unique to response]
 */

/* Modbus Data Formatting... 
 * 
 * TCP/IP ADU [Request]: 
 * 1. Transaction ID (word)
 * 2. Protocol (word)
 * 3. Length (word)         --> bytes remaining
 * 4. Unit ID (byte)
 * 5. Func. Code (byte) |--Modbus PDU--|
 * 6. Start Addr (word) |--Modbus PDU--|
 * 7. No. of Reg (word) |--Modbus PDU--|
 * 
 * TCP/IP ADU [Response]: 
 * 1. Transaction ID (word)
 * 2. Protocol (word)
 * 3. Length (word)
 * 4. Unit ID (byte)
 * 5. Func. Code (byte) |--Modbus PDU--|
 * 6. Byte Count (byte) |--Modbus PDU--|
 * 7. Data (n bytes)    |--Modbus PDU--|
 */

// Create a Modbus TCP request (Function Code 03: Read Holding Registers)
uint8_t modbus_request[] = {
    0x00, 0x01,  // Transaction ID
    0x00, 0x00,  // Protocol ID (Always 0)
    0x00, 0x06,  // Length (6 bytes after this)
    0x01,        // Unit ID (Usually 1)
    0x03,        // Function Code: Read Holding Registers
    0x00, 0x00,  // Start Address (0x0000)
    0x00, 0x02   // Quantity of Registers (2)
};

void parse_request(int32_t length, uint8_t *buf, uint8_t *ip_addr) {
    // sorting modbus variables big endian
    // MBAP Header
    uint8_t modbus_transaction_id[2] = {0, 0};       // Transaction Identifier Hi[0] Lo[1] (2 bytes)      
    uint8_t modbus_protocol[2] = {0, 0};             // Protocol Identifier (2 bytes)
    uint8_t modbus_length[2] = {0, 0};               // Length (2 bytes)
    uint8_t modbus_unit_id[1] = {0};              // Unit Identifier (1 byte)

    // MODBUS request
    uint8_t modbus_function_code[1] = {0};        // Function code (1 byte)
    uint8_t modbus_start_address[2] = {0, 0};        // Starting address (2 bytes)
    uint8_t modbus_wildcard[2] = {0, 0};             // Quantity of Registers/Outputs/Inputs (2 bytes)

    // MODBUS error response
    uint8_t modbus_error[9]; // MBAP header (7) + function code (1) + code (1)

    uint8_t error1 = 0;
    uint8_t error2 = 0;
    uint8_t error3 = 0;
    uint8_t error4 = 0;


    printf("Parse (length): %d\n", (uint8_t)length);
    uint16_t i;
    if (length > 12) {
        printf("Error: Overflow\n");
        return;
    }
    for (i = 0; i < length; i++) {
        if (i == 0) {
            modbus_transaction_id[0] = buf[i];
            printf("Transaction ID: 0x%02x", buf[i]);
        }
        if (i == 1) {
            modbus_transaction_id[1] = buf[i];
            printf("%02x\n", buf[i]);
        }
        if (i == 2) {
            modbus_protocol[0] = buf[i];
            printf("Protocol: 0x%02x", buf[i]);
        }
        if (i == 3) {
            modbus_protocol[1] = buf[i];
            printf("%02x\n", buf[i]);
        }
        if (i == 4) {
            modbus_length[0] = buf[i];
            printf("Length: 0x%02x", buf[i]);
        }
        if (i == 5) {
            modbus_length[1] = buf[i];
            printf("%02x\n", buf[i]);
        }
        if (i == 6) {
            modbus_unit_id[0] = buf[i];
            printf("Unit ID: 0x%02x\n", buf[i]);
        }
        if (i == 7) {
            modbus_function_code[0] = buf[i];
            printf("Function code: 0x%02x\n", buf[i]);
        }
        if (i == 8) {
            modbus_start_address[0] = buf[i];
            printf("Address: 0x%02x", buf[i]);
        }
        if (i == 9) {
            modbus_start_address[1] = buf[i];
            printf("%02x\n", buf[i]);
        }
        if (i == 10) {
            modbus_wildcard[0] = buf[i];
            printf("wildcard value: 0x%02x", buf[i]);
        }
        if (i == 11) {
            modbus_wildcard[1] = buf[i];
            printf("%02x\n", buf[i]);
        }
        if (i > 11 ){
            printf("ERROR: Overflow\n");
        }
    }


    uint16_t number_of_coils = 0; 
    uint16_t number_of_bytes = 0;   
    uint8_t remainder = 0; 
    uint8_t define_size = 0;

    /* For now handle function codes
    0x01 - Read Coils
    0x02 - Read Discrete Inputs
    0x03 - Read Holding Registers
    0x04 - Read Input Register
    0x05 - Write Single Coil

    Below we are predefining the total size of the MODBUS response. By default we will begin
    with a size of 7 from the MBAP header: 
    [2] transaction id
    [2] protocol
    [2] length
    [1] unit id
    2 + 2 + 2 + 1 = 7 
    */
    if (modbus_function_code[0] == 0x01) {
        // determine define size: MBAP header (7) + function code (1) + byte count (1) + N
        // N can be found with reading modbus_wildcard. 
        // Here modbus_wildcard represents the number of bits (coils) we want to read. 1 coil is
        // equal to 1 bit. So if we call to read 8 coils then we need N to be 1. If we want to read
        // 15 coils then we need 1 byte and we have 7 more coils remaining which don't fill a full byte 
        // now we add 1 into our N value to represent another byte and pad the unused bits with 0. 
        number_of_coils = modbus_wildcard[1] | (modbus_wildcard[0] << 8);
        if (!(number_of_coils >= 0x01 || number_of_coils <= 0x17)) {
            error3 = 1;
        }
        number_of_bytes = number_of_coils / 8;
        remainder = number_of_coils % 8;
        if (remainder > 0) {
            number_of_bytes += 1;   // need another byte for the partial bit(s)
        }
        if (!(modbus_start_address[1] + number_of_bytes <= 0x17)) {
            error2 = 1;
        }
        define_size = 7 + 2 + number_of_bytes;
    } else if (modbus_function_code[0] == 0x02) {
        // determine define size: MBAP header (7) + function code (1) + byte count (1) + N
        // N can be found with reading modbus_wildcard.  
        number_of_coils = modbus_wildcard[1] | (modbus_wildcard[0] << 8);
        if (!(number_of_coils >= 0x01 || number_of_coils <= 0x04)) {
            error3 = 1;
        }
        number_of_bytes = number_of_coils / 8;
        remainder = number_of_coils % 8;
        if (remainder > 0) {
            number_of_bytes += 1;   // need another byte for the partial bit(s)
        }
        if (!(modbus_start_address[1] + number_of_bytes <= 0x80)) {
            error2 = 1;
        }
        define_size = 7 + 2 + number_of_bytes;  // MBAP header (7) + function code (1) + byte count (1) + number of bytes (N)
    } else if (modbus_function_code[0] == 0x03) {
        // determine define size: MBAP header (7) + function code (1) + byte count (1) + N
        // In this case the value of modbus_wildcard represents N number of registers which is
        // N * 2. We multiply by 2 as we use two bytes to represent a full register.
        number_of_coils = modbus_wildcard[1] | (modbus_wildcard[0] << 8);   // 1 to 125 (0x7D)
        if (!(number_of_coils >= 0x01 || number_of_coils <= 0x09)) {
            error3 = 1;
        }
        number_of_bytes = number_of_coils * 2;  // our request will represent the register (16 bits) using 2 bytes
        if (!(modbus_start_address[1] + number_of_bytes <= 0x09)) {
            error2 = 1;
        }
        define_size = 7 + 2 + number_of_bytes;  // MBAP header (7) + function code (1) + byte count (1) + number of bytes (N)
    } else if (modbus_function_code[0] == 0x04) {
        // determine define size: MBAP header (7) + function code (1) + byte count (1) + N
        // N can be found with reading modbus_wildcard. 
        // In this case the value of modbus_wildcard represents N number of registers which is
        // N * 2. We multiply by 2 as we use two bytes to represent a full register.
        number_of_coils = modbus_wildcard[1] | (modbus_wildcard[0] << 8);   // 1 to 125 (0x7D)
        if (!(number_of_coils >= 0x01 || number_of_coils <= 0x09)) {
            error3 = 1;
        }
        number_of_bytes = number_of_coils * 2;  // our request will represent the register (16 bits) using 2 bytes
        if (!(modbus_start_address[1] + number_of_bytes <= 0x09)) {
            error2 = 1;
        }
        define_size = 7 + 2 + number_of_bytes;  
    } else if (modbus_function_code[0] == 0x05) {
        if (!(modbus_wildcard[0] == 0xFF || modbus_wildcard[0] == 0x00)) {
            error3 = 1;
        }
        if (!(modbus_start_address[1] == 0x00 || modbus_start_address[1] == 0x03)) {
            error2 = 1;
        }
        define_size = 12;
        printf("Size of modbus_response: %d\n", define_size);
    } else {
        define_size = 7 + 2;    // MBAP header + function code (1) + error code (1)
        error1 = 1;
    }

    uint8_t modbus_response[define_size];

    /* Now update modbus_response starting with the MBAP header 
        MBAP Header: 
        modbus_response[0-1] - transaction id 
        modbus_response[2-3] - modbus protocol
        modbus_response[4-5] - length (number of bytes left) = unit id (1) + function code (1) + N (depending on function code) 
    */
    // We also have to create the MODBUS response (PDU)

    // start out w/ transaction id
    memcpy(&modbus_response[0], modbus_transaction_id, sizeof(modbus_transaction_id));
    // now get modbus protocol; 0x00
    memcpy(&modbus_response[2], modbus_protocol, sizeof(modbus_protocol));
    // unit ID
    modbus_response[6] = modbus_unit_id[0];
    // function code
    modbus_response[7] = modbus_function_code[0];

    if (error1 || error2 || error3) {
        memcpy(&modbus_error[0], modbus_transaction_id, sizeof(modbus_transaction_id));
        memcpy(&modbus_error[2], modbus_protocol, sizeof(modbus_protocol));
        modbus_error[6] = modbus_unit_id[0];
        modbus_error[7] = modbus_function_code[0] + 0x80;
        modbus_error[8] = 1;

        // now send error...
        printf("modbus response (error msg): ");
        for (i = 0; i < sizeof(modbus_error); i++) {
            printf("%02x ", modbus_error[i]);
        }
        printf("\n-----\n");

        int32_t sent_bytes = send(SOCK_MODBUS, modbus_error, sizeof(modbus_error));
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        }
        return;
    }

    /* Now determine the length & send the data */
    // length @ modbus_response[4-5]
    if (modbus_function_code[0] == 0x01) {
        modbus_response[4] = 0; // length HI byte
        // length = unit id (1) + func. code (1) + byte count (1) + N 
        modbus_response[5] = 1 + 1 + 1 + number_of_bytes; // length LO byte
        
        uint8_t i;
        modbus_response[8] = number_of_bytes;   // byte count 
        for (i = 0; i < number_of_bytes; i++) {
            // start at the given address
            modbus_response[9 + i] = coils[modbus_start_address[1] + i];    // just assume low byte only for addr
            if (i == (number_of_bytes - 1)) {
                // now pad the zeroes if we have any remainders... 
                if (remainder > 0) {
                    // perform mask of partial bits and pad rest with 0s
                    uint8_t bitmask;
                    bitmask = (1 << remainder) - 1;
                    modbus_response[9 + i] = coils[modbus_start_address[1] + i] & bitmask;
                }
            }
        } 
    } else if (modbus_function_code[0] == 0x02) {
        modbus_response[4] = 0; // length HI byte
        // length = unit id (1) + func. code (1) + byte count (1) + N 
        modbus_response[5] = 1 + 1 + 1 + number_of_bytes; // length LO byte

        uint8_t i;
        modbus_response[8] = number_of_bytes;   // byte count 
        for (i = 0; i < number_of_bytes; i++) {
            // start at the given address
            modbus_response[9 + i] = inputs[modbus_start_address[1] + i];    // just assume low byte only for addr
            if (i == (number_of_bytes - 1)) {
                // now pad the zeroes if we have any remainders... 
                if (remainder > 0) {
                    // perform mask of partial bits and pad rest with 0s
                    uint8_t bitmask;
                    bitmask = (1 << remainder) - 1;
                    modbus_response[9 + i] = inputs[modbus_start_address[1] + i] & bitmask;
                }
            }
        } 
    } else if (modbus_function_code[0] == 0x03) {
        modbus_response[4] = 0; // length HI byte
        // length = unit id (1) + func. code (1) + byte count (1) + N 
        modbus_response[5] = 1 + 1 + 1 + number_of_bytes; // length LO byte

        uint8_t i;
        modbus_response[8] = number_of_bytes;   // byte count 
        for (i = 0; i < number_of_bytes; i++) {
            // start at the given address
            // HI byte 1st then LO byte 2nd
            modbus_response[9 + (i * 2)] = (holding_register[modbus_start_address[1] + i]) >> 8;
            modbus_response[9 + (i * 2) + 1] = (holding_register[modbus_start_address[1] + i]) & 0xFF;
        } 
    } else if (modbus_function_code[0] == 0x04) {
        modbus_response[4] = 0; // length HI byte
        // length = unit id (1) + func. code (1) + byte count (1) + N 
        modbus_response[5] = 1 + 1 + number_of_bytes; // length LO byte
        uint8_t i;
        modbus_response[8] = number_of_bytes;   // byte count
        for (i = 0; i < number_of_bytes; i++) {
            // start at the given address
            // HI byte 1st then LO byte 2nd
            modbus_response[9 + (i * 2)] = (input_register[modbus_start_address[1] + i]) >> 8;
            modbus_response[9 + (i * 2) + 1] = (input_register[modbus_start_address[1] + i]) & 0xFF;
        } 
    } else if (modbus_function_code[0] == 0x05) {
        modbus_response[4] = 0; // length HI byte
        // length = unit id (1) + func. code (1) + output addr (2) + output value (2) 
        modbus_response[5] = 1 + 1 + 2 + 2; // length LO byte
        // coil address
        memcpy(&modbus_response[8], modbus_start_address, sizeof(modbus_start_address));
        // coil value
        memcpy(&modbus_response[10], modbus_wildcard, sizeof(modbus_wildcard));

        if (modbus_start_address[1] == 0) {
            if (modbus_wildcard[0] == 0xFF) {
                PORTH |= 0x20;
            } else if (modbus_wildcard[0] == 0x00) {
                PORTH &= ~0x20;
            }
        }
        if (modbus_start_address[1] == 3) {
            if (modbus_wildcard[0] == 0xFF) {
                PORTH |= 0x01;
            } else if (modbus_wildcard[0] == 0x00) {
                PORTH &= ~0x01;
            }
        }
    }

    // now send data...
    if (modbus_function_code[0] >= 0x01 || modbus_function_code[0] <= 0x05) {
         printf("modbus response: ");
        for (i = 0; i < sizeof(modbus_response); i++) {
            printf("%02x ", modbus_response[i]);
        }
        printf("\n-----\n");

        int32_t sent_bytes = send(SOCK_MODBUS, modbus_response, sizeof(modbus_response));
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        }
    }

    #if(0)
    if (modbus_function_code[0] == 0x03) {
        // read holding registers | function code
        modbus_response[7] = modbus_function_code[0];
        // byte count
        modbus_response[8] = 0x04;
        // data section
        uint8_t our_data[4] = {0x00, 0xA2, 0xAA};
        our_data[3] = 0x55;
        memcpy(&modbus_response[9], our_data, sizeof(our_data));
        // length finalized 
        uint16_t length = sizeof(our_data) + 3;     // uint id(byte) + func. code (byte) + byte count (word)
        modbus_response[4] = (length & 0xFF00) >> 8;
        modbus_response[5] = length & 0x00FF;
        uint8_t i;
        printf("modbus response: ");
        for (i = 0; i < sizeof(modbus_response); i++) {
            printf("%02x ", modbus_response[i]);
        }
        printf("\n----\n");

        if (modbus_start_address[1] == 0x05) {
            uint8_t modbus_response_error[9] = {
                0x00, 0xff, // transaction id
                0x00, 0x00, // protocol 
                0x00, 0x03,  // length
                0xff,       // unit id
                0x83,       // function code
                0xff,       // error code
            };
            printf("Transaction_id[0]: %d\n", buf[0]);
            printf("Transaction_id[1]: %d\n", buf[1]);
            // start out with transaction ID
            memcpy(&modbus_response_error[0], buf, 2);
            modbus_response_error[6] = modbus_unit_id[0];
            modbus_response_error[8] = 0x02;
            int32_t sent_bytes = send(SOCK_MODBUS, modbus_response_error, sizeof(modbus_response_error));
            if (sent_bytes > 0) {
                printf("Sent %lo bytes\n", sent_bytes);
            } else {
                printf("send() error: %lo\n", sent_bytes);
            }
            return;
        }

        int32_t sent_bytes = send(SOCK_MODBUS, modbus_response, sizeof(modbus_response));
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        } else {
            printf("send() error: %lo\n", sent_bytes);
        }
        printf("Finished sending\n--------\n");
    }
    #endif

    #if(0)
    uint8_t modbus_response[256];   // max 256 for now 

    // before we see what kind of response we're sending we might as well prepare the MBAP section
    memcpy(&modbus_response[0], modbus_transaction_id, sizeof(modbus_transaction_id));  // transaction ID [2 bytes]
    memcpy(&modbus_response[2], modbus_protocol, sizeof(modbus_protocol));              // modbus protocol [2 bytes]
    modbus_response[6] = modbus_unit_id[0];     // unit ID [1 byte]

    if (modbus_function_code[0] == 0x01) {
        // determine the value of length... = unit id [1] + function code [1] + byte count [1] + coil status [N]
        // N can be found with reading modbus_wildcard. 
        uint16_t number_of_coils; 
        uint16_t number_of_bytes;   
        uint8_t remainder; 
        number_of_coils = modbus_wildcard[1] | (modbus_wildcard[0] << 8);
        number_of_bytes = number_of_coils / 8;
        remainder = number_of_coils % 8;
        if (remainder > 0) {
            number_of_bytes += 1;
        }
        number_of_bytes += 3;   // lastly, add 3 for unit code, function code, and byte count 
        // Length HIGH byte
        modbus_response[4] = (number_of_bytes >> 8) & 0xFF; 
        // Length LOW byte
        modbus_response[5] = number_of_bytes & 0xFF;

        number_of_bytes -= 3;

        // function code
        modbus_response[7] = 0x01;
        // byte count 
        modbus_response[8] = number_of_bytes;
        // output status; make sure we get the start address
        // we can assume that we won't use the high byte portion so just check for the low byte
        uint8_t read_coil_address; 
        read_coil_address = modbus_start_address[1];
        uint8_t i = 0; 
        for (i = read_coil_address; i < number_of_bytes; i++) {
            modbus_response[9 + i] = coils[read_coil_address + i];
        }
        // all data is now prepared and is ready to be sent as a response
        int32_t sent_bytes = send(SOCK_MODBUS, modbus_response, 9 + number_of_bytes); // MBPA header + function code + byte count + N
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        } else {
            printf("send() error: %lo\n", sent_bytes);
        }
        printf("Response sent total bytes: %d\n", 9 + number_of_bytes);
        uint8_t a = 0;
        for (a = 0; a < 9 + number_of_bytes; a++) {
            if (a == 0) {
                printf("Transaction ID: %02x ", modbus_response[a]);
            }
            if (a == 1) {
                printf("%02x\n", modbus_response[a]);
            }
            if (a == 2) {
                printf("Protocol: %02x", modbus_response[a]);
            }
            if (a == 3) {
                printf("%02x\n", modbus_response[a]);
            }
            if (a == 4) {
                printf("Length: %02x", modbus_response[a]);
            }
            if (a == 5) {
                printf("%02x\n", modbus_response[a]);
            }
            if (a == 6) {
                printf("Unit ID: %02x\n", modbus_response[a]);
            }
            if (a == 7) {
                printf("Function Code: %02x\n", modbus_response[a]);
            }
            if (a == 8) {
                printf("Byte Count: %02x\n", modbus_response[a]);
                printf("Output Status: ");
            }
            if (a > 8) {
                printf("%02x ", modbus_response[a]);
            }
        }
        printf(" | end of response\n\n");
        for (a = 0; a < 256; a++) {
            modbus_response[a] = 0;
        }
        return;
    } 
    
    if (modbus_function_code[0] == 0x05) {
        // length here
        modbus_response[4] = 0; // Length MSB
        modbus_response[5] = 6; // Length LSB: Unit ID [1 byte] + Function Code [1 byte] + Output Address [2 bytes] + Output Value [2 bytes]
        // write single coil | function code
        modbus_response[7] = modbus_function_code[0];
        // coil address
        memcpy(&modbus_response[8], modbus_start_address, sizeof(modbus_start_address));
        // coil value
        memcpy(&modbus_response[10], modbus_wildcard, sizeof(modbus_wildcard));
        uint8_t i;
        printf("modbus response: ");
        for (i = 0; i < 12; i++) {
            printf("%02x ", modbus_response[i]);
        }
        printf("\n-----\n");

        int32_t sent_bytes = send(SOCK_MODBUS, modbus_response, 12);
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        } else {
            printf("send() error: %lo\n", sent_bytes);
        }

        if (modbus_wildcard[0] == 0xFF && modbus_function_code[0] == 0x05) {
            PORTH |= 0x20;
        } else {
            PORTH &= ~0x20;
        }
        printf("Got 0x05 function code\n");
        uint8_t a;
        for (a = 0; a < 256; a++) {
            modbus_response[a] = 0;
        }
        return;
    }

    if (modbus_function_code[0] == 0x03) {
        // read holding registers | function code
        modbus_response[7] = modbus_function_code[0];
        // byte count
        modbus_response[8] = 0x04;
        // data section
        uint8_t our_data[4] = {0x00, 0xA2, 0x00};
        our_data[3] = blink_delay;
        memcpy(&modbus_response[9], our_data, sizeof(our_data));
        // length finalized
        uint16_t length = sizeof(our_data) + 3; // unit id(byte) + func. code (byte) + byte count (word)        
        modbus_response[4] = (length & 0xFF00) >> 8;
        modbus_response[5] = length & 0x00FF;
        uint8_t i;
        printf("modbus response: ");
        for (i = 0; i < sizeof(modbus_response); i++) {
            printf("%02x ", modbus_response[i]);
        }
        printf("\n-----\n");

        if (modbus_start_address[1] == 0x05) {
            uint8_t modbus_response_error[9] = { 
                0x00, 0xff, // transaction id
                0x00, 0x00, // protocol
                0x00, 0x03, // length
                0xff,       // unit id
                0x83,       // func. code
                0xff,       // error code
            };
            printf("Transaction_id[0]: %d\n", buf[0]);
            printf("Transaction_id[1]: %d\n", buf[1]);
            // start out with transaction ID
            memcpy(&modbus_response_error[0], buf, 2);
            modbus_response_error[6] = modbus_unit_id[0];
            modbus_response_error[8] = 0x02;
            int32_t sent_bytes = send(SOCK_MODBUS, modbus_response_error, sizeof(modbus_response_error));
            if (sent_bytes > 0) {
                printf("Sent %lo bytes\n", sent_bytes);
            } else {
                printf("send() error: %lo\n", sent_bytes);
            }
            return;
        }

        int32_t sent_bytes = send(SOCK_MODBUS, modbus_response, 9);
        if (sent_bytes > 0) {
            printf("Sent %lo bytes\n", sent_bytes);
        } else {
            printf("send() error: %lo\n", sent_bytes);
        }
        uint8_t a;
        for (a = 0; a < 256; a++) {
            modbus_response[a] = 0;
        }
    }
    printf("Finished sending\n------------------------\n");
    #endif
}


int32_t loopback_modbus(uint8_t sn, uint8_t* buf, uint16_t port, int8_t *ip_addr)
{
   int32_t ret;
   uint16_t size = 0, sentsize=0;
   uint8_t destip[4];
   uint16_t destport;
   uint8_t i;

   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         if(getSn_IR(sn) & Sn_IR_CON)
         {
			getSn_DIPR(sn, destip);
			destport = getSn_DPORT(sn);
			printf("%d:Connected - %d.%d.%d.%d : %u\r\n",sn, destip[0], destip[1], destip[2], destip[3], destport);
			setSn_IR(sn,Sn_IR_CON);
         }
		 if((size = getSn_RX_RSR(sn)) > 0) // Don't need to check SOCKERR_BUSY because it doesn't not occur.
         {
			if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
			ret = recv(sn, buf, size);
            printf("ret size: %d\n", (uint8_t)ret);

			if(ret <= 0) return ret;      // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
			size = (uint16_t) ret;
			sentsize = 0;

            #if(0)
			while(size != sentsize)
			{
                // no need to echo it back...
				ret = send(sn, buf+sentsize, size-sentsize);
				if(ret < 0)
				{
					close(sn);
					return ret;
				}
				sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
			}
            #endif
        printf("==============\nRequest received! (MODBUS): ");
        for(i = 0; i < ret; i++) {
            printf("0x%02x ", buf[i]);
            ethBuf2[i] = buf[i];
        }
        printf("\n==============\n\n");
        parse_request(ret, ethBuf2, ip_addr);
        return 10;
        }
        break;
      case SOCK_CLOSE_WAIT :
        printf("%d:CloseWait\r\n",sn);
        if((ret = disconnect(sn)) != SOCK_OK) return ret;
            printf("%d:Socket Closed\r\n", sn);
        break;
    case SOCK_INIT :
        printf("%d:Listen, MODBUS server loopback, port [%d]\r\n", sn, port);
        if( (ret = listen(sn)) != SOCK_OK) return ret;
        break;
    case SOCK_CLOSED:
        printf("%d:MODBUS server loopback start\r\n",sn);

        if((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) return ret;
        printf("%d:Socket opened\r\n",sn);
        break;
    default:
        break;
    }
   return 1;
}

void test_it(void) {
    printf("Got it!\n");
}