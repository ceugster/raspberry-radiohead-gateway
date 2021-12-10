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
//
// Adapted for serving mqtt server by paho.mqtt.cpp by Christian Eugster
// 2021-12-10 changed mqtt client behaviour

#include <bcm2835.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

//#include "RadioHead/RH_RF69.h"
#include "RadioHead/RH_RF95.h"

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

// Create an instance of a driver
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);
//RH_RF95 rf95(RF_CS_PIN);

// Ini file
CSimpleIniA ini;

// Our MQTT Client Configuration
//#define MQTT_TIMEOUT 3000L

//Flag for Ctrl-C
volatile sig_atomic_t force_exit = false;

void sig_handler(int sig) {
	printf("\n%s Break received, exiting!\n", __BASEFILE__);
	force_exit = true;
}

//Main Function
int main(int argc, const char *argv[]) {
	unsigned long led_blink = 0;

	signal(SIGINT, sig_handler);
	printf("Starting %s\n", __BASEFILE__);

	ini.SetUnicode();
	char ini_filename[] = __BASEFILE__;
	char ini_ext[] = ".ini";
	strcpy(ini_filename, __BASEFILE__);
	strcat(ini_filename, ini_ext);

	printf(ini_filename);
	printf("\n");

	if (ini.LoadFile(ini_filename) < 0) {
		printf(
				"An error occurred while trying to open ini file. Please provide a valid ini file\n");
		return 1;
	}
	printf("Content of %s%s\n", __BASEFILE__, ini_ext);

	const char *mqtt_topic = ini.GetValue("mqtt", "topic", NULL);
	printf("mqtt_topic=%s\n", mqtt_topic);
	const char *mqtt_dest_addr = ini.GetValue("mqtt", "dest_addr", NULL);
	printf("mqtt_dest_addr=%s\n", mqtt_dest_addr);
	const char *mqtt_client_id = ini.GetValue("mqtt", "client_id", NULL);
	printf("mqtt_client_id=%s\n", mqtt_client_id);
	const char *node_id = ini.GetValue("lora", "node_id", NULL);
	printf("lora_node_id=%s\n", node_id);
	const char *frequency = ini.GetValue("lora", "frequency", NULL);
	printf("lora_frequency=%s\n\n", frequency);

	uint8_t lora_node_id = (uint8_t) atoi(frequency);
	float lora_frequency = atof(frequency);

	if (!bcm2835_init()) {
		fprintf(stderr, "%s bcm2835_init() failed\n\n", __BASEFILE__);
		return 1;
	}

	printf("RF95 CS=GPIO%d", RF_CS_PIN);

#ifdef RF_LED_PIN
	pinMode(RF_LED_PIN, OUTPUT);
	digitalWrite(RF_LED_PIN, HIGH);
#endif

#ifdef RF_IRQ_PIN
	printf(", IRQ=GPIO%d", RF_IRQ_PIN);
	// IRQ Pin input/pull down
	pinMode(RF_IRQ_PIN, INPUT);
	bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
	// Now we can enable Rising edge detection
	bcm2835_gpio_ren (RF_IRQ_PIN);
#endif

#ifdef RF_RST_PIN
	printf(", RST=GPIO%d", RF_RST_PIN);
	// Pulse a reset on module
	pinMode(RF_RST_PIN, OUTPUT);
	digitalWrite(RF_RST_PIN, LOW);
	bcm2835_delay(150);
	digitalWrite(RF_RST_PIN, HIGH);
	bcm2835_delay(100);
#endif

#ifdef RF_LED_PIN
	printf(", LED=GPIO%d", RF_LED_PIN);
	digitalWrite(RF_LED_PIN, LOW);
#endif

	printf(" OK NodeID=%d @ %3.2fMHz\n", lora_node_id, lora_frequency);

	MQTTClient client;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;
	char topic[256];

	printf("Create MQTT client\n");
	int rc = MQTTClient_create(&client, mqtt_dest_addr, mqtt_client_id,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	if (rc != MQTTCLIENT_SUCCESS) {
		fprintf(stderr, "Creation of MQTT client failed\n");
		return 1;
	}
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	printf("Init RF95 module\n");
	if (!rf95.init()) {
		fprintf(stderr, "RF95 module init failed, Please verify wiring/module\n");
	} else {
		// Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
		// The default transmitter power is 13dBm, using PA_BOOST.
		// If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
		// you can set transmitter powers from 5 to 23 dBm:
		// driver.setTxPower(23, false);
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
		printf("Set lora frequency to %f\n", lora_frequency);
		rf95.setFrequency(lora_frequency);

		// If we need to send something
		// rf95.setThisAddress(lora_node_id);
		rf95.setHeaderFrom(lora_node_id);

		// Be sure to grab all node packet
		// we're sniffing to display, it's a demo
		rf95.setPromiscuous(true);

		// We're ready to listen for incoming message
		printf("Set receive mode\n");
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
				printf("Listening...\n");

				if (rf95.available()) {
#ifdef RF_LED_PIN
					led_blink = millis();
					digitalWrite(RF_LED_PIN, HIGH);
#endif
					// Should be a message for us now
					uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
					uint8_t len = sizeof(buf);
					uint16_t from = rf95.headerFrom();
					uint8_t to = rf95.headerTo();
					uint8_t id = rf95.headerId();
					uint8_t flags = rf95.headerFlags();
					;
					int8_t rssi = rf95.lastRssi();

					if (rf95.recv(buf, &len)) {
						time_t now = time(NULL);
						printf("Timestamp: %s", ctime(&now));
						printf("Packet[%02d] %ddB:\n", len, rssi);
						printbuffer(buf, len);
						printf("\n");

						pubmsg.payload = &buf;
						pubmsg.payloadlen = len;
						pubmsg.qos = 2;
						pubmsg.retained = 0;
						sprintf(topic, "%s/%s", mqtt_topic, mqtt_client_id);
						printf("Topic: %s\n", topic);

						if (!MQTTClient_isConnected(client)) {
							printf("(Re)connecting to MQTT broker on %s ",
									mqtt_dest_addr);
							int count = 0;
							while (!MQTTClient_isConnected(client)) {
								rc = MQTTClient_connect(client, &conn_opts);
								if (rc != MQTTCLIENT_SUCCESS) {
									if (count == 20) {
										printf(".\n");
										count = 0;
									} else {
										printf(".");
										count++;
									}
								}
							}
							printf("OK\n");
						}
						if (MQTTClient_isConnected(client)) {
							printf("Deliver message with token %d ", token);
							rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
							if (rc == MQTTCLIENT_SUCCESS) {
								printf("OK\n");
							}
							else {
								fprintf(stderr, "failed");
							}
						}
					} else {
						printf("receive failed\n");
					}
				}
#ifdef RF_IRQ_PIN
			}
#endif

#ifdef RF_LED_PIN
			// Led blink timer expiration ?
			if (led_blink && millis() - led_blink > 200) {
				led_blink = 0;
				digitalWrite(RF_LED_PIN, LOW);
			}
#endif
			// Let OS doing other tasks
			// For timed critical application you can reduce or delete
			// this delay, but this will charge CPU usage, take care and monitor
			bcm2835_delay(5);
		}
	}
	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);

#ifdef RF_LED_PIN
	digitalWrite(RF_LED_PIN, LOW);
#endif
	printf("\n%s ending\n", __BASEFILE__);
	bcm2835_gpio_clr_ren(RF_IRQ_PIN);
	bcm2835_close();
	return 0;
}

