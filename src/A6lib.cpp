extern "C" {
#include <stdio.h>
#include <stdarg.h>
#include "pdu.h"
}

#include "A6lib.h"

///@cond INTERNAL
#ifdef ARDUINO_ARCH_ESP8266
#	define minimum(a, b) _min(a, b)
#	define maximum(a, b) _max(a, b)
#else // ARDUINO_ARCH_AVR
#	define minimum(a, b) min(a, b)
#	define maximum(a, b) max(a, b)
#endif

#define Literal(arg) String(F(arg))
#define countof(a) (sizeof(a) / sizeof(a[0]))
#define A6_CMD_TIMEOUT 2000
#define A6_CMD_MAX_RETRY 2
#define STREAM_TIMEOUT 200 // ms

#define PLACE_HOLDER "XX"
#define RES_OK "OK"
#define RES_ERR "ERROR"
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
#define CNMI_CMD "+CNMI"
#define CUSD_CMD "+CUSD"
#define CSPN_CMD "+CSPN"
#define CPAS_CMD "+CPAS"
#define CNUM_CMD "+CNUM"
#define CME_CMD "+CME"
#define NOTIF_CMTI "+CMTI"
#define NOTIF_CIEV "+CIEV"
#define UCS2 "UCS2"
#define CR "\r"
#define LF "\n"
#define CTRLZ char(0x1A)
///@endcond

/*!
 * \class A6lib
 * \brief A library for controlling Ai-Thinker A6 GSM modem(also works with others like SIM800).
 *
 * An Arduino library for communicating with the AI-Thinker A6 GSM modem, It currently supports ESP8266 and AVR architectures.
 * This small lib mainly intended for Ai-Thinker A6 modem but may possiblly work with other GSM modems supporting standard AT command set (e.g SIM800, SIM900 ,...).
 * Using this lib is straightforward, you can create an object of A6lib via HardwareSerial, SoftwareSerial or just two pin number for built in SoftwareSerial.
 * Then you usually should power up your module (A6lib::powerUp()) and initlize A6lib object to start communicating with modem at desired baud rate. 
 * from now on, use public APIs to control your modem and get informations from it.
 * Also there's a rich debugging part inisde the library, to enable it define DEBUG in your environment.
 *
 * This lib has been modified to be asynchronous, so currently you can pass your functions to register APIs to catch these events:
 *  -# SMS sent
 *  -# SMS recevied
 *  -# Storage area is full
 *
 * \note A note about A6lib::addHandler(): When you have some important tasks in your code for example reading keypad etc, you can add a main function for running those tasks and pass it to 
 * A6lib::addHandler(), when you pass a valid function, lib will call it whenever it's in waiting state (waiting for modem to reply at some time) and thus it'll prevent locking in that precious time.
 *
 * To get start you can check out examples directory.
 */

 /*!
  * Constructs A6lib object with the given serial \a port.
  * \param port HardwareSerial object for use inside A6lib.
  */
A6lib::A6lib(HardwareSerial* port) : stream{ port } {
	stream->setTimeout(STREAM_TIMEOUT);
	ports.state = PortState::Using_HardWareSerial;
	ports.hport = port;
}

/*!
 * Constructs A6lib object with the given serial \a port.
 * \param port SoftwareSerial object for use inside A6lib
 */
A6lib::A6lib(SoftwareSerial* port) : stream{ port } {
	stream->setTimeout(STREAM_TIMEOUT);
	ports.state = PortState::Using_SoftWareSerial;
	ports.sport = port;
}

/*!
 * Constructs A6lib object with the given pin numbers. this is done by creating new SoftwareSerial object.
 * \param tx_pin SoftwareSerial TX pin
 * \param rx_pin SoftwareSerial RX pin
 */
A6lib::A6lib(uint8_t rx_pin, uint8_t tx_pin) {
	ports.state = PortState::New_SoftwareSerial;
	ports.sport = new SoftwareSerial(rx_pin, tx_pin);
	stream = ports.sport;
	stream->setTimeout(STREAM_TIMEOUT);
}

/*!
 * Destroys A6lib object.
 */
A6lib::~A6lib() {
	if (ports.testState(PortState::New_SoftwareSerial) && ports.sport)
		delete ports.sport;
}
///@cond INTERNAL
#ifdef DEBUG
void A6lib::setDebugStream(Stream* stream) {
	if (!stream)
		return;

	dbg_stream = stream;
	dbg_stream->flush();
}
#endif
void A6lib::dbg(const char* format, ...) const {
#ifdef DEBUG
	if (!dbg_stream)
		return;

	char buff[128];
	va_list args;
	va_start(args, format);
	vsnprintf(buff, sizeof(buff), format, args);
	va_end(args);
	dbg_stream->print(Literal("\n[A6lib] "));
	dbg_stream->print(buff);
#endif
}
///@endcond
/*!
 * This is the A6lib object initlizer routine.
 * you must usually call this after restarting your modem following by A6lib:waitForNetwork().
 * \param max_retry the maximum number of time A6lib object try to setup modem.
 * \return true on success
 */
bool A6lib::start(uint8_t max_retry) {
	bool success = false;
	while (!success && max_retry--) {
		success = begin();
		delay(500);
		dbg(Literal("initializing modem...").c_str());
	}

	return success;
}

/*!
* This method will wait for modem to trigger the registration indication which is the result of correct netowrk registration.
* you must call this usually before A6lib::start().
* \param baud the desired baud rate to start with
* \param time_out the maximum amount of time A6lib object wait for network registration indication.
* \return true on success
*/
bool A6lib::waitForNetwork(unsigned long baud, uint16_t time_out) {
	stream->flush();
	if (!setBaudRate(baud))
		return false;

	dbg(Literal("waiting for modem to register on GSM network...").c_str());
	auto start = millis();
	bool success = false;
	do {
		yield();
		stream->println(ATE_CMD);
#ifdef SIM800_T
		if (getDeviceStatus() == DeviceStatus::Status_Ready) {
			success = true;
			dbg(Literal("modem got ready after %lums").c_str(), millis() - start);
			break;
		}
#elif defined(A6_T)
		auto data = streamData();
		if (data.indexOf(Literal("+CREG: 1").c_str()) != -1) {
			dbg(Literal("modem got ready after %lums").c_str(), millis() - start);
			success = true;
			break;
		}
#endif
	} while (millis() - start < time_out);

	if (!success)
		if (getDeviceStatus() != DeviceStatus::Status_Ready && !isRegsitered())
			dbg(Literal("modem failed to register on network after %lums").c_str(), millis() - start);

	return success;
}

/*!
 * the main handler of A6lib object.
 * this function needs to be called inside main loop regularly, for callbacks to work correctly.
 */
void A6lib::handle() {
	/* there are some notifications, mixed with last modem reply */
	if (lastInterestedReply.length() != 0) {
		parseForNotifications(&lastInterestedReply);
		lastInterestedReply.remove(0);
	}

	if (!isWaiting && stream->available()) {
		auto reply = stream->readString();
		parseForNotifications(&reply);
	}
}
///@cond INTERNAL
void A6lib::parseForNotifications(String* data) {
	if (!data)
		return;

	if (!hasNotifications(*data))
		return;
	
	auto cmtiStart = data->indexOf(NOTIF_CMTI ":");
	auto cmgsStart = data->indexOf(CMGS_CMD ":");
	auto smsFull = data->indexOf(NOTIF_CIEV ":");
	dbg("new SMS indication!");
#ifdef DEBUG
	if (dbg_stream)
		dbg_stream->print(*data);
#endif
	if (cmtiStart != -1) {
		dbg(Literal("incoming SMS:").c_str());
		if (sms_rx_cb) {
			SMSInfo info;
			int indx = 0;
			const auto ok = sscanf(data->c_str(), Literal("%*[^+]+CMTI: \"%*[^\"]\",%d%*s").c_str(), &indx);
			if (ok > 0) {
				info = readSMS(indx);
				sms_rx_cb(indx, info);
			}
		}
	} else if (cmgsStart != -1) {
		dbg(Literal("SMS sent.").c_str());
		if (sms_tx_cb)
			sms_tx_cb();
	} else if (smsFull != -1 && data->indexOf("SMSFULL") != -1) {
		dbg(Literal("modem prefered storage is full!").c_str());
		if (sms_full_cb)
			sms_full_cb();
	}

}

bool A6lib::hasNotifications(const String& arg) {
	return arg.indexOf(NOTIF_CMTI ":") != -1 || arg.indexOf(CMGS_CMD ":") != -1 || arg.indexOf(NOTIF_CIEV ":") != -1;
}
///@endcond
#ifdef A6_T
/*!
 * this optional function will keep the PWR pin of modem in high TTL at start up to correctly powering the module.
 * A6 modem needs this pin to be in high TTL for about 2 sec.
 * \param pin the pin number which is connected to modem PWR pin(or PWR_KEY pin).
 */
void A6lib::powerUp(int pin) {
	dbg(Literal("powering up the modem...").c_str());
	powerOn(pin);
	delay(2000);
	powerOff(pin);
	stream->flush();
}

/*!
 * This function implement a software restart on module(if suppoerted).
 * Note: it will take some time for module to start + register for network.
 * You may also need to reinitilize module with A6lib::start().
 */
void A6lib::softReset() {
	cmd(AT_PREFIX RST_CMD, PLACE_HOLDER, PLACE_HOLDER, 0, 0);
}
#endif
/*!
* This function will do a hard reset on module.
* It's recommended to do this via an NMOS.
* Note: it will take some time for module to start + register for network.
* You may also need to reinitilize module with A6lib::start().
* \param pin the pin number which is connected to modem reset(RST) pin.
*/
void A6lib::hardReset(uint8_t pin) {
	powerOff(pin);
	delay(120);
	powerOn(pin);
}

/*!
* This function will check whether SIM card is inserted or not.
* \return true if SIM is inserted.
*/
bool A6lib::isSIMInserted() {
	String reply;
	
	return cmd(AT_PREFIX "+CPIN?", Literal("+CPIN").c_str(), RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply);
}

String A6lib::getSIMNumber() {
	String reply;
	if (cmd(AT_PREFIX CNUM_CMD, CNUM_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		char buff[32];
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CNUM: \"\",\"+%[^\"]\",%*d%*s").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}

	return String();
}

/*!
* get the current modem working status.
* \return on of the ::DeviceStatus value.
*/
DeviceStatus A6lib::getDeviceStatus() {
	String reply;
	if (cmd(AT_PREFIX CPAS_CMD, CPAS_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		int status = DeviceStatus::Status_Unknown;
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CPAS: %d%*s").c_str(), &status);
		if (ok > 0)
			return static_cast<DeviceStatus>(status);
	}

	return DeviceStatus::Status_Unknown;
}

/*!
* Get the revision identification or firmware version of modem.
* \return If success a String contain firmware version, and if fail an empty string.
*/
String A6lib::getFirmWareVer() {
	String reply;
	if (cmd(AT_PREFIX GMR_CMD, RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		reply.replace(Literal("Revision:"), "");
		char buff[32];
		const auto ok = sscanf(reply.c_str(), Literal("%s[^\r]%[^s]").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}

	return String();
}

/*!
* Get the modem signal strength based on RSSI(measured as dBm).
* \return If success a value between -113dBm and -51dBm and if fail 0.
*/
int A6lib::getRSSI() {
	String reply;
	int rssi = 0;
	if (cmd(AT_PREFIX CSQ_CMD, CSQ_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		/*
			return value:
			0 -> -113 dBm or less
			1 -> -111 dBm
			2-30 -> -109....-53 dBm
			31 -> -51 dBm or greater
			99 -> Uknown
		*/
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CSQ: %d,%*d%*s").c_str(), &rssi);
		if (ok > 0) {
			/* convert to RSSI */
			rssi -= 2;
			rssi *= 2;
			rssi += -109;
			return rssi;
		}
	}

	return rssi;
}

/*!
* Get the modem signal quality level.
* \return if success a value between 0-100 and if fail 255.
*/
uint8_t A6lib::getSignalQuality() {
	auto rssi = getRSSI();
	if (rssi == 0)
		return 255;

	/* convert RSSI to quality */
	uint8_t q;
	if (rssi <= -100) {
		q = 0;
	} else if (rssi >= -50) {
		q = 100;
	} else {
		q = 2 * (rssi + 100);
	}

	return q;
}

/*!
* Get the real time from modem(the return value is not necessary up to date).
* \return if success a value contain time as time_t(epoch), if fail an invalid(-1) value.
*/
time_t A6lib::getRealTimeClock() {
	String reply;
	if (cmd(AT_PREFIX CCLK_CMD "?", CCLK_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		struct tm time;
		time.tm_isdst = -1;
		int tz = 0;
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CCLK: \"%d/%d/%d,%d:%d:%d+%d\"").c_str(), &time.tm_year, &time.tm_mon, &time.tm_mday, &time.tm_hour, &time.tm_min, &time.tm_sec, &tz);
		if (ok > 0) {
			time.tm_year += 2000 - 1900;
			time.tm_mon -= 1;
			auto epoch = mktime(&time) + (tz * 15 * 60);
			return epoch;
		}
	}
	
	return (time_t)(-1);
}

/*!
* Get the real time string from modem. please refer to http://www.cplusplus.com/reference/ctime/strftime/ for format specifier.
* \return if success a string contain local time in format yyyy.MM.dd hh:mm:ss, if fail an empty string.
*/
String A6lib::getRealTimeClockString(const String& format) {
	const auto cclk = getRealTimeClock();
	if (cclk == (time_t)(-1))
		return String();

	char buff[32];
	if (format.length() > 0)
		strftime(buff, sizeof(buff), format.c_str(), localtime(&cclk));
	else
		strftime(buff, sizeof(buff), Literal("%Y.%m.%d,%H:%M:%S").c_str(), localtime(&cclk));

	return String(buff);
}

/*!
* Get the modem IMEI(serial number identification).
* \return if success a string contain IMEI number, if fail an empty string.
*/
String A6lib::getIMEI() {
	String reply;
	if (cmd(AT_PREFIX GSN_CMD, RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		char buff[32];
		const auto ok = sscanf(reply.c_str(), Literal("%s[\r]%*s").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}

	return String();
}

/*!
 * Get the current SMS service center address from modem.
 * \return if success a string contain SCA, if fail an empty string
 */
String A6lib::getSMSSca() {
	String reply;
	if (cmd(AT_PREFIX CSCA_CMD "?", CSCA_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		char buff[32];
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CSCA: \"+%[^\"]\",%*d%*s").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}
	
	return String();
}

/*!
* Get the network registration status of modem.
* \return on of the ::RegisterStatus value
*/
RegisterStatus A6lib::getRegisterStatus() {
	String reply;
	if (cmd(AT_PREFIX CREG_CMD "?", CREG_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		int status = static_cast<int>(RegisterStatus::Unknown);
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CREG: %*d,%d%*s").c_str(), &status);
		if (ok > 0)
			return static_cast<RegisterStatus>(status);
	}

	return RegisterStatus::Unknown;
}

/*!
* Get the Network operator name. note that the name is read from SIM card.
* \return if success a String contain the operator name, else an empty String
*/
#ifdef SIM800_T
String A6lib::getOperatorName() {
	String reply;
	if (cmd(AT_PREFIX CSPN_CMD "?", CSPN_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		char buff[32];
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CSPN: \"%[^\"]\",%*d%*s").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}

	return String();
}
#endif

///@cond INTERNAL
String A6lib::deviceStatusToString(DeviceStatus st) {
	switch (st) {
	case Status_Ready:
		return Literal("Ready");
		break;
	case Status_Unknown:
	default:
		return Literal("Unknown");
		break;
	case Status_Ringing:
		return Literal("Ringing");
		break;
	case Status_Call_In_Progress:
		return Literal("Call In Progress");
		break;
	}
}

String A6lib::registerStatusToString(RegisterStatus st) {
	switch (st) {
	case NotRegistered:
		return Literal("not registered");
		break;
	case Registered_HomeNetwork:
		return Literal("registered, home network");
		break;
	case Searching_To_Register:
		return Literal("not registered, currently searching a new operator");
		break;
	case Register_Denied:
		return Literal("registration denied");
		break;
	default:
	case Unknown:
		return Literal("unknown");
		break;
	case Registered_Roaming:
		return Literal("registered, roaming");
		break;
	}
}
///@endcond
/*!
 * Send new command to modem.
 * command should be a valid AT command, otherwise modem will return error with corresponding error code(this function will append \r\n to command).
 * Note: you may want to check modem is busy or not with A6lib::isBusy().
 * \param command the valid command to be sent with AT prefix
 * \param reply_timeout the amount of time(as ms) we wait for reply
 * \return if success an string contain modem reply, otherwise contain error code
 */
String A6lib::sendCommand(const String& command, uint16_t reply_timeout) {
	String reply;
	cmd(command.c_str(), RES_OK, RES_ERR, reply_timeout, A6_CMD_MAX_RETRY, &reply);

	return reply;
}

///@cond INTERNAL
// Dial a number.
void A6lib::dial(String number) {
	char buffer[50];

	dbg("Dialing number...");

	sprintf(buffer, "ATD%s;", number.c_str());
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2);
}


// Redial the last number.
void A6lib::redial() {
	dbg("Redialing last number...");
	cmd("AT+DLST", "OK", "CONNECT", A6_CMD_TIMEOUT, 2);
}


// Answer a call.
void A6lib::answer() {
	cmd("ATA", "OK", "yy", A6_CMD_TIMEOUT, 2);
}


// Hang up the phone.
void A6lib::hangUp() {
	cmd("ATH", "OK", "yy", A6_CMD_TIMEOUT, 2);
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
		dbg("Extra comma found.");
		cinfo.number = cinfo.number.substring(0, comma_index);
	}

	return cinfo;
}

// Set the volume for the speaker. level should be a number between 5 and
// 8 inclusive.
void A6lib::setVol(byte level) {
	char buffer[30];

	// level should be between 5 and 8.
	level = minimum(maximum(level, 5), 8);
	sprintf(buffer, "AT+CLVL=%d", level);
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2);
}

// Enable the speaker, rather than the headphones. Pass 0 to route audio through
// headphones, 1 through speaker.
void A6lib::enableSpeaker(byte enable) {
	char buffer[30];

	// enable should be between 0 and 1.
	enable = minimum(maximum(enable, 0), 1);
	sprintf(buffer, "AT+SNFS=%d", enable);
	cmd(buffer, "OK", "yy", A6_CMD_TIMEOUT, 2);
}
///@cond INTERNAL

/*!
* Send the USSD(Unstructured Supplementary Service Data) code to modem.
* note that modem CharSet should be in CharSet::GSM otherwise you should handle UCS2 parsing.
* \param ussd_code a valid USSD code that service senter support it(e.g *140*10#)
* \param timeout the amount of time (in milliseconds) we wait for USSD result, if not set defaulted to 3seconds.
* \return String contain USSD result
*/
String A6lib::sendUSSD(const String& ussd_code, uint16_t timeout) {
	char buff[64];
	sprintf(buff, Literal(AT_PREFIX CUSD_CMD "=1,\"%s\",15").c_str(), ussd_code.c_str());
	String reply;
	uint16_t time_out = (timeout == UINT16_MAX) ? A6_CMD_TIMEOUT * 1.5 : timeout;
	if (cmd(buff, CUSD_CMD, RES_ERR, time_out, A6_CMD_MAX_RETRY, &reply)) {
		char buff[128];
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CUSD: %*d, \"%[^\"], %*d%*s").c_str(), buff);
		if (ok > 0)
			return String(buff);
	}

	return reply;
}

/*!
 * Set the modem prefered SMS storage area.
 * It's set to SMSStorageArea::SM (SIM card) by defualt.
 * \param area could be on of the ::SMSStorageArea value
 * \return true on success
 */
bool A6lib::setSMSStorageArea(SMSStorageArea area) {
	String command(AT_PREFIX CPMS_CMD "=");
	/*
		SIM800 options: "SM", "ME", "SM_P", "ME_P", "MT"
		A6 options: "SM", "ME", "MT"
	*/
	switch (area) {
	default:
	case ME:
#ifdef A6_T
		command.concat(Literal("ME,ME,ME").c_str());
#elif defined(SIM800_T)
		command.concat(Literal("\"ME\",\"ME\",\"ME\"").c_str());
#endif
		break;
	case SM:
#ifdef A6_T
		command.concat(Literal("SM,SM,SM").c_str());
#elif defined(SIM800_T)
		command.concat(Literal("\"SM\",\"SM\",\"SM\"").c_str());
#endif
		break;
	case MT:
#ifdef A6_T
		command.concat(Literal("MT,MT,MT").c_str());
#elif defined(SIM800_T)
		command.concat(Literal("\"MT\",\"MT\",\"MT\"").c_str());
#endif
		break;
#ifdef SIM800_T
	case SM_P:
		command.concat(Literal("\"SM_P\",\"SM_P\",\"SM_P\"").c_str());
		break;
	case ME_P:
		command.concat(Literal("\"ME_P\",\"ME_P\",\"ME_P\"").c_str());
		break;
#endif
	}

	return cmd(command.c_str(), CPMS_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
}
///@cond INTERNAL
String A6lib::recordTypeToString(SMSRecordType type) {
	switch (type) {
	default:
	case All:
		return Literal("ALL");
		break;
	case Unread:
		return Literal("REC UNREAD");
		break;
	case Read:
		return Literal("REC READ");
		break;
	}
}
///@endcond
/*!
 * Get the list of available SMS in prefered storage area.
 * \param buff input buffer to store SMS indexes.
 * \param len size of buff
 * \param record on of the ::SMSRecordType.
 * \return if fail -1, otherwise number of founded SMS.
 */
int8_t A6lib::getSMSList(int8_t* buff, uint8_t len, SMSRecordType record) {
	if (buff == nullptr || len < 0)
		return -1;

	String command(AT_PREFIX CMGL_CMD "=\"");
	command.concat(recordTypeToString(record));
	command.concat('"');

	String reply;
	if (!cmd(command.c_str(), CMGL_CMD, RES_OK, A6_CMD_TIMEOUT * 2.5, A6_CMD_MAX_RETRY, &reply))
		return -1;

	if (reply.startsWith(CR LF))
		reply.remove(0, 2);

	memset(buff, 0, len);
	int8_t count = 0; // total number of SMSs
	char c_str[reply.length() + 1];
	c_str[reply.length()] = 0;
	reply.toCharArray(c_str, reply.length());
	reply.remove(0);
	auto  tok = strtok(c_str, CR LF);
	while (tok != nullptr) {
		if (strstr(tok, CMGL_CMD)) {
			int indx;
			const auto ok = sscanf(tok, Literal("+CMGL: %d,%*s").c_str(), &indx);
			if (ok > 0 && indx > 0)
				buff[count++] = indx;
		}
		tok = strtok(nullptr, CR LF);
	}

	return count;
}

/*!
 * Send SMS (in text mode) to specified number.
 * \param number valid destination number without +
 * \param text SMS content in ascii encoding
 * \return true on success
 */
bool A6lib::sendSMS(const String& number, const String& text) {
	if (text.length() > 80 * 2) {
		dbg(Literal("TEXT mode: max ASCII chars exceeded!").c_str());
		return false;
	}

	dbg(Literal("sending SMS to %s").c_str(), number.c_str());
	String command(AT_PREFIX CMGS_CMD "=\"");
	command.concat(number);
	command.concat('"');
	auto success = cmd(command.c_str(), ">", CMGS_CMD, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
	delay(5);
	if (success) {
		stream->print(text.c_str());
		stream->print(CTRLZ);
	}

	return success;
}

/*!
 * Send an ASCII SMS in PDU mode.
 * \param number the detination phone number which should begin with international code
 * \param content the SMS content in ASCII and up to 160 chars
 * \return true on success
 */
bool A6lib::sendPDU(const String& number, const String& content) {
	if (content.length() > 80 * 2) {
		dbg(Literal("PDU mode: max ASCII chars exceeded!").c_str());
		return false;
	}

	/* switch to PDU mode */
	auto success = cmd(AT_PREFIX CMGF_CMD "=0", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
	if (!success)
		return false;

	auto sca = getSMSSca();
	if (!sca.length()) {
		cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY * 2);
		return false;
	}

	dbg(Literal("send PDU to %s").c_str(), number.c_str());
	String hex_str;
	int nbyte = 0;
	{
		uint8_t pdu[140 + 20];
		nbyte = pdu_encode(sca.c_str(), number.c_str(), content.c_str(), content.length(), pdu, sizeof(pdu));
		hex_str.reserve(nbyte * 2);
		toHex(&hex_str, pdu, nbyte);
	}
	dbg(Literal("PDU mode: encode ASCII SMS to %d byte PDU").c_str(), nbyte);
	if (nbyte > 0) {
		{
			String command(AT_PREFIX CMGS_CMD "=");
			auto tpdu_len = nbyte - ceilf(sca.length() / 2.0) - 2;
			command.concat(String((int)tpdu_len, DEC));
			success = cmd(command.c_str(), ">", CMGS_CMD, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
		}
		delay(100);
		if (success) {
			stream->print(hex_str);
			stream->print(CTRLZ);
		}
	}
	cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY * 2);

	return success;
}

/*!
 * Send a UCS2 SMS in PDU mode.
 * \param number the detination phone number which should begin with international code
 * \param content the SMS content coded in UCS2 format and up to 70 chars.
 * \param len the number of UCS2 chars in \a content
 * \return true on success
 */
bool A6lib::sendPDU(const String& number, uint16_t* content, uint8_t len) {
	if (len > 70) {
		dbg(Literal("PDU mode: max UCS2 chars length exceeded!").c_str());
		return false;
	}

	/* switch to PDU mode */
	auto success = cmd(AT_PREFIX CMGF_CMD "=0", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
	if (!success)
		return false;

	auto sca = getSMSSca();
	if (!sca.length()) {
		cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY * 2);
		return false;
	}

	dbg(Literal("send PDU to %s").c_str(), number.c_str());
	String hex_str;
	int nbyte = 0;
	{
		uint8_t pdu[140 + 20];
		nbyte = pdu_encodew(sca.c_str(), number.c_str(), content, len, pdu, sizeof(pdu));
		hex_str.reserve(nbyte * 2);
		toHex(&hex_str, pdu, nbyte);
	}
	dbg(Literal("PDU mode: encode UCS2 SMS to %d byte PDU").c_str(), nbyte);
	if (nbyte > 0) {
		{
			String command(AT_PREFIX CMGS_CMD "=");
			auto tpdu_len = nbyte - ceilf(sca.length() / 2.0) - 2;
			command.concat(String((int)tpdu_len, DEC));
			success = cmd(command.c_str(), ">", CMGS_CMD, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
		}
		delay(100);
		if (success) {
			stream->print(hex_str);
			stream->print(CTRLZ);
		}
	}
	cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY * 2);

	return success;
}

/*!
 * Read a SMS in modem prefered storage area
 * \param index sms index in storage area
 * \return a SMSInfo object contain SMS information(number+date+timestamp) on success, and if fail an empty SMSInfo object.
 */
SMSInfo A6lib::readSMS(uint8_t index) {
	String reply;
	String command(AT_PREFIX CMGR_CMD "=");
	command.concat(String(index, DEC));

	SMSInfo info;
	if (cmd(command.c_str(), CMGR_CMD, RES_OK, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY, &reply)) {
		char phone[16];
		char time[32];
		char content[160];
#ifdef A6_T
		const auto ok = sscanf(reply.c_str(), Literal("%*[^+]+CMGR: \"%*[^\"]\",\"+%[^\"]\",,\"%[^\"]\"\r\n%[^OK]").c_str(), phone, time, content);
#else
		bool ok = false, has_contact_part = reply.indexOf(",\"\",") == -1;
		if (has_contact_part)
			ok = sscanf(reply.c_str(), Literal("%*[^+]+CMGR: \"%*[^\"]\",\"+%[^\"]\",\"%*[^\"]\",\"%[^\"]\"\r\n%[^OK]").c_str(), phone, time, content);
		else
			ok = sscanf(reply.c_str(), Literal("%*[^+]+CMGR: \"%*[^\"]\",\"+%[^\"]\",\"\",\"%[^\"]\"\r\n%[^OK]").c_str(), phone, time, content);
#endif
		if (ok > 0) {
			info.number = String(phone);
			info.dateTime = toTime(time, Literal("%Y/%m/%d,%H:%M:%S"));
			info.message = String(content);
			if (info.message.endsWith(CR LF))
				info.message.remove(info.message.length() - 2, 2);
			return info;
		}
	}

	return info;
}

/*!
 * Delete a SMS from modem prefered storage area.
 * Note: if del_all is true, index will be ignored and all SMS will be deleted in storage area.
 * \param index sms index in storage area
 * \return true on success
 */
bool A6lib::deleteSMS(uint8_t index, bool del_all) {
	String command(AT_PREFIX CMGD_CMD "=");
	if (del_all)
		command.concat(Literal("1,4"));
	else
		command.concat(String(index, DEC));

	return cmd(command.c_str(), RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
}

/*!
 * set the module charset.
 * \param charset the required charset.
 * could be on of the ::Charset value
 * \return true on success.
 */
bool A6lib::setCharSet(CharSet set) {
	String command(AT_PREFIX CSCS_CMD "=");
	command.concat("\"" + charsetToString(set) + "\"");

	return cmd(command.c_str(), RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
}
///@cond INTERNAL
String A6lib::charsetToString(CharSet set) {
	switch (set) {
	default:
	case Gsm:
		return Literal("GSM");
		break;
	case Ucs2:
		return Literal("UCS2");
		break;
	case Hex:
		return Literal("HEX");
		break;
	case Pccp936:
		return Literal("PCCP936");
		break;
	}
}
///@endcond
/*!
 * Add the A6lib main handler callback.
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
 * This function will register your callback and will call it when a SMS is sent.
 * \param cb pointer to callback function
 */
void A6lib::onSMSSent(sms_tx_cb_t cb) {
	if (cb)
		sms_tx_cb = cb;
	else // if user pass null -> we shouldn't callback for it
		sms_tx_cb = nullptr;
}

/*!
 * This function will register your callback and will call it when new SMS arrives.
 * \param cb pointer to callback function
 */
void A6lib::onSMSReceived(sms_rx_cb_t cb) {
	if (cb)
		sms_rx_cb = cb;
	else
		sms_rx_cb = nullptr;
}

/*!
 * This function will register your callback and will call it when modem prefered storage area is full.
 * \param cb pointer to callback function
 */
void A6lib::onSMSStorageFull(sms_full_cb_t cb) {
	if (cb)
		sms_full_cb = cb;
	else
		sms_full_cb = nullptr;
}


///@cond INTERNAL
String A6lib::toTime(const char* cclk_str, const String& format) {
	/* cclk_str should be in this format: yy/MM/dd,hh:mm:ss+tz */
	struct tm time_stamp;
	time_stamp.tm_isdst = -1;
	int tz = 0;
	const auto ok = sscanf(cclk_str, Literal("%d/%d/%d,%d:%d:%d+%d").c_str(), &time_stamp.tm_year, &time_stamp.tm_mon, &time_stamp.tm_mday, &time_stamp.tm_hour, &time_stamp.tm_min, &time_stamp.tm_sec, &tz);
	if (ok) {
		/* sim800 return time's year as 2-digit but A6 as 4-digit */
		auto y = time_stamp.tm_year;
		if (y > 999)
			time_stamp.tm_year += -1900;
		else
			time_stamp.tm_year += 2000 - 1900;
		time_stamp.tm_mon -= 1;
		auto epoch = mktime(&time_stamp) + (tz * 15 * 60);

		char buff[32];
		strftime(buff, sizeof(buff), format.c_str(), localtime(&epoch));
		return String(buff);
	}

	return String();
}

void A6lib::toHex(String* in, uint8_t* pdu, uint8_t len) {
	if (!in || !pdu || len == 0)
		return;

	auto hex = [](uint8_t h) {
		char tmp[3];
		sprintf(tmp, "%02x", h);
		String str(tmp);
		str.toUpperCase();

		return str;
	};

	for (size_t i = 0; i < len; i++)
		in->concat(hex(pdu[i]));
}

bool A6lib::begin() {
	bool success = true;

	/* SMS format -> text mode */
	success = success && cmd(AT_PREFIX CMGF_CMD "=1", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
	/* SMS indications -> On */
#ifdef A6_T
	success = success && cmd(AT_PREFIX CNMI_CMD "=0,1,0,0,0", RES_OK, PLACE_HOLDER, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
#elif defined(SIM800_T)
	success = success && cmd(AT_PREFIX CNMI_CMD "=1,1,0,0,0", RES_OK, RES_ERR, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY);
#endif
	/* SMS storage area -> SIM */
	success = success && setSMSStorageArea(SMSStorageArea::SM);
	/* char set -> UCS2 */
	success = success && setCharSet(CharSet::Gsm);

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

bool A6lib::setBaudRate(unsigned long baud) {
	if (ports.testState(PortState::Using_SoftWareSerial))
		dbg(Literal("starting with SoftwareSerial object").c_str());
	else if (ports.testState(PortState::Using_HardWareSerial))
		dbg(Literal("starting with HardwareSerial object").c_str());
	else
		dbg(Literal("starting with new SoftwareSerial object").c_str());

	if (ports.isSoftwareSerial())
		ports.sport->begin(baud);
	else
		ports.hport->begin(baud);
	delay(50);

	dbg(Literal("setting baud rate(%lu) on the module...").c_str(), baud);
	String command(AT_PREFIX IPR_CMD "=");
	command.concat(baud);

	return cmd(command.c_str(), RES_OK, IPR_CMD, A6_CMD_TIMEOUT, A6_CMD_MAX_RETRY * 4);
}

String A6lib::streamData() const {
	String reply;
	if (stream->available())
		reply = stream->readString();

	/* replace NULLs with 0xFF so we can match on them. */
	for (size_t x = 0; x < reply.length(); x++) {
		if (reply.charAt(x) == 0)
			reply.setCharAt(x, 0xFF);
	}

	return reply;
}

bool A6lib::cmd(const char *command, const char *resp1, const char *resp2, uint16_t timeout, uint8_t max_retry, String *response) {
	bool success = false;
	while (max_retry-- && !success) {
		dbg(Literal("issuing command: %s").c_str(), command);
		stream->println(command);
		stream->flush();
		yield();
		success = wait(resp1, resp2, timeout, response);
	}

	return success;
}

bool A6lib::wait(const char *response1, const char *response2, uint16_t timeout, String *response) {
	dbg(Literal("waiting for reply...").c_str());
	auto start = millis();
	isWaiting = true;
	bool success = false;
	String reply;
	reply.reserve(64);

	do {
		yield();
		if (handler_cb)
			handler_cb();
		reply.concat(streamData());
		if (reply.length() && (reply.indexOf(response1) != -1 || reply.indexOf(response2) != -1)) {
			success = true;
			dbg("reply in %lu ms:\n", millis() - start);
#ifdef DEBUG
			if (dbg_stream)
				dbg_stream->print(reply);
#endif
			/* maybe some notifications included in command's reply, so we check for sure */
			if (hasNotifications(reply))
				lastInterestedReply = reply; // schedule for calling callbacks

			if (response)
				*response = reply;
			break;
		}
	} while (millis() - start < timeout);
	isWaiting = false;
	if ((millis() - start > timeout) && !reply.length()) {
		success = false;
		dbg("reply timeout out!");
	}

	return success;
}
///@endcond