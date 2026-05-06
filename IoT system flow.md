# IoT system flow

The four moving parts of the system — the ESP8266 in the bin, the ThingSpeak cloud channel, the web dashboard at https://dtutrash.netlify.app/, and the IFTTT push-notification path — never talk to each other directly. **ThingSpeak is the only thing in the middle:** every other component reads from or writes to its numbered fields, and that is the entire pipeline.

```mermaid
flowchart LR
    ESP["🗑️ ESP8266<br/>(smart bin firmware)"]
    TS["☁️ ThingSpeak<br/>channel 3364403"]
    DASH["📱 Web dashboard<br/>dtutrash.netlify.app"]
    REACT{{"ThingSpeak React<br/>rule trigger"}}
    IFTTT["IFTTT webhook<br/>(bag_full · low_bags)"]
    PHONE["📲 Phone<br/>push notification"]

    ESP -- "sensor data<br/>fields 1, 2, 3, 4, 6<br/>every 16 s" --> TS
    TS -- "bag-count command<br/>field 5<br/>polled every 30 s" --> ESP

    TS -- "live readings<br/>fields 1, 2, 3, 6<br/>every 5 s" --> DASH
    DASH -- "set bag count<br/>field 5<br/>on Save" --> TS

    TS --> REACT
    REACT -- "field 4 = 1" --> IFTTT
    REACT -- "field 2 < 3" --> IFTTT
    IFTTT -- "phone notification" --> PHONE

    classDef esp    fill:#c9f31d,stroke:#0a0a0a,stroke-width:2px,color:#0a0a0a;
    classDef cloud  fill:#f4ede1,stroke:#0a0a0a,stroke-width:2px,color:#0a0a0a;
    classDef phone  fill:#ffffff,stroke:#0a0a0a,stroke-width:2px,color:#0a0a0a;
    classDef alert  fill:#ff3b1c,stroke:#0a0a0a,stroke-width:2px,color:#ffffff;
    class ESP esp;
    class TS,REACT cloud;
    class DASH,PHONE phone;
    class IFTTT alert;
```

## Field map

| Field | Meaning | Written by | Read by |
|---|---|---|---|
| 1 | Distance (cm) from ultrasonic sensor | ESP | Dashboard |
| 2 | Bags remaining | ESP | Dashboard, IFTTT React |
| 3 | State (0=LOAD, 1=CHECK, 2=CLOSE, 3=EMPTY_ME) | ESP | Dashboard |
| 4 | Bag-full event (1 = full) | ESP | IFTTT React |
| 5 | Set-bag-count command | Dashboard (user) | ESP |
| 6 | Temperature (°C) from LM35 | ESP | Dashboard |

## Timing summary

| Direction | Cadence |
|---|---|
| ESP → ThingSpeak (push) | every 16 s, when data has changed |
| ESP ← ThingSpeak (poll field 5) | every 30 s |
| Dashboard ← ThingSpeak (refresh) | every 5 s |
| Dashboard → ThingSpeak (set bag count) | on user click |
| ThingSpeak React → IFTTT | event-driven |
| IFTTT → phone | event-driven |
