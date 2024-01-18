#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <netinet/in.h>

using namespace std;

// void Reassembler::send()
// {

// }

// void Reassembler::clearup_buffer()
// {

// }

void Reassembler::try_close(Writer &writer)
{
    if (m_is_last_substring && !bytes_pending())
    {
        writer.close();
    }
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (first_index >= output.available_capacity())
    {
        try_close(output);
        return;
    }

    m_is_last_substring = is_last_substring;
    try_close(output);
}

uint64_t Reassembler::bytes_pending() const
{
    uint64_t count = 0;
    for (auto [key, value] : m_buffer)
    {
        count += value.size();
    }
    return count;
}
