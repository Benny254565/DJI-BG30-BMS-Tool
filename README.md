**Reading a "dead" DJI RS 2 BG30 battery grip with a Raspberry Pi Pico (read-only)**

My DJI RS 2 Pro's BG30 battery grip stopped charging after being left flat. I opened it up to see whether the battery chip had locked itself.

**What's inside:** a 4S Li-ion pack (15.4 V nominal) and a TI BQ40Z307 fuel gauge chip, firmware 1.01. The top connector has 6 pins, including P+, P-, SCL and SDA. The chip talks SMBus at address `0x0B`.

**What I used:**

- A Raspberry Pi Pico 2 W running the Arduino Pico core
- Three wires - see attached pin out diagram
- battery P- to Pico GND - BLACK
- SDA to GP4 - YELLOW
- SCL to GP5 - BLUE

- The Pico's built-in pull-ups at 10 kHz, so no resistors were needed
- **P+ not connected to anything.** It carries pack voltage and will kill the Pico.

**Tip:** the chip only answered while USB-C was plugged into the grip. With no charger attached, it stayed asleep.

### To read flash to Pi pico 2w, open serial monitor at 115200 baud, connect USB to pack and press enter in the serial monitor ###

**What it showed on mine:**

- Pack at 5.15 V, with the cells at 1.32 V, 1.34 V, 1.29 V and 1.19 V
- Safety status and permanent-failure status both clear, so the chip **wasn't locked**
- Sealed, which is its normal mode
- Battery status showing the "fully discharged" and "stop charging/discharging" alarms

So the chip wasn't bricked. It was refusing to charge because every cell was far below the safe minimum of about 2.5 V. No unlocking will fix that. The only options are a very careful per-cell recovery charge or a new grip.

**Safety:** Li-ion cells left this low can develop internal shorts. If you try to recover them, charge one cell at a time at about 100 mA or less, outdoors on a non-flammable surface, and never leave them unattended. Then do a 24-hour hold test, and scrap any cell that loses voltage.

The sketch is below. It only reads and never writes to the battery. It prints a plain-English report: identity, voltages, capacity and health, and status flags with their meaning. Use it at your own risk.


===== DJI BG30 battery chip report (read only) =====

-- Identity --
Manufacturer name          ATL NVT
Device name                BA01HG702
Cell chemistry             2826
Manufacture date           2021-01-04
Serial number              20391
Chip type                  0x4307 (chip type)
Chip firmware              v1.01

-- Live readings --
Pack voltage               5.0 V
Current                    0 mA (0 = no current)
Temperature                23.1 C
Cell 1 voltage             1.3 V
Cell 2 voltage             1.3 V
Cell 3 voltage             1.2 V
Cell 4 voltage             1.2 V

-- Capacity and health --
Charge level               0.0 %
Charge remaining           0.0 mAh
Full charge capacity now   1810.0 mAh
Capacity when new          1950.0 mAh
Design voltage             15.4 V
Health (vs new)            no reply
Charge cycles used         0.0 

-- Status --
Battery status code        0x4AD0
                           - Stop charging alarm
                           - Stop discharging alarm (cells too low)
                           - Remaining charge alarm
                           - Gauge started up OK
                           - In discharge mode
                           - Fully discharged
Safety status              0x00000001 - protection tripped
Permanent failure          0x00000000 - not failed
Operation status code      0x00006F00
Security mode              sealed (normal locked mode)

===== End of report =====
