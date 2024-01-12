#include "reassembler.hh"
#include <cstdint>
#include <iostream>
#include <netinet/in.h>

using namespace std;

Reassembler::Classify_Returns Reassembler::classify(uint64_t first_index, const std::string_view &data,
                                                    uint64_t available_capacity) const
{
    std::cout << "################################# insert: " << data << " ###################################\n";
    // std::cout << "available_capacity" << available_capacity << '\n';
    // std::cout << "first_index: " << first_index << ","
            //   << "m_next_index: " << m_next_index << '\n';
    std::cout << "available_capacity - m_buf_availeble_capacity :" << available_capacity - m_buf_availeble_capacity << '\n';
    

    if (!available_capacity)
    {
        return Beyond_Available_Capacity;
    }
    if (first_index == m_next_index)
    {
        if(data.size() > available_capacity)
        {
            return Need_Cut_And_Push;
        }
        return Direct_Push;
    }
    if (first_index == m_pre_index && data.size() > m_pre_data_size)
    {
        return Overlap;
    }
    if (data.size() <= available_capacity - m_buf_availeble_capacity)
    {
        return Save_The_Buffer;
    }
    return Beyond_Available_Capacity;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    auto continue_push = [&]() {
        while (m_reassemble_buf.find(m_next_index) != m_reassemble_buf.end())
        {
            m_pre_data_size = m_reassemble_buf[m_next_index].size();
            output.push(std::move(m_reassemble_buf[m_next_index]));
            m_pre_index = m_next_index;
            m_next_index += m_pre_data_size;
            m_reassemble_buf.erase(m_pre_index);
            m_buf_availeble_capacity -= m_pre_data_size;
        }
    };

    // uint64_t avail_cap = 0;

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
        // avail_cap = output.available_capacity() - m_buf_availeble_capacity;
        // data = data.substr(m_pre_data_size, avail_cap);
        data = data.substr(m_pre_data_size);
        m_pre_data_size = data.size();
        output.push(move(data));
        m_pre_index = m_pre_index + m_pre_data_size;
        break;
        
    case Save_The_Buffer:
        m_buf_availeble_capacity = data.size();
        m_reassemble_buf[first_index] = std::move(data);
        break;
    }
    if (is_last_substring)
    {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    // Your code here.
    return m_buf_availeble_capacity;
}
