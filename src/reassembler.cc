#include "reassembler.hh"
#include "byte_stream.hh"
#include <cstdint>
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

bool Reassembler::organize(uint64_t first_index, std::string &data)
{
    // 如果后面有元素合并成一个package
    // 
    if (first_index == m_next_index)
    {
        return true;
    }
    // 需要看前后有没有被重叠的package,有的话都合并成一个，覆盖的直接删掉
    if (first_index > m_next_index)
    {

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
        // 不是垃圾数据就整理buffer
        if (organize(first_index, data))
        {
            // 如果需要发送的
            send(output);
        }
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
