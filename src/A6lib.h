#ifndef A6LIB_H
#define A6LIB_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>

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

struct SMSInfo {
	SMSInfo() : number{}, date{}, message{} {

	}

	String number;
	String date;
	String message;
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

enum RegisterStatus {
	NotRegistered = 0,
	Registered_HomeNetwork = 1,
	Searching_To_Register = 2,
	Register_Denied = 3,
	Unknow = 4,
	Registered_Roaming = 5,
};

enum SMSStorageArea {
	ME = 1, /* modem storage area */
	SM, /* sim card storage area */
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

	void handle();
	bool start(unsigned long baud, uint8_t max_retry);

	void powerUp(int pin);
	void hardReset(uint8_t pin);
	void softReset();

	String getFirmWareVer();
	int8_t getRSSI();
	uint8_t getSignalQuality();
	String getRealTimeClock();
	String getIMEI();
	String getSMSSca();
	RegisterStatus getRegisterStatus();
	static String registerStatusToString(RegisterStatus);

	bool setPreferedStorage(SMSStorageArea);
	bool setCharSet(const String &charset);
	bool sendSMS(const String& number, const String& text);
	bool sendPDU(const String& number, const String& content);
	bool sendPDU(const String& number, wchar_t* content, uint8_t len);
	SMSInfo readSMS(uint8_t index);
	bool deleteSMS(uint8_t index);
	int8_t getSMSList(int8_t* buff, uint8_t len, SMSRecordType record);

	void dial(String number);
	void redial();
	void answer();
	void hangUp();
	callInfo checkCallStatus();
	void setVol(byte level);
	void enableSpeaker(byte enable);

	void addHandler(void_cb_t);
	void onSMSSent(sms_tx_cb_t);
	void onSMSReceived(sms_rx_cb_t);
	void onSMSStorageFull(sms_full_cb_t);

	String sendCommand(const String& command, uint16_t reply_timeout = 2000);
	///@cond INTERNAL
	bool isBusy() {
		return isWaiting;
	}
	///@endcond

protected:
	///@cond INTERNAL
	bool begin(unsigned long baud);

	unsigned long detectBaudRate();
	bool setBaudRate(unsigned long baud);

	void powerOn(uint8_t pin) const;
	void powerOff(uint8_t pin) const;

	void parseForNotifications(String* data);
	bool hasNotifications(const String& arg);

	String readFromSerial() const;
	bool cmd(const char *command, const char *resp1, const char *resp2, uint16_t timeout, uint8_t max_retry, String *response);
	bool wait(const char *resp1, const char *resp2, uint16_t timeout, String *response);
	///@endcond

private:
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

#endif // A6LIB_H
