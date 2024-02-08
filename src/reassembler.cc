#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <type_traits>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (m_bool_buffer.empty())
    {
        // initialized = true;
        m_buffer.resize(output.available_capacity(), '\0');
        m_bool_buffer.resize(output.available_capacity(), false);
    }

    m_is_recive_FIN |= is_last_substring;

    const uint64_t first_unassemble = output.bytes_pushed();
    const uint64_t first_unacceptable = output.available_capacity() + first_unassemble;

    if (first_unassemble == first_unacceptable || first_index >= first_unacceptable ||
        first_index + data.size() <= first_unassemble)
    {
        if (m_is_recive_FIN && !bytes_pending())
        {
            output.close();
        }
        return;
    }

    uint64_t begin_index = first_index;

    if (first_index < first_unassemble)
    {
        begin_index = first_unassemble;
        data = data.substr(first_unassemble - first_index);
    }

    if (begin_index + data.size() > first_unacceptable)
    {
        data = data.erase(first_unacceptable - begin_index);
    }

    save_the_buffer(data, begin_index);
    push(output, first_unassemble);

    if (m_is_recive_FIN && !bytes_pending())
    {
        output.close();
    }
}

void Reassembler::save_the_buffer(const std::string &data, const uint64_t begin_index)
{
    uint64_t absolute_index = begin_index % m_buffer.size();
    if (absolute_index + data.size() > m_buffer.size())
    { // 发生了环绕
        std::copy(data.begin(), data.begin() + (m_buffer.size() - absolute_index), m_buffer.begin() + absolute_index);
        std::copy(data.begin() + (m_buffer.size() - absolute_index), data.end(), m_buffer.begin());
        std::fill(m_bool_buffer.begin() + absolute_index, m_bool_buffer.end(), true);
        std::fill(m_bool_buffer.begin(), m_bool_buffer.begin() + (data.size() - (m_buffer.size() - absolute_index)),
                  true);
    }
    else
    {
        std::copy(data.begin(), data.end(), m_buffer.begin() + absolute_index);
        std::fill(m_bool_buffer.begin() + absolute_index, m_bool_buffer.begin() + absolute_index + data.size(), true);
    }
}

void Reassembler::push(Writer &output, const uint64_t first_unassemble)
{
    uint64_t absolute_index = first_unassemble % m_bool_buffer.size();
    // 数组为空，直接返回
    if (!bytes_pending())
    {
        return;
    }
    uint64_t count = absolute_index;
    bool around = false;
    while (m_bool_buffer[count])
    {
        ++count;
        if (count == m_bool_buffer.size())
        {
            std::string copied_str(count - absolute_index, '\0');
            std::copy(m_buffer.begin() + absolute_index, m_buffer.end(), copied_str.begin());
            std::fill(m_bool_buffer.begin() + absolute_index, m_bool_buffer.end(), false);
            output.push(std::move(copied_str));
            count = 0;
            around = true;
        }
    }
    if (count)
    {
        if (around)
        {
            std::string copied_str(count, '\0');
            std::copy(m_buffer.begin(), m_buffer.begin() + count, copied_str.begin());
            std::fill(m_bool_buffer.begin(), m_bool_buffer.begin() + count, false);
            output.push(std::move(copied_str));
            return;
        }
        std::string copied_str(count - absolute_index, '\0');
        std::copy(m_buffer.begin() + absolute_index, m_buffer.begin() + count, copied_str.begin());
        std::fill(m_bool_buffer.begin() + absolute_index, m_bool_buffer.begin() + count, false);
        output.push(std::move(copied_str));
    }
}

uint64_t Reassembler::bytes_pending() const
{
    return std::count(m_bool_buffer.begin(), m_bool_buffer.end(), true);
}
