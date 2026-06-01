
/**
 * @file socket.cpp
 * @brief Реализация сокетного слоя W5500.
 */

#include "socket.h"

static uint16_t local_port = 49152U;

static uint16_t socket_min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

uint8_t socket(SOCKET socket, uint8_t protocol, uint16_t port, uint8_t flag)
{
    if ((socket >= MAX_SOCK_NUM)
        || ((protocol != SnMR::TCP) && (protocol != SnMR::UDP) && (protocol != SnMR::IPRAW) && (protocol != SnMR::MACRAW) && (protocol != SnMR::PPPOE)))
    {
        return 0U;
    }

    close(socket);

    w5500.writeSnMR(socket, static_cast<uint8_t>(protocol | flag));

    if (port == 0U)
    {
        ++local_port;

        if (local_port == 0U)
        {
            local_port = 49152U;
        }

        port = local_port;
    }

    w5500.writeSnPORT(socket, port);
    w5500.execCmdSn(socket, Sock_OPEN);

    return 1U;
}

void close(SOCKET socket)
{
    if (socket >= MAX_SOCK_NUM)
    {
        return;
    }

    w5500.execCmdSn(socket, Sock_CLOSE);
    w5500.writeSnIR(socket, 0xFFU);
}

uint8_t listen(SOCKET socket)
{
    if (socket >= MAX_SOCK_NUM)
    {
        return 0U;
    }

    if (w5500.readSnSR(socket) != SnSR::INIT)
    {
        return 0U;
    }

    w5500.execCmdSn(socket, Sock_LISTEN);

    return 1U;
}

uint8_t connect(SOCKET socket, uint8_t* address, uint16_t port)
{
    if ((socket >= MAX_SOCK_NUM) || (address == 0) || (port == 0U))
    {
        return 0U;
    }

    if (((address[0] == 0U) && (address[1] == 0U) && (address[2] == 0U) && (address[3] == 0U))
        || ((address[0] == 255U) && (address[1] == 255U) && (address[2] == 255U) && (address[3] == 255U)))
    {
        return 0U;
    }

    w5500.writeSnDIPR(socket, address);
    w5500.writeSnDPORT(socket, port);
    w5500.execCmdSn(socket, Sock_CONNECT);

    return 1U;
}

void disconnect(SOCKET socket)
{
    if (socket >= MAX_SOCK_NUM)
    {
        return;
    }

    w5500.execCmdSn(socket, Sock_DISCON);
}

uint16_t send(SOCKET socket, const uint8_t* buffer, uint16_t length)
{
    if ((socket >= MAX_SOCK_NUM) || (buffer == 0) || (length == 0U))
    {
        return 0U;
    }

    if (w5500.readSnSR(socket) != SnSR::ESTABLISHED)
    {
        return 0U;
    }

    uint16_t free_size = 0U;
    const uint32_t wait_started = millis();

    do
    {
        free_size = w5500.getTXFreeSize(socket);
    }
    while ((free_size < length) && ((uint32_t)(millis() - wait_started) < 200U));

    if (free_size < length)
    {
        length = free_size;
    }

    if (length == 0U)
    {
        return 0U;
    }

    w5500.send_data_processing(socket, buffer, length);
    w5500.execCmdSn(socket, Sock_SEND);

    const uint32_t send_started = millis();

    for (;;)
    {
        const uint8_t interrupt = w5500.readSnIR(socket);

        if ((interrupt & SnIR::SEND_OK) != 0U)
        {
            w5500.writeSnIR(socket, SnIR::SEND_OK);
            return length;
        }

        if ((interrupt & SnIR::TIMEOUT) != 0U)
        {
            w5500.writeSnIR(socket, SnIR::TIMEOUT);
            return 0U;
        }

        if ((uint32_t)(millis() - send_started) >= 200U)
        {
            return 0U;
        }
    }
}

int16_t recv(SOCKET socket, uint8_t* buffer, int16_t length)
{
    if ((socket >= MAX_SOCK_NUM) || (buffer == 0) || (length <= 0))
    {
        return 0;
    }

    uint16_t received = w5500.getRXReceivedSize(socket);

    if (received == 0U)
    {
        return 0;
    }

    const uint16_t read_length = socket_min_u16(static_cast<uint16_t>(length), received);

    w5500.recv_data_processing(socket, buffer, read_length, 0U);
    w5500.execCmdSn(socket, Sock_RECV);

    return static_cast<int16_t>(read_length);
}

uint16_t peek(SOCKET socket, uint8_t* buffer)
{
    if ((socket >= MAX_SOCK_NUM) || (buffer == 0))
    {
        return 0U;
    }

    if (w5500.getRXReceivedSize(socket) == 0U)
    {
        return 0U;
    }

    w5500.recv_data_processing(socket, buffer, 1U, 1U);

    return 1U;
}

uint16_t sendto(SOCKET socket, const uint8_t* buffer, uint16_t length, uint8_t* address, uint16_t port)
{
    if ((socket >= MAX_SOCK_NUM) || (buffer == 0) || (address == 0) || (port == 0U))
    {
        return 0U;
    }

    if (w5500.readSnSR(socket) != SnSR::UDP)
    {
        return 0U;
    }

    w5500.writeSnDIPR(socket, address);
    w5500.writeSnDPORT(socket, port);
    w5500.send_data_processing(socket, buffer, length);
    w5500.execCmdSn(socket, Sock_SEND);

    const uint32_t started = millis();

    for (;;)
    {
        const uint8_t interrupt = w5500.readSnIR(socket);

        if ((interrupt & SnIR::SEND_OK) != 0U)
        {
            w5500.writeSnIR(socket, SnIR::SEND_OK);
            return length;
        }

        if ((interrupt & SnIR::TIMEOUT) != 0U)
        {
            w5500.writeSnIR(socket, SnIR::TIMEOUT);
            return 0U;
        }

        if ((uint32_t)(millis() - started) >= 200U)
        {
            return 0U;
        }
    }
}

uint16_t recvfrom(SOCKET socket, uint8_t* buffer, uint16_t length, uint8_t* address, uint16_t* port)
{
    uint8_t header[8];

    if ((socket >= MAX_SOCK_NUM) || (buffer == 0))
    {
        return 0U;
    }

    if (w5500.readSnSR(socket) != SnSR::UDP)
    {
        return 0U;
    }

    if (w5500.getRXReceivedSize(socket) < sizeof(header))
    {
        return 0U;
    }

    w5500.recv_data_processing(socket, header, sizeof(header), 0U);

    if (address != 0)
    {
        address[0] = header[0];
        address[1] = header[1];
        address[2] = header[2];
        address[3] = header[3];
    }

    if (port != 0)
    {
        *port = static_cast<uint16_t>((static_cast<uint16_t>(header[4]) << 8) | header[5]);
    }

    const uint16_t packet_length = static_cast<uint16_t>((static_cast<uint16_t>(header[6]) << 8) | header[7]);
    const uint16_t read_length = socket_min_u16(packet_length, length);

    if (read_length > 0U)
    {
        w5500.recv_data_processing(socket, buffer, read_length, 0U);
    }

    if (packet_length > read_length)
    {
        uint8_t discard[16];
        uint16_t remaining = static_cast<uint16_t>(packet_length - read_length);

        while (remaining > 0U)
        {
            const uint16_t chunk = socket_min_u16(remaining, sizeof(discard));
            w5500.recv_data_processing(socket, discard, chunk, 0U);
            remaining = static_cast<uint16_t>(remaining - chunk);
        }
    }

    w5500.execCmdSn(socket, Sock_RECV);

    return read_length;
}

int startUDP(SOCKET socket, uint8_t* address, uint16_t port)
{
    if ((socket >= MAX_SOCK_NUM) || (address == 0) || (port == 0U))
    {
        return 0;
    }

    w5500.writeSnDIPR(socket, address);
    w5500.writeSnDPORT(socket, port);

    return 1;
}

uint16_t bufferData(SOCKET socket, uint16_t offset, const uint8_t* buffer, uint16_t length)
{
    if ((socket >= MAX_SOCK_NUM) || (buffer == 0))
    {
        return 0U;
    }

    w5500.send_data_processing_offset(socket, offset, buffer, length);

    return length;
}

int sendUDP(SOCKET socket)
{
    if (socket >= MAX_SOCK_NUM)
    {
        return 0;
    }

    w5500.execCmdSn(socket, Sock_SEND);

    return 1;
}

void flush(SOCKET socket)
{
    uint8_t discard[16];

    while (w5500.getRXReceivedSize(socket) > 0U)
    {
        (void)recv(socket, discard, sizeof(discard));
    }
}
