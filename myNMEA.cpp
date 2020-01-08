#include "myNMEA.h"

static long exp10(uint8_t b)
{
	long r = 1;
	while (b--)
		r *= 10;
	return r;
}


static char toHex(uint8_t nibble)
{
	if (nibble >= 10)
		return nibble + 'A' - 10;
	else
		return nibble + '0';

}


const char* myNMEA::skipField(const char* s)
{
	if (s == NULL)
		return NULL;

	while (!isEndOfFields(*s)) {
		if (*s == ',') {
			// Check next character
			if (isEndOfFields(*++s))
				break;
			else
				return s;
		}
		++s;
	}
	return NULL; // End of string or valid sentence
}


unsigned int myNMEA::parseUnsignedInt(const char *s, uint8_t len)
{
	int r = 0;
	while (len--)
		r = 10 * r + *s++ - '0';
	return r;
}


long myNMEA::parseFloat(const char* s, uint8_t log10Multiplier, const char** eptr)
{
	int8_t neg = 1;
	long r = 0;
	while (isspace(*s))
		++s;
	if (*s == '-') {
		neg = -1;
		++s;
	}
	else if (*s == '+')
		++s;

	while (isdigit(*s))
		r = 10*r + *s++ - '0';
	r *= exp10(log10Multiplier);

	if (*s == '.') {
		++s;
		long frac = 0;
		while (isdigit(*s) && log10Multiplier) {
			frac = 10 * frac + *s++ -'0';
			--log10Multiplier;
		}
		frac *= exp10(log10Multiplier);
		r += frac;
	}
	r *= neg; // Include effect of any minus sign

	if (eptr)
		*eptr = skipField(s);

	return r;
}


const char* myNMEA::parseField(const char* s, char *result, int len)
{
	if (s == NULL)
		return NULL;

	int i = 0;
	while (*s != ',' && !isEndOfFields(*s)) {
		if (result && i++ < len)
			*result++ = *s;
		++s;
	}
	if (result && i < len)
		*result = '\0'; // Terminate unless too long

	if (*s == ',')
		return ++s; // Location of start of next field
	else
		return NULL; // End of string or valid sentence
}


const char* myNMEA::generateChecksum(const char* s, char* checksum)
{
	uint8_t c = 0;
	// Initial $ is omitted from checksum, if present ignore it.
	if (*s == '$')
		++s;

	while (*s != '\0' && *s != '*')
		c ^= *s++;

	if (checksum) {
		checksum[0] = toHex(c / 16);
		checksum[1] = toHex(c % 16);
	}
	return s;
}


bool myNMEA::testChecksum(const char* s)
{
	char checksum[2];
	const char* p = generateChecksum(s, checksum);
	return *p == '*' && p[1] == checksum[0] && p[2] == checksum[1];
}


Stream& myNMEA::sendSentence(Stream& s, const char* sentence)
{
	char checksum[3];
	generateChecksum(sentence, checksum);
	checksum[2] = '\0';
	s.print(sentence);
	s.print('*');
	s.print(checksum);
	s.print("\r\n");
	return s;
}


myNMEA::myNMEA(void) :
	_badChecksumHandler(NULL),
	_unknownSentenceHandler(NULL)
{
	setBuffer(NULL, 0);
	clear();
}


myNMEA::myNMEA(void* buf, uint8_t len) :
	_badChecksumHandler(NULL),
	_unknownSentenceHandler(NULL)
{
	setBuffer(buf, len);
	clear();
}


void myNMEA::setBuffer(void* buf, uint8_t len)
{
	_bufferLen = len;
	_buffer = (char*)buf;
	_ptr = _buffer;
	if (_bufferLen) {
		*_ptr = '\0';
		_buffer[_bufferLen - 1] = '\0';
	}
}


void myNMEA::clear(void)
{
	_isValid = false;
	_year = _month = _day = 0;
	_hour = _minute = _second = 99;
  _power_on_hour = _power_on_minute = _power_on_second = 99;
  _power_off_hour = _power_off_minute = _power_off_second = 99;

}


bool myNMEA::process(char c)
{
	if (_buffer == NULL || _bufferLen == 0)
		return false;
	if (c == '\0' || c == '\n' || c == '\r') {
		// Terminate buffer then reset pointer
		*_ptr = '\0';
		_ptr = _buffer;

		if (*_buffer == '$' && testChecksum(_buffer)) {
			// Valid message
      Serial.println("Valid message");
			const char* data;
		  _talkerID = '\0';
			data = parseField(&_buffer[1], &_messageID[0], sizeof(_messageID));
      Serial.println(_messageID);
			if (strcmp(&_messageID[0], "PNBLL") == 0)
				return processNBLL(data);
			else if (_unknownSentenceHandler)
				(*_unknownSentenceHandler)(*this);
		}
		else {
			if (_badChecksumHandler && *_buffer != '\0') // don't send empty buffers as bad checksums!
				(*_badChecksumHandler)(*this);
		}
		// Return true for a complete, non-empty, sentence (even if not a valid one).
		return *_buffer != '\0'; //
	}//~End of sentence delimiter
	else {
		*_ptr = c;
		if (_ptr < &_buffer[_bufferLen - 1])
			++_ptr;
	}

	return false;
}


const char* myNMEA::parseTime(const char* s,int destination)
{
  //destination: 0 - current time, 1 - power on time, 2 - power of time
	if ((*s == ',') || (destination<0) || (destination>2))
		return skipField(s);
  switch(destination)
  {
    case 0:
    	_hour = parseUnsignedInt(s, 2);
    	_minute = parseUnsignedInt(s + 2, 2);
    	_second = parseUnsignedInt(s + 4, 2);
      break;
    case 1:
      _power_on_hour = parseUnsignedInt(s, 2);
      _power_on_minute = parseUnsignedInt(s + 2, 2);
      _power_on_second = parseUnsignedInt(s + 4, 2);
      break;
    case 2:
      _power_off_hour = parseUnsignedInt(s, 2);
      _power_off_minute = parseUnsignedInt(s + 2, 2);
      _power_off_second = parseUnsignedInt(s + 4, 2);
      break;
    default:
      break;
  }
	return skipField(s + 6);
}


const char* myNMEA::parseDate(const char* s)
{
	if (*s == ',')
		return skipField(s);
	_day = parseUnsignedInt(s, 2);
	_month = parseUnsignedInt(s + 2, 2);
	_year = parseUnsignedInt(s + 4, 4);
	return skipField(s + 8);
}

bool myNMEA::processNBLL(const char* s)
{
	s = parseTime(s,0);//current time
  Serial.println(_hour);
  Serial.println(_minute);
  Serial.println(_second);
  s = parseDate(s);
	_isValid = (*s == 'A');
  if(_isValid==true) Serial.println("VALID"); else Serial.println("INVALID");
	s += 2; // Skip validity and comma
	s = parseTime(s,1);//power on time
  s = parseTime(s,2);//power off time
	return true;
}
