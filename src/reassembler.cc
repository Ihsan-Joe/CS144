#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (!initialized)
    {
        initialized = true;
        m_buffer.resize(output.available_capacity());
        m_bool_buffer.resize(output.available_capacity());
    }

    m_is_recive_FIN = m_is_recive_FIN ? m_is_recive_FIN : is_last_substring;

    const uint64_t first_unassemble = output.bytes_pushed();
    const uint64_t first_unacceptable = output.available_capacity() + first_unassemble;

    if (first_index >= first_unacceptable || first_index + data.size() <= first_unassemble)
    {
        if (m_is_recive_FIN && (m_loop_begin == m_loop_end))
        {
            output.close();
        }
        return;
    }

    // 计算出在循环列表中的位置
    uint64_t begin_index = (first_index >= first_unassemble) ? first_index - first_unassemble : m_loop_begin;
    uint64_t end_index = (first_index + data.size() <= first_unacceptable)
                             ? first_index + data.size() - first_unassemble
                             : first_unacceptable - first_unassemble;

    save_the_buffer(data, begin_index, end_index);
    push(output);

    if (m_is_recive_FIN && (m_loop_begin == m_loop_end))
    {
        output.close();
    }
}

bool Reassembler::save_the_buffer(std::string &data, const uint64_t begin_index, const uint64_t end_index)
{
    uint64_t capacity = m_buffer.size();
    if (m_is_full)
    {
        return false;
    }
    uint64_t from_Lbegin_to_Bend = capacity - begin_index;
    uint64_t length = std::min(end_index, from_Lbegin_to_Bend);
    std::copy(data.begin(), data.begin() + length, m_buffer.begin() + begin_index);
    std::fill(m_bool_buffer.begin() + begin_index, m_bool_buffer.begin() + begin_index + length, true);
    if (length < end_index)
    {
        std::copy(data.begin() + length, data.end(), m_buffer.begin());
        std::fill(m_bool_buffer.begin(), m_bool_buffer.begin() + (end_index - length), true);
    }
    m_loop_end = (m_loop_end + end_index) % capacity;
    if (m_loop_begin == m_loop_end)
    {
        m_is_full = true;
        m_is_empty = false;
    }
    return true;
}

bool Reassembler::push(Writer &output)
{
    // 缓冲区为空,或者没有数据需要push
    uint64_t capacity = m_buffer.size();
    if (m_is_empty || !m_bool_buffer[m_loop_begin])
    {
        return false;
    }
    
    if (capacity == 1)
    {
        output.push(std::string(1, m_buffer[0]));
        m_bool_buffer[0] =  false;
    }

    uint64_t current = m_loop_begin;

    // 查找第一个 false 的位置
    while (m_bool_buffer[current] && current != m_loop_end)
    {
        current = (current + 1) % capacity;
    }

    // 字符串大小
    uint64_t need_space = current - m_loop_begin;
    std::string data;
    data.resize(need_space);

    if (m_loop_begin < m_loop_end)
    {
        std::copy(m_buffer.begin() + m_loop_begin, m_buffer.begin() + current, data.begin());
        std::fill(m_bool_buffer.begin() + m_loop_begin, m_bool_buffer.begin() + current, false);
    }
    else
    {
        std::copy(m_buffer.begin() + m_loop_begin, m_buffer.end(), data.begin());
        std::fill(m_bool_buffer.begin() + m_loop_begin, m_bool_buffer.end(), false);
        std::copy(m_buffer.begin(), m_buffer.begin() + current, data.begin() + (capacity - m_loop_begin));
        std::fill(m_bool_buffer.begin(), m_bool_buffer.begin() + current, false);
    }

    output.push(std::move(data));
    m_loop_begin = current;
    if(m_loop_begin == m_loop_end)
    {
        m_is_full = false;
        m_is_empty = true;
    }
    return true;
}

uint64_t Reassembler::bytes_pending() const
{
    return (m_loop_begin > m_loop_end) ? (m_loop_begin - m_loop_end) : (m_loop_end - m_loop_begin + m_buffer.size());
}
