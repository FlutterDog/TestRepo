
/**
 * @file EthernetClient.cpp
 * @brief Реализация TCP-клиента W5500.
 */

#include "EthernetClient.h"
#include "IGAS_Eth.h"
#include "utility/socket.h"

uint16_t EthernetClient::source_port_ = 49152U;

EthernetClient::EthernetClient()
    : socket_(MAX_SOCK_NUM)
{
}

EthernetClient::EthernetClient(uint8_t socket)
    : socket_(socket)
{
}

uint8_t EthernetClient::status(void)
{
    if (socket_ >= MAX_SOCK_NUM)
    {
        return SnSR::CLOSED;
    }

    return w5500.readSnSR(socket_);
}

int EthernetClient::connect(const char* host, uint16_t port)
{
    (void)host;
    (void)port;
    return 0;
}

int EthernetClient::connect(IPAddress ip, uint16_t port)
{
    if (socket_ != MAX_SOCK_NUM)
    {
        return 0;
    }

    for (uint8_t index = 0U; index < MAX_SOCK_NUM; ++index)
    {
        const uint8_t state = w5500.readSnSR(index);

        if ((state == SnSR::CLOSED) || (state == SnSR::FIN_WAIT) || (state == SnSR::CLOSE_WAIT))
        {
            socket_ = index;
            break;
        }
    }

    if (socket_ == MAX_SOCK_NUM)
    {
        return 0;
    }

    ++source_port_;

    if (source_port_ == 0U)
    {
        source_port_ = 49152U;
    }

    if (socket(socket_, SnMR::TCP, source_port_, 0U) == 0U)
    {
        socket_ = MAX_SOCK_NUM;
        return 0;
    }

    if (::connect(socket_, rawIPAddress(ip), port) == 0U)
    {
        socket_ = MAX_SOCK_NUM;
        return 0;
    }

    const uint32_t started = millis();

    while (status() != SnSR::ESTABLISHED)
    {
        if ((status() == SnSR::CLOSED) || ((uint32_t)(millis() - started) >= 300U))
        {
            socket_ = MAX_SOCK_NUM;
            return 0;
        }
    }

    return 1;
}

size_t EthernetClient::write(uint8_t value)
{
    return write(&value, 1U);
}

size_t EthernetClient::write(const uint8_t* buffer, size_t size)
{
    if ((socket_ >= MAX_SOCK_NUM) || (buffer == 0))
    {
        return 0U;
    }

    return send(socket_, buffer, static_cast<uint16_t>(size));
}

int EthernetClient::available(void)
{
    if (socket_ >= MAX_SOCK_NUM)
    {
        return 0;
    }

    return static_cast<int>(w5500.getRXReceivedSize(socket_));
}

int EthernetClient::read(void)
{
    uint8_t value;

    if (read(&value, 1U) <= 0)
    {
        return -1;
    }

    return value;
}

int EthernetClient::read(uint8_t* buffer, size_t size)
{
    if ((socket_ >= MAX_SOCK_NUM) || (buffer == 0))
    {
        return 0;
    }

    return recv(socket_, buffer, static_cast<int16_t>(size));
}

int EthernetClient::peek(void)
{
    uint8_t value;

    if ((socket_ >= MAX_SOCK_NUM) || (::peek(socket_, &value) == 0U))
    {
        return -1;
    }

    return value;
}

void EthernetClient::flush(void)
{
    if (socket_ < MAX_SOCK_NUM)
    {
        ::flush(socket_);
    }
}

void EthernetClient::stop(void)
{
    if (socket_ >= MAX_SOCK_NUM)
    {
        return;
    }

    disconnect(socket_);

    const uint32_t started = millis();

    while ((status() != SnSR::CLOSED) && ((uint32_t)(millis() - started) < 200U))
    {
    }

    close(socket_);
    EthernetClass::_server_port[socket_] = 0U;
    socket_ = MAX_SOCK_NUM;
}

uint8_t EthernetClient::connected(void)
{
    const uint8_t state = status();

    return ((state == SnSR::ESTABLISHED) || (state == SnSR::CLOSE_WAIT)) ? 1U : 0U;
}

EthernetClient::operator bool()
{
    return (socket_ < MAX_SOCK_NUM);
}

bool EthernetClient::operator==(const EthernetClient& client) const
{
    return socket_ == client.socket_;
}

bool EthernetClient::operator!=(const EthernetClient& client) const
{
    return !(*this == client);
}

uint8_t EthernetClient::getSocketNumber(void)
{
    return socket_;
}

IPAddress EthernetClient::remoteIP(void)
{
    uint8_t address[4] = { 0U, 0U, 0U, 0U };

    if (socket_ < MAX_SOCK_NUM)
    {
        w5500.readSnDIPR(socket_, address);
    }

    return IPAddress(address);
}

uint16_t EthernetClient::remotePort(void)
{
    if (socket_ >= MAX_SOCK_NUM)
    {
        return 0U;
    }

    return w5500.readSnDPORT(socket_);
}
