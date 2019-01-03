#ifndef A6LIB_H
#define A6LIB_H

extern "C" {
#include<time.h>
}

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>

/* comment the following to disable them */
//#define DEBUG
#define SIM800_T
//#define A6_T

///@cond INTERNAL
enum call_direction {
	DIR_OUTGOING = 0,
	DIR_INCOMING = 1
};

enum call_state {
	CALL_ACTIVE = 0,
	CALL_HELD = 1,
	CALL_DIALING = 2,
	CALL_ALERTING = 3,
	CALL_INCOMING = 4,
	CALL_WAITING = 5,
	CALL_RELEASE = 7
};

enum call_mode {
	MODE_VOICE = 0,
	MODE_DATA = 1,
	MODE_FAX = 2,
	MODE_VOICE_THEN_DATA_VMODE = 3,
	MODE_VOICE_AND_DATA_VMODE = 4,
	MODE_VOICE_AND_FAX_VMODE = 5,
	MODE_VOICE_THEN_DATA_DMODE = 6,
	MODE_VOICE_AND_DATA_DMODE = 7,
	MODE_VOICE_AND_FAX_FMODE = 8,
	MODE_UNKNOWN = 9
};

struct callInfo {
	int index;
	call_direction direction;
	call_state state;
	call_mode mode;
	int multiparty;
	String number;
	int type;
};
///@endcond

enum DeviceStatus {
	Status_Ready = 0,
	Status_Unknown = 2,
	Status_Ringing = 3,
	Status_Call_In_Progress = 4,
};

enum CharSet {
	Gsm,
	Ucs2,
	Hex,
	Pccp936
};

enum RegisterStatus {
	NotRegistered = 0,
	Registered_HomeNetwork = 1,
	Searching_To_Register = 2,
	Register_Denied = 3,
	Unknown = 4,
	Registered_Roaming = 5,
};

class SMSInfo {
public:
	SMSInfo() : number{}, dateTime{}, message{} {

	}

	String number;
	String dateTime;
	String message;
};

enum SMSStorageArea {
	ME = 1, /* modem storage area */
	SM, /* sim card storage area */
	MT, /* all storage areas associated with modem or mobile termination */
#ifdef SIM800_T
	SM_P,
	ME_P,
#endif
};

enum SMSRecordType {
	All,
	Unread,
	Read,
};

typedef void(*void_cb_t)(void);
typedef void (*sms_rx_cb_t)(uint8_t indx, const SMSInfo&);
typedef void(*sms_tx_cb_t)(void);
typedef void_cb_t sms_full_cb_t;

class A6lib {
public:
	A6lib(HardwareSerial* port);
	A6lib(SoftwareSerial* port);
	A6lib(uint8_t rx_pin, uint8_t tx_pin);
	~A6lib();
#ifdef DEBUG
	void setDebugStream(Stream*);
#endif
	void handle();
	bool start(uint8_t max_retry);
	bool waitForNetwork(unsigned long baud, uint16_t time_out /* ms */);
#ifdef A6_T
	void powerUp(int pin);
	void softReset();
#endif
	void hardReset(uint8_t pin);

	String getSIMNumber();
	DeviceStatus getDeviceStatus();
	String getFirmWareVer();
	int getRSSI();
	uint8_t getSignalQuality();
	time_t getRealTimeClock();
	String getRealTimeClockString(const String& format = String());
	String getIMEI();
	String getSMSSca();
	RegisterStatus getRegisterStatus();
#ifdef SIM800_T
	String getOperatorName();
#endif

	///@cond INTERNAL
	static String deviceStatusToString(DeviceStatus);
	static String registerStatusToString(RegisterStatus);
	static String charsetToString(CharSet);
	static String recordTypeToString(SMSRecordType);
	///@endcond

	String sendUSSD(const String& ussd_code, uint16_t timeout = -1);
	bool setSMSStorageArea(SMSStorageArea);
	bool setCharSet(CharSet);
	bool sendSMS(const String& number, const String& text);
	bool sendPDU(const String& number, const String& content);
	bool sendPDU(const String& number, uint16_t* content, uint8_t len);
	SMSInfo readSMS(uint8_t index);
	bool deleteSMS(uint8_t index, bool del_all = false);
	int8_t getSMSList(int8_t* buff, uint8_t len, SMSRecordType record);

	///@cond INTERNAL
	void dial(String number);
	void redial();
	void answer();
	void hangUp();
	callInfo checkCallStatus();
	void setVol(byte level);
	void enableSpeaker(byte enable);
	///@endcond

	void addHandler(void_cb_t);
	void onSMSSent(sms_tx_cb_t);
	void onSMSReceived(sms_rx_cb_t);
	void onSMSStorageFull(sms_full_cb_t);

	///@cond INTERNAL
	bool isSIMInserted();
	bool isBusy() {
		return isWaiting;
	}
	bool isRegsitered() {
		const auto status = getRegisterStatus();
		return status == Registered_HomeNetwork || status == Registered_Roaming;
	}
	void setStreamTimeOut(uint16_t);
	///@endcond

	String sendCommand(const String& command, uint16_t reply_timeout = 2000);

protected:
	///@cond INTERNAL
	void dbg(const char* format, ...) const;
	static String toTime(const char* cclk_str, const String& format);
	static void toHex(String* in, uint8_t* pdu, uint8_t pdu_len);

	bool begin();
	bool setBaudRate(unsigned long baud);

	void powerOn(uint8_t pin) const;
	void powerOff(uint8_t pin) const;

	void parseForNotifications(String* data);
	bool hasNotifications(const String& arg);

	String streamData() const;
	bool cmd(const char *command, const char *resp1, const char *resp2, uint16_t timeout, uint8_t max_retry, String *response = nullptr);
	bool wait(const char *resp1, const char *resp2, uint16_t timeout, String *response);
	///@endcond

private:
#ifdef DEBUG
	Stream* dbg_stream = nullptr;
#endif
	Stream* stream = nullptr;
	bool isWaiting = false;
	struct SerialPorts {
		enum PortState {
			Using_SoftWareSerial = 1,
			Using_HardWareSerial,
			New_SoftwareSerial,
		};
		PortState state;
		bool testState(PortState st) const {
			return state == st;
		}
		bool isSoftwareSerial() const {
			return state == Using_SoftWareSerial || state == New_SoftwareSerial;
		}
		union {
			HardwareSerial* hport;
			SoftwareSerial* sport;
		};
	} ports;
	using PortState = SerialPorts::PortState;

	void_cb_t handler_cb;
	sms_rx_cb_t sms_rx_cb;
	sms_tx_cb_t sms_tx_cb;
	sms_full_cb_t sms_full_cb;

	String lastInterestedReply;
};

#endif // !A6LIB_H
