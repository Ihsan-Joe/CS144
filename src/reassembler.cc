#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <netinet/in.h>

using namespace std;

void Reassembler::send(Writer &output)
{
    while (!m_buffer.empty() && m_buffer[0] != '\0')
    {
        // 找到此次发送的字符串的结束位置
        auto end_index = m_buffer.find('\0', 0);
        if (end_index == std::string::npos)
        {
            end_index = m_buffer.size();
        }
        // 发送
        output.push(m_buffer.substr(0, end_index));
        // 存储这次发送的信息
        m_pre_index = m_next_index;
        m_pre_data_size = end_index - m_next_index;

        // 缩减缓冲区
        m_buffer = m_buffer.substr(end_index);

        // 给next_index加上此次发送的字符串大小，为准备下一次发送
        m_next_index += end_index;
    }
}

void Reassembler::try_close(Writer &output, bool is_last_substring)
{
    if (is_last_substring)
    {
        m_is_last_substring = true;
    }

    if (m_is_last_substring && bytes_pending() == 0)
    {
        output.close();
    }
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    auto available_capacity = output.available_capacity();
    // log(func_calld, data);
    if (first_index > m_next_index && first_index - m_next_index > available_capacity)
    {
        try_close(output, is_last_substring);
        return;
    }

    m_buffer.resize(available_capacity);

    if (first_index == m_next_index)
    {
        // 存储这次发送的package的信息
        m_pre_data_size = data.size();
        m_pre_index = first_index;

        // 去除比m_buffer.size多出来的部分
        if (data.size() > available_capacity)
        {
            data.erase(available_capacity);
        }

        m_buffer.replace(0, data.size(), data);
        send(output);
    }
    else if (first_index > m_next_index)
    {

        // 因为缓冲区的第0个坐标就是m_next_index
        auto insert_position = first_index - m_next_index;
        if (data.size() > available_capacity - insert_position)
        {
            // 删除比缓冲区大小多出来的那部分
            data.erase(available_capacity - insert_position);
        }
        m_buffer.replace(insert_position, data.size(), data);
    }
    else if (first_index + data.size() > m_next_index)
    {
        // 去掉前面已经push过的内容
        data = data.substr(m_next_index - first_index);
        // 去掉后面超过
        if (data.size() > available_capacity)
        {
            data.erase(available_capacity);
        }
        m_buffer.replace(0, data.size(), data);
        send(output);
    }
    try_close(output, is_last_substring);
    func_calld++;

    // 剩下的有: 这两种情况直接丢弃
    // first_index < m_next_index && first_index + data.size() < m_next_index
    // first_index < m_next_index && first_index + data.size() == m_next_index
}

uint64_t Reassembler::bytes_pending() const
{
    return m_buffer.size() - std::count(m_buffer.begin(), m_buffer.end(), '\0');
}
