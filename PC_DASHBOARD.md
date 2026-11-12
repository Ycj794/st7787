# STM32 PC dashboard

The STM32F103C8T6 receives serial data from a CH340 USB-to-UART adapter and
draws on the configured 240x240 ST7789 display. The host can send status
metrics, cycle prepared pictures, or stream a low-resolution preview of the
Windows desktop.

## Wiring

Keep the display wiring configured in `Core/Inc/st7789.h`:

| Display signal | STM32 pin |
| --- | --- |
| SCK | PA5 |
| MOSI / SDA | PA7 |
| CS | PA3 |
| D/C | PA4 |
| RESET | PA6 |
| GND | GND |

Use the display module's documented supply voltage and connect its ground to
the board ground.

USART1 is configured for 115200 baud, 8 data bits, no parity, and 1 stop bit:

| CH340 / USB-to-UART | STM32 |
| --- | --- |
| TXD | PA10 (USART1 RX) |
| RXD | PA9 (USART1 TX; optional for this one-way dashboard) |
| GND | GND |

Use a 3.3 V TTL UART adapter, or verify its logic levels are safe for the
STM32. Do not connect an RS-232 voltage-level port. Some minimum-system boards
already route their USB-to-serial bridge to PA9/PA10; if yours does, no extra
adapter wiring is needed.

## Build and run

1. Build and flash the `st7787` target with Keil.
2. Install Python 3 and the host dependency:

   ```powershell
   py -m pip install -r host\requirements.txt
   ```

3. Find the CH340 COM port in Windows Device Manager, then start the host
   sender for the status dashboard:

   ```powershell
   py host\pc_dashboard.py COM5 --text "Hello from Windows"
   ```

The display updates once per second. The note is ASCII-only and is limited to
24 characters. Stop the sender with Ctrl+C.

## Cycle the supplied pictures

Build and flash the firmware, then run:

```powershell
py host\pc_dashboard.py COM5 --images host\images
```

The host cycles through the eight `.rgb565` pictures in that folder. Each file
is a 120x120 RGB565 image; the firmware enlarges it to fill the 240x240 display
without keeping a full frame in RAM. At 115200 baud, sending each image takes
about 2.5 seconds, plus the 3-second display interval. Change the interval with
`--interval`, for example `--interval 1`. Stop the slideshow with Ctrl+C.

## Preview the Windows desktop

Run:

```powershell
py host\pc_dashboard.py COM5 --screen
```

The PC captures its desktop, scales it proportionally into a 120x120 preview,
and sends frames continuously. At the configured 115200 baud rate, each frame
takes about 2.5 seconds to transfer, so this is a slow preview rather than a
smooth, real-time monitor. Stop it with Ctrl+C. This mode requires Pillow,
installed by the `host\requirements.txt` command above.

## Serial message

In dashboard mode, the host sends one newline-terminated ASCII line per second:

```text
cpu_percent,memory_percent,HH:MM:SS,note
```

In image and screen modes, it sends `IMG\n`, followed by 120 rows of 120 RGB565
pixels. Each pixel is sent high byte first. The CH340 driver creates the COM
port used by the sender script.
