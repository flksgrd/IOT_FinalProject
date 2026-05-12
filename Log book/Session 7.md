#### Date: 01/05/2026

| Name               | Study Number | Present | Absent | Note | Signature |
| ------------------ | :----------: | :-----: | :----: | :--: | --------- |
| Jonas Jensen       |   s240324    |    x    |        |      |           |
| Andreas Jacobsen   |   s241123    |    x    |        |      |           |
| Sigurd Hestbech    |   s245534    |    x    |        |      |           |
| Tobias Nilsson     |   s233987    |    x    |        |      |           |
| Mads Rudolph       |   s246132    |    x    |        |      |           |
| Anders Falkesgaard |   s245905    |    x    |        |      |           |
#### Task carried out during the session:
- The mechanism is having trouble to get a proper grip on the strings of the bag, Sigurd have changed the rolls to accommodate two O-rings, we will test tomorrow after the print is done.
- The MOSFET added in last session was removed because it conflicted with the TX pin and broke Serial logging. Instead the stepper coil pins are now pulled LOW after each move, which gives the same idle-current saving without an extra component.
- Mads redesigned the web dashboard: translated the UI from Danish to English, added a temperature card with a thermometer SVG, and added a status pill that flips to OFFLINE when the ESP hasn't pushed data for >90 s.