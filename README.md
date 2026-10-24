# ESP32-S3 Cyrillic HID Dongle

[Русский](#русский) · [English](#english)

## Русский

Прошивка превращает **LILYGO T-Dongle-S3 V1.1** в настоящую USB HID-клавиатуру и мышь. Текст отправляется с телефона или ноутбука через BLE либо резервную Wi-Fi-точку, а донгл печатает его на целевом компьютере без установки драйверов и программ.

Главное отличие этой версии — корректная обработка русского UTF-8 и преобразование кириллицы в физические клавиши раскладки **ЙЦУКЕН**.

### Возможности v1.0.0 / firmware v9

- русский UTF-8: строчные и заглавные буквы, `ё` и основные знаки препинания;
- стабильная печать без потери символов: отдельные HID-нажатия и отпускания по 8 мс;
- управление через Web Bluetooth или встроенную Wi-Fi-страницу;
- USB HID-клавиатура и мышь;
- BLE и Wi-Fi имя: **`Клавиатура`**;
- пароль резервной Wi-Fi-сети: **`12345678`**;
- вертикальный дисплей ST7735 80×160;
- полноэкранная заставка «КИБЕР РФ» в течение 4 секунд;
- отображение состояния BLE/Wi-Fi и повторный запуск BLE-рекламы после отключения;
- кнопка `Win/Cmd` отправляет `Win+Space` для смены раскладки Windows.

> [!IMPORTANT]
> Целевой компьютер сам определяет активную раскладку. Перед русским текстом включите `RUS`, перед английским — `ENG`. Кнопка `Win/Cmd` в панели отправляет `Win+Space`.

### Проверенное оборудование

| Компонент | Статус |
|---|---|
| LILYGO T-Dongle-S3 V1.1 | ✅ Проверено на реальном устройстве |
| ESP32-S3, 16 MB Flash | ✅ |
| ST7735 80×160 | ✅ |
| Samsung Galaxy S25 Ultra + Chrome | ✅ BLE-передача русского текста |
| Windows USB HID | ✅ Клавиатура и мышь |

### Сборка в VS Code + PlatformIO

1. Скачайте репозиторий.
2. Откройте папку `firmware/lilygo-cyrillic-v9` в VS Code.
3. Установите расширение PlatformIO.
4. Нажмите **Build** (галочка).
5. Отключите донгл, зажмите боковую кнопку, вставьте его в USB и отпустите через 2–3 секунды.
6. Нажмите **Upload** (стрелка вправо).
7. После `SUCCESS` переподключите донгл без удержания кнопки.

Панель управления BLE исходного проекта:

<https://kolec94.github.io/esp32-s3-hid-dongle/web/control.html>

### Структура

| Путь | Назначение |
|---|---|
| `firmware/lilygo-cyrillic-v9/` | Проверенная русская прошивка PlatformIO |
| `firmware/lilygo-cyrillic-v9/keyboard-dongle.ino` | BLE/Wi-Fi, UTF-8 → ЙЦУКЕН, USB HID |
| `firmware/lilygo-cyrillic-v9/boards/lilygo.h` | Экран, пины и ориентация LILYGO |
| `firmware/lilygo-cyrillic-v9/boards/cyber_star_v8_splash.h` | Готовая заставка 80×160 в RGB565 |
| `firmware/keyboard-dongle/` | Исходная многоплатная версия upstream |

### Происхождение проекта

Это форк проекта [kolec94/esp32-s3-hid-dongle](https://github.com/kolec94/esp32-s3-hid-dongle). Ветка сохраняет исходную историю и авторство. Русская раскладка, персональная конфигурация LILYGO, экранная заставка и исправление надёжности HID добавлены и аппаратно проверены в этой версии.

Используйте устройство только на собственных компьютерах или там, где у вас есть явное разрешение.

---

## English

This fork turns a **LILYGO T-Dongle-S3 V1.1** into a USB HID keyboard and mouse controlled over BLE or Wi-Fi. Its main addition is reliable Russian UTF-8 input mapped to physical **ЙЦУКЕН** keys, with explicit 8 ms key-down and key-up gaps to prevent lost characters.

The firmware was hardware-tested with a Samsung Galaxy S25 Ultra, Chrome, Windows, the onboard ST7735 80×160 display, and the LILYGO T-Dongle-S3 V1.1.

Open `firmware/lilygo-cyrillic-v9` in VS Code with PlatformIO to build and upload it. The target computer must already have the matching Russian or English keyboard layout selected.

Use this device only on computers you own or are explicitly authorized to control.
