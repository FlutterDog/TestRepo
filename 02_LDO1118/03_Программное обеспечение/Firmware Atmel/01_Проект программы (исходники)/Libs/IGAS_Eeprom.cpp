/**
 * @file IGAS_Eeprom.cpp
 * @brief Очередь отложенной записи EEPROM и примитивы сохранения параметров.
 *
 * @details
 * Класс ::IGAS_Eeprom_Class реализует неблокирующую стратегию работы с EEPROM:
 * - вызовы save*() не пишут в EEPROM напрямую, а ставят операции в кольцевую очередь;
 * - eepromRing() периодически «сливает» очередь порциями, учитывая готовность EEPROM;
 * - запись выполняется через EEPROM.update(), что уменьшает износ (перезапись только при изменении).
 *
 * Это позволяет вызывать сохранение параметров из разных частей кода без длинных задержек,
 * но требует регулярной диспетчеризации eepromRing() в основном цикле/тикере.
 *
 * @note Критическая секция защищает только модификацию индексов/буферов кольца.
 *       Сама запись в EEPROM выполняется вне критических секций.
 */

#include "IGAS_Eeprom.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Локальные хелперы: очень короткая критическая секция (SREG + cli/sei).      */
/* -------------------------------------------------------------------------- */

namespace {

static inline uint8_t irq_save_and_disable()
{
	const uint8_t s = SREG;
	cli();
	return s;
}

static inline void irq_restore(uint8_t s)
{
	SREG = s;
}

} // namespace

/* -------------------------------------------------------------------------- */

IGAS_Eeprom_Class::IGAS_Eeprom_Class()
	: ringAddr(0)
	, ringData(0)
	, in_(0)
	, out_(0)
{
}

/**
 * @brief Поместить операцию записи байта в кольцевую очередь.
 * @param addr_  Адрес EEPROM.
 * @param digit_ Байт данных.
 *
 * @details
 * Функция потокобезопасна относительно прерываний: защищает обновление буферов и индексов.
 * При переполнении очереди самый старый элемент вытесняется (lossy queue).
 *
 * @warning При переполнении теряются самые старые операции записи. Это приемлемо для параметров,
 *          которые часто обновляются и где важнее «последнее значение», чем полная история.
 */
void IGAS_Eeprom_Class::inRing(int16_t addr_, uint8_t digit_)
{
	const uint8_t s = irq_save_and_disable();

	addrBuffer[in_] = addr_;
	dataBuffer[in_] = digit_;

	uint8_t next = (uint8_t)(in_ + 1);
	if (next >= RING_SIZE) {
		next = 0;
	}

	// Переполнение кольца: сдвигаем out_, перезаписывая самый старый элемент.
	if (next == out_) {
		uint8_t o = (uint8_t)(out_ + 1);
		if (o >= RING_SIZE) {
			o = 0;
		}
		out_ = o;
	}

	in_ = next;
	irq_restore(s);
}

/**
 * @brief Извлечь следующий элемент очереди в ringAddr/ringData.
 *
 * @details
 * Предполагается вызов только из контекста, где гарантировано:
 * - очередь не пуста (in_ != out_);
 * - нет параллельного изменения out_ другим контекстом.
 *
 * В данной реализации outRing() используется только из eepromRing() (основной поток),
 * поэтому отдельная критическая секция не требуется.
 */
void IGAS_Eeprom_Class::outRing()
{
	ringAddr = addrBuffer[out_];
	ringData = dataBuffer[out_];

	uint8_t next = (uint8_t)(out_ + 1);
	if (next >= RING_SIZE) {
		next = 0;
	}
	out_ = next;
}

/**
 * @brief Периодическая обработка очереди EEPROM.
 * @return 1, если очередь пуста и EEPROM готов (idle); 0 — если есть работа или EEPROM занят.
 *
 * @details
 * Сливает ограниченное число операций за вызов (квота), чтобы не блокировать основной цикл.
 * Запись выполняется через EEPROM.update() — физическая запись происходит только при отличии
 * байта в EEPROM от нового значения.
 */
uint8_t IGAS_Eeprom_Class::eepromRing()
{
	// Нет отложенных операций: возвращаем «готовность» EEPROM как признак idle.
	if (in_ == out_) {
		return eeprom_is_ready() ? 1u : 0u;
	}

	// Квота на один вызов: баланс между скоростью «слива» и длительностью блокировки.
	uint8_t quota = 2;

	while (quota && (in_ != out_) && eeprom_is_ready()) {
		outRing();
		EEPROM.update(ringAddr, ringData);
		--quota;
	}

	return ((in_ == out_) && eeprom_is_ready()) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* Примитивы сохранения/загрузки                                               */
/* -------------------------------------------------------------------------- */

void IGAS_Eeprom_Class::saveInt(int16_t addr_, int16_t digit_)
{
	// Little-endian: low затем high.
	inRing(addr_,     (uint8_t)(digit_ & 0xFFu));
	inRing(addr_ + 1, (uint8_t)((uint16_t)digit_ >> 8));
}

int16_t IGAS_Eeprom_Class::loadInt(int16_t addr_)
{
	const uint16_t lo = (uint16_t)EEPROM.read(addr_);
	const uint16_t hi = (uint16_t)EEPROM.read(addr_ + 1);
	return (int16_t)((hi << 8) | lo);
}

void IGAS_Eeprom_Class::saveSignedInt(int16_t addr_, int16_t digit_)
{
	// Формат: 2 байта величины + 1 байт знака (0/1).
	uint8_t sign_ = 0;
	int16_t mag   = digit_;

	if (digit_ < 0) {
		mag   = (int16_t)(-digit_);
		sign_ = 1;
	}

	inRing(addr_,     (uint8_t)(mag & 0xFFu));
	inRing(addr_ + 1, (uint8_t)((uint16_t)mag >> 8));
	inRing(addr_ + 2, sign_);
}

int16_t IGAS_Eeprom_Class::loadSignedInt(int16_t addr_)
{
	const uint16_t lo   = (uint16_t)EEPROM.read(addr_);
	const uint16_t hi   = (uint16_t)EEPROM.read(addr_ + 1);
	const uint8_t  sign = (uint8_t)EEPROM.read(addr_ + 2);

	const int16_t val = (int16_t)((hi << 8) | lo);
	return (sign == 0u) ? val : (int16_t)(-val);
}

void IGAS_Eeprom_Class::saveSigned(int16_t addr_, int16_t digit_)
{
	// Упрощённый формат: 1 байт величины + 1 байт знака.
	uint8_t sign = 0;
	int16_t mag  = digit_;

	if (digit_ < 0) {
		mag  = (int16_t)(-digit_);
		sign = 1;
	}

	inRing(addr_,     (uint8_t)(mag & 0xFFu));
	inRing(addr_ + 1, sign);
}

int16_t IGAS_Eeprom_Class::loadSign(int16_t addr_)
{
	const int16_t mag  = (int16_t)EEPROM.read(addr_);
	const uint8_t sign = (uint8_t)EEPROM.read(addr_ + 1);
	return (sign == 0u) ? mag : (int16_t)(-mag);
}

void IGAS_Eeprom_Class::saveByte(int16_t addr_, uint8_t digit_)
{
	inRing(addr_, digit_);
}

/**
 * @brief Загрузить float из EEPROM (совместимо с историческим форматом хранения).
 * @param addr_     Базовый адрес (4 байта).
 * @param default_  Значение по умолчанию, если EEPROM содержит 0xFF 0xFF 0xFF 0xFF.
 * @return Прочитанное значение или default_.
 *
 * @details
 * Используется «обратный порядок байт» (big-endian представление в EEPROM),
 * чтобы оставаться совместимым с исходным кодом/данными.
 */
float IGAS_Eeprom_Class::loadFloat(int16_t addr_, float default_)
{
	union {
		float   f;
		uint8_t b[4];
	} u;

	uint8_t ff = 0;

	for (uint8_t i = 0; i < 4; ++i) {
		const uint8_t v = (uint8_t)EEPROM.read(addr_ + i);
		u.b[3u - i] = v; // обратный порядок байт
		if (v == 0xFFu) {
			++ff;
		}
	}

	if (ff == 4u) {
		return default_;
	}

	return u.f;
}

void IGAS_Eeprom_Class::saveFloat(int16_t addr_, float value)
{
	union {
		float   f;
		uint8_t b[4];
	} u;

	u.f = value;

	for (uint8_t i = 0; i < 4; ++i) {
		inRing(addr_ + i, u.b[3u - i]); // обратный порядок байт
	}
}
