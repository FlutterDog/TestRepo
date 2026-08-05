/**
 * @file fixture_firmware.hpp
 * @brief Первый исполняемый proof Lorentz Fixture Bridge: TCP 2101 <-> S1.
 *
 * Файл намеренно header-only и включается только из main.cpp. Это позволяет
 * проверить архитектуру на существующем Microchip Studio project без ручного
 * редактирования списка translation units. После аппаратного подтверждения S1
 * код будет разделён на обычные fixture service/HAL файлы.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../board/lcp_board.hpp"
#include "../../board/lcp_ethernet.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../board/lcp_rs485.hpp"
#include "../../hal/w5500_lite.hpp"
#include "../../platform/platform.hpp"
#include "../diagnostics/watchdog_status.hpp"
#include "../version.hpp"

extern "C"
{
#include "FreeRTOS.h"
#include "task.h"
}

namespace fixture_firmware
{
constexpr uint16_t S1_TCP_PORT = 2101U;
constexpr uint16_t BRIDGE_BUFFER_SIZE = 512U;
constexpr uint16_t FORWARD_CHUNK_SIZE = 128U;
constexpr uint16_t CONSOLE_BUFFER_SIZE = 64U;
constexpr uint16_t FIXTURE_TASK_STACK_WORDS = 2048U;
constexpr UBaseType_t FIXTURE_TASK_PRIORITY = 2U;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 500U;
constexpr uint8_t W5500_SN_SR_ESTABLISHED = 0x17U;

static_assert(BRIDGE_BUFFER_SIZE <= 0xFFFFU,
              "fixture ring index is uint16_t");

struct ByteRing
{
    uint8_t data[BRIDGE_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t used;
};

struct BridgeCounters
{
    uint32_t tcp_connections;
    uint32_t tcp_disconnects;
    uint64_t tcp_rx_bytes;
    uint64_t tcp_tx_bytes;
    uint64_t uart_rx_bytes;
    uint64_t uart_tx_bytes;
    uint32_t tcp_errors;
    uint32_t uart_write_errors;
    uint32_t tcp_to_uart_overflows;
    uint32_t uart_to_tcp_overflows;
    uint32_t last_activity_ms;
};

struct Runtime
{
    const ModbusRtuTransport* s1;
    ByteRing tcp_to_uart;
    ByteRing uart_to_tcp;
    BridgeCounters counters;
    uint8_t ethernet_initialized;
    uint8_t tcp_connected;
    uint8_t last_socket_status;
    uint8_t usb_was_open;
    char console_buffer[CONSOLE_BUFFER_SIZE];
    uint16_t console_length;
    uint32_t last_heartbeat_ms;
    uint8_t heartbeat_level;
};

Runtime g_runtime = {};
TaskHandle_t g_fixture_task_handle = nullptr;

const W5500NetworkConfig FIXTURE_NETWORK =
{
    { { 0x02U, 0x4CU, 0x46U, 0x58U, 0x00U, 0x01U } },
    { { 192U, 168U, 1U, 200U } },
    { { 192U, 168U, 1U, 254U } },
    { { 255U, 255U, 255U, 0U } }
};

void ring_clear(ByteRing& ring)
{
    ring.head = 0U;
    ring.tail = 0U;
    ring.used = 0U;
}

uint16_t ring_free(const ByteRing& ring)
{
    return static_cast<uint16_t>(BRIDGE_BUFFER_SIZE - ring.used);
}

uint16_t ring_push(ByteRing& ring, const uint8_t* data, uint16_t length)
{
    if (data == 0)
    {
        return 0U;
    }

    uint16_t pushed = 0U;
    while ((pushed < length) && (ring.used < BRIDGE_BUFFER_SIZE))
    {
        ring.data[ring.head] = data[pushed++];
        ring.head = static_cast<uint16_t>((ring.head + 1U) % BRIDGE_BUFFER_SIZE);
        ++ring.used;
    }
    return pushed;
}

uint16_t ring_peek(const ByteRing& ring, uint8_t* output, uint16_t capacity)
{
    if ((output == 0) || (capacity == 0U))
    {
        return 0U;
    }

    uint16_t index = ring.tail;
    uint16_t copied = 0U;
    while ((copied < capacity) && (copied < ring.used))
    {
        output[copied++] = ring.data[index];
        index = static_cast<uint16_t>((index + 1U) % BRIDGE_BUFFER_SIZE);
    }
    return copied;
}

void ring_drop(ByteRing& ring, uint16_t length)
{
    if (length > ring.used)
    {
        length = ring.used;
    }
    ring.tail = static_cast<uint16_t>((ring.tail + length) % BRIDGE_BUFFER_SIZE);
    ring.used = static_cast<uint16_t>(ring.used - length);
}

void print_ip(const W5500IpAddress& address)
{
    for (uint8_t index = 0U; index < 4U; ++index)
    {
        if (index != 0U)
        {
            SerialUSB.print('.');
        }
        SerialUSB.print(address.octet[index], DEC);
    }
}

void print_version(void)
{
    SerialUSB.print("name = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_NAME);
    SerialUSB.print("\r\nversion = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_VERSION);
    SerialUSB.print("\r\nstage = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_STAGE);
    SerialUSB.print("\r\ntarget = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_TARGET);
    SerialUSB.print("\r\nproduct = LCP2116-FIXTURE\r\n");
}

void print_status(void)
{
    SerialUSB.print("\r\n=== FIXTURE BRIDGE STATUS ===\r\n");
    print_version();
    SerialUSB.print("ethernet = ");
    SerialUSB.print(g_runtime.ethernet_initialized ? "ready" : "fault");
    SerialUSB.print("\r\nlink = ");
    SerialUSB.print(w5500_lite_link_up(LCP_ETHERNET_1) ? "up" : "down");
    SerialUSB.print("\r\nip = ");
    print_ip(FIXTURE_NETWORK.ip);
    SerialUSB.print("\r\nS1 present = ");
    SerialUSB.print(lcp_field_port_present(LCP_FIELD_PORT_S1) ? "yes" : "no");
    SerialUSB.print("\r\nS1 tcp port = ");
    SerialUSB.print(S1_TCP_PORT, DEC);
    SerialUSB.print("\r\nS1 tcp state = ");
    SerialUSB.print(g_runtime.tcp_connected ? "CONNECTED" : "LISTENING");
    SerialUSB.print("\r\nS1 socket status = 0x");
    SerialUSB.print(g_runtime.last_socket_status, HEX);
    SerialUSB.print("\r\ntcp connections = ");
    SerialUSB.print(g_runtime.counters.tcp_connections, DEC);
    SerialUSB.print("\r\ntcp disconnects = ");
    SerialUSB.print(g_runtime.counters.tcp_disconnects, DEC);
    SerialUSB.print("\r\ntcp rx bytes = ");
    SerialUSB.print(static_cast<uint32_t>(g_runtime.counters.tcp_rx_bytes), DEC);
    SerialUSB.print("\r\ntcp tx bytes = ");
    SerialUSB.print(static_cast<uint32_t>(g_runtime.counters.tcp_tx_bytes), DEC);
    SerialUSB.print("\r\nuart rx bytes = ");
    SerialUSB.print(static_cast<uint32_t>(g_runtime.counters.uart_rx_bytes), DEC);
    SerialUSB.print("\r\nuart tx bytes = ");
    SerialUSB.print(static_cast<uint32_t>(g_runtime.counters.uart_tx_bytes), DEC);
    SerialUSB.print("\r\ntcp errors = ");
    SerialUSB.print(g_runtime.counters.tcp_errors, DEC);
    SerialUSB.print("\r\nuart write errors = ");
    SerialUSB.print(g_runtime.counters.uart_write_errors, DEC);
    SerialUSB.print("\r\ntcp->uart overflow = ");
    SerialUSB.print(g_runtime.counters.tcp_to_uart_overflows, DEC);
    SerialUSB.print("\r\nuart->tcp overflow = ");
    SerialUSB.print(g_runtime.counters.uart_to_tcp_overflows, DEC);
    SerialUSB.print("\r\nuart hardware errors = ");
    SerialUSB.print(lcp_field_port_error_count(LCP_FIELD_PORT_S1), DEC);
    SerialUSB.print("\r\nqueued tcp->uart = ");
    SerialUSB.print(g_runtime.tcp_to_uart.used, DEC);
    SerialUSB.print("\r\nqueued uart->tcp = ");
    SerialUSB.print(g_runtime.uart_to_tcp.used, DEC);
    SerialUSB.print("\r\n=============================\r\n\r\n");
}

void clear_counters(void)
{
    memset(&g_runtime.counters, 0, sizeof(g_runtime.counters));
    SerialUSB.print("fixture counters cleared\r\n");
}

void handle_command(char* command)
{
    if (command == 0)
    {
        return;
    }

    for (uint16_t index = 0U; command[index] != '\0'; ++index)
    {
        if ((command[index] >= 'A') && (command[index] <= 'Z'))
        {
            command[index] = static_cast<char>(command[index] + ('a' - 'A'));
        }
    }

    if ((strcmp(command, "version") == 0) || (strcmp(command, "ver") == 0))
    {
        print_version();
    }
    else if ((strcmp(command, "status") == 0) ||
             (strcmp(command, "bridge") == 0) ||
             (strcmp(command, "bridge list") == 0))
    {
        print_status();
    }
    else if ((strcmp(command, "counters clear") == 0) ||
             (strcmp(command, "bridge counters clear") == 0))
    {
        clear_counters();
    }
    else if ((strcmp(command, "help") == 0) || (strcmp(command, "?") == 0))
    {
        SerialUSB.print("version | status | bridge | counters clear | help\r\n");
    }
    else if (command[0] != '\0')
    {
        SerialUSB.print("unknown command; use help\r\n");
    }
}

void console_poll(void)
{
    const uint8_t usb_open = SerialUSB ? 1U : 0U;
    if ((usb_open != 0U) && (g_runtime.usb_was_open == 0U))
    {
        SerialUSB.print("\r\nLorentz Fixture Bridge ready. Type help.\r\n");
    }
    g_runtime.usb_was_open = usb_open;

    if (usb_open == 0U)
    {
        g_runtime.console_length = 0U;
        return;
    }

    while (SerialUSB.available() > 0)
    {
        const int value = SerialUSB.read();
        if (value < 0)
        {
            break;
        }

        const char character = static_cast<char>(value);
        if ((character == '\r') || (character == '\n'))
        {
            if (g_runtime.console_length != 0U)
            {
                g_runtime.console_buffer[g_runtime.console_length] = '\0';
                handle_command(g_runtime.console_buffer);
                g_runtime.console_length = 0U;
            }
            continue;
        }

        if (g_runtime.console_length < (CONSOLE_BUFFER_SIZE - 1U))
        {
            g_runtime.console_buffer[g_runtime.console_length++] = character;
        }
        else
        {
            g_runtime.console_length = 0U;
            SerialUSB.print("command too long\r\n");
        }
    }
}

void update_socket_state(void)
{
    const uint8_t status = w5500_lite_tcp_server_status(LCP_ETHERNET_1);
    const uint8_t connected = (status == W5500_SN_SR_ESTABLISHED) ? 1U : 0U;

    if ((connected != 0U) && (g_runtime.tcp_connected == 0U))
    {
        ++g_runtime.counters.tcp_connections;
    }
    else if ((connected == 0U) && (g_runtime.tcp_connected != 0U))
    {
        ++g_runtime.counters.tcp_disconnects;
        ring_clear(g_runtime.tcp_to_uart);
        ring_clear(g_runtime.uart_to_tcp);
    }

    g_runtime.tcp_connected = connected;
    g_runtime.last_socket_status = status;
}

void receive_tcp(void)
{
    uint8_t buffer[FORWARD_CHUNK_SIZE];
    const uint16_t free_bytes = ring_free(g_runtime.tcp_to_uart);
    const uint16_t capacity = (free_bytes < FORWARD_CHUNK_SIZE) ?
        free_bytes : FORWARD_CHUNK_SIZE;

    if (capacity == 0U)
    {
        return;
    }

    const uint16_t received = w5500_lite_tcp_server_receive(
        LCP_ETHERNET_1,
        S1_TCP_PORT,
        buffer,
        capacity);

    if (received == 0U)
    {
        return;
    }

    const uint16_t pushed = ring_push(g_runtime.tcp_to_uart, buffer, received);
    g_runtime.counters.tcp_rx_bytes += pushed;
    g_runtime.counters.last_activity_ms = millis();

    if (pushed != received)
    {
        ++g_runtime.counters.tcp_to_uart_overflows;
    }
}

void send_uart(void)
{
    if ((g_runtime.s1 == 0) || (g_runtime.tcp_to_uart.used == 0U) ||
        (g_runtime.s1->tx_idle() == 0U))
    {
        return;
    }

    uint8_t buffer[FORWARD_CHUNK_SIZE];
    const uint16_t requested = ring_peek(g_runtime.tcp_to_uart,
                                         buffer,
                                         FORWARD_CHUNK_SIZE);
    const size_t written = g_runtime.s1->write(buffer, requested);

    if (written == 0U)
    {
        ++g_runtime.counters.uart_write_errors;
        return;
    }

    ring_drop(g_runtime.tcp_to_uart, static_cast<uint16_t>(written));
    g_runtime.counters.uart_tx_bytes += written;
    g_runtime.counters.last_activity_ms = millis();

    if (written != requested)
    {
        ++g_runtime.counters.uart_write_errors;
    }
}

void receive_uart(void)
{
    if (g_runtime.s1 == 0)
    {
        return;
    }

    size_t available = g_runtime.s1->available();
    while ((available > 0U) && (ring_free(g_runtime.uart_to_tcp) > 0U))
    {
        const int value = g_runtime.s1->read();
        if (value < 0)
        {
            break;
        }

        const uint8_t byte_value = static_cast<uint8_t>(value);
        (void)ring_push(g_runtime.uart_to_tcp, &byte_value, 1U);
        ++g_runtime.counters.uart_rx_bytes;
        g_runtime.counters.last_activity_ms = millis();
        --available;
    }

    if ((available > 0U) && (ring_free(g_runtime.uart_to_tcp) == 0U))
    {
        ++g_runtime.counters.uart_to_tcp_overflows;
    }
}

void send_tcp(void)
{
    if ((g_runtime.tcp_connected == 0U) ||
        (g_runtime.uart_to_tcp.used == 0U))
    {
        return;
    }

    uint8_t buffer[FORWARD_CHUNK_SIZE];
    const uint16_t requested = ring_peek(g_runtime.uart_to_tcp,
                                         buffer,
                                         FORWARD_CHUNK_SIZE);
    const uint16_t sent = w5500_lite_tcp_server_send(LCP_ETHERNET_1,
                                                     buffer,
                                                     requested);

    if (sent == 0U)
    {
        return;
    }

    ring_drop(g_runtime.uart_to_tcp, sent);
    g_runtime.counters.tcp_tx_bytes += sent;
    g_runtime.counters.last_activity_ms = millis();

    if (sent != requested)
    {
        ++g_runtime.counters.tcp_errors;
    }
}

void heartbeat_poll(void)
{
    const uint32_t now_ms = millis();
    if (static_cast<uint32_t>(now_ms - g_runtime.last_heartbeat_ms) <
        HEARTBEAT_PERIOD_MS)
    {
        return;
    }

    g_runtime.last_heartbeat_ms = now_ms;
    g_runtime.heartbeat_level =
        (g_runtime.heartbeat_level == LOW) ? HIGH : LOW;
    digitalWrite(LCP_PIN_PLC_OK, g_runtime.heartbeat_level);
}

void setup(void)
{
    memset(&g_runtime, 0, sizeof(g_runtime));
    ring_clear(g_runtime.tcp_to_uart);
    ring_clear(g_runtime.uart_to_tcp);

    lcp_board_init_gpio();
    lcp_rs485_init_builtin_ports();
    lcp_ethernet_init_pins();

    pinMode(LCP_PIN_PLC_OK, OUTPUT);
    digitalWrite(LCP_PIN_PLC_OK, LOW);
    SerialUSB.begin(115200U, SERIAL_8N1);
    SPI.begin();

    const LcpFieldPortConfig serial_configs[LCP_FIELD_PORT_COUNT] =
    {
        { 9600U, HAL_UART_FRAME_8N1 },
        { 9600U, HAL_UART_FRAME_8N1 },
        { 1200U, HAL_UART_FRAME_8N1 },
        { 9600U, HAL_UART_FRAME_8N1 }
    };
    lcp_field_ports_init(serial_configs);
    g_runtime.s1 = &lcp_field_port_transport(LCP_FIELD_PORT_S1);

    g_runtime.ethernet_initialized =
        w5500_lite_begin(LCP_ETHERNET_1, FIXTURE_NETWORK);
    if (g_runtime.ethernet_initialized != 0U)
    {
        w5500_lite_tcp_server_begin(LCP_ETHERNET_1, S1_TCP_PORT);
    }

    watchdog_status_init();
    g_runtime.last_heartbeat_ms = millis();
}

void poll(void)
{
    heartbeat_poll();
    console_poll();

    if (g_runtime.ethernet_initialized != 0U)
    {
        update_socket_state();
        receive_tcp();
        send_uart();
        receive_uart();
        send_tcp();
    }

    watchdog_status_poll();
}

__attribute__((noreturn)) void fatal_stop(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void task(void* argument)
{
    (void)argument;
    setup();
    for (;;)
    {
        poll();
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

void start(void)
{
    const BaseType_t result = xTaskCreate(task,
                                          "FIXTURE",
                                          FIXTURE_TASK_STACK_WORDS,
                                          nullptr,
                                          FIXTURE_TASK_PRIORITY,
                                          &g_fixture_task_handle);
    configASSERT(result == pdPASS);
    if (result != pdPASS)
    {
        fatal_stop();
    }

    vTaskStartScheduler();
    fatal_stop();
}

}  // namespace fixture_firmware
