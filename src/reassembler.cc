#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
#include <iterator>

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
        return true;
    }
    // 已经push过的package
    if (first_index < m_next_index && first_index + data_size <= m_pre_size)
    {
        return true;
    }
    // 没数据
    if (data_size == 0)
    {
        return true;
    }
    return false;
}

void Reassembler::send(Writer &writer)
{
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{

    // 处理垃圾数据
    if (garbage_package(first_index, data.size(), output.available_capacity()))
    {
        m_is_last_substring = is_last_substring;
        try_close(output);
    }

    if (first_index == m_next_index)
    {
    }

    else if (first_index > m_next_index)
    {
    }

    // first_index < m_next_index && first_index + data_size > m_pre_size
    // 此类型需要把从m_next_index开始发送，也就是data.substr(m_next_index - first_index)
    else
    {
        auto handle = [&]() {
            
            auto it = m_end_index_buffer.upper_bound(first_index + data.size());
        
            if (it != m_end_index_buffer.end())
            {
                int distance = std::distance(m_end_index_buffer.begin(), it);
                auto upper = m_start_index_buffer.begin();
                std::advance(upper, distance);
                if (*upper < first_index + data.size())
                {
                    m_package_buffer[*upper] = m_package_buffer[*upper].substr(first_index + data.size() - *upper);
                    
                }
                it--;
                m_end_index_buffer.erase(m_end_index_buffer.begin(), it);
                for()
                {
                    m_package_buffer.erase(*it);
                    
                }
                return;
            }
        
        
        };
        handle();
    }

    m_is_last_substring = is_last_substring;
    try_close(output);
}

uint64_t Reassembler::bytes_pending() const
{
    uint64_t count = 0;
    for (const auto &item : m_package_buffer)
    {
        count += item.second.size();
    }
    return count;
}
