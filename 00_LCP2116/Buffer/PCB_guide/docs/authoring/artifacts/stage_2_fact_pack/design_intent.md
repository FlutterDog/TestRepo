# DESIGN INTENT

## Назначение документа

ℹ️ Этот файл фиксирует инженерный замысел по крупным узлам платы на основе freeze-комплекта и product manuals.

⚠️ Это не доказательство электрической реализации само по себе, а контролируемая интерпретация, опирающаяся на факты Stage two.

---

## Design intent entries

### Block ID

* `DI-001`

### Subsystem

* `processor module`

### Why this block exists

* обеспечить центральный вычислительный и координирующий узел для модулей и внешних устройств семейства `Lorentz`.

### What the client / integrator should understand

* это не периферийная плата-адаптер, а основной CPU / communication hub модуля.

### Supported use / intended role

* выполнение управляющей логики;
* координация обмена с модулями I/O;
* хранение конфигурации и журналов;
* интеграция с HMI и внешними устройствами.

### What should not be overclaimed

* не описывать модуль как универсальный промышленный компьютер широкого профиля;
* не переносить firmware-level функции в hardware claims.

### Notes for future draft

* хороший угол для текста: «центральный процессорный модуль с насыщенной коммуникационной архитектурой и локальным хранением данных».

---

### Block ID

* `DI-002`

### Subsystem

* `ethernet`

### Why this block exists

* обеспечить сетевую связность модуля с внешними устройствами и человеко-машинным интерфейсом.

### What the client / integrator should understand

* два Ethernet-домена сделаны не ради формального наличия сетевых разъёмов, а ради разделения ролей связи и интеграции.

### Supported use / intended role

* подключение сетевых устройств;
* связь с панелями / сервисными узлами;
* сетевой обмен внутри системы.

### What should not be overclaimed

* не обещать все возможные protocol features контроллера `W5500` как уже реализованные в изделии;
* не обещать сетевое резервирование, если оно не закрыто отдельным источником.

### Notes for future draft

* уместно показывать этот блок как один из главных differentiator модуля по сравнению с более простыми CPU boards.

---

### Block ID

* `DI-003`

### Subsystem

* `rs-485 and x2x`

### Why this block exists

* обеспечить работу контроллера в среде распределённых полевых устройств и модулей ввода-вывода.

### What the client / integrator should understand

* коммуникационная архитектура модуля ориентирована на промышленную периферию, а не только на локальную логику.

### Supported use / intended role

* связь с внешними устройствами по `RS-485`;
* связь с модулями семейства `Lorentz` по `X2X`.

### What should not be overclaimed

* не обещать точное количество внешних пользовательских `RS-485` портов до закрытия конфликта по `X2X`;
* не смешивать physical-layer факт наличия transceiver и user-facing port mapping.

### Notes for future draft

* полезна формулировка «группа последовательных интерфейсов промышленного класса плюс отдельная внутренняя шина модульной периферии».

---

### Block ID

* `DI-004`

### Subsystem

* `power`

### Why this block exists

* принять входное промышленное питание и сформировать внутренние rails для логики, коммуникации и сервисных узлов.

### What the client / integrator should understand

* power architecture не сводится к одному стабилизатору; она включает защиту, преобразование и распределение питания по доменам.

### Supported use / intended role

* работа модуля от `24 V`;
* локальное формирование низковольтных уровней;
* повышение живучести при ошибках подключения и импульсных воздействиях.

### What should not be overclaimed

* не обещать конкретные запасы по EMC / surge без испытаний или official protection documentation.

### Notes for future draft

* текст лучше строить через цепочку: `24 V input -> protection -> conversion -> local rails`.

---

### Block ID

* `DI-005`

### Subsystem

* `data retention and service`

### Why this block exists

* сделать модуль пригодным для реальной эксплуатации, а не только для лабораторного запуска.

### What the client / integrator should understand

* наличие `uSD`, батарейного резервирования времени, USB service port, LED-индикации и кнопок говорит о законченном эксплуатационном изделии.

### Supported use / intended role

* хранение конфигурации;
* ведение журналов;
* сервисное обслуживание;
* локальная диагностика состояния.

### What should not be overclaimed

* не обещать конкретные software workflows, если они не описаны отдельным документом;
* не приписывать этим узлам роль, не подтверждённую manuals.

### Notes for future draft

* это хороший блок для раздела про эксплуатационную зрелость модуля.
