# Grow a Plant — Cooperative ESP-NOW Game

A cooperative two-player game built with two ESP32 LILYGO T-Displays communicating wirelessly through **ESP-NOW**.

One player provides **Water** and the other provides **Sunlight**. Both players must press the buttons on their devices to supply enough of each resource for a shared plant to grow from a seed into a flower.

![Gameplay Demo](media/demo.gif)

## Design Goal

The project remixes a competitive button-mashing game into a cooperative experience.

Instead of racing against each other, the two players contribute different resources:

- **Player 0:** Water
- **Player 1:** Sunlight

The plant progresses through:

**Seed → Sprout → Stem → Leaves → Flower**

Both resources are required. If one player contributes much more than the other, the plant waits for the missing resource.

For example:

```text
Water:    18 / 20
Sunlight:  5 / 20
```

The plant's growth is limited by the Sunlight value and the display prompts the players that it needs more sunlight.

## Hardware

Each player needs:

- 1 × ESP32 LILYGO / TTGO T-Display
- USB cable or battery for power

No additional sensors are required. The game uses the two built-in buttons on each T-Display.

## Software

- Arduino IDE
- ESP32 Arduino core
- `WiFi.h`
- `esp_now.h`
- `TFT_eSPI.h`

## ESP-NOW Communication

Both ESP32s broadcast their current button-press count using ESP-NOW.

Each message contains:

```cpp
typedef struct {
  uint8_t teamId;
  uint8_t playerId;
  uint8_t isReset;
  uint16_t presses;
} Message;
```

`TEAM_ID` must be identical on both devices.

Each device must have a different `PLAYER_ID`:

```cpp
// Water board
#define PLAYER_ID 0

// Sunlight board
#define PLAYER_ID 1
```

## Cooperative Growth Logic

The two resource totals are stored as:

```cpp
counts[0]   // Water
counts[1]   // Sunlight
```

The combined number of button presses can be calculated with:

```cpp
int teamTotal = counts[0] + counts[1];
```

However, the plant's actual growth uses:

```cpp
int growth = min(counts[0], counts[1]);
```

This makes the game genuinely cooperative. One player cannot complete the plant alone because both Water and Sunlight must reach the target.

## Running the Game

1. Install ESP32 support in Arduino IDE.
2. Open `src/grow_a_plant.ino`.
3. Set the same `TEAM_ID` on both boards.
4. Set one board to:

```cpp
#define PLAYER_ID 0
```

5. Set the other board to:

```cpp
#define PLAYER_ID 1
```

6. Select **ESP32 Dev Module** in Arduino IDE.
7. Select the correct serial port.
8. Upload the sketch to each board.
9. Power both ESP32s near one another.
10. Press either built-in button to contribute your resource.

The displays communicate wirelessly through ESP-NOW without requiring a Wi-Fi network.

## Gameplay

### Player 0 — Water

Each button press adds one unit of Water.

### Player 1 — Sunlight

Each button press adds one unit of Sunlight.

The plant changes visually as the two resources increase:

- **0–3:** Seed
- **4–7:** Sprout
- **8–11:** Stem
- **12–19:** Leaves / flower bud
- **20:** Full flower

If one resource falls behind, the screen displays:

```text
Needs WATER!
```

or:

```text
Needs SUNLIGHT!
```

Once both players reach the target, the plant reaches **FULL BLOOM**.

Either player can then press a button to begin a new round, and the reset is sent to both devices through ESP-NOW.

## Wireless Reliability

Each ESP32 periodically rebroadcasts its current count every `HEARTBEAT_MS` milliseconds:

```cpp
#define HEARTBEAT_MS 250
```

This helps the two displays stay synchronized if an ESP-NOW packet is lost.

## Project Structure

```text
espnow-grow-a-plant/
├── README.md
├── src/
│   └── grow_a_plant.ino
└── media/
    ├── demo.gif
    ├── gameplay.jpg
    └── boards.jpg
```

## Documentation

![Two-Player Setup](media/boards.jpg)

![Gameplay](media/gameplay.jpg)

For more information about the design process, technical implementation, and development of the cooperative mechanic, see the accompanying blog post:

[Read the full project blog post](YOUR_NOTION_BLOG_URL)
