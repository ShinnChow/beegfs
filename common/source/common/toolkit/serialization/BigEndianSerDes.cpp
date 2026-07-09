#include "BigEndianSerDes.h"
#include "Byteswap.h"

BigEndianSerDes& BigEndianSerDes::operator%(uint32_t& value)
{
   if (bufferOffset + sizeof(uint32_t) > bufferSize)
   {
      setBad();
      return *this;
   }

   if (isReading)
   {
      uint32_t raw;
      std::memcpy(&raw, constBuffer + bufferOffset, sizeof(uint32_t));
      value = BE_TO_HOST_32(raw);
   }
   else if (buffer)
   {
      uint32_t raw = HOST_TO_BE_32(value);
      std::memcpy(buffer + bufferOffset, &raw, sizeof(uint32_t));
   }

   bufferOffset += sizeof(uint32_t);
   return *this;
}

BigEndianSerDes& BigEndianSerDes::operator%(char& value)
{
   if (bufferOffset + 1 > bufferSize)
   {
      setBad();
      return *this;
   }

   if (isReading)
   {
      value = constBuffer[bufferOffset];
   }
   else if (buffer)
   {
      buffer[bufferOffset] = value;
   }

   bufferOffset += 1;
   return *this;
}

void BigEndianSerDes::processString(std::string& str, uint32_t length)
{
   // Round up to a 4-byte boundary in size_t arithmetic. Computing (length + 3)
   // in 32-bit unsigned wraps for length >= 0xFFFFFFFD, yielding paddedLen == 0
   // and bypassing the bounds check below (potential ~4 GiB OOB read).
   size_t paddedLen = ((size_t) length + 3) & ~(size_t) 3;

   // A single field can never be longer than the whole buffer. On the sizing
   // pass bufferSize is SIZE_MAX, and on the write pass a valid length always
   // fits within the total size, so this only rejects malformed read input.
   if (length > bufferSize || bufferOffset + paddedLen > bufferSize)
   {
      setBad();
      return;
   }

   if (isReading)
   {
      str.assign(constBuffer + bufferOffset, length);
   }
   else if (buffer)
   {
      std::memcpy(buffer + bufferOffset, str.data(), length);
      // Zero-pad to 4-byte boundary
      for (size_t i = length; i < paddedLen; i++)
      {
         buffer[bufferOffset + i] = 0;
      }
   }
   bufferOffset += paddedLen;
}