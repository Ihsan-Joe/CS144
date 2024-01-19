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

void Reassembler::continue_push(Writer &writer)
{

}

void Reassembler::clearup_buffer()
{
    if (m_buffer.empty())
    {
        return;
    }
    auto begin = m_buffer.begin();
    if (m_pre_index < begin->first)
    {
        
    }
    
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    const auto available_capacity = output.available_capacity();
    
    // 如果first_index的位置在缓冲区之外，那么直接丢弃；
    if (first_index > m_next_index && first_index - m_next_index >= available_capacity)
    {
        try_close(output);
        return;
    }
    
    if (first_index == m_next_index)
    {
        if (data.size() > available_capacity)
        {
            data.erase(available_capacity);
        }

        // 更新上一次发送的package信息
        m_pre_index = m_next_index;
        m_pre_size = data.size();
    
        // 更新下一个要发送的package的index
        m_next_index += data.size();

        output.push(std::move(data));
        clearup_buffer();
        continue_push(output);
        
    }

    else if (first_index > m_next_index)
    {

    }

    else if (first_index == m_pre_index && data.size() > m_pre_size)
    {

    }

    else if (first_index > m_pre_index && first_index + data.size() > m_next_index)
    {

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
