#ifndef MYNMEA_H
#define MYNMEA_H

#include <limits.h>

#include <Arduino.h>

class myNMEA {
public:

	static const char* skipField(const char* s);
	static unsigned int parseUnsignedInt(const char* s, uint8_t len);
	static long parseFloat(const char* s, uint8_t log10Multiplier,
						   const char** eptr = NULL);
	static const char* parseToComma(const char* s, char *result = NULL,
									int len = 0);
	static const char* parseField(const char* s, char *result = NULL,
								  int len = 0);
	static const char* generateChecksum(const char* s, char* checksum);
	static bool testChecksum(const char* s);

	// Write myNMEA sentence to output stream. Sentence must start with
	// '$', the checksum and <CR><NL> terminators will be appended
	// automatically.
	static Stream& sendSentence(Stream &s, const char* sentence);

	// Object with no buffer allocated, must call setBuffer later
	myNMEA(void);

	// User must decide and allocate the buffer
	myNMEA(void* buffer, uint8_t len);

	void setBuffer(void* buf, uint8_t len);

	// Clear all fix information. isValid() will return false, Year,
	// month and day will all be zero. Hour, minute and second time will
	// be set to 99. Speed, course and altitude will be set to
	// LONG_MIN; the altitude validity flag will be false. Latitude and
	// longitude will be set to 999 degrees.
	void clear(void);

	// Validity of latest fix
	bool isValid(void) const {
		return _isValid;
	}

	uint16_t getYear(void) const {
		return _year;
	}

	uint8_t getMonth(void) const {
		return _month;
	}

	uint8_t getDay(void) const {
		return _day;
	}

	uint8_t getHour(void) const {
		return _hour;
	}

	uint8_t getMinute(void) const {
		return _minute;
	}

	uint8_t getSecond(void) const {
		return _second;
	}

  uint8_t getPowerOnHour(void) const {
    return _power_on_hour;
  }

  uint8_t getPowerOnMinute(void) const {
    return _power_on_minute;
  }

  uint8_t getPowerOnSecond(void) const {
    return _power_on_second;
  }

  uint8_t getPowerOffHour(void) const {
    return _power_off_hour;
  }

  uint8_t getPowerOffMinute(void) const {
    return _power_off_minute;
  }

  uint8_t getPowerOffSecond(void) const {
    return _power_off_second;
  }

	bool process(char c);

	void setBadChecksumHandler(void (*handler)(const myNMEA& nmea)) {
		_badChecksumHandler = handler;
	}

	void setUnknownSentenceHandler(void (*handler)(const myNMEA& nmea)) {
		_unknownSentenceHandler = handler;
	}

	// Current myNMEA sentence.
	const char* getSentence(void) const {
		return _buffer;
	}

	// Talker ID for current myNMEA sentence
	char getTalkerID(void) const {
		return _talkerID;
	}

	// Message ID for current myNMEA sentence
	const char* getMessageID(void) const {
		return (const char*)_messageID;
	}


protected:
	static inline bool isEndOfFields(char c) {
		return c == '*' || c == '\0' || c == '\r' || c == '\n';
	}

	const char* parseTime(const char* s, int destination);
	const char* parseDate(const char* s);

	bool processNBLL(const char* s);

private:
	// Sentence buffer and associated pointers
	// static const uint8_t _bufferLen = 83; // 82 + NULL
	// char _buffer[_bufferLen];
	uint8_t _bufferLen;
	char* _buffer;
	char *_ptr;

	// Information from current myNMEA sentence
	char _talkerID;
	char _messageID[6];

	// Variables parsed and kept for user
	bool _isValid;
	uint16_t _year;
	uint8_t _month, _day, _hour, _minute, _second;
  uint8_t _power_on_hour, _power_on_minute, _power_on_second;
  uint8_t _power_off_hour, _power_off_minute, _power_off_second;

	void (*_badChecksumHandler)(const myNMEA &nmea);
	void (*_unknownSentenceHandler)(const myNMEA &nmea);

};


#endif
