#### Date: 07/05/2026

| Name               | Study Number | Present | Absent | Note | Signature |
| ------------------ | :----------: | :-----: | :----: | :--: | --------- |
| Jonas Jensen       |   s240324    |         |        |      |           |
| Andreas Jacobsen   |   s241123    |         |        |      |           |
| Sigurd Hestbech    |   s245534    |         |        |      |           |
| Tobias Nilsson     |   s233987    |         |        |      |           |
| Mads Rudolph       |   s246132    |         |        |      |           |
| Anders Falkesgaard |   s245905    |         |        |      |           |
#### Task carried out during the session:
- Anders added an SSD1306 OLED display and a temperature sensor to the bin. Started with a DHT11 (temp + humidity) but switched to a simpler LM35 (temperature only), and updated the ThingSpeak push to send the value on Field 6.
- Jonas fixed the stepper pinout (moved IN4 from D1 to D0 so the OLED can use the I2C SCL line) and adjusted the closing distance in EMPTY_ME.
- The team calibrated the LM35 ADC reference voltage to 2.667 V and added an IIR filter (10-sample average + 30/70 weighted blend) to smooth the temperature readings.
- The MOSFET added in last session was removed because it conflicted with the TX pin and broke Serial logging. Instead the stepper coil pins are now pulled LOW after each move, which gives the same idle-current saving without an extra component.
- Anders redesigned the web dashboard: translated the UI from Danish to English, added a temperature card with a thermometer SVG, and added a status pill that flips to OFFLINE when the ESP hasn't pushed data for >90 s.
- Anders wrote an explainer document (`How the web dashboard works.md`) describing the ESP ↔ ThingSpeak ↔ dashboard flow for the rest of the team and the report.

#### Task to be done before next week:
