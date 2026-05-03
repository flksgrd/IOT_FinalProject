# How the web dashboard works

A short explainer for the rest of the team. The dashboard at **https://dtutrash.netlify.app/** is just one HTML file (`docs/index.html`). This document describes how that file talks to the ESP — because it doesn't, not directly.

> [!abstract] In one sentence
> The dashboard talks to ThingSpeak. The ESP talks to ThingSpeak. They never talk to each other. ThingSpeak is the cloud noticeboard in the middle — both sides read and write to the same numbered fields, and that's the entire IoT pipeline.

## The big idea

```
   [ESP]                                            [Dashboard]
     |                                                   |
     |  HTTP requests                     HTTP requests  |
     |        \                               /          |
     |         v                             v           |
     |              +--------------------+               |
     +------------->|     ThingSpeak     |<--------------+
                    |  channel 3364403   |
                    +--------------------+
```

> [!info] What ThingSpeak actually is
> A cloud database with **8 numbered "fields"** per channel. Anyone with the right key can read or write those fields over the internet. Think of the channel like a struct in shared memory that both the ESP and the browser can read and write.

## ThingSpeak fields = shared variables

We use 5 of the 8 fields:

| Field | Meaning | Who writes it | Who reads it |
|---|---|---|---|
| 1 | Distance (cm) from ultrasonic sensor | ESP | Dashboard |
| 2 | Bags remaining | ESP | Dashboard + ESP |
| 3 | State (0=LOAD, 1=CHECK, 2=CLOSE, 3=EMPTY_ME) | ESP | Dashboard |
| 4 | Bag-full event flag (1 = full) | ESP | IFTTT (via React) |
| 5 | Set-bag-count command | Dashboard (you) | ESP |

> [!tip] The two API keys
> ThingSpeak gates access with a **read key** and a **write key**. They are hard-coded into both the sketch and the HTML — both sides need them to talk to the same channel.
>
> ```c
> // sketch_apr16a.ino
> const char* TS_WRITE_KEY = "DAFOBTGX2VWN91LU";
> const char* TS_READ_KEY  = "U38AWRSBJVBTPPJK";
> ```
>
> ```js
> // docs/index.html
> const READ_KEY  = 'U38AWRSBJVBTPPJK';
> const WRITE_KEY = 'DAFOBTGX2VWN91LU';
> ```

## "HTTP request" is just a URL fetch

> [!info] The mystery, demystified
> Sending an HTTP request to ThingSpeak is just **asking a URL to be opened over the internet** — exactly like loading a webpage in a browser. The reply is a chunk of text (in our case JSON, which is just a structured text format). There are only two operations: reading and writing.

### 1. Reading data — `GET .../feeds.json`

The dashboard wants the latest values, so it asks:

```
https://api.thingspeak.com/channels/3364403/feeds.json?api_key=READ_KEY&results=10
```

ThingSpeak replies with a JSON text blob containing the last 10 entries. The dashboard picks the most recent one that has ESP sensor data in it and updates the screen. It does this on a 5-second timer:

```js
// docs/index.html
setInterval(refresh, REFRESH_MS);   // REFRESH_MS = 5000
```

The ESP does almost the same thing for field 5 (the remote bag-count command), but only every 30 s to leave CPU time for the state machine:

```c
// sketch_apr16a.ino — tsCheckBagCommand()
String url = String("http://api.thingspeak.com/channels/") + TS_CHANNEL_ID
           + "/fields/5/last.json?api_key=" + TS_READ_KEY;
http.begin(client, url);
int code = http.GET();
```

### 2. Writing data — `GET .../update`

Writing is also just a URL. The values you want to write go in as query parameters:

```
http://api.thingspeak.com/update?api_key=WRITE_KEY&field1=15.3&field2=8&field3=1&field4=0
```

That single URL pushes distance, bags remaining, state, and the bag-full flag in one go. The ESP does this every ~16 s (free tier requires ≥ 15 s between writes):

```c
// sketch_apr16a.ino — tsPush()
String url = String("http://api.thingspeak.com/update?api_key=") + TS_WRITE_KEY
           + "&field2=" + String(bagsRemaining)
           + "&field3=" + String((int)CurrentState)
           + "&field4=" + String(pendingBagFull ? 1 : 0);
if (lastDistance >= 0) url += "&field1=" + String(lastDistance, 1);
http.begin(client, url);
http.GET();
```

When you type "20" in the dashboard's input box and click Save, the dashboard fires the same kind of URL — but only with field 5:

```
https://api.thingspeak.com/update?api_key=WRITE_KEY&field5=20
```

Within 30 s the ESP's `tsCheckBagCommand()` poll sees a new entry on field 5, copies the value into `bagsRemaining`, and confirms by writing it back on field 2 on the next push. That is why the toast says **"Set to 20 bags – ESP fetches in <30s"**.

## End-to-end timing

| Event | Where | Cadence |
|---|---|---|
| ESP measures distance, updates state | `Ultra_Sense()` in CHECK state | ~5 s |
| ESP pushes field 1/2/3/4 | `tsPush()` | every 16 s if data changed |
| ESP polls field 5 | `tsCheckBagCommand()` | every 30 s |
| Dashboard re-reads ThingSpeak | `refresh()` | every 5 s |
| Dashboard writes field 5 | `setBags()` | only when you click Save |

> [!example] Worst-case round-trip
> "I clicked Save → bin shows new count on screen":
>
> 30 s (ESP polls field 5) + 16 s (ESP push throttle) + 5 s (dashboard re-read) ≈ **50 seconds**.
>
> In practice it's usually faster — the ESP's poll might be about to fire, and the push doesn't always wait the full 16 s.

## Two non-obvious bits the dashboard does

> [!warning] Pick the latest *ESP* entry, not the latest entry
> When you write a new bag count, ThingSpeak makes a new entry where `field5=20` but `field1/2/3` are all `null` (we didn't write those). That entry is technically "newer" than the last ESP reading, but it isn't a real measurement.
>
> The dashboard pulls the last 10 entries and walks **backwards** to find the most recent one with `field1` populated:
>
> ```js
> const f = feeds.slice().reverse().find(x => x && x.field1 != null);
> ```
>
> Without this filter, every time you set a bag count the dashboard would briefly flash `--` for distance/bags/state.

> [!warning] The `[ONLINE]` / `[OFFLINE]` pill
> The pill in the top right is **not** based on whether the HTTP request succeeded — ThingSpeak will happily serve cached data even when the ESP is dead and on the floor.
>
> Instead it checks how old the last ESP reading is. If the latest ESP entry is older than 90 s, the pill flips to OFFLINE:
>
> ```js
> const STALE_MS = 90 * 1000;          // 3× the ESP's polling cycle
> const age = Date.now() - ts.getTime();
> setStatus(!isNaN(age) && age <= STALE_MS);
> ```

## Push notifications (the IFTTT path)

This works the same way: ThingSpeak in the middle, two parties that never meet directly.

```
[ESP] --writes field4=1--> [ThingSpeak React] --webhook--> [IFTTT applet] --> phone notification
```

ThingSpeak's "React" feature watches a field for a condition (e.g., `field4 = 1`) and fires a webhook URL to IFTTT, which sends the phone notification. The ESP does not know IFTTT exists. The phone does not know the ESP exists.

## File map

| File | What it does |
|---|---|
| `Arduino kode/Kode kladde/sketch_apr16a/sketch_apr16a.ino` | All ESP code: state machine, sensor, motor, WiFi, ThingSpeak push/read |
| `docs/index.html` | The whole web dashboard. One file. Plain HTML/CSS/JavaScript. Hosted by Netlify. |

> [!note] No server of our own
> There is no server **we** maintain. Netlify just serves the static HTML to the browser. ThingSpeak is the only thing in the middle, and it's a free service we don't operate. That's why the system keeps working when our laptops are closed.

> [!abstract] TL;DR for the report
> The dashboard is a single static HTML page. It sends HTTP requests to ThingSpeak to read sensor data (every 5 s) and to write bag-count commands (on demand). The ESP independently pushes sensor data to the same ThingSpeak channel every 16 s and polls for bag-count commands every 30 s. ThingSpeak is the only link between the two; neither side has direct knowledge of the other.
