# IOT Final Project — Smart Trash Bin

A WiFi-connected smart trash bin built on ESP8266 that automatically closes a trash bag when full, tracks bag inventory, and pushes notifications to your phone.

DTU project by Anders Falkesgaard, Mads Rudolph, Jonas Jensen, Tobias Nilsson, Sigurd Hestbech, Andreas Jacobsen.

## Features
- Ultrasonic fill detection — auto-closes the bag when contents reach ≤10 cm from the sensor
- Stepper-driven bag-closer (28BYJ-48 + ULN2003)
- Bag inventory counter with "low bags" warning
- Push notifications to phone via ThingSpeak React → IFTTT
- Web dashboard at **https://dtutrash.netlify.app/** to view status and set bag count remotely
- Live data + history graphs on ThingSpeak

## IoT pipeline

**ThingSpeak channel 3364403** — fields:

| Field | Meaning |
|---|---|
| 1 | Distance (cm) |
| 2 | Bags remaining |
| 3 | Current state (0=LOAD, 1=CHECK, 2=CLOSE, 3=EMPTY_ME) |
| 4 | Bag-full event (1 = full, 0 = otherwise) |
| 5 | Set bag count (write here from phone to update inventory) |

Push throttled to 16 s (free-tier minimum), Field 5 polled every 30 s.

**IFTTT applets** (triggered by ThingSpeak React → ThingHTTP):
- `bag_full` — when Field 4 = 1 → "🗑️ Trash bag is full — please replace it"
- `low_bags` — when Field 2 < 3 → "⚠️ Running low on trash bags"

**Web dashboard** (`docs/index.html`, deployed via Netlify):
- Shows current bags, distance, state
- Input field for setting bag count (writes to Field 5 → ESP picks it up within 30 s)
- Auto-refreshes every 5 s

