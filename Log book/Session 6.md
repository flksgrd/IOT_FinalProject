#### Date: 30/04/2026

| Name               | Study Number | Present | Absent | Note | Signature |
| ------------------ | :----------: | :-----: | :----: | :--: | --------- |
| Jonas Jensen       |   s240324    |    x    |        |      |           |
| Andreas Jacobsen   |   s241123    |    x    |        |      |           |
| Sigurd Hestbech    |   s245534    |    x    |        |      |           |
| Tobias Nilsson     |   s233987    |    x    |        |      |           |
| Mads Rudolph       |   s246132    |    x    |        |      |           |
| Anders Falkesgaard |   s245905    |    x    |        |      |           |
We started the day in Lyngby where we heard the presentation and had a status meeting with our mentor. Afterwards we drove to Ballerup, where we have access to a coworking space as well as two 3D-printers. 
#### Task carried out during the session:
- Mads integrated ThingSpeak and IFTTT into the firmware: the bin now pushes bag count, distance, state and bag-full events to the cloud, and reads bag-count updates from the phone via Field 5.
- Set up the ThingSpeak channel with ThingHTTP entries and React rules so a "bag full" or "low bags" event triggers a push notification on the phone via IFTTT.
- Built and deployed a small web dashboard at https://dtutrash.netlify.app/ for viewing live bag count, distance and state, with an input field to set the bag count remotely.
- Updated the README to reflect the current IoT pipeline and the new dashboard.
- Jonas and Andreas worked on optimizing the code (adjusted stepper distance, code cleanup) and troubleshooting the closing mechanism.
- Added a MOSFET on the stepper-driver power line to make it more efficient and reduce idle current.
- Tobias started designing the custom PCB-board. 
