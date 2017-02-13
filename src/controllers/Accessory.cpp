#include "Accessory.h"
#include <Wire.h>
#include "Servo.h"

Accessory::Accessory(uint8_t data_pin, uint8_t sclk_pin) {
	_sda_pin = data_pin;
	_scl_pin = sclk_pin;
	_clockSpacing = 1;
	ackTimeout = 100;
	_usePullUpClock = false;

}
/**
 * Reads the device type from the controller
 */
ControllerType Accessory::identifyController() {
	_burstReadWithAddress(0xfe);

	if (_dataarray[0] == 0x01)
		if (_dataarray[1] == 0x01)
			return WIICLASSIC; // Classic Controller

	if (_dataarray[0] == 0x00)
		if (_dataarray[1] == 0x00)
			return NUNCHUCK; // nunchuck

	// It's something else.
	_burstReadWithAddress(0xfa);

	if (_dataarray[0] == 0x00)
		if (_dataarray[1] == 0x00)
			if (_dataarray[2] == 0xa4)
				if (_dataarray[3] == 0x20)
					if (_dataarray[4] == 0x01)
						if (_dataarray[5] == 0x03)
							return GuitarHeroController; // Guitar Hero Controller

	if (_dataarray[0] == 0x01)
		if (_dataarray[1] == 0x00)
			if (_dataarray[2] == 0xa4)
				if (_dataarray[3] == 0x20)
					if (_dataarray[4] == 0x01)
						if (_dataarray[5] == 0x03)
							return GuitarHeroWorldTourDrums; // Guitar Hero World Tour Drums

	if (_dataarray[0] == 0x03)
		if (_dataarray[1] == 0x00)
			if (_dataarray[2] == 0xa4)
				if (_dataarray[3] == 0x20)
					if (_dataarray[4] == 0x01)
						if (_dataarray[5] == 0x03)
							return Turntable; // Guitar Hero World Tour Drums

	if (_dataarray[0] == 0x00)
		if (_dataarray[1] == 0x00)
			if (_dataarray[2] == 0xa4)
				if (_dataarray[3] == 0x20)
					if (_dataarray[4] == 0x01)
						if (_dataarray[5] == 0x11)
							return DrumController; // Taiko no Tatsujin TaTaCon (Drum controller)

	if (_dataarray[0] == 0xFF)
		if (_dataarray[1] == 0x00)
			if (_dataarray[2] == 0xa4)
				if (_dataarray[3] == 0x20)
					if (_dataarray[4] == 0x00)
						if (_dataarray[5] == 0x13)
							return DrawsomeTablet; // Drawsome Tablet

	return Unknown;
}

/*
 * public function to read data
 */
void Accessory::readData() {

	_burstRead();
  _applyMaps();
}

uint8_t* Accessory::getDataArray() {
	return _dataarray;
}

void Accessory::setDataArray(uint8_t data[6]) {
	for (int i = 0; i < 6; i++)
		_dataarray[i] = data[i];
}

void Accessory::printInputs(Stream& stream) {
	stream.print("Accessory Bytes:\t");
	for (int i = 0; i < 6; i++) {
		if (_dataarray[i] < 0x10) {
			stream.print("0");
		}
		stream.print(_dataarray[i], HEX);
		stream.print(" ");
	}
	stream.println("");
}

int Accessory::decodeInt(uint8_t msbbyte, uint8_t msbstart, uint8_t msbend,
		uint8_t csbbyte, uint8_t csbstart, uint8_t csbend, uint8_t lsbbyte,
		uint8_t lsbstart, uint8_t lsbend, int16_t aMin, int16_t aMid, int16_t aMax) {
// 5 bit int split across 3 bytes. what... the... fuck... nintendo...
  bool msbflag=false,csbflag=false,lsbflag=false;
	if (msbbyte > 5)
		msbflag=true;
	if (csbbyte > 5)
		csbflag=true;
	if (lsbbyte > 5)
		lsbflag=true;

	int32_t analog = 0;
	uint32_t lpart;
	lpart = (lsbflag)?0:_dataarray[lsbbyte];
	lpart = lpart >> lsbstart;
	lpart = lpart & (0xFF >> (7 - (lsbend - lsbstart)));

	uint32_t cpart;
	cpart = (csbflag)?0:_dataarray[csbbyte];
	cpart = cpart >> csbstart;
	cpart = cpart & (0xFF >> (7 - (csbend - csbstart)));

	cpart = cpart << ((lsbend - lsbstart) + 1);

	uint32_t mpart;
	mpart = (lsbflag)?0:_dataarray[msbbyte];
	mpart = mpart >> msbstart;
	mpart = mpart & (0xFF >> (7 - (msbend - msbstart)));

	mpart = mpart << (((lsbend - lsbstart) + 1) + ((csbend - csbstart) + 1));

	analog = lpart | cpart | mpart;
	//analog = analog + offset;
	//analog = (analog*scale);

	return analog;

}

bool Accessory::decodeBit(uint8_t byte, uint8_t bit, bool activeLow) {
	if (byte > 5)
		return false;
	uint8_t swb = _dataarray[byte];
	uint8_t sw = (swb >> bit) & 0x01;
	return activeLow ? (!sw) : (sw);
}

void Accessory::_sendStart(byte addr) {
	_dataHigh();
	_clockHigh();
	_dataLow();
	_clockLow();
	_shiftOut(addr);

}

void Accessory::_sendStop() {
	_clockLow();
	_clockHigh();
	_dataHigh();
	pinMode(_sda_pin, INPUT);
}

void Accessory::_sendNack() {
	_clockLow();
	_dataHigh();
	_clockHigh();
	_clockLow();
	pinMode(_sda_pin, INPUT);
}

void Accessory::_sendAck() {
	_clockLow();
	_dataLow();
	_clockHigh();
	_clockLow();
	pinMode(_sda_pin, INPUT);
}

void Accessory::_dataHigh() {
	if (_usePullUpClock) {
		pinMode(_sda_pin, INPUT);
	} else {
		pinMode(_sda_pin, OUTPUT);
		digitalWrite(_sda_pin, HIGH);
	}

}
void Accessory::_dataLow() {
	pinMode(_sda_pin, OUTPUT);
	digitalWrite(_sda_pin, LOW);

}
void Accessory::_clockHigh() {

	if (_usePullUpClock) {
		_clockStallCheck();
	} else {
		pinMode(_scl_pin, OUTPUT);
		digitalWrite(_scl_pin, HIGH);
	}
	
	if (_clockSpacing > 0)
		delayMicroseconds(_clockSpacing);

}
void Accessory::_clockLow() {
	pinMode(_scl_pin, OUTPUT);
	digitalWrite(_scl_pin, LOW);
	if (_clockSpacing > 0)
		delayMicroseconds(_clockSpacing);

}

void Accessory::_clockStallCheck() {
	pinMode(_scl_pin, INPUT);

	unsigned long time = millis();
	while (digitalRead(_scl_pin) != HIGH && (time + ackTimeout) < millis()) {
	}

}
void Accessory::_waitForAck() {
	pinMode(_sda_pin, INPUT);
	_clockHigh();
	unsigned long time = millis();
	while (digitalRead(_sda_pin) == HIGH && (time + ackTimeout) < millis()) {
	}

	_clockLow();
}

uint8_t Accessory::_readByte() {
	pinMode(_sda_pin, INPUT);

	uint8_t value = 0;
	for (int i = 0; i < 8; ++i) {
		_clockHigh();
		value |= (digitalRead(_sda_pin) << (7 - i));
		_clockLow();
	}
	return value;
}

void Accessory::_writeByte(uint8_t value) {
	_shiftOut(value);
}
void Accessory::initBytes() {

	// improved startup procedure from http://playground.arduino.cc/Main/AccessoryClass
	_writeRegister(0xF0, 0x55);
	_writeRegister(0xFB, 0x00);

}

void Accessory::usePullUpClock(bool mode) {
	_usePullUpClock = mode;
}

void Accessory::_shiftOut(uint8_t val) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		if ((val & (1 << (7 - i))) == 0) {
			_dataLow();
		} else
			_dataHigh();
		_clockHigh();
		_clockLow();
	}
}

void Accessory::begin() {

	_use_hw = false;
	if ((_sda_pin == SDA) and (_scl_pin == SCL)) {
		_use_hw = true;
		Wire.begin();
		Serial.println("Starting Wire");

	}
	initBytes();

	delay(100);
	_burstRead();
	delay(100);
	_burstRead();
	Serial.println("Initialization Done");

}

void Accessory::_burstRead() {
	_burstReadWithAddress(0);
}

void Accessory::_burstReadWithAddress(uint8_t addr) {
	int readAmnt = 6;

	if (_use_hw) {
		// send conversion command
		Wire.beginTransmission(I2C_ADDR);
		Wire.write(addr);
		Wire.endTransmission();

		// wait for data to be converted
		delay(1);

		// read data
		Wire.readBytes(_dataarray,
				Wire.requestFrom(I2C_ADDR, sizeof(_dataarray)));

	} else {
		// send conversion command
		_sendStart(I2C_ADDR_W);
		_waitForAck();
		_writeByte(addr);
		_waitForAck();
		_sendStop();
		
		// wait for data to be converted
		delay(1);
		_sendStart(I2C_ADDR_R);
		_waitForAck();

		for (int i = 0; i < readAmnt; i++) {
			delayMicroseconds(40);
			_dataarray[i] = _readByte();
			if (i < (readAmnt - 1))
				_sendAck();
			else
				_sendNack();
		}
		_sendStop();
	}
}

void Accessory::_writeRegister(uint8_t reg, uint8_t value) {
	if (_use_hw) {
		Wire.beginTransmission(I2C_ADDR);
		Wire.write(reg);
		Wire.write(value);
		Wire.endTransmission();
	} else {
		_sendStart(I2C_ADDR_W);
		_waitForAck();
		_writeByte(reg);
		_waitForAck();
		_writeByte(value);
		_waitForAck();
		_sendStop();
	}
}

uint8_t Accessory::addAnalogMap(uint8_t msbbyte, uint8_t msbstart, uint8_t msbend,
			uint8_t csbbyte, uint8_t csbstart, uint8_t csbend, uint8_t lsbbyte,
			uint8_t lsbstart, uint8_t lsbend,int16_t aMin, int16_t aMid, int16_t aMax, uint8_t sMin, uint8_t sMax,
			uint8_t sZero, uint8_t sChan){
	    inputMapping* im = (inputMapping*) malloc(sizeof(inputMapping));
	    if (im==0) return -1;
	    Serial.print("Malloc'd:\t0x"); Serial.println((int)im, HEX);
	    // populate mapping struct
	    im->type = ANALOG;
	    im->aMsbbyte=msbbyte;
	    im->aMsbstart=msbstart;
	    im->aMsbend=msbend;
	    
	    im->aCsbbyte=csbbyte;
	    im->aCsbstart=csbstart;
	    im->aCsbend=csbend;
	    
	    im->aLsbbyte=lsbbyte;
	    im->aLsbstart=lsbstart;
	    im->aLsbend=lsbend;
	    
	    im->aMin=aMin;
	    im->aMid=aMid;
	    im->aMax=aMax;
	    
	    im->servoMax=sMax;
	    im->servoMin=sMin;
	    im->servoZero=sZero;
	    
	    im->sChan=sChan;
	    
	    im->servo = Servo();
	    im->servo.attach(sChan);
	    im->servo.write(sZero);
	    
	    // Add to list
	    // Are we first
	    if (_firstMap==0) _firstMap = im;
	    else {
	      // Walk the list
	      inputMapping* m=_firstMap;
	      while(m->nextMap!=0) m=m->nextMap;
	      // Ok we're at the end. Add our map.
	      m->nextMap=im;
	    }
}

uint8_t Accessory::addDigitalMap(uint8_t byte, uint8_t bit, bool activeLow,
		uint8_t sMin, uint8_t sMax, uint8_t sZero, uint8_t sChan) {
inputMapping* im = (inputMapping*) malloc(sizeof(inputMapping));
	    if (im==0) return -1;
	    Serial.print("Malloc'd:\t0x"); Serial.println((int)im, HEX);
	    // populate mapping struct
	    im->type = DIGITAL;
	    im->dByte=byte;
	    im->dBit=bit;
	    
	    im->servoMax=sMax;
	    im->servoMin=sMin;
	    im->servoZero=sZero;
	    
	    im->sChan=sChan;
	    im->servo.attach(sChan);
	    
	    // Add to list
	    // Are we first
	    if (_firstMap==0) _firstMap = im;
	    else {
	      // Walk the list
	      inputMapping* m=_firstMap;
	      while(m->nextMap!=0) m=m->nextMap;
	      // Ok we're at the end. Add our map.
	      m->nextMap=im;
	    }
}

void Accessory::printMaps(Stream& stream) {
   stream.println("Active Maps");
   if (_firstMap==0) stream.println("/tNo Maps");
	    else {
	      // Walk the list
	      inputMapping* m=_firstMap;
	      int cnt=0;
	      do {
	        stream.print("\t["); stream.print(cnt); stream.print("]  "); stream.print("Servo: ");stream.print(m->sChan);stream.print("(");
	        stream.print(m->servoMin);stream.print(",");
	        stream.print(m->servoZero);stream.print(",");
	        stream.print(m->servoMax);stream.print(") ");
	        
	        if (m->type == ANALOG){
	        stream.print("ANALOG ");
	          if (m->aMsbbyte != UNUSED){
	            stream.print("BIT");stream.print(m->aMsbbyte);
	            stream.print("[");
	            stream.print(m->aMsbend);
	            stream.print(":");
	            stream.print(m->aMsbstart);
	            stream.print("] ");
	          }
	          if (m->aCsbbyte != UNUSED){
	            stream.print("BIT");stream.print(m->aCsbbyte);
	            stream.print("[");
	            stream.print(m->aCsbend);
	            stream.print(":");
	            stream.print(m->aCsbstart);
	            stream.print("] ");
	          }
	          if (m->aLsbbyte != UNUSED){
	            stream.print("BIT");stream.print(m->aLsbbyte);
	            stream.print("[");
	            stream.print(m->aLsbend);
	            stream.print(":");
	            stream.print(m->aLsbstart);
	            stream.print("] ");
	          }
	          stream.print("min: "); stream.print(m->aMin);
	          stream.print(" mind: "); stream.print(m->aMid);
	          stream.print(" max: "); stream.print(m->aMax);
	        
	        } else {
	          stream.print("DIGITAL ");
	          stream.print("BIT");stream.print(m->dByte);
	          stream.print("[");
	          stream.print(m->dBit);
	          stream.print("]");
	        }
	        
	        stream.println("");
	        m=m->nextMap;
	        cnt++;
	      } while(m!=0);
	    }

}

void Accessory::_applyMaps(){
  inputMapping* m=_firstMap;
  int cnt=0;
  
  if (m==0) return; // no maps. bail.
  do {
    switch(m->type){
      case ANALOG: {
        int val =   decodeInt(
          m->aMsbbyte,m->aMsbstart,m->aMsbend,        // MSB 
          m->aCsbbyte,m->aCsbstart,m->aCsbend,        // CSB 
          m->aLsbbyte,m->aLsbstart,m->aLsbend,        // LSB 
          m->aMin,m->aMid,m->aMax);                         // bounds
          
        // map to servo
        uint8_t pos=smap(val,m->aMax,m->aMid,m->aMin,m->servoMax,m->servoZero,m->servoMin);
        
        // update servo
        m->servo.write(pos);
        Serial.print(m->sChan); Serial.print(" pos: "); Serial.println(pos);
      }
      break;
      case DIGITAL: {
        bool val = decodeBit(m->dByte,m->dBit,m->dActiveLow);
        if (val) m->servo.write(m->servoMax);
        else m->servo.write(m->servoMin);
      }
      break;
    }
    m=m->nextMap;
	  cnt++;
  } while(m!=0);

}

uint8_t Accessory::getMapCount() {

}
void Accessory::removeMaps() {
}
void Accessory::removeMap(uint8_t id) {
}

int16_t Accessory::smap(int16_t val, int16_t aMax, int16_t aMid, int16_t aMin, int16_t sMax, int16_t sZero, int16_t sMin){
  if (val>aMid) {
    return map(val,aMid,aMax,sZero,sMax);
  } else if (val<aMid) {
    return map(val,aMin,aMid,sMin,sZero);
  }
  return sZero;
}
