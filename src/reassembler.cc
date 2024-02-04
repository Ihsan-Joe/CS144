#include "byte_stream.hh"
#include "reassembler.hh"
#include <cstdint>
#include <iterator>
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
    for (const auto& [key, value] : m_buffer)
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
            try_close(output);
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
        direct_push(m_next_index + data.size(), data, output);
    }

    // first_index < m_next_index
    else if (first_index + data.size() > m_next_index)
    {
        data = data.substr(m_next_index - first_index);
        if (data.size() > output.available_capacity())
        {
            data.erase(output.available_capacity());
        }
        direct_push(m_next_index + data.size(), data, output);
    }
    try_close(output);
}

void Reassembler::save_the_buffer(uint64_t first_index, std::string &data)
{
    auto it_front = m_buffer.lower_bound(first_index);
    auto it_last = m_buffer.lower_bound(first_index + data.size());

    if (it_front != m_buffer.end())
    {
        if (m_buffer.size() != 1 && first_index == it_front->first)
        {
            m_buffer.erase(std::next(it_front), handle_package_overlap(it_last, first_index, data));
            it_front->second.replace(0, data.size(), data);
            return;
        }
        
        if (m_buffer.size() == 1 && first_index == it_front->first)
        {
            it_front->second.replace(0, data.size(), data);
            return;
        }
    }

    if (it_front == m_buffer.begin())
    {
        if (first_index < it_front->first)
        {
            m_buffer.erase(it_front, handle_package_overlap(it_last, first_index, data));
            m_buffer.insert({first_index, data});
            return;
        }
    }
    // 如果是最靠前的元素
    else
    {
        --it_front;
        if (first_index >= it_front->first + it_front->second.size())
        {
            m_buffer.erase(std::next(it_front), handle_package_overlap(it_last, first_index, data));
            m_buffer.insert({first_index, data});
            return;
        }
    }

    if (first_index + data.size() >= it_front->first + it_front->second.size())
    {
        it_front->second.erase(first_index - it_front->first);
        m_buffer.erase(std::next(it_front), handle_package_overlap(it_last, first_index, data));
        m_buffer.insert({first_index, data});
        return;
    }
    it_front->second.replace(first_index - it_front->first, data.size(), data);
}

my_map::iterator Reassembler::handle_package_overlap(my_map::iterator it_last, uint64_t first_index, std::string &data)
{
    if (it_last == m_buffer.begin())
    {
        return it_last;
    }

    if (it_last != m_buffer.end())
    {
        if (first_index + data.size() == it_last->first)
        {
            return it_last;
        }
    }

    // if (first_index + data.size() < it_last->first)
    --it_last;
    if (first_index + data.size() >= it_last->first + it_last->second.size())
    {
        return std::next(it_last);
    }
    // if (first_index + data.size() < it_last->first + it_last->second.size())
    m_buffer.insert({first_index + data.size(), it_last->second.substr(first_index + data.size() - it_last->first)});
    return m_buffer.find(first_index + data.size());
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
    // if (it->first < end_position)
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
