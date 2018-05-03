//#define SEND_PDU
//#define SEND_SMS
//#define DST_NUM ""

#define DEBUG
#define DEBUG_PORT Serial

#include <A6lib.h>

#define PWR_PIN 0
A6lib A6(&Serial);
bool once = false;

void new_sms_event(uint8_t indx, const SMSInfo& info) {
	DEBUG_PORT.printf("\nEvent: new SMS\n");
	DEBUG_PORT.printf("Number: %s, Date: %s, Content: %s\n", info.number.c_str(), info.date.c_str(), info.message.c_str());
}

void sms_sent_event() {
	DEBUG_PORT.printf("\nEvent: SMS sent");

	/* send PDU now */
#ifdef SEND_PDU
	if (!once) {
		once = true;
		wchar_t content[] = { 0x0048, 0x0069, 0x0021 }; // Hi!
		A6.sendPDU(DST_NUM, content, 3);
	}
#endif
}

void storage_full_event() {
	DEBUG_PORT.printf("\nEvent: Modem prefered storage area is full\n");
}

void run() {
	/* User critical task (e.g handling KeyPad etc) could be here */
}

void setup() {
#ifdef DEBUG
	DEBUG_PORT.begin(115200);
	delay(100);
#endif
	A6.onSMSReceived(&new_sms_event);
	A6.onSMSSent(&sms_sent_event);
	A6.onSMSStorageFull(&storage_full_event);
	A6.addHandler(&run);
	A6.powerUp(PWR_PIN);
	A6.start(9600, 3);

	DEBUG_PORT.printf("\nModem info: \n");
	DEBUG_PORT.printf("IMEI: %s\n", A6.getIMEI().c_str());
	DEBUG_PORT.printf("FirmWare version: %s\n", A6.getFirmWareVer().c_str());
	DEBUG_PORT.printf("Network registration status: %s\n", A6lib::registerStatusToString(A6.getRegisterStatus()).c_str());
	DEBUG_PORT.printf("SMS service center address: %s\n", A6.getSMSSca().c_str());
	DEBUG_PORT.printf("Signal RSSI: %d\n", A6.getRSSI());
	DEBUG_PORT.printf("Signal quality: %d\n", A6.getSignalQuality());
	DEBUG_PORT.printf("RTC: %s\n", A6.getRealTimeClock().c_str());
	DEBUG_PORT.println();

	A6.setPreferedStorage(SMSStorageArea::SM);

	int8_t buff[32];
	auto c = A6.getSMSList(buff, sizeof(buff), SMSRecordType::All);
	if (c >= 0) {
		DEBUG_PORT.printf("%d SMS found\n", c);
		for (uint8_t i = 0; i < c; i++) {
			auto info = A6.readSMS(buff[i]);
			DEBUG_PORT.printf("SMS[%d] = Number: %s, Date: %s, Content: %s\n", buff[i], info.number.c_str(), info.date.c_str(), info.message.c_str());
		}
	}

	auto reply = A6.sendCommand("AT+CPIN?");
	DEBUG_PORT.printf("Reply for (AT+CPIN?): %s\n", reply.c_str());

#ifdef SEND_SMS
	A6.sendSMS(DST_NUM, "Hi!");
	/* check for SMS sent event */
#endif
}

void loop() {
	A6.handle();
}
