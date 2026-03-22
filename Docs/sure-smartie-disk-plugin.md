# sure_smartie_disk_plugin: плагин метрик дисков

## Назначение

`sure_smartie_disk_plugin` собирает информацию по всем смонтированным дискам (`/dev/*`) и
экспортирует метрики:

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

- `disk.count` — количество обнаруженных смонтированных дисков;
- `disk.total_gb` — суммарная ёмкость, формат `123.4G`;
- `disk.used_gb` — суммарно занято, формат `56.7G`;
- `disk.used_percent` — суммарный процент занятости `0..100`.

По каждому диску (`N` начинается с `1`, порядок: по `mount point`):

- `disk.N.device` — устройство, например `/dev/nvme0n1p2`;
- `disk.N.mount` — точка монтирования, например `/`;
- `disk.N.fs` — тип ФС, например `ext4`;
- `disk.N.total_gb` — ёмкость `123.4G`;
- `disk.N.used_gb` — занято `56.7G`;
- `disk.N.used_percent` — процент занятости `0..100`.

## Проверка

```bash
./build/sure-smartie-linux --config path/to/config.json --stdout --once
```

Если плагин подключён корректно, в шаблонах появятся значения `disk.*`.

## Ограничения и нюансы

- Плагин учитывает только **смонтированные** файловые системы на устройствах `/dev/*`.
- Loop-устройства (`/dev/loop*`) и псевдо-ФС (`proc`, `sysfs`, `tmpfs` и т.д.) игнорируются.
- Несмонтированные диски в метрики не попадают, потому что для них нельзя корректно посчитать
  `used`.
