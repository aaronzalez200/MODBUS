#ifndef _MODBUS_H_
#define _MODBUS_H_

void send_modbus_request(uint8_t sn, uint8_t* buf, uint16_t port, uint8_t *ip_addr);
void send_tcp_request(uint8_t sn, uint8_t* buf, uint16_t port, int8_t *ip_addr);
void test_it(void);
int32_t loopback_modbus(uint8_t sn, uint8_t* buf, uint16_t port, int8_t *ip_addr);

#endif