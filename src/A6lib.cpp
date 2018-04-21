#include <stdarg.h>

#include "A6lib.h"
extern "C" {
#include "pdu.h"
}

#define A6_STATUS_OK 0
#define A6_NOTOK 1
#define A6_TIMEOUT 2
#define A6_STATUS_FAILURE 3

#define countof(a) (sizeof(a) / sizeof(a[0]))
#define A6_CMD_TIMEOUT 2000
#define A6_CMD_MAX_RETRY 3

#define PLACE_HOLDER "XX"
#define RES_OK "OK"
#define AT_PREFIX "AT"
#define RST_CMD "+RST=1"
#define GMR_CMD "+GMR"
#define CSQ_CMD "+CSQ"
#define CCLK_CMD "+CCLK"
#define GSN_CMD "+GSN"
#define CREG_CMD "+CREG"
#define IPR_CMD "+IPR"
#define ATF_CMD "AT&F"
#define ATE_CMD "ATE0"
#define CPMS_CMD "+CPMS"
#define CSCS_CMD "+CSCS"
#define CMGD_CMD "+CMGD"
#define CMGS_CMD "+CMGS"
#define CMGL_CMD "+CMGL"
#define CMGR_CMD "+CMGR"
#define CSCA_CMD "+CSCA"
#define CMGF_CMD "+CMGF"
#define UCS2 "UCS2"
#define CR "\r"
#define LF "\n"

#if defined(DEBUG) & !defined(DEBUG_PORT)
#	define DEBUG_PORT Serial
#endif

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

void normalize_response(String* arg) {
	if (arg == NULL)
		return;

	arg->replace("\n", "");
	arg->replace(RES_OK, "");
	arg->replace("\r", "");
}

bool extract_sms(const String& prefix, String* data, SMSInfo* info) {
	if (info == nullptr || data == nullptr)
		return false;

	auto start = data->indexOf(prefix);
	auto delimiter = data->indexOf(",,");

	info->number = data->substring(start + prefix.length() + 3 /* <SPACE><"><+> */, delimiter - 1);
	delimiter += 3;
	info->date = data->substring(delimiter, delimiter + 22);
	delimiter += 22;
	info->message = data->substring(delimiter + 3);

	return true;
}

void to_hex_str(String* in, uint8_t* pdu, uint8_t len) {
	if (!in || !pdu || len == 0)
		return;

	auto hex = [](uint8_t h) {
		char tmp[3];
		sprintf(tmp, "%02x", h);
		String str(tmp);
		str.toUpperCase();
		
		return str;
	};

	for (size_t i = 0; i < len; i++) {
		in->concat(hex(pdu[i]));
	}
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
A6lib::A6lib(uint8_t rx_pin, uint8_t tx_pin) {
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

/*!
 * \brief A6lib::start This is the A6lib object initlizer routine.
 * you must call this usually once in setup routine.
 * \param baud the desired baud rate to start with
 * \param max_retry the maximum number of time A6lib object try to etablish connection.
 * \return true on success
 */
bool A6lib::start(unsigned long baud, uint8_t max_retry) {
	if (ports.testState(PortState::Using_SoftWareSerial))
		LOG("Starting with SoftwareSerial object");
	else if (ports.testState(PortState::Using_HardWareSerial))
		LOG("Starting with HardwareSerial object");
	else
		LOG("Starting with new SoftwareSerial object");

	bool success = false;
	while (!success && max_retry--) {
		success = begin(baud);
		delay(1000);
		LOG("Waiting for module to be ready...");
	}

	return success;
}

/*!
 * \brief A6lib::handle the main handler of A6lib object.
 * this function needs to be called inside main loop regularly, for callbacks to work correctly.
 */
void A6lib::handle() {
	if (stream->available()) {
		/* 
			sms reception notification format:
			\r\n<+CMTI:> "<storage area>",index\r\n
			
			sms sent notification format:
			+CMGS: <%d>\n

			sms storage full notification:
			+CIEV: "SMSFULL",%d
		*/
		auto data = stream->readString();
		if (data.startsWith(CR LF))
			data.remove(0, 2);
		if (data.endsWith(CR LF))
			data.remove(data.length() - 2, 2);

		auto cmtiStart = data.indexOf("+CMTI:");
		auto cmgsStart = data.indexOf("+CMGS:");
		auto smsFull = data.indexOf("+CIEV:");
		if (cmtiStart != -1) {
			LOG("New SMS received");
			SMSInfo info;
			uint8_t indx = 0;
			auto end = data.indexOf(',');
			data.remove(cmtiStart, end - cmtiStart + 1);
			indx = data.toInt();
			info = readSMS(indx);
			if (sms_rx_cb)
				sms_rx_cb(indx, info);
		} else if (cmgsStart != -1) {
			LOG("SMS sent");
			if (sms_tx_cb)
				sms_tx_cb();
		} else if (smsFull != -1 && data.indexOf("SMSFULL") != -1) {
			LOG("Modem prefered storage is full!");
			if (sms_full_cb)
				sms_full_cb();
		}
	}
}

/*!
 * \brief A6lib::powerUp this optional function will keep the PWR pin of modem in high TTL at start up to correctly powering module.
 * Module needs this pin to be high TTl for about 2 sec.
 * \param pin the pin number which is connected to module PWR pin.
 */
void A6lib::powerUp(int pin) {
	LOG("Powering up the module...");
	powerOff(pin);
	delay(2000);
	powerOn(pin);
	stream->flush();
}

/*!
 * \brief A6lib::hardReset This function will do a hard reset on module.
 * It's recommended to do this via an NMOS.
 * Note: it will take some time for module to start + register for network.
 * \param pin the pin number which is connected to module reset(RST) pin.
 */
void A6lib::hardReset(uint8_t pin) {
	powerOn(pin);
	delay(3000);
	powerOff(pin);
}

/*!
 * \brief A6lib::softReset This function implement a software restart on module.
 *  Note: it will take some time for module to start + register for network.
 */
void A6lib::softReset() {
	cmd(AT_PREFIX RST_CMD, PLACE_HOLDER, PLACE_HOLDER, 0, 0, nullptr);
}

/*!
* \brief A6lib::getFirmWareVer Get the revision identification or firmware version of modem.
* \return If success a String contain firmware version, and if fail an empty string.
*/
String A6lib::getFirmWareVer() {
	String ver;
	if (cmd(AT_PREFIX GMR_CMD, RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &ver)) {
		normalize_response(&ver);
		ver.replace("Revision: ", "");
		return ver;
	}

	return ver;
}

/*!
* \brief A6lib::getRSSI Get the modem signal strength based on RSSI(measured as dBm).
* \return If success a value between -113dBm and -51dBm and if fail 0.
*/
int8_t A6lib::getRSSI() {
	String response;
	int8_t rssi = 0;
	if (cmd(AT_PREFIX CSQ_CMD, RES_OK, CSQ_CMD, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		/*
		return value:
		0 -> -113 dBm or less
		1 -> -111 dBm
		2-30 -> -109....-53 dBm
		31 -> -51 dBm or greater
		99 -> Uknown
		*/
		normalize_response(&response);
		auto sub = response.substring(response.indexOf(':') + 2, response.indexOf(','));
		if (sub.length() != 0) {
			/* convert to RSSI */
			rssi = sub.toInt();
			rssi -= 2;
			rssi *= 2;
			rssi += -109;
			return rssi;
		}
	}

	return rssi;
}

/*!
* \brief A6lib::getSignalQuality Get the modem signal quality level.
* \return if success a value between 0-100 and if fail 0.
*/
uint8_t A6lib::getSignalQuality() {
	auto rssi = getRSSI();
	if (rssi == 0)
		return rssi;

	/* convert RSSI to quality */
	uint8_t q;
	if (rssi <= -100) {
		q = 0;
	}
	else if (rssi >= -50) {
		q = 100;
	}
	else {
		q = 2 * (rssi + 100);
	}

	return q;
}

/*!
* \brief A6lib::getRealTimeClock Get the real time from modem.
* \return if success a string contain time in format yy/mm/dd,hh:mm:ss+zz, if fail an empty string.
*/
String A6lib::getRealTimeClock() {
	String response;
	String command(AT_PREFIX CCLK_CMD);
	command.concat('?');
	if (cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		normalize_response(&response);
		return response.substring(response.indexOf(':') + 3, response.length() - 1);
	}
	
	return String();
}

/*!
* \brief A6lib::getIMEI Get the modem IMEI.
* \return if success a string contain IMEI number, if fail an empty string.
*/
String A6lib::getIMEI() {
	String response;
	if (cmd(AT_PREFIX GSN_CMD, RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		normalize_response(&response);
		return response;
	}

	return String();
}

/*!
 * \brief A6lib::getSMSSca Get the current SMS service center address from modem.
 * \return if success a string contain SCA, if fail an empty string
 */
String A6lib::getSMSSca() {
	String response;
	String command(AT_PREFIX CSCA_CMD);
	command.concat('?');
	if (cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		response = response.substring(response.indexOf(':') + 3, response.indexOf(',') - 1);
	}
	
	return response;
}

/*!
* \brief A6lib::getRegisterStatus Get the network registration status of modem.
* \return on of the ::RegisterStatus value
*/
RegisterStatus A6lib::getRegisterStatus() {
	String response;
	String command(AT_PREFIX CREG_CMD);
	command.concat('?');
	if (cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		normalize_response(&response);
		auto sub = response.substring(response.indexOf(':') + 2, response.indexOf(','));
		if (sub.length() != 0)
			return static_cast<RegisterStatus>(sub.toInt());
	}

	return RegisterStatus::Unknow;
}

String A6lib::registerStatusToString(RegisterStatus st) {
	switch (st) {
	case NotRegistered:
		return String("not registered");
		break;
	case Registered_HomeNetwork:
		return String("registered, home network");
		break;
	case Searching_To_Register:
		return String("not registered, currently searching a new operator");
		break;
	case Register_Denied:
		return String("registration denied");
		break;
	case Unknow:
		return String("unknown");
		break;
	case Registered_Roaming:
		return String("registered, roaming");
		break;
	default:
		break;
	}
}

/*!
 * \brief A6lib::sendCommand Send new command to modem.
 * command should be a valid AT command, otherwise modem will return error with corresponding error code.
 * \param command the command to be sent with AT prefix
 * \return if success an string contain modem reply, otherwise contain error code
 */
String A6lib::sendCommand(const String& command) {
	String reply;
	cmd(command.c_str(), PLACE_HOLDER, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply);

	return reply;
}

// Dial a number.
void A6lib::dial(String number) {
	char buffer[50];

	LOG("Dialing number...");

	sprintf(buffer, "ATD%s;", number.c_str());
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Redial the last number.
void A6lib::redial() {
	LOG("Redialing last number...");
	cmd("AT+DLST", "OK", "CONNECT", A6_CMD_TIMEOUT, 2, NULL);
}


// Answer a call.
void A6lib::answer() {
	cmd("ATA", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Hang up the phone.
void A6lib::hangUp() {
	cmd("ATH", "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}


// Check whether there is an active call.
callInfo A6lib::checkCallStatus() {
	char number[50];
	String response = "";
	uint32_t respStart = 0, matched = 0;
	callInfo cinfo;

	// Issue the command and wait for the response.
	cmd("AT+CLCC", "OK", "+CLCC", A6_CMD_TIMEOUT, 2, &response);

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

// Set the volume for the speaker. level should be a number between 5 and
// 8 inclusive.
void A6lib::setVol(byte level) {
	char buffer[30];

	// level should be between 5 and 8.
	level = _min(_max(level, 5), 8);
	sprintf(buffer, "AT+CLVL=%d", level);
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}

// Enable the speaker, rather than the headphones. Pass 0 to route audio through
// headphones, 1 through speaker.
void A6lib::enableSpeaker(byte enable) {
	char buffer[30];

	// enable should be between 0 and 1.
	enable = _min(_max(enable, 0), 1);
	sprintf(buffer, "AT+SNFS=%d", enable);
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2, NULL);
}

/*!
 * \brief A6lib::setPreferedStorage Set the modem prefered storage area
 * \param area could be on of the SMSStorageArea
 * \return true on success
 */
bool A6lib::setPreferedStorage(SMSStorageArea area) {
	String command(AT_PREFIX CPMS_CMD);
	command.concat('=');
	switch (area) {
	case ME:
		command.concat("ME,ME,ME");
		break;
	default:
	case SM:
		command.concat("SM,SM,SM");
		break;
	}

	return cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
}

/*!
 * \brief A6lib::getSMSList Get the list of available SMS in prefered storage area.
 * \param buff input buffer to store SMS indexes.
 * \param len size of buff
 * \param record on of the SMSRecordType.
 * \return if fail -1, otherwise number of founded SMS.
 */
int8_t A6lib::getSMSList(int8_t* buff, uint8_t len, SMSRecordType record) {
	if (buff == NULL)
		return -1;

	String command(AT_PREFIX CMGL_CMD);
	command.concat('=');
	command.concat('"');	
	switch (record) {
	default:
	case All:
		command.concat("ALL");
		break;
	case Unread:
		command.concat("REC UNREAD");
		break;
	case Read:
		command.concat("REC READ");
		break;
	}
	command.concat('"');

	String response;
	if (!cmd(command.c_str(), "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT * 2, A6_CMD_MAX_RETRY, &response))
		return -1;

	memset(buff, 0, len);
	int8_t count = 0;
	char c_str[response.length() + 1];
	response.toCharArray(c_str, response.length());
	response = ""; // free up SRAM
	auto tok = strtok(c_str, CR LF);
	while (tok != NULL && count < len) {
		String tmp(tok);
		auto indx = tmp.substring(tmp.indexOf(CMGL_CMD ":") + 7, tmp.indexOf(',')).toInt();
		if (indx) {
			buff[count] = indx;
			count++;
		}
		tok = strtok(NULL, CR LF);
	}

	return count;
}

/*!
 * \brief A6lib::sendSMS Send SMS (in text mode) to specified number.
 * \param number valid destination number without +
 * \param text SMS content in ascii encoding
 * \return true on success
 */
bool A6lib::sendSMS(const String& number, const String& text) {
	char ctrlZ[] = { 0x1a, 0x00 };
	if (text.length() > 80 * 2) {
		LOG("TEXT mode: Max ASCII chars exceeded!");
		return false;
	}

	LOG("Sending SMS to %s", number.c_str());
	String command(AT_PREFIX CMGS_CMD);
	command.concat('=');
	command.concat('"');
	command.concat(number);
	command.concat('"');
	auto success = cmd(command.c_str(), ">", PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, NULL);
	delay(100);
	if (success) {
		stream->print(text.c_str());
		stream->print(ctrlZ);
	}

	return success;
}

/*!
 * \brief A6lib::sendPDU Send an ASCII SMS in PDU mode.
 * \param number the detination phone number which should begin with international code
 * \param content the SMS content in ASCII and up to 160 chars
 * \return true on success
 */
bool A6lib::sendPDU(const String& number, const String& content) {
	if (content.length() > 80 * 2) {
		LOG("PDU mode: Max ASCII chars exceeded!");
		return false;
	}

	/* switch to PDU mode */
	auto success = cmd(AT_PREFIX CMGF_CMD "=0", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
	if (!success)
		return false;

	auto sca = getSMSSca();
	if (!sca.length()) {
		cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
		return false;
	}

	LOG("Send PDU to %s", number.c_str());
	if (sca.startsWith("00"))
		sca.remove(0, 2);
	else if (sca.startsWith("0"))
		sca.remove(0, 1);

	String hex_str;
	int nbyte = 0;
	{
		uint8_t pdu[140 + 20];
		nbyte = pdu_encode(sca.c_str(), number.c_str(), content.c_str(), content.length(), pdu, sizeof(pdu));
		hex_str.reserve(nbyte * 2);
		to_hex_str(&hex_str, pdu, nbyte);
	}
	LOG("PDU mode: encode ASCII SMS to %d byte PDU", nbyte);
	if (nbyte > 0) {
		{
			String command(AT_PREFIX CMGS_CMD);
			command.concat('=');
			auto tpdu_len = nbyte - ceilf(sca.length() / 2.0) - 2;
			command.concat(String((int)tpdu_len, DEC));
			success = cmd(command.c_str(), ">", PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
		}
		delay(100);
		if (success) {
			char ctrlZ[] = { 0x1a, 0x00 };
			stream->print(hex_str);
			stream->print(ctrlZ);
		}
	}
	cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	return success;
}

/*!
 * \brief A6lib::sendPDU Send a UCS2 SMS in PDU mode.
 * \param number the detination phone number which should begin with international code
 * \param content the SMS content coded in UCS2 format and up to 70 chars.
 * \param len the number of UCS2 chars in \a content
 * \return true on success
 */
bool A6lib::sendPDU(const String& number, wchar_t* content, uint8_t len) {
	if (len > 70) {
		LOG("PDU mode: Max UCS2 chars length exceeded!");
		return false;
	}

	/* switch to PDU mode */
	auto success = cmd(AT_PREFIX CMGF_CMD "=0", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
	if (!success)
		return false;

	auto sca = getSMSSca();
	if (!sca.length()) {
		cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
		return false;
	}

	LOG("Send PDU to %s", number.c_str());
	if (sca.startsWith("00"))
		sca.remove(0, 2);
	else if (sca.startsWith("0"))
		sca.remove(0, 1);

	String hex_str;
	int nbyte = 0;
	{
		uint8_t pdu[140 + 20];
		nbyte = pdu_encodew(sca.c_str(), number.c_str(), content, len, pdu, sizeof(pdu));
		hex_str.reserve(nbyte * 2);
		to_hex_str(&hex_str, pdu, nbyte);
	}
	LOG("PDU mode: encode UCS2 SMS to %d byte PDU", nbyte);
	if (nbyte > 0) {
		{
			String command(AT_PREFIX CMGS_CMD);
			command.concat('=');
			auto tpdu_len = nbyte - ceilf(sca.length() / 2.0) - 2;
			command.concat(String((int)tpdu_len, DEC));
			success = cmd(command.c_str(), ">", PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
		}
		delay(100);
		if (success) {
			char ctrlZ[] = { 0x1a, 0x00 };
			stream->print(hex_str);
			stream->print(ctrlZ);
		}
	}
	cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	return success;
}

/*!
 * \brief A6lib::readSMS Read a SMS in modem prefered storage area
 * \param index sms index in storage area
 * \return a SMSInfo object contain SMS information(number+date+timestamp) on success, and if fail an empty SMSInfo object.
 */
SMSInfo A6lib::readSMS(uint8_t index) {
	String response;
	String command(AT_PREFIX CMGR_CMD);
	command.concat('=');
	command.concat(String(index, DEC));

	SMSInfo info;
	if (cmd(command.c_str(), "\xff\r\nOK\r\n", "\r\nOK\r\n", A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &response)) {
		response.replace("\"REC READ\",", "");
		response.replace("\"REC UNREAD\",", "");
		if (response.startsWith(CR LF))
			response.remove(0, 2);
		if (response.endsWith(CR LF))
			response.remove(response.length() - 2, 2);
		extract_sms(CMGR_CMD ":", &response, &info);
	}

	return info;
}

/*!
 * \brief A6lib::deleteSMS Delete a SMS from modem prefered storage area.
 * \param index sms index in storage area
 * \return true on success
 */
bool A6lib::deleteSMS(uint8_t index) {
	String command(AT_PREFIX CMGD_CMD);
	command.concat('=');
	command.concat(String(index, DEC));

	return cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, NULL);
}

/*!
 * \brief A6lib::setCharSet set the module charset.
 * \param charset the required charset.
 * Could be on of the following:
 * GSM
 * UCS2
 * HEX
 * PCCP936
 * \return true on success.
 */
bool A6lib::setCharSet(const String &charset) {
	String command(AT_PREFIX CSCS_CMD);
	command.concat('=');
	command.concat(charset);

	return cmd(command.c_str(), RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);
}

/*!
 * \brief A6lib::addHandler Add the A6lib main handler callback.
 * A6lib will call this handler when it is inside the waiting routine. it'll prevent lock in your code when you have some critical tasks to run.
 * Note: The result of passing loop() to this function is undefined!
 */
void A6lib::addHandler(void_cb_t cb) {
	if (cb)
		handler_cb = cb;
	else
		handler_cb = nullptr;
}

/*!
 * \brief A6lib::onSMSSent This function will register your callback and will call it when a SMS is sent.
 * \param cb pointer to callback function
 */
void A6lib::onSMSSent(sms_tx_cb_t cb) {
	if (cb)
		sms_tx_cb = cb;
	else // if user pass null -> we shouldn't callback for it
		sms_tx_cb = nullptr;
}

/*!
 * \brief A6lib::onSMSReceived This function will register your callback and will call it when new SMS arrives.
 * \param cb pointer to callback function
 */
void A6lib::onSMSReceived(sms_rx_cb_t cb) {
	if (cb)
		sms_rx_cb = cb;
	else
		sms_rx_cb = nullptr;
}

/*!
 * \brief A6lib::onSMSStorageFull This function will register your callback and will call it when modem prefered storage area is full.
 * \param cb pointer to callback function
 */
void A6lib::onSMSStorageFull(sms_full_cb_t cb) {
	if (cb)
		sms_full_cb = cb;
	else
		sms_full_cb = nullptr;
}



bool A6lib::begin(unsigned long baud) {
	bool success = true;
	stream->flush();

	if (!setBaudRate(baud))
		return false;

	// Factory reset.
	cmd(ATF_CMD, RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	// Echo off.
	cmd(ATE_CMD, RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	// Switch audio to headset.
	enableSpeaker(0);

	// Set caller ID on.
	cmd("AT+CLIP=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	// Set SMS to text mode.
	success = success & cmd("AT+CMGF=1", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	// Turn SMS indicators ON.
	cmd("AT+CNMI=0,1,0,0,0", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, nullptr);

	success = success & setPreferedStorage(SMSStorageArea::ME);

	// Set SMS character set.
	success = success & setCharSet(UCS2);

	return success;
}

void A6lib::powerOff(uint8_t pin) const {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);
}

void A6lib::powerOn(uint8_t pin) const {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, HIGH);
}

unsigned long A6lib::detectBaudRate() {
	LOG("Autodetecting serial baud rate...");
	unsigned long baud = 0;
	unsigned long rates[] = { 9600, 115200 };
	for (int i = 0; i < countof(rates); i++) {
		baud = rates[i];
		LOG("Trying baud rate %lu...", baud);
		if (ports.isSoftwareSerial())
			ports.sport->begin(baud);
		else
			ports.hport->begin(baud);

		delay(100);
		if (cmd("AT", RES_OK, "+CME", A6_CMD_TIMEOUT / 2, A6_CMD_MAX_RETRY * 2, NULL))
			return baud;
	}
	LOG("Couldn't detect the rate.");

	return 0;
}

bool A6lib::setBaudRate(unsigned long baud) {
	auto rate = detectBaudRate();
	if (!rate)
		return false;

	auto serialBaud = ports.isSoftwareSerial() ? ports.sport->baudRate() : ports.hport->baudRate();
	if (baud == rate && baud == serialBaud) // all in well
		return true;

	LOG("Setting baud rate(%lu) on the module...", baud);
	String command(AT_PREFIX IPR_CMD);
	command.concat('=');
	command.concat(baud);
	if (cmd(command.c_str(), RES_OK, IPR_CMD "=", A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, NULL)) {
		if (ports.isSoftwareSerial())
			ports.sport->begin(rate);
		else
			ports.hport->begin(rate);
		return true;
	} else {
		return false;
	}
}

String A6lib::readFromSerial() const {
	String reply;

	if (stream->available())
		reply = stream->readString();

	// XXX: Replace NULs with \xff so we can match on them.
	for (int x = 0; x < reply.length(); x++) {
		if (reply.charAt(x) == 0)
			reply.setCharAt(x, 255);
	}

	return reply;
}

bool A6lib::cmd(const char *command, const char *resp1, const char *resp2, uint16_t timeout, uint8_t max_retry, String *response) {
	bool success = false;
	stream->flush();

	while (max_retry-- && !success) {
		LOG("Issuing command: %s", command);

		stream->write(command);
		stream->write('\r');
		success = wait(resp1, resp2, timeout, response);
	}

	return success;
}

bool A6lib::wait(const char *resp1, const char *resp2, uint16_t timeout, String *response) {
	LOG("Waiting for reply...");
	unsigned long entry = millis();
	String reply;
	bool success = false;
	do {
		reply += readFromSerial();
		yield();
		if (handler_cb) /* prevent lock in user code */
			handler_cb();
	} while (((reply.indexOf(resp1) + reply.indexOf(resp2)) == -2) && ((millis() - entry) < timeout));

	if (reply.length() != 0) {
		LOG("Reply in %lu ms\n", millis() - entry);
#ifdef DEBUG
		DEBUG_PORT.print(reply);
#endif
	}

	if (response != nullptr)
		*response = reply;

	if ((millis() - entry) >= timeout) {
		success = false;
		LOG("Timed out.");
	} else {
		if (reply.indexOf(resp1) + reply.indexOf(resp2) > -2) {
			LOG("Reply OK.");
			success = true;
		} else {
			LOG("Reply NOT OK.");
			success = false;
		}
	}

	return success;
}
