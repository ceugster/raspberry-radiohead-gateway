// rf95_server.cpp
//
// Example program showing how to use RH_RF95 on Raspberry Pi
// Uses the bcm2835 library to access the GPIO pins to drive the RFM95 module
// Requires bcm2835 library to be already installed
// http://www.airspayce.com/mikem/bcm2835/
// Use the Makefile in this directory:
// cd example/raspi/rf95
// make
// sudo ./rf95_server
//
// Contributed by Charles-Henri Hallard based on sample RH_NRF24 by Mike Poublon

#include <bcm2835.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "RadioHead/RH_RF69.h"
#include "RadioHead/RH_RF95.h"
#include "RadioHead/RHReliableDatagram.h"

#include <MQTTClient.h>
#include "SimpleIni/SimpleIni.h"

// define hardware used change to fit your need
// Uncomment the board you have, if not listed
// uncommment custom board and set wiring tin custom section

// LoRasPi board
// see https://github.com/hallard/LoRasPI
//#define BOARD_LORASPI

// iC880A and LinkLab Lora Gateway Shield (if RF module plugged into)
// see https://github.com/ch2i/iC880A-Raspberry-PI
//#define BOARD_IC880A_PLATE

// Raspberri PI Lora Gateway for multiple modules
// see https://github.com/hallard/RPI-Lora-Gateway
//#define BOARD_PI_LORA_GATEWAY

// Dragino Raspberry PI hat
// see https://github.com/dragino/Lora
#define BOARD_DRAGINO_PIHAT

// Now we include RasPi_Boards.h so this will expose defined
// constants with CS/IRQ/RESET/on board LED pins definition
#include "../RasPiBoards.h"

// Our RFM95 Configuration
#define RF_FREQUENCY  868.00
#define RF_NODE_ID    1

// Create an instance of a driver
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);
//RH_RF95 rf95(RF_CS_PIN);

// Ini file
CSimpleIniA ini;

// Our MQTT Client Configuration
#define TIMEOUT 10000L

//Flag for Ctrl-C
volatile sig_atomic_t force_exit = false;

void sig_handler(int sig)
{
  printf("\n%s Break received, exiting!\n", __BASEFILE__);
  force_exit=true;
}

//Main Function
int main (int argc, const char* argv[] )
{
  unsigned long led_blink = 0;

  signal(SIGINT, sig_handler);
  printf( "Starting %s\n", __BASEFILE__);

  ini.SetUnicode();
  if (ini.LoadFile("rf95_server.ini") < 0) {
    printf("Error occurred while trying to open ini file. Please provide a valid ini file\n");
    return 1;
  }
  printf("Content of rf95_server.ini\n");
  const char *project = ini.GetValue("common", "project", NULL);
  printf("project=%s\n", project);
  const char *dest_addr = ini.GetValue("common", "dest_addr", NULL);
  printf("dest_addr=%s\n", dest_addr);
  const char *client_id = ini.GetValue("common", "client_id", NULL);
  printf("client_id=%s\n\n", client_id);

  if (!bcm2835_init()) {
    fprintf( stderr, "%s bcm2835_init() Failed\n\n", __BASEFILE__ );
    return 1;
  }

  printf( "RF95 CS=GPIO%d", RF_CS_PIN);

#ifdef RF_LED_PIN
  pinMode(RF_LED_PIN, OUTPUT);
  digitalWrite(RF_LED_PIN, HIGH );
#endif

#ifdef RF_IRQ_PIN
  printf( ", IRQ=GPIO%d", RF_IRQ_PIN );
  // IRQ Pin input/pull down
  pinMode(RF_IRQ_PIN, INPUT);
  bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
  // Now we can enable Rising edge detection
  bcm2835_gpio_ren(RF_IRQ_PIN);
#endif

#ifdef RF_RST_PIN
  printf( ", RST=GPIO%d", RF_RST_PIN );
  // Pulse a reset on module
  pinMode(RF_RST_PIN, OUTPUT);
  digitalWrite(RF_RST_PIN, LOW );
  bcm2835_delay(150);
  digitalWrite(RF_RST_PIN, HIGH );
  bcm2835_delay(100);
#endif

#ifdef RF_LED_PIN
  printf( ", LED=GPIO%d", RF_LED_PIN );
  digitalWrite(RF_LED_PIN, LOW );
#endif

  printf( " OK NodeID=%d @ %3.2fMHz\n", RF_NODE_ID, RF_FREQUENCY );

  MQTTClient client;
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  MQTTClient_deliveryToken token;
  int rc;
  char topic[128];

  printf("Create MQTT client\n");
  if ((MQTTClient_create(&client, dest_addr, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
    fprintf(stderr, "Creation of MQTT client failed \n");
    return 1;
  }
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;

  printf("Connect to MQTT broker on %s\n", dest_addr);
  if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
    fprintf(stderr, "Could not connect to MQTT broker: %d\n", rc);
    return 1;
  }

  printf("Init RF95 module");
  RHReliableDatagram manager(rf95, RF_NODE_ID);
  if (!manager.init()) {
    fprintf( stderr, "\nRF95 module init failed, Please verify wiring/module\n" );
  } else {
    // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

    // The default transmitter power is 13dBm, using PA_BOOST.
    // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
    // you can set transmitter powers from 5 to 23 dBm:
    //  driver.setTxPower(23, false);
    // If you are using Modtronix inAir4 or inAir9,or any other module which uses the
    // transmitter RFO pins and not the PA_BOOST pins
    // then you can configure the power transmitter power for -1 to 14 dBm and with useRFO true. 
    // Failure to do that will result in extremely low transmit powers.
    // rf95.setTxPower(14, true);


    // RF95 Modules don't have RFO pin connected, so just use PA_BOOST
    // check your country max power useable, in EU it's +14dB
    rf95.setTxPower(14, false);

    // You can optionally require this module to wait until Channel Activity
    // Detection shows no activity on the channel before transmitting by setting
    // the CAD timeout to non-zero:
    //rf95.setCADTimeout(10000);

    // Adjust Frequency
    printf("set lora frequency to %d", RF_FREQUENCY);
    rf95.setFrequency(RF_FREQUENCY);

    // If we need to send something
    manager.setThisAddress(RF_NODE_ID);
    manager.setHeaderFrom(RF_NODE_ID);

    // Be sure to grab all node packet
    // we're sniffing to display, it's a demo
    rf95.setPromiscuous(true);

    // We're ready to listen for incoming message
    printf("set receive mode");
    rf95.setModeRx();

    //Begin the main body of code
    while (!force_exit) {

#ifdef RF_IRQ_PIN
      // We have a IRQ pin ,pool it instead reading
      // Modules IRQ registers from SPI in each loop

      // Rising edge fired ?
      if (bcm2835_gpio_eds(RF_IRQ_PIN)) {
        // Now clear the eds flag by setting it to 1
        bcm2835_gpio_set_eds(RF_IRQ_PIN);
        //printf("Packet Received, Rising event detect for pin GPIO%d\n", RF_IRQ_PIN);
#endif

        printf( "Listening...\n" );

        if (rf95.available()) {
#ifdef RF_LED_PIN
          led_blink = millis();
          digitalWrite(RF_LED_PIN, HIGH);
#endif
          // Should be a message for us now
          uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
          uint8_t len  = sizeof(buf);
          uint16_t from = manager.headerFrom();
          uint8_t to   = manager.headerTo();
          uint8_t id   = manager.headerId();
          uint8_t flags= manager.headerFlags();;
          int8_t rssi  = rf95.lastRssi();

          if (manager.recvfromAckTimeout(buf, &len, 3000)) {
            printf("Packet[%02d] #%d => #%d %ddB:\n", len, from, to, rssi);
            printbuffer(buf, len);
            printf("\n");

            pubmsg.payload = &buf;
            pubmsg.payloadlen = len;
            pubmsg.qos = 2;
            pubmsg.retained = 0;
            sprintf(topic, "%s/%s/%d", project, client_id, from);

            if ((rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
              fprintf(stderr, "Publish failed: %d\n", rc);
              force_exit = 1;
            } else {
              if ((rc = MQTTClient_waitForCompletion(client, token, TIMEOUT)) == MQTTCLIENT_SUCCESS) {
                printf("Message with token %d delivered successfully\n", token);
              }
            }
          } else {
            Serial.print("receive failed\n");
          }
        }

#ifdef RF_IRQ_PIN
      }
#endif

#ifdef RF_LED_PIN
      // Led blink timer expiration ?
      if (led_blink && millis()-led_blink>200) {
        led_blink = 0;
        digitalWrite(RF_LED_PIN, LOW);
      }
#endif
      // Let OS doing other tasks
      // For timed critical appliation you can reduce or delete
      // this delay, but this will charge CPU usage, take care and monitor
      bcm2835_delay(5);
    }
  }
  MQTTClient_disconnect(client, 10000);
  MQTTClient_destroy(&client);

#ifdef RF_LED_PIN
  digitalWrite(RF_LED_PIN, LOW );
#endif
  printf( "\n%s ending\n", __BASEFILE__ );
  bcm2835_gpio_clr_ren(RF_IRQ_PIN);
  bcm2835_close();
  return 0;
}
