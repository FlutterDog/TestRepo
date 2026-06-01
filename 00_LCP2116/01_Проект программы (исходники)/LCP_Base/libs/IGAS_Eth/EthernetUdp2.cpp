
/**
 * @file EthernetUdp2.cpp
 * @brief Реализация UDP-интерфейса W5500.
 */

#include "EthernetUdp2.h"
#include "utility/socket.h"

#include <string.h>

EthernetUDP::EthernetUDP()
    : socket_(MAX_SOCK_NUM),
      local_port_(0U),
      remote_ip_(),
      remote_port_(0U),
      remaining_(0U),
      tx_length_(0U)
{
}

uint8_t EthernetUDP::begin(uint16_t port)
{
    if (socket_ != MAX_SOCK_NUM)
    {
        return 0U;
    }

    for (SOCKET socket_index = 0U; socket_index < MAX_SOCK_NUM; ++socket_index)
    {
        const uint8_t state = w5500.readSnSR(socket_index);

        if ((state == SnSR::CLOSED) || (state == SnSR::FIN_WAIT) || (state == SnSR::CLOSE_WAIT))
        {
            socket_ = socket_index;
            break;
        }
    }

    if (socket_ == MAX_SOCK_NUM)
    {
        return 0U;
    }

    local_port_ = port;

    return socket(socket_, SnMR::UDP, local_port_, 0U);
}

void EthernetUDP::stop(void)
{
    if (socket_ < MAX_SOCK_NUM)
    {
        close(socket_);
        socket_ = MAX_SOCK_NUM;
    }

    remaining_ = 0U;
    tx_length_ = 0U;
}

int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
    if ((socket_ == MAX_SOCK_NUM) || (port == 0U))
    {
        return 0;
    }

    remote_ip_ = ip;
    remote_port_ = port;
    tx_length_ = 0U;

    return 1;
}

int EthernetUDP::endPacket(void)
{
    if (socket_ == MAX_SOCK_NUM)
    {
        return 0;
    }

    const uint16_t sent = sendto(socket_, tx_buffer_, tx_length_, rawIPAddress(remote_ip_), remote_port_);

    tx_length_ = 0U;

    return (sent > 0U) ? 1 : 0;
}

size_t EthernetUDP::write(uint8_t value)
{
    return write(&value, 1U);
}

size_t EthernetUDP::write(const uint8_t* buffer, size_t size)
{
    if (buffer == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    while ((written < size) && (tx_length_ < UDP_TX_PACKET_MAX_SIZE))
    {
        tx_buffer_[tx_length_] = buffer[written];
        ++tx_length_;
        ++written;
    }

    return written;
}

int EthernetUDP::parsePacket(void)
{
    uint8_t header[8];

    if (socket_ == MAX_SOCK_NUM)
    {
        return 0;
    }

    if (w5500.getRXReceivedSize(socket_) < sizeof(header))
    {
        return 0;
    }

    w5500.recv_data_processing(socket_, header, sizeof(header), 0U);
    w5500.execCmdSn(socket_, Sock_RECV);

    uint8_t remote[4];
    remote[0] = header[0];
    remote[1] = header[1];
    remote[2] = header[2];
    remote[3] = header[3];

    remote_ip_ = IPAddress(remote);
    remote_port_ = static_cast<uint16_t>((static_cast<uint16_t>(header[4]) << 8) | header[5]);
    remaining_ = static_cast<uint16_t>((static_cast<uint16_t>(header[6]) << 8) | header[7]);

    return remaining_;
}

int EthernetUDP::available(void)
{
    return remaining_;
}

int EthernetUDP::read(void)
{
    uint8_t value;

    if (read(&value, 1U) <= 0)
    {
        return -1;
    }

    return value;
}

int EthernetUDP::read(unsigned char* buffer, size_t length)
{
    if ((socket_ == MAX_SOCK_NUM) || (buffer == 0) || (remaining_ == 0U))
    {
        return 0;
    }

    const uint16_t read_length = (length < remaining_) ? static_cast<uint16_t>(length) : remaining_;

    w5500.recv_data_processing(socket_, buffer, read_length, 0U);
    w5500.execCmdSn(socket_, Sock_RECV);

    remaining_ = static_cast<uint16_t>(remaining_ - read_length);

    return read_length;
}

int EthernetUDP::read(char* buffer, size_t length)
{
    return read(reinterpret_cast<unsigned char*>(buffer), length);
}

int EthernetUDP::peek(void)
{
    uint8_t value;

    if ((socket_ == MAX_SOCK_NUM) || (remaining_ == 0U))
    {
        return -1;
    }

    w5500.recv_data_processing(socket_, &value, 1U, 1U);

    return value;
}

void EthernetUDP::flush(void)
{
    uint8_t discard[16];

    while (remaining_ > 0U)
    {
        const uint16_t chunk = (remaining_ > sizeof(discard)) ? sizeof(discard) : remaining_;
        (void)read(discard, chunk);
    }
}

IPAddress EthernetUDP::remoteIP(void)
{
    return remote_ip_;
}

uint16_t EthernetUDP::remotePort(void)
{
    return remote_port_;
}
