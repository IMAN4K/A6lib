#include <stdarg.h>

#include "A6lib.h"

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define A6_CMD_TIMEOUT 2000

#ifdef DEBUG
#ifndef DEBUG_PORT
#	define DEBUG_PORT Serial
#endif
#endif // DEBUG

void LOG(const char* format, ...) {
#ifdef DEBUG
	char buff[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	DEBUG_PORT.print("\n[A6lib] ");
	DEBUG_PORT.print(buff);
#endif // DEBUG
}


/*!
 * \class A6lib Constructs A6lib object.
 * \brief
 */

/*!
 * \brief A6lib::A6lib Constructs A6lib object with the given serial port.
 * \param port HardwareSerial object for use inside A6lib.
 */
A6lib::A6lib(HardwareSerial* port) : stream{ port } {
	stream->setTimeout(100);
	ports.state = PortState::Using_HardWareSerial;
	ports.hport = port;
}

/*!
 * \brief A6lib::A6lib Constructs A6lib object with the given serial port.
 * \param port SoftwareSerial object for use inside A6lib
 */
A6lib::A6lib(SoftwareSerial* port) : stream{ port } {
	stream->setTimeout(100);
	ports.state = PortState::Using_SoftWareSerial;
	ports.sport = port;
}

/*!
 * \brief A6lib::A6lib Constructs A6lib object with the given pin numbers. this is done by creating new SoftwareSerial object.
 * \param tx_pin SoftwareSerial TX pin
 * \param rx_pin SoftwareSerial RX pin
 */
A6lib::A6lib(uint8_t tx_pin, uint8_t rx_pin) {
	ports.state = PortState::New_SoftwareSerial;
	ports.sport = new SoftwareSerial(rx_pin, tx_pin);
	stream = ports.sport;
	stream->setTimeout(100);
}

/*!
 * \brief A6lib::~A6lib Destroys A6lib object.
 */
A6lib::~A6lib() {
	if (ports.testState(PortState::New_SoftwareSerial) && ports.sport)
		delete ports.sport;
}


// Block until the module is ready.
byte A6lib::blockUntilReady(long baudRate) {

	byte response = A6_NOTOK;
	while (A6_OK != response) {
		response = begin(baudRate);
		// This means the modem has failed to initialize and we need to reboot
		// it.
		if (A6_FAILURE == response) {
			return A6_FAILURE;
		}
		delay(1000);
		LOG("Waiting for module to be ready...");
	}
	return A6_OK;
}


// Initialize the software serial connection and change the baud rate from the
// default (autodetected) to the desired speed.
byte A6lib::begin(long baudRate) {
	if (ports.testState(PortState::Using_SoftWareSerial))
		LOG("starting with SoftwareSerial object");
	else if (ports.testState(PortState::Using_HardWareSerial))
		LOG("starting with HardwareSerial object");
	else
		LOG("starting with new SoftwareSerial object");
	stream->flush();

	if (A6_OK != setRate(baudRate)) {
		return A6_NOTOK;
	}

	// Factory reset.
	A6command("AT&F", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);

	// Echo off.
	A6command("ATE0", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);

	// Switch audio to headset.
	enableSpeaker(0);

	// Set caller ID on.
	A6command("AT+CLIP=1", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);

	// Set SMS to text mode.
	A6command("AT+CMGF=1", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);

	// Turn SMS indicators off.
	A6command("AT+CNMI=1,0", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);

	// Set SMS storage to the GSM modem. If this doesn't work for you, try changing the command to:
	// "AT+CPMS=SM,SM,SM"
	if (A6_OK != A6command("AT+CPMS=ME,ME,ME", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL))
		// This may sometimes fail, in which case the modem needs to be
		// rebooted.
	{
		return A6_FAILURE;
	}

	// Set SMS character set.
	setSMScharset("UCS2");

	return A6_OK;
}


// Reboot the module by setting the specified pin HIGH, then LOW. The pin should
// be connected to a P-MOSFET, not the A6's POWER pin.
void A6lib::powerCycle(int pin) {
	LOG("Power-cycling module...");

	powerOff(pin);

	delay(2000);

	powerOn(pin);

	// Give the module some time to settle.
	LOG("Done, waiting for the module to initialize...");
	delay(20000);
	LOG("Done.");

	stream->flush();
}


// Turn the modem power completely off.
void A6lib::powerOff(int pin) {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);
}


// Turn the modem power on.
void A6lib::powerOn(int pin) {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, HIGH);
}


// Dial a number.
void A6lib::dial(String number) {
	char buffer[50];

	LOG("Dialing number...");

	sprintf(buffer, "ATD%s;", number.c_str());
	A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Redial the last number.
void A6lib::redial() {
	LOG("Redialing last number...");
	A6command("AT+DLST", "OK", "CONNECT", A6_CMD_TIMEOUT, 2, NULL);
}


// Answer a call.
void A6lib::answer() {
	A6command("ATA", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Hang up the phone.
void A6lib::hangUp() {
	A6command("ATH", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Check whether there is an active call.
callInfo A6lib::checkCallStatus() {
	char number[50];
	String response = "";
	uint32_t respStart = 0, matched = 0;
	callInfo cinfo;

	// Issue the command and wait for the response.
	A6command("AT+CLCC", "OK", "+CLCC", A6_CMD_TIMEOUT, 2, &response);

	// Parse the response if it contains a valid +CLCC.
	respStart = response.indexOf("+CLCC");
	if (respStart >= 0) {
		matched = sscanf(response.substring(respStart).c_str(), "+CLCC: %d,%d,%d,%d,%d,\"%s\",%d", &cinfo.index, &cinfo.direction, &cinfo.state, &cinfo.mode, &cinfo.multiparty, number, &cinfo.type);
		cinfo.number = String(number);
	}

	uint8_t comma_index = cinfo.number.indexOf('"');
	if (comma_index != -1) {
		LOG("Extra comma found.");
		cinfo.number = cinfo.number.substring(0, comma_index);
	}

	return cinfo;
}


// Get the strength of the GSM signal.
int A6lib::getSignalStrength() {
	String response = "";
	uint32_t respStart = 0;
	int strength, error = 0;

	// Issue the command and wait for the response.
	A6command("AT+CSQ", "OK", "+CSQ", A6_CMD_TIMEOUT, 2, &response);

	respStart = response.indexOf("+CSQ");
	if (respStart < 0) {
		return 0;
	}

	sscanf(response.substring(respStart).c_str(), "+CSQ: %d,%d",
		&strength, &error);

	// Bring value range 0..31 to 0..100%, don't mind rounding..
	strength = (strength * 100) / 31;
	return strength;
}


// Get the real time from the modem. Time will be returned as yy/MM/dd,hh:mm:ss+XX
String A6lib::getRealTimeClock() {
	String response = "";

	// Issue the command and wait for the response.
	A6command("AT+CCLK?", "OK", "yy", A6_CMD_TIMEOUT, 1, &response);
	int respStart = response.indexOf("+CCLK: \"") + 8;
	response.setCharAt(respStart - 1, '-');

	return response.substring(respStart, response.indexOf("\""));
}


// Send an SMS.
byte A6lib::sendSMS(String number, String text) {
	char ctrlZ[2] = { 0x1a, 0x00 };
	char buffer[100];

	if (text.length() > 159) {
		// We can't send messages longer than 160 characters.
		return A6_NOTOK;
	}

	LOG("Sending SMS to ");
	LOG(number);
	LOG("...");

	sprintf(buffer, "AT+CMGS=\"%s\"", number.c_str());
	A6command(buffer, ">", "yy", A6_CMD_TIMEOUT, 2, NULL);
	delay(100);
	stream->println(text.c_str());
	stream->println(ctrlZ);
	stream->println();

	return A6_OK;
}


// Retrieve the number and locations of unread SMS messages.
int A6lib::getUnreadSMSLocs(int* buf, int maxItems) {
	return getSMSLocsOfType(buf, maxItems, "REC UNREAD");
}

// Retrieve the number and locations of all SMS messages.
int A6lib::getSMSLocs(int* buf, int maxItems) {
	return getSMSLocsOfType(buf, maxItems, "ALL");
}

// Retrieve the number and locations of all SMS messages.
int A6lib::getSMSLocsOfType(int* buf, int maxItems, String type) {
	String seqStart = "+CMGL: ";
	String response = "";

	String command = "AT+CMGL=\"";
	command += type;
	command += "\"";

	// Issue the command and wait for the response.
	byte status = A6command(command.c_str(), "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT, 2, &response);

	int seqStartLen = seqStart.length();
	int responseLen = response.length();
	int index, occurrences = 0;

	// Start looking for the +CMGL string.
	for (int i = 0; i < (responseLen - seqStartLen); i++) {
		// If we found a response and it's less than occurrences, add it.
		if (response.substring(i, i + seqStartLen) == seqStart && occurrences < maxItems) {
			// Parse the position out of the reply.
			sscanf(response.substring(i, i + 12).c_str(), "+CMGL: %u,%*s", &index);

			buf[occurrences] = index;
			occurrences++;
		}
	}
	return occurrences;
}

// Return the SMS at index.
SMSInfo A6lib::readSMS(int index) {
	String response = "";
	char buffer[30];

	// Issue the command and wait for the response.
	sprintf(buffer, "AT+CMGR=%d", index);
	A6command(buffer, "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT, 2, &response);

	char message[200];
	char number[50];
	char date[50];
	char type[10];
	int respStart = 0, matched = 0;
	SMSInfo sms;

	// Parse the response if it contains a valid +CLCC.
	respStart = response.indexOf("+CMGR");
	if (respStart >= 0) {
		// Parse the message header.
		matched = sscanf(response.substring(respStart).c_str(), "+CMGR: \"REC %s\",\"%s\",,\"%s\"\r\n", type, number, date);
		sms.number = String(number);
		sms.date = String(date);
		// The rest is the message, extract it.
		sms.message = response.substring(strlen(type) + strlen(number) + strlen(date) + 24, response.length() - 8);
	}
	return sms;
}

// Delete the SMS at index.
byte A6lib::deleteSMS(int index) {
	char buffer[20];
	sprintf(buffer, "AT+CMGD=%d", index);
	return A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Set the SMS charset.
byte A6lib::setSMScharset(String charset) {
	char buffer[30];

	sprintf(buffer, "AT+CSCS=\"%s\"", charset.c_str());
	return A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Set the volume for the speaker. level should be a number between 5 and
// 8 inclusive.
void A6lib::setVol(byte level) {
	char buffer[30];

	// level should be between 5 and 8.
	level = _min(_max(level, 5), 8);
	sprintf(buffer, "AT+CLVL=%d", level);
	A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Enable the speaker, rather than the headphones. Pass 0 to route audio through
// headphones, 1 through speaker.
void A6lib::enableSpeaker(byte enable) {
	char buffer[30];

	// enable should be between 0 and 1.
	enable = _min(_max(enable, 0), 1);
	sprintf(buffer, "AT+SNFS=%d", enable);
	A6command(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}



/////////////////////////////////////////////
// Private methods.
//


// Autodetect the connection rate.

long A6lib::detectRate() {
	unsigned long rate = 0;
	unsigned long rates[] = { 9600, 115200 };

	// Try to autodetect the rate.
	LOG("Autodetecting connection rate...");
	for (int i = 0; i < countof(rates); i++) {
		rate = rates[i];

		Serial1.printf("sumping ports");
		if (ports.hport != nullptr)
			Serial.println("hardware ports is present....");
		if (ports.sport != nullptr)
			Serial1.println("software port is present...");

		if (ports.hport != nullptr)
			ports.hport->begin(rate);
		else if (ports.sport != nullptr)
			ports.sport->begin(rate);
		LOG("Trying rate ");
		LOG(rate);
		LOG("...");

		delay(100);
		if (A6command("\rAT", "OK", "+CME", 2000, 2, NULL) == A6_OK) {
			return rate;
		}
	}

	LOG("Couldn't detect the rate.");

	return A6_NOTOK;
}


// Set the A6 baud rate.
char A6lib::setRate(long baudRate) {
	int rate = 0;

	rate = detectRate();
	if (rate == A6_NOTOK) {
		return A6_NOTOK;
	}

	// The rate is already the desired rate, return.
	//if (rate == baudRate) return OK;

	LOG("Setting baud rate on the module...");

	// Change the rate to the requested.
	char buffer[30];
	sprintf(buffer, "AT+IPR=%d", baudRate);
	A6command(buffer, "OK", "+IPR=", A6_CMD_TIMEOUT, 3, NULL);

	LOG("Switching to the new rate...");
	// Begin the connection again at the requested rate.
	if (ports.hport != nullptr)
		ports.hport->begin(rate);
	else if (ports.sport != nullptr)
		ports.sport->begin(rate);
	LOG("Rate set.");

	return A6_OK;
}


// Read some data from the A6 in a non-blocking manner.
String A6lib::read() {
	String reply = "";
	
	if (stream->available()) {
		reply = stream->readString();
	}

	// XXX: Replace NULs with \xff so we can match on them.
	for (int x = 0; x < reply.length(); x++) {
		if (reply.charAt(x) == 0) {
			reply.setCharAt(x, 255);
		}
	}
	return reply;
}


// Issue a command.
byte A6lib::A6command(const char *command, const char *resp1, const char *resp2, int timeout, int repetitions, String *response) {
	byte returnValue = A6_NOTOK;
	byte count = 0;

	// Get rid of any buffered output.
	stream->flush();

	while (count < repetitions && returnValue != A6_OK) {
		LOG("Issuing command: ");
		LOG(command);

		stream->write(command);
		stream->write('\r');

		if (A6waitFor(resp1, resp2, timeout, response) == A6_OK) {
			returnValue = A6_OK;
		}
		else {
			returnValue = A6_NOTOK;
		}
		count++;
	}
	return returnValue;
}


// Wait for responses.
byte A6lib::A6waitFor(const char *resp1, const char *resp2, int timeout, String *response) {
	unsigned long entry = millis();
	int count = 0;
	String reply = "";
	byte retVal = 99;
	do {
		reply += read();
		yield();
	} while (((reply.indexOf(resp1) + reply.indexOf(resp2)) == -2) && ((millis() - entry) < timeout));

	if (reply != "") {
		LOG("Reply in ");
		LOG((millis() - entry));
		LOG(" ms: ");
		LOG(reply);
	}

	if (response != NULL) {
		*response = reply;
	}

	if ((millis() - entry) >= timeout) {
		retVal = A6_TIMEOUT;
		LOG("Timed out.");
	}
	else {
		if (reply.indexOf(resp1) + reply.indexOf(resp2) > -2) {
			LOG("Reply OK.");
			retVal = A6_OK;
		}
		else {
			LOG("Reply NOT OK.");
			retVal = A6_NOTOK;
		}
	}
	return retVal;
}
