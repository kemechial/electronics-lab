# Reference documents

Experiment READMEs cite these documents by tag and page, e.g. `RM p.367`. The PDFs are vendor documents and are **not stored in this repository** (`docs/ref/*.pdf` and `docs/ref/*.PDF` are gitignored). Download them yourself into this folder under the file names below.

## Where to get the documents

The document number and revision of each local copy were read from the PDF page footers. Page numbers in the READMEs refer to these revisions; a different revision may have different page numbers.

| Tag | Document | Number | Revision (from footer) | Publisher | Local file name |
|-----|----------|--------|------------------------|-----------|-----------------|
| DS | STM32F401xB/STM32F401xC datasheet (production data) | DS9716 | Rev 11 | STMicroelectronics | `DS_stm32f401cc.pdf` |
| RM | STM32F401xB/C and STM32F401xD/E reference manual | RM0368 | Rev 6 | STMicroelectronics | `rm0368-stm32f401xbc-and-stm32f401xde-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` |
| PM | STM32 Cortex-M4 MCUs and MPUs programming manual | PM0214 | Rev 10 | STMicroelectronics | `pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf` |
| AN2834 | How to optimize the ADC accuracy in the STM32 MCUs | AN2834 | Rev 10 | STMicroelectronics | `an2834-how-to-optimize-the-adc-accuracy-in-the-stm32-mcus-stmicroelectronics.pdf` |
| AN5537 | How to use ADC oversampling techniques to improve signal-to-noise ratio on STM32 MCUs | AN5537 | Rev 2 | STMicroelectronics | `an5537-how-to-use-adc-oversampling-techniques-to-improve-signaltonoise-ratio-on-stm32-mcus-stmicroelectronics.pdf` |
| DS-F103 | STM32F103x8/xB datasheet (medium-density) | DS5319 | Rev 20 (July 2025) | STMicroelectronics | `DS_stm32f103c8.pdf` |
| RM0008 | STM32F101/102/103/105/107 reference manual | RM0008 | Rev 21 | STMicroelectronics | `rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf` |
| Q-2N2222A | 2N2222A general purpose NPN transistor, TO-92 (3 pages); **the datasheet for the exp06 part**, pin order 1 = E, 2 = B, 3 = C | — | Version 2026-03-10 | Diotec Semiconductor | `2n2222a.pdf` |
| P2N2222A | P2N2222A NPN transistor; **family reference only**, different manufacturer and pin order (1 = C, 3 = E), not valid for the exp06 part | — | — | onsemi | not in docs/ref |
| SSD1306 | SSD1306 OLED driver controller, "Advance Information" | SSD1306 | Rev 1.1 (Apr 2008) | Solomon Systech | `SSD1306.pdf` |
| DHT | DHT11 Humidity & Temperature Sensor, translated datasheet (9 pages) | none | **none in footer** (footer is only "Page \| N"). The file metadata has a create date of 2010-07-30; that is not a revision and may belong to an embedded object. | OSEPP translation of the manufacturer (Aosong) datasheet; disclaimer on p.9 | `DHT11.PDF` |

How to find them:

- **ST documents** (DS9716, RM0368, PM0214, AN2834, AN5537): search the document number on st.com. The datasheet is also on the STM32F401CC product page under Documentation. Check that the revision in the footer matches the table, or note the new revision here.
- **SSD1306:** the Solomon Systech datasheet is distributed by OLED module vendors; search "SSD1306 Rev 1.1".
- **DHT11:** the OSEPP translation is distributed by sensor vendors; search "DHT11 Humidity & Temperature Sensor OSEPP". Without a revision number, check that the copy has 9 pages with timing figures 3–5 on pp.6–8.

## Used by

| Experiment | Documents and pages read |
|------------|--------------------------|
| exp02_dht11 | DHT pp.3–8; DS p.26 (Table 4), p.44 (Table 9); RM pp.95, 154, 333, 364, 367 |
| exp06_transistor_led | DS5319: Table 5 p.29 (PB0 = LQFP48 pin 18, TIM3_CH3, I/O not FT), Tables 6–7 p.37 (absolute maximum, IIO ±25 mA), Table 37 p.64 (output voltage, ±8 mA, PC13–PC15 ±3 mA); RM0008 Table 44 p.178 (TIM3 remap); Q-2N2222A p.1 (pin order, limits, VCEsat), p.2 (hFE, RthJA); P2N2222A Fig. 11 as family reference for the VBE = 0.7 V assumption |
