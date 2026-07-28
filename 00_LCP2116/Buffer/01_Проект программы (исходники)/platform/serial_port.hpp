
/**
 * @file serial_port.hpp
 * @brief Платформенный интерфейс аппаратного последовательного порта.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "print.hpp"
#include "../hal/sam3x_uart.hpp"

/**
 * @brief Аппаратный последовательный порт.
 *
 * Класс связывает прикладной потоковый интерфейс `Print` с HAL UART/USART
 * микроконтроллера ATSAM3X8E. Передача выполняется через соответствующий
 * HAL-объект, включая управление направлением RS-485 на уровне UART HAL.
 */
class SerialPort : public Print
{
public:
    /**
     * @brief Создаёт объект последовательного порта.
     *
     * @param port_id Идентификатор аппаратного UART/USART.
     */
    explicit SerialPort(HalUartId port_id);

    /**
     * @brief Инициализирует порт с форматом кадра 8N1.
     *
     * @param baudrate Скорость обмена в bit/s.
     */
    void begin(uint32_t baudrate);

    /**
     * @brief Инициализирует порт с заданным форматом кадра.
     *
     * @param baudrate Скорость обмена в bit/s.
     * @param frame Формат кадра: `SERIAL_8N1`, `SERIAL_8O1` или `SERIAL_8E1`.
     */
    void begin(uint32_t baudrate, uint32_t frame);

    /**
     * @brief Останавливает порт и переводит аппаратный интерфейс в неактивное состояние.
     */
    void end(void);

    /**
     * @brief Возвращает количество байтов, доступных для чтения.
     *
     * @return Количество байтов в приёмном буфере.
     */
    int available(void);

    /**
     * @brief Считывает один байт из приёмного буфера.
     *
     * @return Значение байта `0..255` или `-1`, если данных нет.
     */
    int read(void);

    /**
     * @brief Просматривает следующий байт без удаления из буфера.
     *
     * @return Значение байта `0..255` или `-1`, если данных нет.
     */
    int peek(void);

    /**
     * @brief Ожидает завершения передачи всех данных из выходного буфера.
     */
    void flush(void);

    /**
     * @brief Передаёт один байт.
     *
     * @param value Передаваемый байт.
     *
     * @return Количество поставленных в передачу байтов.
     */
    size_t write(uint8_t value) override;

    /**
     * @brief Передаёт массив байтов.
     *
     * @param buffer Указатель на буфер передачи.
     * @param size Количество байтов.
     *
     * @return Количество поставленных в передачу байтов.
     */
    size_t write(const uint8_t* buffer, size_t size);

    using Print::write;

    /**
     * @brief Возвращает счётчик ошибок приёма.
     *
     * @return Количество ошибок, зафиксированных HAL UART/USART.
     */
    uint32_t errorCount(void) const;

    /**
     * @brief Возвращает признак инициализированного порта.
     */
    operator bool() const;

private:
    HalUartFrame convert_frame(uint32_t frame) const;

    HalUartId port_id_;
    uint8_t initialized_;
};
