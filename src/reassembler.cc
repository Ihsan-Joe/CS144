#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <iterator>
#include <map>
#include <netinet/in.h>
#include <string>

using namespace std;
void Reassembler::try_close(Writer &writer) const
{
    if (m_is_last_substring && m_buffer.empty())
    {
        writer.close();
    }
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

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{

    m_is_last_substring = m_is_last_substring ? m_is_last_substring : is_last_substring;
    // 只有有空间的时候才可以进行其他操作

    if (!output.available_capacity())
    {
        try_close(output);
        return;
    }

    if (first_index > m_next_index)
    {
        // 存储index大于当前的有效缓冲区
        if (first_index - m_next_index > output.available_capacity())
        {
            try_close(output);
            return;
        }
        if (data.size() + (first_index - m_next_index) > output.available_capacity())
        {
            data.erase(m_next_index + output.available_capacity() - first_index);
        }

        if (data.empty())
        {
            try_close(output);
            return;
        }

        if (m_buffer.empty())
        {
            m_buffer.insert({first_index, data});
            return;
        }
        save_the_buffer(first_index, data);
    }

    else if (first_index == m_next_index)
    {
        // 对数据进行裁剪，取能装下缓冲区的数据
        if (data.size() > output.available_capacity())
        {
            data.erase(output.available_capacity());
        }
        direct_push(first_index + data.size(), data, output);
    }

    // first_index < m_next_index
    else if (first_index + data.size() > m_next_index)
    {
        data = data.substr(m_next_index - first_index);
        if (data.size() > output.available_capacity())
        {
            data.erase(output.available_capacity());
        }
        direct_push(data.size(), data, output);
    }
    try_close(output);
}

void Reassembler::handle_package_end(uint64_t end_position,
                                     std::map<uint64_t, std::string>::iterator first_index_position)
{
    for (auto end_it = first_index_position; end_it != m_buffer.end(); end_it++)
    {
        // if (end_it->first >= end_position)
        // {
        //     return;
        // }

        if (end_it->first + end_it->second.size() > end_position)
        {
            end_it->second.substr(end_position - end_it->first);
        }

        if (end_it->first + end_it->second.size() < end_position)
        {
            m_buffer.erase(end_it);
        }

        if (end_it == std::prev(m_buffer.end()))
        {
            return;
        }

        m_buffer.erase(end_it);
    }
}

void Reassembler::save_the_buffer(uint64_t first_index, std::string &data)
{
    for (auto start_it = m_buffer.begin(); start_it != m_buffer.end(); start_it++)
    {
        // 找出first_index放置位置
        if (first_index < start_it->first)
        {
            if (start_it != m_buffer.begin())
            {
                start_it--;
            }
            handle_package_end(first_index + data.size(), start_it);
            m_buffer.insert({first_index, std::move(data)});
            return;
        }

        if (first_index == start_it->first)
        {
            // 如果在一个package里面
            if (start_it->first + start_it->second.size() > first_index + data.size())
            {
                start_it->second.replace(first_index, data.size(), data);
                return;
            }
            handle_package_end(first_index + data.size(), start_it);
            start_it->second = move(data);
            return;
        }

        if (first_index < start_it->first + start_it->second.size())
        {
            start_it->second.erase(first_index - start_it->first);
            handle_package_end(first_index + data.size(), start_it);
            m_buffer.insert({first_index, std::move(data)});
            return;
        }

        if (start_it == std::prev(m_buffer.end()))
        {
            m_buffer.insert({first_index, std::move(data)});
            return;
        }
    }
}

void Reassembler::direct_push(uint64_t end_position, std::string &data, Writer &writer)
{

    // lower_bound 只返回等于或者大于的元素，因此，下面不对it->first < end_position做判断
    auto it = m_buffer.lower_bound(end_position);

    // 没跟package冲突
    if (it == m_buffer.begin())
    {
        send(data, writer);
        return;
    }

    // 之前的全是覆盖，没重叠
    if (it->first == end_position)
    {
        // it--;
        m_buffer.erase(m_buffer.begin(), it);
        send(data, writer);
        return;
    }

    // 跟前一个package有重叠
    it--;
    if (it->first + it->second.size() > end_position)
    {
        // 没重叠的字符串加到data后面
        data += it->second.substr(end_position - it->first);
        // 一块删除有重叠的package
        m_buffer.erase(m_buffer.begin(), std::next(it));
        send(data, writer);
        return;
    }

    // it->first + it->second <= end_position
    m_buffer.erase(m_buffer.begin(), std::next(it));
    send(data, writer);
}

void Reassembler::send(std::string &data, Writer &writer)
{
    m_next_index += data.size();
    writer.push(data);
    while (!m_buffer.empty())
    {
        auto begin = m_buffer.begin();
        if (begin->first != m_next_index)
        {
            break;
        }
        writer.push(begin->second);
        m_next_index += begin->second.size();
        m_buffer.erase(begin);
    }
}
