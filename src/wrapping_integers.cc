#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point)
{
    return Wrap32{static_cast<uint32_t>(n) + zero_point.raw_value_};
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    Wrap32 checkpoint_mod = wrap(checkpoint, zero_point);
    // 找出checkpoint和当前序号32位的时候的距离
    int32_t offset = raw_value_ - checkpoint_mod.raw_value_;
    int64_t as = checkpoint + offset;
    return as >= 0?as:as + (1UL << 32);
}
