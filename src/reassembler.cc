#include "reassembler.hh"
#include <cstdint>
#include <iostream>
#include <netinet/in.h>

using namespace std;

Reassembler::Classify_Returns Reassembler::classify(uint64_t first_index, const std::string_view &data,
                                                    uint64_t available_capacity) const
{
    if (!available_capacity)
    {
        return Beyond_Available_Capacity;
    }
    if (first_index == m_next_index)
    {
        if (data.size() > available_capacity)
        {
            return Need_Cut_And_Push;
        }
        return Direct_Push;
    }
    if (first_index == m_pre_index && data.size() > m_pre_data_size)
    {
        std::cout << "in overlap\n";
        return Overlap;
    }
    if (data.size() <= available_capacity - m_buf_use_capacity)
    {
        std::cout << "in Save_The_Buffer\n";
        return Save_The_Buffer;
    }
    return Beyond_Available_Capacity;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    m_func_calld++;
    if(output.is_closed())
    {
        return;
    }
    auto continue_push = [&]() {
        while (m_reassemble_buf.find(m_next_index) != m_reassemble_buf.end())
        {
            m_pre_data_size = m_reassemble_buf[m_next_index].size();
            output.push(std::move(m_reassemble_buf[m_next_index]));
            m_reassemble_buf.erase(m_pre_index);
            m_pre_index = m_next_index;
            m_next_index += m_pre_data_size;
            m_buf_use_capacity -= m_pre_data_size;
        }
    };

    switch (classify(first_index, data, output.available_capacity()))
    {
    case Beyond_Available_Capacity:
        break;
    
    case Direct_Push:
        m_pre_data_size = data.size();
        output.push(std::move(data));
        m_pre_index = m_next_index;
        m_next_index += m_pre_data_size;
        continue_push();
        break;
    
    case Need_Cut_And_Push:
        data = data.substr(0, output.available_capacity());
        m_pre_data_size = data.size();
        output.push(data);
        m_pre_index = m_next_index;
        m_next_index = m_pre_data_size;
        break;
    
    case Overlap:
        data = data.substr(m_pre_data_size);
        m_pre_data_size = data.size();
        output.push(move(data));
        m_pre_index = m_pre_index + m_pre_data_size;
        break;

    case Save_The_Buffer:
        if(m_first_unassembled_index > first_index)
        {
            m_first_unassembled_index = first_index;
        }
        m_buf_use_capacity += data.size();
        m_reassemble_buf[first_index] = std::move(data);
        break;
    }

    if (is_last_substring)
    {

        m_write_close = true;
    }
    std::cout << "m_buf_use_capacity: " << m_buf_use_capacity <<'\n';
    if (m_write_close && !m_buf_use_capacity)
    {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    return m_buf_use_capacity;
}
