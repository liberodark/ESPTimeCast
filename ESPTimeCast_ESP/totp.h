#ifndef TOTP_H
#define TOTP_H

#include <Arduino.h>
#include <time.h>

// Compatibility macro for ESP8266/ESP32
#ifdef ESP32
  #define WDT_FEED() yield()
#else // ESP8266
  #ifndef WDT_FEED
    #define WDT_FEED() ESP.wdtFeed()
  #endif
#endif

class SimpleTOTP {
private:
  static const int MAX_KEY_LENGTH = 32;
  uint8_t _hmacKey[MAX_KEY_LENGTH];
  uint8_t _keyLength;
  uint8_t _timeStep;
  uint16_t _yieldCounter;

  inline uint32_t rol32(uint32_t number, uint8_t bits) {
    return ((number << bits) | (number >> (32 - bits)));
  }

  // Safe yield - avoids yield in interrupt contexts
  inline void safeYield() {
    _yieldCounter++;
    if ((_yieldCounter & 0xFF) == 0) {
      #ifdef ESP8266
        if (!isInterrupt()) {
          yield();
        }
      #else
        yield();
      #endif
    }
  }

  void sha1(const uint8_t* data, size_t dataLen, uint8_t* hash) {
    // SHA1 initial state
    uint32_t state[5] = {
      0x67452301UL,
      0xefcdab89UL,
      0x98badcfeUL,
      0x10325476UL,
      0xc3d2e1f0UL
    };

    uint32_t w[16];
    uint8_t buffer[64];
    size_t bufferOffset = 0;
    uint64_t totalBits = dataLen * 8;

    // Process data
    for (size_t i = 0; i < dataLen; i++) {
      buffer[bufferOffset++] = data[i];

      if (bufferOffset == 64) {
        // Convert to big-endian words
        for (int j = 0; j < 16; j++) {
          w[j] = ((uint32_t)buffer[j*4] << 24) |
                 ((uint32_t)buffer[j*4+1] << 16) |
                 ((uint32_t)buffer[j*4+2] << 8) |
                 ((uint32_t)buffer[j*4+3]);
        }
        hashBlock(state, w);
        bufferOffset = 0;
      }
    }

    // Padding
    buffer[bufferOffset++] = 0x80;

    if (bufferOffset > 56) {
      while (bufferOffset < 64) {
        buffer[bufferOffset++] = 0;
      }

      for (int j = 0; j < 16; j++) {
        w[j] = ((uint32_t)buffer[j*4] << 24) |
               ((uint32_t)buffer[j*4+1] << 16) |
               ((uint32_t)buffer[j*4+2] << 8) |
               ((uint32_t)buffer[j*4+3]);
      }
      hashBlock(state, w);
      bufferOffset = 0;
    }

    while (bufferOffset < 56) {
      buffer[bufferOffset++] = 0;
    }

    // Add length
    for (int i = 0; i < 8; i++) {
      buffer[56 + i] = (totalBits >> ((7 - i) * 8)) & 0xFF;
    }

    for (int j = 0; j < 16; j++) {
      w[j] = ((uint32_t)buffer[j*4] << 24) |
             ((uint32_t)buffer[j*4+1] << 16) |
             ((uint32_t)buffer[j*4+2] << 8) |
             ((uint32_t)buffer[j*4+3]);
    }
    hashBlock(state, w);

    // Convert result
    for (int i = 0; i < 5; i++) {
      hash[i*4] = (state[i] >> 24) & 0xFF;
      hash[i*4+1] = (state[i] >> 16) & 0xFF;
      hash[i*4+2] = (state[i] >> 8) & 0xFF;
      hash[i*4+3] = state[i] & 0xFF;
    }
  }

  void hashBlock(uint32_t* state, uint32_t* w) {
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t temp;

    // Message schedule
    uint32_t w_extended[80];
    for (int i = 0; i < 16; i++) {
      w_extended[i] = w[i];
    }

    for (int i = 16; i < 80; i++) {
      w_extended[i] = rol32(w_extended[i-3] ^ w_extended[i-8] ^
                           w_extended[i-14] ^ w_extended[i-16], 1);
      if ((i & 0x0F) == 0) {
        WDT_FEED();
      }
    }

    // Compression
    for (int i = 0; i < 80; i++) {
      if (i < 20) {
        temp = ((b & c) | ((~b) & d)) + 0x5a827999UL;
      } else if (i < 40) {
        temp = (b ^ c ^ d) + 0x6ed9eba1UL;
      } else if (i < 60) {
        temp = ((b & c) | (b & d) | (c & d)) + 0x8f1bbcdcUL;
      } else {
        temp = (b ^ c ^ d) + 0xca62c1d6UL;
      }

      temp += rol32(a, 5) + e + w_extended[i];
      e = d;
      d = c;
      c = rol32(b, 30);
      b = a;
      a = temp;

      if ((i & 0x0F) == 0) {
        WDT_FEED();
      }
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
  }

  void hmacSha1(const uint8_t* key, uint8_t keyLength,
                const uint8_t* data, uint8_t dataLen, uint8_t* result) {
    uint8_t keyBuffer[64];
    uint8_t innerPad[64];
    uint8_t outerPad[64];
    uint8_t innerHash[20];

    // Prepare key
    memset(keyBuffer, 0, 64);
    if (keyLength > 64) {
      sha1(key, keyLength, keyBuffer);
      keyLength = 20;
    } else {
      memcpy(keyBuffer, key, keyLength);
    }

    // Create pads
    for (int i = 0; i < 64; i++) {
      innerPad[i] = keyBuffer[i] ^ 0x36;
      outerPad[i] = keyBuffer[i] ^ 0x5c;
    }

    // Inner hash
    uint8_t tempBuffer[84];
    memcpy(tempBuffer, innerPad, 64);
    memcpy(tempBuffer + 64, data, dataLen);
    sha1(tempBuffer, 64 + dataLen, innerHash);

    WDT_FEED();

    // Outer hash
    memcpy(tempBuffer, outerPad, 64);
    memcpy(tempBuffer + 64, innerHash, 20);
    sha1(tempBuffer, 84, result);
  }

  #ifdef ESP8266
  bool isInterrupt() {
    uint32_t ps;
    __asm__ __volatile__("rsr %0,ps":"=a" (ps));
    return (ps & 0x0F) != 0;
  }
  #endif

public:
  SimpleTOTP() : _keyLength(0), _timeStep(30), _yieldCounter(0) {
    memset(_hmacKey, 0, sizeof(_hmacKey));
  }

  SimpleTOTP(const uint8_t* key, size_t keyLen, uint8_t timeStep = 30)
    : _yieldCounter(0) {
    setSecret(key, keyLen);
    _timeStep = timeStep;
  }

  bool setSecret(const uint8_t* key, size_t keyLen) {
    if (keyLen == 0 || keyLen > MAX_KEY_LENGTH) {
      return false;
    }

    _keyLength = keyLen;
    memcpy(_hmacKey, key, keyLen);
    return true;
  }

  String getCode(time_t timeStamp) {
    if (_keyLength == 0 || timeStamp == 0) {
      return String("000000");
    }

    uint32_t steps = timeStamp / _timeStep;
    return getCodeFromSteps(steps);
  }

  String getCodeFromSteps(uint32_t steps) {
    if (_keyLength == 0) {
      return String("000000");
    }

    _yieldCounter = 0;

    // Convert steps to 8-byte big-endian
    uint8_t timeBytes[8];
    memset(timeBytes, 0, 8);
    for (int i = 4; i < 8; i++) {
      timeBytes[i] = (steps >> ((7 - i) * 8)) & 0xFF;
    }

    // Calculate HMAC
    uint8_t hash[20];
    hmacSha1(_hmacKey, _keyLength, timeBytes, 8, hash);

    // Dynamic truncation
    uint8_t offset = hash[19] & 0x0F;
    uint32_t code = 0;

    code = ((uint32_t)(hash[offset] & 0x7F) << 24) |
           ((uint32_t)hash[offset + 1] << 16) |
           ((uint32_t)hash[offset + 2] << 8) |
           ((uint32_t)hash[offset + 3]);

    code %= 1000000;

    char buffer[7];
    sprintf(buffer, "%06lu", (unsigned long)code);

    return String(buffer);
  }

  bool verify(const String& userCode, time_t timestamp, uint8_t windowSize = 1) {
    if (_keyLength == 0 || userCode.length() != 6) {
      return false;
    }

    for (int i = -windowSize; i <= windowSize; i++) {
      WDT_FEED();
      time_t testTime = timestamp + (i * _timeStep);
      if (testTime > 0) {
        WDT_FEED();
        String expectedCode = getCode(testTime);
        WDT_FEED();
        if (expectedCode == userCode) {
          return true;
        }
      }
      delay(2);
    }
    return false;
  }
};

#endif // TOTP_H