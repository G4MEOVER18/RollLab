# RollLab Research

```
 ██████╗ ██╗  ██╗███╗   ███╗███████╗ ██████╗ ██╗   ██╗███████╗██████╗
██╔════╝ ██║  ██║████╗ ████║██╔════╝██╔═══██╗██║   ██║██╔════╝██╔══██╗
██║  ███╗███████║██╔████╔██║█████╗  ██║   ██║██║   ██║█████╗  ██████╔╝
██║   ██║╚════██║██║╚██╔╝██║██╔══╝  ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗
╚██████╔╝     ██║██║ ╚═╝ ██║███████╗╚██████╔╝ ╚████╔╝ ███████╗██║  ██║
 ╚═════╝      ╚═╝╚═╝     ╚═╝╚══════╝ ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝
```

**Rolling Code Vulnerability Lab** — educational Sub-GHz research app for Flipper Zero.

[![Firmware](https://img.shields.io/badge/Firmware-G4MEOVER--FW%20v1.0-blue)](https://github.com/G4MEOVER18/G4MEOVER-FW)
[![API](https://img.shields.io/badge/API-87.1%20mntm--012-green)](https://github.com/G4MEOVER18/G4MEOVER-FW)
[![Author](https://img.shields.io/badge/Author-G4MEOVER18-red)](https://github.com/G4MEOVER18)

---

## What it does

RollLab teaches rolling-code security concepts through four interactive modes:

| Mode | Description |
|------|-------------|
| **Analyze Signal** | Capture a signal and display edge count, timing (min/max/avg), OOK/FSK heuristic, preamble detection |
| **Replay Attack** | Capture → immediately replay. Tests whether a receiver has basic replay protection |
| **Rollback Probe** | Capture reference code, let receiver advance 1–3 steps, replay old code. Tests backward acceptance window |
| **Sync Window Probe** | Capture reference code, advance receiver 5–15 steps, replay old code. Tests resync window size |

### Attack flow (Rollback Probe)

```
1.  [Flipper RX]  Capture code at counter N
2.  [User]        Press keyfob 1–3× normally (receiver advances to N+3)
3.  [Flipper TX]  Replay code N (from step 1)
     → If receiver accepts: ROLLBACK VULNERABLE
     → If receiver rejects: protected
```

---

## Installation

Copy `rolllab_research.fap` to `/apps/Sub-GHz/` on your Flipper Zero SD card.

**Requires:** Flipper Zero with G4MEOVER-FW or Momentum firmware, internal CC1101.

**Frequency:** 433.92 MHz · **Modulation:** AM650 (OOK)

---

## Building from source

```bash
ufbt          # in the rolllab/ directory
```

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) and Momentum mntm-012 SDK.

---

## Related projects

| Repo | Description |
|------|-------------|
| [RollJam](https://github.com/G4MEOVER18/RollJam) | 3-phase jam+capture+replay attack PoC |
| [ProtoPirate](https://github.com/G4MEOVER18/ProtoPirate) | 27+ protocol decoder/encoder for Car Keyfobs |
| [G4MEOVER-FW](https://github.com/G4MEOVER18/G4MEOVER-FW) | Custom Flipper Zero firmware (Momentum fork) |

---

## Legal

This tool is for **authorized security testing, CTF competitions, and educational research only**.  
Use only on hardware you own or have explicit written permission to test.  
The author is not responsible for any misuse.

---

## Support the project

If this tool saves you time or helps your research:

- **Bitcoin:** `39vZWmnUwDReQ15BwqQXzyqVQ6U8LardEf`
- **PayPal:** [paypal.me/Freakbank1](https://paypal.me/Freakbank1)

---

**G4MEOVER18** · [GitHub](https://github.com/G4MEOVER18)
