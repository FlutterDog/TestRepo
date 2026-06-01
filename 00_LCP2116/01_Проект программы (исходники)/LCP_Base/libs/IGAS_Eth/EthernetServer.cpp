
/**
 * @file EthernetServer.cpp
 * @brief Реализация TCP-сервера W5500.
 */

#include "EthernetServer.h"
#include "IGAS_Eth.h"
#include "utility/socket.h"

static uint8_t last_socket = 0U;

EthernetServer::EthernetServer(uint16_t port)
    : port_(port)
{
}

void EthernetServer::begin(void)
{
    for (SOCKET socket_index = 0U; socket_index < MAX_SOCK_NUM; ++socket_index)
    {
        if (w5500.readSnSR(socket_index) == SnSR::CLOSED)
        {
            if (socket(socket_index, SnMR::TCP, port_, 0U) != 0U)
            {
                (void)listen(socket_index);
                EthernetClass::_server_port[socket_index] = port_;
            }

            return;
        }
    }
}

void EthernetServer::accept(void)
{
    uint8_t listening = 0U;

    for (SOCKET socket_index = 0U; socket_index < MAX_SOCK_NUM; ++socket_index)
    {
        if (EthernetClass::_server_port[socket_index] != port_)
        {
            continue;
        }

        const uint8_t state = w5500.readSnSR(socket_index);

        if (state == SnSR::LISTEN)
        {
            listening = 1U;
        }
        else if (state == SnSR::CLOSE_WAIT)
        {
            EthernetClient client(socket_index);
            client.stop();
        }
        else if (state == SnSR::CLOSED)
        {
            EthernetClass::_server_port[socket_index] = 0U;
        }
    }

    if (listening == 0U)
    {
        begin();
    }
}

EthernetClient EthernetServer::available(void)
{
    accept();

    for (uint8_t count = 0U; count < MAX_SOCK_NUM; ++count)
    {
        const SOCKET socket_index = last_socket;
        last_socket = static_cast<uint8_t>((last_socket + 1U) % MAX_SOCK_NUM);

        if (EthernetClass::_server_port[socket_index] != port_)
        {
            continue;
        }

        const uint8_t state = w5500.readSnSR(socket_index);

        if (((state == SnSR::ESTABLISHED) || (state == SnSR::CLOSE_WAIT))
            && (w5500.getRXReceivedSize(socket_index) > 0U))
        {
            return EthernetClient(socket_index);
        }
    }

    return EthernetClient();
}

size_t EthernetServer::write(uint8_t value)
{
    return write(&value, 1U);
}

size_t EthernetServer::write(const uint8_t* buffer, size_t size)
{
    size_t written = 0U;

    for (SOCKET socket_index = 0U; socket_index < MAX_SOCK_NUM; ++socket_index)
    {
        if (EthernetClass::_server_port[socket_index] != port_)
        {
            continue;
        }

        if (w5500.readSnSR(socket_index) == SnSR::ESTABLISHED)
        {
            EthernetClient client(socket_index);
            written += client.write(buffer, size);
        }
    }

    return written;
}
