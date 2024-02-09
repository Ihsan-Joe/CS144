#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <netinet/in.h>
#include <string>
#include <utility>

using namespace std;

void Reassembler::try_close(Writer &output) const
{
    if (m_is_last_substring && !bytes_pending())
    {
        output.close();
    }
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    m_is_last_substring |= is_last_substring;
    const uint64_t first_unassemble = output.bytes_pushed();
    const uint64_t first_unacceptable = output.available_capacity() + first_unassemble;
    if (first_unassemble == first_unacceptable || first_index >= first_unacceptable ||
        first_index + data.size() <= first_unassemble)
    {
        try_close(output);
        return;
    }

    uint64_t begin_index = first_index - first_unassemble;
    if (first_index < first_unassemble)
    {
        begin_index = 0;
        data = data.substr(first_unassemble - first_index);
    }

    if (begin_index + data.size() > first_unacceptable - first_unassemble)
    {
        data.erase(first_unacceptable - first_unassemble - begin_index);
    }

    if (first_unacceptable - first_unassemble > m_bool_buffer.size())
    {
        m_buffer.resize(first_unacceptable - first_unassemble, '\0');
        m_bool_buffer.resize(first_unacceptable - first_unassemble, false);
    }

    m_buffer.replace(begin_index, data.size(), data);
    m_bool_buffer.replace(begin_index, data.size(), data.size(), true);

    // if (!m_bool_buffer[0]) {return;} //这样会减少不少时间，也会通过测试，但是不能直接return
    if (m_bool_buffer[0])
    {
        uint64_t distance = m_bool_buffer.find(false, 0);
        distance = (distance != std::string::npos) ? distance : m_bool_buffer.size();
        output.push(m_buffer.substr(0, distance));
        m_bool_buffer.erase(m_bool_buffer.begin(), m_bool_buffer.begin() + distance);
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + distance);
    }
    try_close(output);
}

uint64_t Reassembler::bytes_pending() const
{
    uint64_t count = 0;
    for (const auto item : m_bool_buffer)
    {
        if (item)
        {
            count++;
        }
    }
    return count;
    // return std::count(m_bool_buffer.begin(), m_bool_buffer.end(), true); // 这个速度较慢
}
