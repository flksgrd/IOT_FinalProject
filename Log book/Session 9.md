#### Date: 03/05/2026

| Name               | Study Number | Present | Absent | Note | Signature |
| ------------------ | :----------: | :-----: | :----: | :--: | --------- |
| Jonas Jensen       |   s240324    |    x    |        |      |           |
| Andreas Jacobsen   |   s241123    |    x    |        |      |           |
| Sigurd Hestbech    |   s245534    |    x    |        |      |           |
| Tobias Nilsson     |   s233987    |    x    |        |      |           |
| Mads Rudolph       |   s246132    |    x    |        |      |           |
| Anders Falkesgaard |   s245905    |    x    |        |      |           |
#### Task carried out during the session:
- Started setting up the Poster with the different relevant sections (State, Schematic, Photo, ThingsSpeak)
- Anders calibrated the LM35 ADC reference voltage to 2.667 V via an external thermometer and datasheet.
- Andreas and Anders added an IIR filter (10-sample average + 30/70 weighted blend) to smooth the temperature readings. It fluctuated quite a bit due to imprecise ADC on the ESP8266, it is also very affected by the noise from the Wifi-module.
- Jonas fixed the stepper pinout (moved IN4 from D1 to D0 so the OLED can use the I2C SCL line) and adjusted the closing distance in EMPTY_ME.
- Mads wrote an explainer document (`How the web dashboard works.md`) describing the ESP ↔ ThingSpeak ↔ dashboard flow for the rest of the team and the report.
- Tobias' first PCB is finished, but lacks full functionality, he will redesign and go for a v2
- Sigurd made more revisions to CAD and setup more files for print.