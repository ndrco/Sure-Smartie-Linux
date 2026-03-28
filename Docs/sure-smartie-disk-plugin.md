# sure_smartie_disk_plugin: плагин метрик дисков

## Назначение

`sure_smartie_disk_plugin` формирует фиксированный список точек монтирования:

- первый диск всегда `/`;
- далее записи из `/etc/fstab` с точкой монтирования `/mnt` или `/mnt/*` (в порядке появления в
  `fstab`).

Для этого списка плагин экспортирует метрики:

- общая ёмкость;
- занятый объём;
- процент заполнения;
- те же значения по каждому диску отдельно.

Плагин удобен для экранов с контролем заполнения корневого раздела и суммарного места на
подключённых накопителях.

## Где находится плагин

После сборки в репозитории:

```text
./build/sure_smartie_disk_plugin.so
```

После system-wide установки:

```text
/usr/local/lib/sure-smartie-linux/plugins/sure_smartie_disk_plugin.so
```

## Подключение в конфиг

```json
{
  "providers": ["system"],
  "plugin_paths": ["./build/sure_smartie_disk_plugin.so"],
  "screens": [
    {
      "name": "disk",
      "interval_ms": 2500,
      "lines": [
        "DSK {disk.used_percent}% {bar:disk.used_percent,6}",
        "{disk.used_gb}/{disk.total_gb}",
        "R {disk.1.used_percent}% {disk.1.used_gb}",
        "{system.time}"
      ]
    }
  ]
}
```

## Экспортируемые метрики

Суммарные:

- `disk.count` — количество отслеживаемых точек (`/` + `/mnt*` из `fstab`);
- `disk.total_gb` — суммарная ёмкость, формат `123.4G` или `-`;
- `disk.used_gb` — суммарно занято, формат `56.7G` или `-`;
- `disk.used_percent` — суммарный процент занятости `0..100` или `-`.

По каждому диску (`N` начинается с `1`: `disk.1` это `/`, остальные — `/mnt*` из `fstab`):

- `disk.N.device` — устройство, например `/dev/nvme0n1p2`;
- `disk.N.device_short` — сокращённое имя устройства, например `nvme0n1p2`;
- `disk.N.mount` — точка монтирования, например `/`;
- `disk.N.mount_short` — сокращённая точка монтирования, например `timeshift` для
  `/mnt/timeshift`;
- `disk.N.fs` — тип ФС, например `ext4`;
- `disk.N.mounted` — `1`, если точка смонтирована, иначе `0`;
- `disk.N.total_gb` — ёмкость `123.4G` или `-`;
- `disk.N.used_gb` — занято `56.7G` или `-`;
- `disk.N.used_percent` — процент занятости `0..100` или `-`.

## Проверка

```bash
./build/sure-smartie-linux --config path/to/config.json --stdout --once
```

Если плагин подключён корректно, в шаблонах появятся значения `disk.*`.

## Ограничения и нюансы

- Парсинг `/etc/fstab` кэшируется и обновляется только при изменении `mtime` файла.
- Метрики размера (`total/used/percent`) кэшируются на `5` секунд.
- `autofs`-placeholder (например `device=systemd-1`, `fs=autofs`) считается "не смонтированным":
  плагин не вызывает для него `statvfs`, чтобы не триггерить долгий automount-таймаут.
- Для точек, которых сейчас нет в списке монтирований, размерные метрики отдаются как `-`.
- Суммарные размерные метрики (`disk.total_gb`, `disk.used_gb`, `disk.used_percent`) тоже
  отдаются как `-`, если хотя бы одна отслеживаемая точка не смонтирована.
