/* uncomment following two lines, to enable SMS or PDU sending */
//#define SEND_PDU
//#define SEND_SMS
//#define DST_NUM ""

#define DEBUG_PORT Serial
#include <A6lib.h>

extern "C" {
#include <stdarg.h>
}

#ifdef A6_T
#define PWR_PIN 0
#endif

A6lib modem(&Serial);
bool once = false;

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

void new_sms_event(uint8_t indx, const SMSInfo& info) {
	log(&DEBUG_PORT, "\nEvent: new SMS\n");
	log(&DEBUG_PORT, "Number: %s, Date: %s, Content: %s\n", info.number.c_str(), info.dateTime.c_str(), info.message.c_str());
}

void sms_sent_event() {
	log(&DEBUG_PORT, "\nEvent: SMS sent");
#ifdef SEND_PDU
	/* send PDU now */
	if (!once) {
		once = true;
		uint16_t content[] = { 0x0048, 0x0069, 0x0021 }; // Hi!
		modem.sendPDU(DST_NUM, content, sizeof(content) / sizeof(uint16_t));
	}
#endif
}

void storage_full_event() {
	log(&DEBUG_PORT, "\nEvent: Modem prefered storage area is full\n");
}

void run() {
	/* User critical task (e.g handling KeyPad etc) could be here */
}

void setup() {
#ifdef DEBUG
	DEBUG_PORT.begin(115200);
	delay(100);
#endif
	modem.onSMSReceived(&new_sms_event);
	modem.onSMSSent(&sms_sent_event);
	modem.onSMSStorageFull(&storage_full_event);
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

	log(&DEBUG_PORT, "\nModem info: \n");
	log(&DEBUG_PORT, "IMEI: %s\n", modem.getIMEI().c_str());
	log(&DEBUG_PORT, "FirmWare version: %s\n", modem.getFirmWareVer().c_str());
	log(&DEBUG_PORT, "Network registration status: %s\n", A6lib::registerStatusToString(modem.getRegisterStatus()).c_str());
	log(&DEBUG_PORT, "SMS service center address: %s\n", modem.getSMSSca().c_str());
	log(&DEBUG_PORT, "Signal RSSI: %d\n", modem.getRSSI());
	log(&DEBUG_PORT, "Signal quality: %d\n", modem.getSignalQuality());
	log(&DEBUG_PORT, "RTC: %s\n", modem.getRealTimeClockString().c_str());
	DEBUG_PORT.println();

	modem.setCharSet(CharSet::Gsm);
	modem.setSMSStorageArea(SMSStorageArea::SM);

	int8_t buff[32];
	auto c = modem.getSMSList(buff, sizeof(buff), SMSRecordType::All);
	if (c >= 0) {
		log(&DEBUG_PORT, "%d SMS found\n", c);
		for (uint8_t i = 0; i < c; i++) {
			auto info = modem.readSMS(buff[i]);
			log(&DEBUG_PORT, "SMS[%d] = Number: %s, Date: %s, Content: %s\n", buff[i], info.number.c_str(), info.dateTime.c_str(), info.message.c_str());
		}
	}

	auto reply = modem.sendCommand("AT+CPIN?");
	log(&DEBUG_PORT, "Reply for (AT+CPIN?): \n");
	DEBUG_PORT.print(reply.c_str());

#ifdef SEND_SMS
	modem.sendSMS(DST_NUM, "Hi!");
	/* check for SMS sent event */
#endif
}

void loop() {
	modem.handle();
}
