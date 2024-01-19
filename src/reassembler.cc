#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <iterator>
#include <netinet/in.h>

using namespace std;

void Reassembler::try_close(Writer &writer) const
{
    if (m_is_last_substring && !bytes_pending())
    {
        writer.close();
    }
}

bool Reassembler::garbage_package(uint64_t first_index, uint64_t data_size, uint64_t available_capacity) const
{
    // 缓存位置超过缓存大小
    if (first_index > m_next_index && first_index - m_next_index > available_capacity)
    {
        return false;
    }
    // 已经push过的package
    if (first_index < m_next_index && first_index + data_size <= m_pre_size)
    {
        return false;
    }
    // 没数据
    if (data_size == 0)
    {
        return false;
    }
    return true;
}

void Reassembler::organize(uint64_t first_index, std::string &data, Writer &writer)
{
    auto handler = [&](uint64_t end_position) {
        auto it = m_buffer.lower_bound(end_position);

        // 没跟package冲突
        if (it == m_buffer.begin())
        {
            send(writer);
            return;
        }
        
        // 没重叠, 全是覆盖
        if (it == m_buffer.end() || it->first + it->second.size() < end_position)
        {
            m_buffer.erase(m_buffer.begin(), it);
            send(writer);
            return;
        }

        // 之前的全是覆盖，没重叠
        if (it->first == end_position)
        {
            it--;
            m_buffer.erase(m_buffer.begin(), it);
            send(writer);
            return;
        }

        // 把有重叠的package的data加到当前的data,然后删除有重叠的package
        data = data.substr(m_next_index - first_index) + it->second.substr(end_position);
        m_buffer.erase(m_buffer.begin(), it);
    };
    // 如果后面有元素合并成一个package
    if (first_index == m_next_index)
    {
        handler(first_index + data.size());
        return;
    }

    if (first_index < m_next_index && first_index + data.size() > m_next_index)
    {
        handler(first_index + data.size() - m_next_index);
        return;
    }

    // 需要看前后有没有被重叠的package,有的话都合并成一个，覆盖的直接删掉
    if (first_index > m_next_index)
    {
    }
}

void Reassembler::send(Writer &writer)
{
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    // 处理垃圾数据
    if (garbage_package(first_index, data.size(), output.available_capacity()))
    {
        // 不是垃圾数据就整理buffer
        organize(first_index, data, output);
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
