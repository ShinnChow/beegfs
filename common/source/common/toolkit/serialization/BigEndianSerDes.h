#pragma once

#include <common/Common.h>

/**
 * Unified big-endian serialization/deserialization class for XDR format.
 *
 * This class provides a unified interface for both reading (deserialization)
 * and writing (serialization) data in XDR (External Data Representation) format,
 * which uses big-endian byte ordering.
 *
 * Usage:
 * - For serialization: BigEndianSerDes ser(buffer, bufferSize)
 * - For deserialization: BigEndianSerDes des(constBuffer, bufferSize)
 * - Use operator% for uint32_t and char serialization
 * - Use processString() for string handling with XDR padding
 */
class BigEndianSerDes
{
   public:
   /**
    * Constructor for serialization (writing)
    * @param buffer writable buffer for output
    * @param bufferSize size of the buffer
    */
   BigEndianSerDes(char* buffer, size_t bufferSize)
      : buffer(buffer), constBuffer(nullptr), bufferSize(bufferSize),
        bufferOffset(0), isBad(false), isReading(false)
   {}

   /**
    * Constructor for deserialization (reading)
    * @param buffer read-only buffer containing data to deserialize
    * @param bufferSize size of the buffer
    */
   BigEndianSerDes(const char* buffer, size_t bufferSize)
      : buffer(nullptr), constBuffer(buffer), bufferSize(bufferSize),
        bufferOffset(0), isBad(false), isReading(true)
   {}

   // Explicitely forbid copy constructor and copy assignment operator
   BigEndianSerDes(const BigEndianSerDes&) = delete;
   BigEndianSerDes& operator=(const BigEndianSerDes&) = delete;

   BigEndianSerDes(BigEndianSerDes&& other) noexcept
      : buffer(other.buffer), constBuffer(other.constBuffer), bufferSize(other.bufferSize),
        bufferOffset(other.bufferOffset), isBad(other.isBad), isReading(other.isReading)
   {
      // Reset the moved-from object
      other.buffer = nullptr;
      other.constBuffer = nullptr;
      other.bufferSize = 0;
      other.bufferOffset = 0;
      other.isBad = true;  // Mark as invalid
      other.isReading = false;
   }

   BigEndianSerDes& operator=(BigEndianSerDes&& other) noexcept
   {
      if (this != &other)
      {
         buffer = other.buffer;
         constBuffer = other.constBuffer;
         bufferSize = other.bufferSize;
         bufferOffset = other.bufferOffset;
         isBad = other.isBad;
         isReading = other.isReading;

         // Reset the moved-from object
         other.buffer = nullptr;
         other.constBuffer = nullptr;
         other.bufferSize = 0;
         other.bufferOffset = 0;
         other.isBad = true;  // Mark as invalid
         other.isReading = false;
      }
      return *this;
   }

   /**
    * Check if the serializer/deserializer is in good state
    * @return true if no errors occurred and within buffer bounds
    */
   bool good() const { return !isBad && bufferOffset <= bufferSize; }

   /**
    * Get current size/offset in the buffer
    * @return number of bytes processed so far
    */
   size_t size() const { return bufferOffset; }

   /**
    * Mark the serializer as bad (error state)
    */
   void setBad() { isBad = true; }

   /**
    * Unified operator for uint32_t - handles both read and write
    * Converts to/from big-endian format as required by XDR
    */
   BigEndianSerDes& operator%(uint32_t& value);

   /**
    * Unified operator for char - handles both read and write
    */
   BigEndianSerDes& operator%(char& value);

   /**
    * String handling with XDR padding
    * Strings in XDR will be padded to adhere 4-byte boundaries.
    * @param str string to read/write
    * @param length length of the string (not including padding)
    */
   void processString(std::string& str, uint32_t length);

   private:
      char* buffer;                // For writing (serialization)
      const char* constBuffer;     // For reading (deserialization)
      size_t bufferSize;
      size_t bufferOffset;
      bool isBad;
      bool isReading;
};