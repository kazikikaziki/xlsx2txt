#pragma once
#include <inttypes.h>

namespace Kamilo {

/// crc32b を求める
///
/// <a href="https://qiita.com/tobira-code/items/dbcffc41f54201130b6c">Cyclic Redundancy Check(CRC)を理解する</a>
///
/// @note crc32b は crc32 とは異なる。
/// crc32b は zip などで使われているものと同じ<br>
/// <a href="https://stackoverflow.com/questions/15861058/what-is-the-difference-between-crc32-and-crc32b">what-is-the-difference-between-crc32-and-crc32b</a>
class KCrc32 {
public:
	static const uint32_t INIT = ~0;
	static uint32_t fromByte(uint8_t data, uint32_t crc);
	static uint32_t fromData(const void *data, int size);
	static uint32_t fromString(const char *str);
};


namespace Test {
void Test_crc32();
}

} // namesapce
