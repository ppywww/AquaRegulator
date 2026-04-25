#pragma once

#include <iostream>
#include <vector>

#include "modbus.h"

class ModbusTCP {
public:
    ModbusTCP(const char* ip_address, int port) {
        _ctx = modbus_new_tcp(ip_address, port);
        if (!_ctx) {
            throw std::runtime_error("Failed to create Modbus context");
        }
    }

    ~ModbusTCP() {
        if (_ctx) {
            modbus_close(_ctx);
            modbus_free(_ctx);
        }
    }

    void connect() {
        if (modbus_connect(_ctx) == -1) {
            throw std::runtime_error(std::string("Connection failed: ") + modbus_strerror(errno));
        }
    }

    int readRegisters(int addr, int nb, uint16_t* dest) {
        int rc = modbus_read_registers(_ctx, addr, nb, dest);
        if (rc == -1) {
            throw std::runtime_error(std::string("Read failed: ") + modbus_strerror(errno));
        }
        return rc;
    }

    void writeRegister(int reg_addr, const uint16_t value) {
        if (modbus_write_register(_ctx, reg_addr, value) == -1) {
            throw std::runtime_error(std::string("Write failed: ") + modbus_strerror(errno));
        }
    }

    void writeRegisters(int addr, int nb, const uint16_t* data) {
        if (modbus_write_registers(_ctx, addr, nb, data) == -1) {
            throw std::runtime_error(std::string("Write multiple registers failed: ") + modbus_strerror(errno));
        }
    }

private:
    modbus_t* _ctx = nullptr;
};