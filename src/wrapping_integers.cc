#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point)
{
    return Wrap32{static_cast<uint32_t>(n) + zero_point.raw_value_};
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    uint64_t wrap_count = checkpoint / ((1UL << 32));
    uint32_t offset = raw_value_ - zero_point.raw_value_;
    uint64_t asn01 = (wrap_count > 0) ? offset + (1UL << 32) * (wrap_count - 1) : offset;
    uint64_t asn02 = offset + (1UL << 32) * wrap_count;
    uint64_t asn03 = offset + (1UL << 32) * (wrap_count + 1);

    uint64_t ret_asn01 = (asn01 > checkpoint) ? (asn01 - checkpoint) : (checkpoint - asn01);
    uint64_t ret_asn02 = (asn02 > checkpoint) ? (asn02 - checkpoint) : (checkpoint - asn02);
    uint64_t ret_asn03 = asn03 - checkpoint;

    uint64_t asn = min(min(ret_asn01, ret_asn02), ret_asn03);

    if (asn == ret_asn01)
    {
        return asn01;
    }
    if (asn == ret_asn02)
    {
        return asn02;
    }
    return asn03;
}
