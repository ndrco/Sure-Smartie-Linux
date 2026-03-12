# sure-smartie-gui: подробная инструкция по настройке дисплея

## Назначение

`sure-smartie-gui` нужен для подготовки и проверки JSON-конфигурации, которую затем использует
рантайм `sure-smartie-linux`. GUI позволяет:

- открыть существующий конфиг;
- изменить параметры дисплея;
- включить или выключить встроенные провайдеры;
- подключить `.so` плагины;
- создать несколько экранов и задать их ротацию;
- посмотреть live preview, построенный тем же `TemplateEngine`, что и в CLI;
- проверить конфиг через встроенную валидацию.

Важно: GUI не пишет данные напрямую в физический индикатор. Он редактирует конфиг и показывает
локальный preview. После сохранения конфигурации её применяет `sure-smartie-linux`.

## Быстрый старт

### 1. Сборка

```bash
cmake -S . -B build
cmake --build build
```

Если `Qt6 Widgets` доступен, будет собран бинарник:

```bash
./build/sure-smartie-gui
```

### 2. Запуск

Открыть GUI с конфигом по умолчанию:

```bash
./build/sure-smartie-gui
```

Открыть конкретный конфиг:

```bash
./build/sure-smartie-gui configs/sure-example.json
```

### Где находится `config.json` по умолчанию

Нужно различать три режима.

При запуске установленного рантайма без явного `--config` используется:

```text
/usr/local/etc/sure-smartie-linux/config.json
```

Если этот файл отсутствует, рантайм автоматически пытается использовать:

```text
/usr/local/etc/sure-smartie-linux/config.json.example
```

При запуске GUI без аргумента пути он пытается открыть конфиг из рабочей копии репозитория:

1. `configs/stdout-example.json`
2. `configs/sure-example.json`

То есть `sure-smartie-gui` по умолчанию не ищет установленный системный `config.json`,
а работает с ближайшим примером из проекта, если вы запускаете его из дерева исходников.

### 3. Применение на реальном устройстве

После сохранения конфига:

```bash
./build/sure-smartie-linux --config path/to/config.json
```

Для предварительной проверки без циклического вывода:

```bash
./build/sure-smartie-linux --config path/to/config.json --validate-config
./build/sure-smartie-linux --config path/to/config.json --once
```

## Пошаговая настройка в GUI

### Project

В блоке `Project` видны:

- `Config source`: откуда загружен текущий конфиг;
- `Buffer state`: есть ли несохранённые изменения.

Рекомендуемый workflow:

1. Открыть готовый конфиг через `File -> Open...` или начать с буфера по умолчанию.
2. Внести изменения.
3. Проверить `Preview` и `Validation`.
4. Сохранить через `File -> Save` или `File -> Save As...`.

### Display Settings

Здесь задаются базовые параметры индикатора.

- `Device`: путь к serial-устройству, например `/dev/ttyUSB1` или `/dev/serial/by-id/...`.
- `Refresh ms`: частота обновления рантайма.
- `Baudrate`: скорость порта, для SURE обычно `9600`.
- `Display type`:
  - `sure` для реального SURE LCD;
  - `stdout` для безопасного тестового режима.
- `Columns`, `Rows`: геометрия дисплея. Базовый таргет проекта: `20 x 4`.
- `Backlight`: включение подсветки.
- `Contrast`, `Brightness`: значения `1..254`.

Практический порядок настройки:

1. Для разработки сначала использовать `display.type = stdout` и `device = /dev/null`.
2. Когда шаблоны готовы, переключить `display.type` на `sure`.
3. Указать реальное устройство.
4. Проверить `Validation`, затем запускать CLI.

По умолчаниию устанновлено `device = /dev/null` - это безопасная заглушка вместо реального serial-порта.
В режиме `stdout` приложение не должно отправлять команды на физический дисплей; и `/dev/null` явно показывает, что конфиг используется только для preview, тестов и отладки. Для настоящего индикатора здесь нужно указывать реальный путь, например `/dev/ttyUSB1` или `/dev/serial/by-id/...`.

### Providers & Plugins

Блок состоит из двух частей.

`Builtin providers`:

- `cpu`
- `gpu`
- `ram`
- `system`
- `network`

Эти чекбоксы включают встроенные источники метрик. Если провайдер не используется в шаблонах,
его лучше отключить, чтобы не собирать лишние данные.

`Plugin paths`:

- сюда добавляются `.so` плагины;
- каждый путь указывает на отдельную динамическую библиотеку;
- GUI и CLI попробуют загрузить эти библиотеки при построении preview или рантайма.

Важный нюанс: провайдеры из плагинов **не добавляются** в массив `providers`.
Поле `providers` зарезервировано только для встроенных провайдеров.
Плагин подключается исключительно через `plugin_paths`.

### Screens Editor

Каждый экран состоит из:

- `Name`: человекочитаемое имя;
- `Interval ms`: сколько экран показывается перед переключением;
- `Lines`: шаблоны строк дисплея.

Кнопки:

- `Add`: добавить экран;
- `Remove`: удалить экран;
- `Duplicate`: скопировать экран;
- `Move Up`, `Move Down`: поменять порядок ротации.

Рекомендации:

- делать экран под одну задачу: производительность, сеть, время, статус сервиса;
- не перегружать одну строку метриками, потому что всё будет жёстко обрезано по ширине;
- использовать preview line counters `est x/20`, чтобы не терять данные на усечении.

### Preview

`Preview` показывает то, что увидит пользователь на LCD.

- `Screen`: экран для предпросмотра;
- `Rotation preview`: имитация автоматической ротации;
- `Refresh ms`: частота обновления preview;
- `Refresh now`: принудительное обновление.

Что полезно проверять в preview:

- строка помещается в 20 символов;
- числа и бары не “съедают” важную информацию;
- при длинном hostname или IP остаётся полезная часть текста;
- screen order и interval выглядят логично в rotation preview.

### Validation

`Validation` показывает ошибки и предупреждения.

Типовые случаи:

- отрицательные или нулевые `refresh_ms`, `interval_ms`, `cols`, `rows`;
- `contrast` или `brightness` вне диапазона `1..254`;
- несуществующий `device` или `plugin_paths`;
- лишние строки сверх количества `rows`;
- незакрытый шаблон `{...}`;
- слишком широкая строка, которая будет усечена.

Если конфиг валиден, GUI показывает:

```text
[info] config is valid
```

## Синтаксис строк экрана

Каждая строка в `screens[].lines[]` проходит через `TemplateEngine`.

Поддерживаются два основных механизма.

### 1. Подстановка метрик

Формат:

```text
{metric.key}
```

Примеры:

```text
{cpu.load}
{system.time}
{net.ip}
{gpu.mem_used}
```

Если метрика отсутствует, движок подставит `-`.

### 2. Bar macro

Формат:

```text
{bar:metric,width}
{bar:metric,width,max}
```

Примеры:

```text
{bar:cpu.load,6}
{bar:gpu.load,8}
{bar:gpu.mem_percent,8,100}
```

Особенности:

- по умолчанию максимум равен `100`;
- bar занимает ровно `width` знакомест;
- в preview и на физическом LCD используются специальные псевдографические символы;
- некорректный bar-шаблон попадает в validation как ошибка.

### 3. Абсолютное позиционирование по знакоместу

Формат:

```text
{at:column}
```

Пример:

```text
CPU {cpu.load}%{at:12}{ram.percent}%
```

Что делает макрос:

- `column` задаётся в знакоместах, начиная с `1`;
- если текущий вывод короче нужной позиции, движок добавляет пробелы до указанной колонки;
- если текущий вывод уже длиннее, макрос ничего не сдвигает назад.

Это полезно, когда первая метрика “гуляет” по длине, а следующая должна начинаться строго с
фиксированного места.

Пример:

```text
CPU 8%   {at:12}14%
CPU 42%  {at:12}14%
CPU 100% {at:12}14%
```

Во всех трёх случаях вторая метрика начнёт печататься с одного и того же знакоместа.

### 4. Обрезание и дополнение

После рендеринга строка:

- обрезается до `display.cols`;
- если короче, дополняется пробелами;
- управляющие символы заменяются пробелом.

Это значит, что нужно проектировать макеты под точную ширину дисплея, а не рассчитывать на перенос.

## Готовые примеры экранов 20x4

Ниже примеры в формате:

- строка шаблона;
- ожидаемая идея отображения.

Содержимое будет зависеть от реальных метрик конкретной машины.

### Вариант 1. Базовый экран производительности

```json
{
  "name": "overview",
  "interval_ms": 2500,
  "lines": [
    "CPU {bar:cpu.load,6} {cpu.load}%",
    "GPU {bar:gpu.load,6} {gpu.load}%",
    "RAM {ram.percent}% {ram.used_gb}",
    "{system.time} {system.hostname}"
  ]
}
```

Пример вывода:

```text
CPU ███░░░ 47%
GPU ██░░░░ 18%
RAM 32% 10.4G
14:37 ws-node
```

### Вариант 2. Экран сети

```json
{
  "name": "network",
  "interval_ms": 2500,
  "lines": [
    "NET {net.iface}",
    "RX {net.rx_total}",
    "TX {net.tx_total}",
    "{net.ip}"
  ]
}
```

Пример вывода:

```text
NET enp4s0
RX 128.4G
TX 14.9G
192.168.1.44
```

### Вариант 3. Температуры и частоты

```json
{
  "name": "temps",
  "interval_ms": 2000,
  "lines": [
    "CPU {cpu.temp}C {cpu.power_w}W",
    "CPUF {cpu.fan_rpm}rpm",
    "GPU {gpu.temp}C {gpu.power_w}W",
    "G-F {gpu.fan_percent}%"
  ]
}
```

Пример вывода:

```text
CPU 61C 72.4W
CPUF 1180rpm
GPU 49C 88.5W
G-F 37%
```

### Вариант 3b. Частоты, вентиляторы и аптайм

```json
{
  "name": "thermals",
  "interval_ms": 2000,
  "lines": [
    "CPU {cpu.clock} {cpu.fan_percent}%",
    "GPU {gpu.vendor} {gpu.fan_rpm}",
    "VRAM {gpu.mem_used}",
    "UP {system.uptime}"
  ]
}
```

Пример вывода:

```text
CPU 4850M 46%
GPU nvidia 1320
VRAM 3.1G
UP 2d 06h
```

### Вариант 4. Экран с акцентом на время и статус

```json
{
  "name": "clock",
  "interval_ms": 5000,
  "lines": [
    "{system.time}",
    "{system.hostname}",
    "UP {system.uptime}",
    "IP {net.ip}"
  ]
}
```

Пример вывода:

```text
14:37
build-box
UP 3d 04h
IP 10.0.0.25
```

### Вариант 5. Экран загрузки памяти GPU

```json
{
  "name": "gpu-memory",
  "interval_ms": 2500,
  "lines": [
    "{gpu.name}",
    "LOAD {gpu.load}%",
    "{bar:gpu.mem_percent,10} {gpu.mem_percent}%",
    "{gpu.mem_used}/{gpu.mem_total}"
  ]
}
```

Пример вывода:

```text
RTX 4070
LOAD 65%
██████░░░░ 58%
7.0G/12.0G
```

Замечание: если `gpu.name` слишком длинное, строка будет усечена. Для компактного макета лучше
использовать `{gpu.vendor}` или вручную короткую подпись.

### Вариант 6. Экран плагина

Для демо-плагина из репозитория:

```json
{
  "name": "plugin-demo",
  "interval_ms": 2000,
  "lines": [
    "PLG {demo.message}",
    "COUNT {demo.counter}",
    "{system.time}",
    "{system.hostname}"
  ]
}
```

Пример вывода:

```text
PLG plugin-ok
COUNT 42
14:37
workstation
```

## Полный перечень возможностей встроенных провайдеров

Ниже перечислены фактически экспортируемые ключи метрик и ограничения каждого провайдера.

### `cpu`

Источник:

- `/proc/stat`
- `/sys/class/thermal`
- `/proc/cpuinfo`

Ключи:

- `cpu.load`: загрузка CPU в процентах, целое число `0..100`;
- `cpu.temp`: температура CPU в `C`, строка вида `61`, либо `--`, если датчик не найден;
- `cpu.clock`: усреднённая текущая частота по доступным строкам `cpu MHz` из `/proc/cpuinfo`,
  строка вида `4850M`, либо `--`;
- `cpu.power_w`: потребляемая мощность CPU в ваттах, строка вида `72.4`, либо `--`;
- `cpu.fan_rpm`: скорость вентилятора CPU в оборотах в минуту, строка вида `1180`, либо `--`;
- `cpu.fan_percent`: скорость вентилятора CPU в процентах, строка вида `46`, либо `--`.

Особенности:

- `cpu.load` рассчитывается по разнице между двумя чтениями `/proc/stat`;
- на первом цикле после старта загрузка может быть `0`, потому что предыдущей выборки ещё нет;
- `cpu.clock` больше не берётся из первого попавшегося логического ядра, а усредняется по
  доступным ядрам, чтобы значение меньше дёргалось;
- поиск температуры эвристический, поэтому на некоторых системах может вернуться `--`;
- `cpu.power_w` читается из `powercap` или `hwmon`, а при отсутствии мгновенного значения
  может оцениваться по изменению `energy_uj` между циклами;
- `cpu.fan_rpm` и `cpu.fan_percent` зависят от того, экспортирует ли материнская плата или EC
  соответствующие `fan*_input` и `pwm*` в `/sys/class/hwmon`.

### `gpu`

Источник:

- сначала `nvidia-smi`, если доступен;
- затем `/sys/class/drm` и `hwmon`.

Ключи:

- `gpu.name`: имя GPU;
- `gpu.vendor`: `nvidia`, `amd`, `intel` или `gpu`;
- `gpu.load`: загрузка GPU в процентах;
- `gpu.temp`: температура GPU;
- `gpu.mem_used`: занятая видеопамять, строка вида `3.1G`;
- `gpu.mem_total`: общий объём видеопамяти, строка вида `12.0G`;
- `gpu.mem_percent`: процент использования VRAM;
- `gpu.power_w`: потребляемая мощность GPU в ваттах, строка вида `88.5`, либо `--`;
- `gpu.fan_rpm`: скорость вентилятора GPU в оборотах в минуту, строка вида `1320`, либо `--`;
- `gpu.fan_percent`: скорость вентилятора GPU в процентах, строка вида `37`, либо `--`.

Особенности:

- если метрика недоступна, обычно возвращается `--`;
- на NVIDIA точность и полнота данных зависят от `nvidia-smi`;
- на AMD/Intel набор доступных полей зависит от sysfs и драйвера;
- `gpu.power_w` и `gpu.fan_percent` на NVIDIA обычно доступны через `nvidia-smi`;
- `gpu.fan_rpm`, а также power и fan на AMD/Intel читаются через `hwmon` под `/sys/class/drm`;
- для компактных экранов не стоит всегда выводить `gpu.name`, оно может быть слишком длинным.

### `ram`

Источник:

- `/proc/meminfo`

Ключи:

- `ram.percent`: процент используемой RAM;
- `ram.used_gb`: занято в гигабайтах, строка вида `10.4G`;
- `ram.total_gb`: общий объём RAM, строка вида `31.2G`.

Особенности:

- если есть `MemAvailable`, используется она;
- если нет, используется `MemFree`.

### `system`

Источник:

- `gethostname()`
- системное локальное время
- `/proc/uptime`

Ключи:

- `system.hostname`: имя хоста;
- `system.time`: локальное время в формате `HH:MM`;
- `system.uptime`: строка аптайма.

Форматы `system.uptime`:

- `17m`
- `03h 42m`
- `2d 06h`

Особенности:

- `hostname` не укорачивается автоматически;
- если hostname длинный, строка будет усечена по ширине дисплея.

### `network`

Источник:

- `getifaddrs()`
- `/proc/net/dev`

Ключи:

- `net.iface`: имя основного интерфейса;
- `net.ip`: IPv4 адрес основного интерфейса;
- `net.rx_total`: суммарно принято, строка вида `128.4G`;
- `net.tx_total`: суммарно передано, строка вида `14.9G`.

Особенности:

- выбирается первый интерфейс `UP`, не loopback и с IPv4;
- если такой интерфейс не найден, используются заглушки `-`;
- это именно суммарные счётчики, а не мгновенная скорость.

## Практические рекомендации по проектированию экранов

### 1. Думать в знакоместах

Для `20x4` надо проектировать каждую строку как фиксированную ширину `20`.

Примеры компактных обозначений:

- `CPU` вместо `Processor`
- `RX` и `TX` вместо `Received` и `Transmit`
- `UP` вместо `Uptime`
- `VRAM` вместо `Video memory`

### 2. Не совмещать длинные значения в одной строке

Плохой вариант:

```text
{system.time} {system.hostname} {net.ip}
```

Лучше:

```text
{system.time} {system.hostname}
{net.ip}
```

### 3. Проверять bar-ширину заранее

На дисплее 20x4 bar на 10-12 символов выглядит заметно, но быстро “съедает” строку.

Практически удобные размеры:

- `4..6` для компактного индикатора рядом с числом;
- `8..10` для отдельной статусной строки.

### 4. Держать важные значения левее

Так как длинные строки обрезаются справа, критичные данные лучше ставить в начале:

Хорошо:

```text
IP {net.ip}
```

Хуже:

```text
ADDR {system.hostname} {net.ip}
```

### 5. Учитывать недоступные метрики

Некоторые системы не отдают `gpu.temp` или `cpu.temp`. Для таких экранов лучше делать строки,
которые остаются понятными даже при `--`.

## Плагины: как писать и подключать

### Что умеет плагин

Плагин может добавить собственный `IProvider`, который пишет метрики в `MetricMap`.

Минимальный контракт:

- реализовать `sure_smartie::providers::IProvider`;
- экспортировать `extern "C"` entry point `sure_smartie_provider_plugin`;
- вернуть `ProviderPluginDescriptor` с правильной `api_version`;
- предоставить функции `create` и `destroy`.

Базовый интерфейс:

```cpp
class IProvider {
 public:
  virtual ~IProvider() = default;
  virtual std::string name() const = 0;
  virtual void collect(core::MetricMap& metrics) = 0;
};
```

Entry point должен называться строго:

```cpp
sure_smartie_provider_plugin
```

Текущая версия API:

```cpp
sure_smartie::plugins::kProviderPluginApiVersion == 1
```

### Пример простого плагина

Сокращённый вариант по мотивам `plugins/sure_smartie_demo_plugin.cpp`:

```cpp
#include "sure_smartie/plugins/ProviderPlugin.hpp"

class DemoProvider final : public sure_smartie::providers::IProvider {
 public:
  std::string name() const override { return "demo"; }

  void collect(sure_smartie::core::MetricMap& metrics) override {
    metrics["demo.message"] = "plugin-ok";
    metrics["demo.counter"] = "42";
  }
};

sure_smartie::providers::IProvider* createProvider() {
  return new DemoProvider();
}

void destroyProvider(sure_smartie::providers::IProvider* provider) {
  delete provider;
}

extern "C" const sure_smartie::plugins::ProviderPluginDescriptor*
sure_smartie_provider_plugin() {
  static const sure_smartie::plugins::ProviderPluginDescriptor descriptor{
      .api_version = sure_smartie::plugins::kProviderPluginApiVersion,
      .name = "sure_smartie_demo_plugin",
      .create = &createProvider,
      .destroy = &destroyProvider,
  };
  return &descriptor;
}
```

### Сборка внешнего плагина

После установки проекта доступен SDK:

```cmake
find_package(SureSmartieLinux CONFIG REQUIRED)
target_link_libraries(your_plugin PRIVATE SureSmartieLinux::sure_smartie_plugin_sdk)
```

Также устанавливается пример:

```text
/usr/local/share/sure-smartie-linux/sdk/example-plugin
```

### Подключение плагина в GUI

1. Собрать `.so`.
2. Открыть `sure-smartie-gui`.
3. В `Providers & Plugins` нажать `Add plugin`.
4. Выбрать путь к библиотеке.
5. Добавить строки экрана, использующие ключи метрик плагина.
6. Проверить `Preview` и `Validation`.
7. Сохранить конфиг.

Эквивалент в JSON:

```json
{
  "providers": ["system"],
  "plugin_paths": ["./build/my_plugin.so"],
  "screens": [
    {
      "name": "plugin",
      "interval_ms": 2000,
      "lines": [
        "{my.metric}",
        "{system.time}"
      ]
    }
  ]
}
```

## Рекомендации по разработке плагинов

### 1. Использовать префикс в ключах

Хорошо:

- `weather.temp`
- `weather.city`
- `ups.status`
- `build.queue`

Плохо:

- `temp`
- `status`
- `count`

Так вы не пересечётесь со встроенными метриками или другими плагинами.

### 2. Возвращать короткие строки

LCD маленький, поэтому метрики должны быть готовы к прямому выводу:

- `24C`, а не `24 degrees Celsius`
- `OK`, а не `service healthy`
- `3.2G`, а не `3.2 gigabytes`

### 3. Не блокировать `collect()`

`collect()` вызывается в цикле обновления. Если плагин долго ходит в сеть или в serial,
он будет тормозить весь экран.

Рекомендуется:

- кэшировать данные;
- использовать короткие таймауты;
- выполнять тяжёлый I/O асинхронно вне `collect()`, если это потребуется.

### 4. Не бросать исключения без необходимости

Если ошибка временная, лучше вернуть запасные значения:

- `weather.temp = "--"`
- `weather.status = "err"`

Чем чаще плагин бросает исключение, тем больше warning в diagnostics и тем беднее preview.

### 5. Делать `name()` стабильным

`name()` используется в диагностике. Не надо делать его динамическим или зависящим от данных.

Хорошо:

- `weather`
- `ups`
- `external-example`

### 6. Один плагин, одна ответственность

Если источник данных логически цельный, один `.so` с одним provider обычно проще сопровождать,
чем большой универсальный плагин на всё.

### 7. Продумывать деградацию

Плагин должен быть полезен даже при частично недоступных данных:

- сеть недоступна;
- API вернул ошибку;
- устройство временно отключено.

Хороший подход:

- хранить последнее успешное значение;
- обновлять только изменившиеся ключи;
- на фатальной ошибке возвращать понятные короткие заглушки.

## Рекомендуемый процесс внедрения нового экрана

1. Начать с `display.type = stdout`.
2. Настроить один экран в GUI.
3. Добиться чистого `Validation`.
4. Проверить preview на нескольких циклах.
5. Сохранить конфиг.
6. Запустить `sure-smartie-linux --validate-config`.
7. Запустить `sure-smartie-linux --once`.
8. Только после этого переключать `display.type` на `sure` и работать с физическим устройством.

## Минимальный рабочий конфиг для реального дисплея

```json
{
  "device": "/dev/ttyUSB1",
  "baudrate": 9600,
  "refresh_ms": 1000,
  "display": {
    "type": "sure",
    "cols": 20,
    "rows": 4,
    "backlight": true,
    "contrast": 128,
    "brightness": 192
  },
  "providers": ["cpu", "ram", "system"],
  "plugin_paths": [],
  "screens": [
    {
      "name": "overview",
      "interval_ms": 2500,
      "lines": [
        "CPU {bar:cpu.load,6} {cpu.load}%",
        "RAM {ram.percent}% {ram.used_gb}",
        "UP {system.uptime}",
        "{system.time} {system.hostname}"
      ]
    }
  ]
}
