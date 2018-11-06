/*
     GamingStein Control via Telegram Bot

    The ESP8266 provides enough power to answer the requests of a few users via Telegram, so we do not have to use a separate server for this project.
    However, due to the Telegram requests, the main loop becomes quite slow so I had to work with an interrupt timer. Even though this solution is
    not really lovely, it works quite fine if you can settle for a few seconds response time of the bot. If you want the bot to be really fast and
    efficient, you propably should use a seperate server.

    In terms of hardware, 5 WS2812B LEDs were used, an ESP8266 chip (ESP12F), an AMS1117 and a bit of copper wire. For more information, check
    out the README file in the GitHub repo, or the documentation on the codeforheilbronn.de website.

*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <Ticker.h>

/*
    Wifi, bot and OTA settings (which are defined in the config.h file)
*/

#include "config.h"
const char  ssid1[]      = SSID_1;
const char  password1[]  = PASS_1;
const char  ssid2[]      = SSID_2;
const char  password2[]  = PASS_2;
const char  BotToken[]   = BOT_TOKEN;
const char  adminID[]    = ADMIN_ID;
const char  User1[]      = USER_ID1;
const char  User2[]      = USER_ID2;
const char  User3[]      = USER_ID3;
const char  Hostname[]   = HOSTNAME;
const char  OTAPass[]    = OTAPASS;

/*
   Here, you can change the interval of the interrupt timer in seconds. Maybe, you have to test a few values to find the perfect one for you.
   If the interval is too slow, you can see the updates to the LEDs; if the interval is too fast, the ESP crashes.
*/

float updateAnimation = 0.52;

int Animation = 0;
int currentAnimation = 0;

/*
   Here, you can set the custom keyboard of the Telegram bot. You can easily add new colours – but remember to add them as well down below.
*/

String keyboardJson = "[[\"Deep blue\", \"Blue\", \"Pink\"]," //
                      " [\"Colour change\", \"Turquoise\", \"Purple\"],"
                      " [\"Dark red\", \"Green\", \"Orange\"],"
                      " [\"Warm white\", \"Random\", \"White\"],"
                      " [\"Spectrum cycling\", \"Gold\", \"Rose\"],"
                      " [\"Turn off\"],"
                      " [\"B\n10\", \"R\n20\", \"I\n30\", \"G\n40\", \"H\n50\", \"T\n60\", \"N\n70\", \"E\n80\", \"S\n90\", \"S\n100\"]]";

/*
    FastLED settings
*/

#define LED_PIN     2
#define NUM_LEDS    5
#define BRIGHTNESS  51
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 5000

CRGBPalette16 currentPalette;
TBlendType    currentBlending;

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

WiFiClientSecure client;
UniversalTelegramBot bot(BotToken, client);

ESP8266WiFiMulti wifiMulti;

int Bot_mtbs = 10;    // mean time between scan messages
long Bot_lasttime;    // last time messages' scan has been done
bool Start = false;

Ticker interrupt;

void setup() {
  Serial.begin (115200);

  delay( 500 );   // power-up safety delay

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid1, password1);
  wifiMulti.addAP(ssid2, password2);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  currentPalette = RainbowColors_p;
  currentBlending = LINEARBLEND;

  delay(100);

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  bot.longPoll = 60;
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname(Hostname);
  ArduinoOTA.setPassword(OTAPass);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  interrupt.attach(updateAnimation, interruptHandler); // Starting the interrupt timer
}

void loop() {

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }

  currentAnimation = Animation;

  ArduinoOTA.handle();

}

void handleNewMessages(int numNewMessages) {

  /*
     This method is used to handle the new messages.
  */

  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (chat_id == adminID || chat_id == User1 || chat_id == User2 || chat_id == User3) {
      /*
          Making sure nobody except you and the (three) others you defined in the config.h file can control the GamingStein™.
          Of course, you can add more users if you want.
      */

      if (from_name == "") from_name = "Guest";

      if (text == "/start") {
        String welcome = "Welcome to the control bot of the GamingStein™, " + from_name + ".\n";
        welcome += "Using this bot, you can set the colour theme of your GamingStein™.\n\n";
        welcome += "The Telegram - keyboard which should appear now makes it easy to choose the colour or theme you like.";
        bot.sendMessageWithReplyKeyboard(chat_id, welcome, "", keyboardJson, true);
        delay(500);
      }

      else if (text == ("Off") || text == ("Turn off") || text == ("/off") || text == ("off")) {  ColourRequestReceived(0, "LEDs turned off!", chat_id);  }

      else if (text == ("Spectrum cycling")) {  ColourRequestReceived(1, "Your GamingStein™ was set to Spectrum cycling mode.", chat_id); }

      else if (text == ("White")) { ColourRequestReceived(2, "The colour of your GamingStein™ was set to white.", chat_id); }

      else if (text == ("Pink")) {  ColourRequestReceived(3, "The colour of your GamingStein™ was set to pink.", chat_id); }

      else if (text == ("Purple")) {  ColourRequestReceived(4, "CThe colour of your GamingStein™ was set to purple.", chat_id); }

      else if (text == ("Warm white")) {  ColourRequestReceived(5, "The colour of your GamingStein™ was set to a warm white.", chat_id);  }

      else if (text == ("Blue")) {  ColourRequestReceived(6, "The colour of your GamingStein™ was set to blue.", chat_id);  }

      else if (text == ("Turquoise")) { ColourRequestReceived(7, "The colour of your GamingStein™ was set to turquoise.", chat_id); }

      else if (text == ("Colour change")) { ColourRequestReceived(8, "Your GamingStein™ was set to colour change mode.", chat_id);  }

      else if (text == ("Dark red")) {  ColourRequestReceived(9, "The colour of your GamingStein™ was set to a dark red.", chat_id);  }

      else if (text == ("Green")) { ColourRequestReceived(10, "The colour of your GamingStein™ was set to green.", chat_id);  }

      else if (text == ("Orange")) {  ColourRequestReceived(11, "The colour of your GamingStein™ was set to orange.", chat_id); }

      else if (text == ("Deep blue")) { ColourRequestReceived(12, "The colour of your GamingStein™ was set to a deep blue.", chat_id);  }

      else if (text == ("Gold")) { ColourRequestReceived(13, "The colour of your GamingStein™ was set to gold.", chat_id);  }

      else if (text == ("Rose")) {  ColourRequestReceived(14, "Your GamingStein™ was set to rose.", chat_id); }

      else if (text == ("B\n10")) { BrightnessRequestReceived(25, "The brightness of your GamingStein™ was set to 10%.", chat_id);  }

      else if (text == ("R\n20")) { BrightnessRequestReceived(51, "The brightness of your GamingStein™ was set to 20%.", chat_id);  }

      else if (text == ("I\n30")) { BrightnessRequestReceived(76, "The brightness of your GamingStein™ was set to 30%.", chat_id);  }

      else if (text == ("G\n40")) { BrightnessRequestReceived(102, "The brightness of your GamingStein™ was set to 40%.", chat_id); }

      else if (text == ("H\n50")) { BrightnessRequestReceived(127, "The brightness of your GamingStein™ was set to 50%.", chat_id); }

      else if (text == ("T\n60")) { BrightnessRequestReceived(153, "The brightness of your GamingStein™ was set to 60%.", chat_id); }

      else if (text == ("N\n70")) { BrightnessRequestReceived(178, "The brightness of your GamingStein™ was set to 0%.", chat_id);  }

      else if (text == ("E\n80")) { BrightnessRequestReceived(204, "The brightness of your GamingStein™ was set to 80%.", chat_id); }

      else if (text == ("S\n90")) { BrightnessRequestReceived(229, "The brightness of your GamingStein™ was set to 90%.", chat_id); }

      else if (text == ("S\n100")) { BrightnessRequestReceived(255, "The brightness of your GamingStein™ was set to 100%.", chat_id); }

      else if (text == ("")) {  delay(500); }

      else if (text == "Random") {
        Animation = random(1, 14);
        switch (Animation) {
          case 1:   FeedbackCurrentAnimation("Your GamingStein™ was set to spectrum cycling mode.", chat_id);      break;
          case 2:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to white.", chat_id);        break;
          case 3:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to pink.", chat_id);         break;
          case 4:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to violet-blue.", chat_id);  break;
          case 5:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to a warm white.", chat_id); break;
          case 6:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to blue.", chat_id);         break;
          case 7:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to turquoise", chat_id);     break;
          case 8:   FeedbackCurrentAnimation("Your GamingStein™ was set to colour change mode.", chat_id);         break;
          case 9:   FeedbackCurrentAnimation("The colour of your GamingStein™ was set to a dark red.", chat_id);   break;
          case 10:  FeedbackCurrentAnimation("The colour of your GamingStein™ was set to green.", chat_id);        break;
          case 11:  FeedbackCurrentAnimation("The colour of your GamingStein™ was set to orange.", chat_id);       break;
          case 12:  FeedbackCurrentAnimation("The colour of your GamingStein™ was set to a deep blue.", chat_id);  break;
          case 13:  FeedbackCurrentAnimation("The colour of your GamingStein™ was set to gold.", chat_id);         break;
          case 14:  FeedbackCurrentAnimation("The colour of your GamingStein™ was set to rose.", chat_id);         break;
        }
      }

      else if ((text != "Spectrum cycling") && (text != "") && (text != "/start") && (text != "White") && (text != "Deep blue")
               && (text != "Pink" ) && (text != "Purple") && (text != "Warm white") && (text != "Turquoise") && (text != "Blue")
               && (text != "Colour change") && (text != "B\n10") && (text != "R\n20") && (text != "I\n30") && (text != "G\n40")
               && (text != "H\n50") && (text != "T\n60") && (text != "N\n70") && (text != "E\n80") && (text != "S\n90")
               && (text != "S\n10") && (text != "Dark red") && (text != "Green") && (text != "Orange") && (text != "/off")
               && (text != "Off") && (text != "off") && (text != "Turn off") && (text != "Random") && (text != "Gold")
               && (text != "Rose") && (text != "")) {

        bot.sendMessage(chat_id, "Command not found.");
        delay(500);
      }
    }

    else {
      bot.sendMessage(chat_id, "I'm sorry, but this bot is used to control the lighting effect of a specific GamingStein™"
                      " and is therefore usable only for a few people.");
      delay(500);
    }
  }
}

void ColourRequestReceived (int RequestedAnimation, char MessageText[], String chatID) {
  Animation = RequestedAnimation;
  bot.sendMessage(chatID, MessageText);
  delay(500);
}

void BrightnessRequestReceived(int RequestedBrightness, char MessageText[], String chatID) {
  FastLED.setBrightness(RequestedBrightness);
  bot.sendMessage(chatID, MessageText);
  delay(500);
}

void FeedbackCurrentAnimation(char MessageText[], String chatID) {
  bot.sendMessage(chatID, MessageText);
  delay(500);
}

void interruptHandler () {
  Serial.println("interrupt");

  switch (Animation) {
    case 0:  SetLEDsToColour(0x000000);   break;    // Black
    case 1: // Slow colour-fade
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
      static uint8_t startIndex = 0;
      startIndex = startIndex + 1.9; // motion speed
      FillLEDsFromPaletteColors( startIndex);
      FastLED.show();
      break;
    case 2:   SetLEDsToColour(0xF5FFFF);  break;    // #F5FFFF = Cold White
    case 3:   SetLEDsToColour(0xFF00E1);  break;    // #FF00E1 = Pink
    case 4:   // #8000FF = Violet-Blue with a touch of pink
      leds[0] = 0x8000FF;
      leds[1] = 0xCD00FF;
      leds[2] = 0x8000FF;
      leds[3] = 0x8000FF;
      leds[4] = 0xCD00FF;
      FastLED.show();
      break;
    case 5:    SetLEDsToColour(0xFFF3B8);  break;   // #FFF3B8 = Warm white
    case 6:    SetLEDsToColour(0x009EFF);  break;   // #009EFF = Blue
    case 7:    SetLEDsToColour(0x00FFEA);  break;   // #00FFEA = Turquoise blue
    case 8:    // showing different colours
      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
      startIndex = startIndex + 100; // motion speed
      FillLEDsFromPaletteColors( startIndex);
      FastLED.show();
      break;
    case 9:    SetLEDsToColour(0xFF0000);  break;   // #FF0000 = Dark red
    case 10:   SetLEDsToColour(0x00FF00);  break;   // #00FF00 = Green
    case 11:   SetLEDsToColour(0xFF9300);  break;   // #FF9300 = Orange
    case 12:   SetLEDsToColour(0x0000FF);  break;   // #0000FF = Deep blue
    case 13:   SetLEDsToColour(0xFFD700);  break;   // #FFD700 = Gold
    case 14:   SetLEDsToColour(0xFFDAA2);  break;   // #FFDAA2 = Rose


  }
}

void SetLEDsToColour (CRGB Colour) {
  leds[0] = Colour;
  leds[1] = Colour;
  leds[2] = Colour;
  leds[3] = Colour;
  leds[4] = Colour;
  FastLED.show();
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = 255;

  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}

const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM =
{
  CRGB::Black,
  CRGB::Black
};
