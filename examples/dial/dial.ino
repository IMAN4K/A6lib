#define DEBUG_PORT Serial
#include <A6lib.h>

extern "C" {
#include <stdarg.h>
}

void log(Stream* stream, const char* format, ...) {
#ifdef DEBUG
	char buff[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	if (stream)
		stream->print(buff);
#endif
}

#ifdef A6_T
#define PWR_PIN 0
#endif

/* change pins to whatever you need */
#define GSM_TX_PIN 1
#define GSM_RX_PIN 2
A6lib modem(GSM_RX_PIN, GSM_TX_PIN);

void run() {
	/* User critical task (e.g handling KeyPad etc) could be here */
}

void setup() {
#ifdef DEBUG
	DEBUG_PORT.begin(115200);
	delay(100);
#endif
	modem.addHandler(&run);
#ifdef DEBUG
	modem.setDebugStream(&DEBUG_PORT);
#endif
#ifdef A6_T
	modem.powerUp(PWR_PIN);
#endif
	const auto ok = modem.waitForNetwork(115200, 16000);
	if (ok) {
		delay(8000);
		modem.start(3);
	}
}

void loop() {
	log(&DEBUG_PORT, "Checking call status...");
	callInfo cinfo = modem.checkCallStatus();
	log(&DEBUG_PORT, "Call status checked.");

	int sigStrength = modem.getSignalQuality();
	log(&DEBUG_PORT, "Signal strength percentage: %d\n", sigStrength);

	delay(5000);

	if (cinfo.number != NULL) {
		if (cinfo.direction == DIR_INCOMING && cinfo.number == "919999999999") {
			modem.answer();
		} else {
			modem.hangUp();
		}
		delay(1000);
	} else {
		log(&DEBUG_PORT, "No number yet.");
		delay(1000);
	}
}
