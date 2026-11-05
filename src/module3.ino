/*
  Module 3: Cooperative Grow-a-Plant Game
  ESP32 LILYGO T-Display + ESP-NOW

  Two players work together to grow a plant.

  PLAYER 0 = WATER
  PLAYER 1 = SUNLIGHT

  Each player's button presses contribute one resource.
  The plant needs BOTH resources to grow.

  Growth stages:
    Seed -> Sprout -> Stem -> Leaves -> Flower

  Both ESP32s communicate their current resource counts
  to each other using ESP-NOW.
*/

#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>


// ============================================================
// TEAM SETTINGS
// ============================================================

// IMPORTANT:
// Both partners use the SAME TEAM_ID.
#define TEAM_ID 1

// IMPORTANT:
// Change this on each board.
//
// Player 0 = Water
// Player 1 = Sunlight
#define PLAYER_ID 0

#define MAX_PLAYERS 2

// Each resource needs this many presses for full growth.
#define RESOURCE_TARGET 20

#define BUTTON_A 0
#define BUTTON_B 35

// Resend our state every 250 ms in case a packet is lost.
#define HEARTBEAT_MS 250


// ============================================================
// ESP-NOW
// ============================================================

// Broadcast to every nearby ESP32.
// TEAM_ID prevents other groups' messages from affecting us.
uint8_t broadcastAddress[] = {
  0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF
};


// ============================================================
// DISPLAY
// ============================================================

TFT_eSPI tft = TFT_eSPI();
uint16_t soilColor;

// ============================================================
// GAME STATE
// ============================================================

// counts[0] = WATER presses
// counts[1] = SUNLIGHT presses
volatile uint16_t counts[MAX_PLAYERS] = {0, 0};

// Tracks whether we have heard from each board.
volatile bool seen[MAX_PLAYERS] = {false, false};

// Set when the other ESP32 requests a new round.
volatile bool resetRequested = false;

// Only redraw when something changes.
volatile bool needsRedraw = true;


// Has the flower fully bloomed?
bool gameOver = false;

// Used so a button press immediately after winning
// does not accidentally reset the game.
uint32_t gameOverAt = 0;

// Last time we sent our heartbeat.
uint32_t lastHeartbeat = 0;


// ============================================================
// BUTTON STATE
// ============================================================

bool lastA = HIGH;
bool lastB = HIGH;

uint32_t lastPressTime = 0;


// ============================================================
// MESSAGE STRUCTURE
// ============================================================

// This is the data sent between the two ESP32s.
typedef struct {

  uint8_t teamId;

  uint8_t playerId;

  // 1 means "reset the game."
  uint8_t isReset;

  // Number of presses from this player.
  uint16_t presses;

} Message;


// ============================================================
// RECEIVE ESP-NOW MESSAGE
// ============================================================

void onReceive(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int len
) {

  // Ignore malformed messages.
  if (len != sizeof(Message)) {
    return;
  }


  Message msg;

  memcpy(
    &msg,
    data,
    sizeof(msg)
  );


  // Ignore another group's ESP32.
  if (msg.teamId != TEAM_ID) {
    return;
  }


  // Ignore invalid player numbers.
  if (msg.playerId >= MAX_PLAYERS) {
    return;
  }


  // Ignore messages coming from ourselves.
  if (msg.playerId == PLAYER_ID) {
    return;
  }


  // ----------------------------------------------------------
  // RESET MESSAGE
  // ----------------------------------------------------------

  if (msg.isReset) {

    resetRequested = true;

  }

  // ----------------------------------------------------------
  // NORMAL RESOURCE UPDATE
  // ----------------------------------------------------------

  else {

    counts[msg.playerId] =
      min(
        (int)msg.presses,
        RESOURCE_TARGET
      );

    seen[msg.playerId] = true;
  }


  needsRedraw = true;
}


// ============================================================
// SET UP ESP-NOW
// ============================================================

void setupEspNow() {

  WiFi.mode(WIFI_STA);


  if (esp_now_init() != ESP_OK) {

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_RED);

    tft.drawString(
      "ESP-NOW init failed",
      10,
      60,
      2
    );

    while (true) {
      delay(1000);
    }
  }


  // Tell ESP-NOW which function handles incoming messages.
  esp_now_register_recv_cb(onReceive);


  // Add the broadcast address as a peer.
  esp_now_peer_info_t peer = {};

  memcpy(
    peer.peer_addr,
    broadcastAddress,
    6
  );

  peer.channel = 0;

  peer.encrypt = false;

  esp_now_add_peer(&peer);
}


// ============================================================
// SEND OUR RESOURCE COUNT
// ============================================================

void sendState(bool reset = false) {

  Message msg;

  msg.teamId = TEAM_ID;

  msg.playerId = PLAYER_ID;

  msg.isReset =
    reset ? 1 : 0;

  msg.presses =
    counts[PLAYER_ID];


  esp_now_send(
    broadcastAddress,
    (uint8_t *)&msg,
    sizeof(msg)
  );
}


// ============================================================
// HELPER: TEAM TOTAL
// ============================================================

int getTeamTotal() {

  return
    counts[0]
    +
    counts[1];
}


// ============================================================
// HELPER: BALANCED GROWTH
// ============================================================

// A plant needs BOTH water and sunlight.
//
// Therefore, the resource that is currently lower determines
// how far the plant has grown.
//
// Example:
//
// Water = 15
// Sun   = 7
//
// growth = 7
//
// This prevents one player from completing the game alone.

int getGrowthAmount() {

  return min(
    (int)counts[0],
    (int)counts[1]
  );
}


// ============================================================
// DRAW RESOURCE BAR
// ============================================================

void drawResourceBar(
  int x,
  int y,
  int w,
  int h,
  int amount,
  uint16_t color,
  String label,
  bool isYourResource
) {

  // ----------------------------------------------------------
  // LABEL
  // ----------------------------------------------------------

  tft.setTextColor(
    color,
    TFT_BLACK
  );

  tft.drawString(
    label + " " +
    String(amount) +
    "/" +
    String(RESOURCE_TARGET),
    x,
    y,
    1
  );


  // ----------------------------------------------------------
  // BAR
  // ----------------------------------------------------------

  int fillWidth =
    constrain(
      (amount * w)
      / RESOURCE_TARGET,
      0,
      w
    );


  int barY =
    y + 10;


  // Filled portion
  tft.fillRect(
    x,
    barY,
    fillWidth,
    h,
    color
  );


  // Empty portion
  tft.fillRect(
    x + fillWidth,
    barY,
    w - fillWidth,
    h,
    TFT_BLACK
  );


  // Highlight YOUR resource with a white outline.
  tft.drawRect(
    x - 1,
    barY - 1,
    w + 2,
    h + 2,
    isYourResource
      ? TFT_WHITE
      : TFT_DARKGREY
  );
}


// ============================================================
// DRAW PLANT
// ============================================================

void drawPlant(int growth) {

  // ----------------------------------------------------------
  // SOIL
  // ----------------------------------------------------------

  tft.fillRect(
    0,
    112,
    240,
    23,
    soilColor
  );


  // ==========================================================
  // STAGE 1: SEED
  //
  // growth = 0-3
  // ==========================================================

  if (growth < 4) {

    // Seed
    tft.fillCircle(
      120,
      107,
      5,
      TFT_ORANGE
    );

    return;
  }


  // ==========================================================
  // STAGE 2: SPROUT
  //
  // growth = 4-7
  // ==========================================================

  if (growth < 8) {

    // Small stem
    tft.drawLine(
      120,
      111,
      120,
      96,
      TFT_GREEN
    );

    tft.drawLine(
      121,
      111,
      121,
      96,
      TFT_GREEN
    );


    // Tiny leaves
    tft.fillCircle(
      114,
      96,
      4,
      TFT_GREEN
    );

    tft.fillCircle(
      127,
      93,
      4,
      TFT_GREEN
    );

    return;
  }


  // ==========================================================
  // STAGE 3: STEM
  //
  // growth = 8-11
  // ==========================================================

  if (growth < 12) {

    tft.drawLine(
      120,
      111,
      120,
      72,
      TFT_GREEN
    );

    tft.drawLine(
      121,
      111,
      121,
      72,
      TFT_GREEN
    );


    // Small side shoots
    tft.drawLine(
      120,
      91,
      108,
      84,
      TFT_GREEN
    );

    tft.drawLine(
      121,
      82,
      134,
      75,
      TFT_GREEN
    );

    return;
  }


  // ==========================================================
  // STAGE 4: LEAVES
  //
  // growth = 12-19
  // ==========================================================

  if (growth < RESOURCE_TARGET) {

    // Tall stem
    tft.drawLine(
      120,
      111,
      120,
      55,
      TFT_GREEN
    );

    tft.drawLine(
      121,
      111,
      121,
      55,
      TFT_GREEN
    );


    // Left branch
    tft.drawLine(
      120,
      91,
      105,
      82,
      TFT_GREEN
    );


    // Left leaf
    tft.fillCircle(
      101,
      79,
      7,
      TFT_GREEN
    );


    // Right branch
    tft.drawLine(
      121,
      78,
      138,
      69,
      TFT_GREEN
    );


    // Right leaf
    tft.fillCircle(
      143,
      66,
      7,
      TFT_GREEN
    );


    // Small flower bud
    tft.fillCircle(
      120,
      53,
      5,
      TFT_YELLOW
    );

    return;
  }


  // ==========================================================
  // STAGE 5: FULL FLOWER
  //
  // growth = 20
  // ==========================================================

  // Stem
  tft.drawLine(
    120,
    111,
    120,
    51,
    TFT_GREEN
  );

  tft.drawLine(
    121,
    111,
    121,
    51,
    TFT_GREEN
  );


  // ----------------------------------------------------------
  // LEAVES
  // ----------------------------------------------------------

  tft.drawLine(
    120,
    90,
    104,
    81,
    TFT_GREEN
  );

  tft.fillCircle(
    99,
    78,
    8,
    TFT_GREEN
  );


  tft.drawLine(
    121,
    79,
    138,
    70,
    TFT_GREEN
  );

  tft.fillCircle(
    144,
    67,
    8,
    TFT_GREEN
  );


  // ----------------------------------------------------------
  // FLOWER PETALS
  // ----------------------------------------------------------

  // Top
  tft.fillCircle(
    120,
    39,
    8,
    TFT_MAGENTA
  );


  // Left
  tft.fillCircle(
    109,
    49,
    8,
    TFT_MAGENTA
  );


  // Right
  tft.fillCircle(
    131,
    49,
    8,
    TFT_MAGENTA
  );


  // Bottom-left
  tft.fillCircle(
    113,
    59,
    8,
    TFT_MAGENTA
  );


  // Bottom-right
  tft.fillCircle(
    127,
    59,
    8,
    TFT_MAGENTA
  );


  // Flower center
  tft.fillCircle(
    120,
    50,
    7,
    TFT_YELLOW
  );
}


// ============================================================
// DETERMINE GROWTH STAGE NAME
// ============================================================

String getStageName(int growth) {

  if (growth < 4) {
    return "SEED";
  }

  if (growth < 8) {
    return "SPROUT";
  }

  if (growth < 12) {
    return "STEM";
  }

  if (growth < RESOURCE_TARGET) {
    return "LEAVES";
  }

  return "FLOWER";
}


// ============================================================
// DRAW SCREEN
// ============================================================

void drawScreen() {

  // We always know our own board exists.
  seen[PLAYER_ID] = true;


  int water =
    counts[0];

  int sunlight =
    counts[1];


  int teamTotal =
    getTeamTotal();


  int growth =
    getGrowthAmount();


  // Clear screen.
  tft.fillScreen(
    TFT_BLACK
  );


  // ----------------------------------------------------------
  // RESOURCE BARS
  // ----------------------------------------------------------

  drawResourceBar(
    5,
    3,
    90,
    7,
    water,
    TFT_CYAN,
    "WATER",
    PLAYER_ID == 0
  );


  drawResourceBar(
    145,
    3,
    90,
    7,
    sunlight,
    TFT_YELLOW,
    "SUN",
    PLAYER_ID == 1
  );


  // ----------------------------------------------------------
  // PLANT
  // ----------------------------------------------------------

  drawPlant(
    growth
  );


  // ----------------------------------------------------------
  // STATUS TEXT
  // ----------------------------------------------------------

  tft.setTextDatum(
    MC_DATUM
  );


  // ----------------------------------------------------------
  // GAME COMPLETE
  // ----------------------------------------------------------

  if (gameOver) {

    tft.setTextColor(
      TFT_WHITE,
      TFT_BLACK
    );

    tft.drawString(
      "FULL BLOOM!",
      120,
      29,
      2
    );


    tft.setTextColor(
      TFT_GREENYELLOW,
      TFT_BLACK
    );

    tft.drawString(
      "Press for new seed",
      120,
      126,
      1
    );
  }


  // ----------------------------------------------------------
  // STILL GROWING
  // ----------------------------------------------------------

  else {

    tft.setTextColor(
      TFT_WHITE,
      TFT_BLACK
    );

    tft.drawString(
      getStageName(growth),
      120,
      29,
      1
    );


    // --------------------------------------------------------
    // COOPERATIVE FEEDBACK
    //
    // Tell the players which resource is currently limiting
    // the plant's growth.
    // --------------------------------------------------------

    if (water < sunlight) {

      tft.setTextColor(
        TFT_CYAN,
        soilColor
      );

      tft.drawString(
        "Needs WATER!",
        120,
        126,
        1
      );

    }

    else if (sunlight < water) {

      tft.setTextColor(
        TFT_YELLOW,
        soilColor
      );

      tft.drawString(
        "Needs SUNLIGHT!",
        120,
        126,
        1
      );

    }

    else {

      tft.setTextColor(
        TFT_WHITE,
        soilColor
      );

      tft.drawString(
        "Keep growing together!",
        120,
        126,
        1
      );
    }
  }


  tft.setTextDatum(
    TL_DATUM
  );
}


// ============================================================
// START NEW ROUND
// ============================================================

void startNewRound() {

  for (
    int i = 0;
    i < MAX_PLAYERS;
    i++
  ) {

    counts[i] = 0;
  }


  gameOver = false;

  needsRedraw = true;
}


// ============================================================
// CHECK FOR FULL BLOOM
// ============================================================

void checkForBloom() {

  if (gameOver) {
    return;
  }


  // BOTH resources must reach the target.
  if (
    counts[0] >= RESOURCE_TARGET
    &&
    counts[1] >= RESOURCE_TARGET
  ) {

    gameOver = true;

    gameOverAt = millis();

    needsRedraw = true;
  }
}


// ============================================================
// BUTTON EDGE DETECTION
// ============================================================

bool buttonPressed(
  uint8_t pin,
  bool &last
) {

  bool now =
    digitalRead(pin);


  // Detect HIGH -> LOW transition.
  bool pressed =
    (
      last == HIGH
      &&
      now == LOW
    );


  last = now;


  return pressed;
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);


  pinMode(
    BUTTON_A,
    INPUT_PULLUP
  );


  // GPIO 35 has an external pull-up
  // on the LILYGO board.
  pinMode(
    BUTTON_B,
    INPUT
  );


  // ----------------------------------------------------------
  // DISPLAY
  // ----------------------------------------------------------

  tft.init();

  soilColor = tft.color565(100, 55, 25);

  tft.setRotation(1);

  tft.fillScreen(
    TFT_BLACK
  );


  // ----------------------------------------------------------
  // ESP-NOW
  // ----------------------------------------------------------

  setupEspNow();


  // Our own player exists.
  seen[PLAYER_ID] = true;


  // Immediately announce ourselves.
  sendState();


  drawScreen();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  // ----------------------------------------------------------
  // HANDLE RESET FROM OTHER ESP32
  // ----------------------------------------------------------

  if (resetRequested) {

    resetRequested = false;

    startNewRound();
  }


  // ----------------------------------------------------------
  // READ BUTTONS
  // ----------------------------------------------------------

  bool a =
    buttonPressed(
      BUTTON_A,
      lastA
    );


  bool b =
    buttonPressed(
      BUTTON_B,
      lastB
    );


  // Either physical button counts.
  //
  // 20 ms debounce prevents accidental double presses.
  bool pressed =
    (a || b)
    &&
    (
      millis()
      - lastPressTime
      > 20
    );


  // ----------------------------------------------------------
  // BUTTON PRESSED
  // ----------------------------------------------------------

  if (pressed) {

    lastPressTime =
      millis();


    // --------------------------------------------------------
    // GROWING
    // --------------------------------------------------------

    if (!gameOver) {

      // Only allow our resource to increase to the target.
      if (
        counts[PLAYER_ID]
        < RESOURCE_TARGET
      ) {

        counts[PLAYER_ID] =
          counts[PLAYER_ID] + 1;


        // Send the new value to our partner.
        sendState();


        needsRedraw =
          true;
      }
    }


    // --------------------------------------------------------
    // NEW ROUND
    // --------------------------------------------------------

    else if (
      millis()
      - gameOverAt
      > 1500
    ) {

      // Reset our own board.
      startNewRound();


      // Tell partner to reset too.
      sendState(true);
    }
  }


  // ----------------------------------------------------------
  // CHECK WHETHER PLANT BLOOMED
  // ----------------------------------------------------------

  checkForBloom();


  // ----------------------------------------------------------
  // HEARTBEAT
  //
  // Periodically resend our resource count.
  // If an ESP-NOW packet was dropped, the other board will
  // eventually receive the correct value.
  // ----------------------------------------------------------

  if (
    !gameOver
    &&
    millis()
    - lastHeartbeat
    > HEARTBEAT_MS
  ) {

    lastHeartbeat =
      millis();

    sendState();
  }


  // ----------------------------------------------------------
  // REDRAW
  // ----------------------------------------------------------

  if (needsRedraw) {

    needsRedraw =
      false;

    drawScreen();
  }
}
