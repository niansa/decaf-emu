#pragma once

#include "common/murmur3.h"

namespace gpu
{

class DataHash
{
public:
   inline DataHash()
   {
   }

   inline bool operator!=(const DataHash &rhs) const
   {
      return mHash[0] != rhs.mHash[0] || mHash[1] != rhs.mHash[1];
   }

   inline bool operator==(const DataHash &rhs) const
   {
      return mHash[0] == rhs.mHash[0] && mHash[1] == rhs.mHash[1];
   }

   inline static DataHash hash(void *data, size_t size)
   {
      DataHash out;
      MurmurHash3_x64_128(data, static_cast<int>(size), 0, &out.mHash);
      return out;
   }

private:
   uint64_t mHash[2] = { 0 };

};

}