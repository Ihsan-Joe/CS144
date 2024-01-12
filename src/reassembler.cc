#include "reassembler.hh"
#include <cstdint>
#include <iostream>
#include <iterator>
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
    if (output.is_closed())
    {
        return;
    }

    auto init_m_buf_first_data = [&]() {
        if (0 == m_buf_use_capacity)
        {
            m_buf_first_data.index = 0;
            m_buf_first_data.data_string = "";
            return;
        }
        m_buf_first_data = m_reassemble_buf.front();
    };
    init_m_buf_first_data();

    // 函数处理:
    // 当此data内容有跟buffer里面first data有重叠。
    // 此时需要把buffer里面有重叠的data截取到合适的大小，把index改成m_next_index
    // 如果涉及多个data，那么看buffer中最后一个有重叠的data，如果是被完全覆盖不用管，如果不是那么需要截取
    auto repair_overlap = [&]() {
        if (0 == m_buf_use_capacity)
        {
            return;
        }
        auto begin = m_reassemble_buf.begin();
        auto end = m_reassemble_buf.end();
        bool the_last_data = false;

        for (; begin != end; begin++)
        {
            auto next_data = std::next(begin);
            if (next_data == end)
            {
                the_last_data = true;
            }

            m_buf_use_capacity -= begin->data_string.size();
            m_reassemble_buf.pop_front();
            if (m_next_index > next_data->index && !the_last_data)
            {
                continue;
            }

            // 需要截取，调整index的只有这个
            if (m_next_index < begin->index + begin->data_string.size())
            {
                auto rest_of_size = begin->index + begin->data_string.size() - m_next_index;
                begin->data_string = begin->data_string.substr(rest_of_size);
                m_buf_use_capacity -= rest_of_size;
                begin->index += rest_of_size;
            }
            // if:
            //  1.m_next_index > begin->index + begin->data_string.size())
            //  2.m_next_index == next_index
            //  3.m_next_index == begin->index + begin->data_string.size()
            break;
        }
    };

    auto continue_push = [&]() {
        while (0 != m_buf_use_capacity && m_next_index == m_buf_first_data.index)
        {
            m_pre_data_size = m_buf_first_data.data_string.size();
            output.push(std::move(m_buf_first_data.data_string));
            m_reassemble_buf.pop_front();
            init_m_buf_first_data();
            m_pre_index = m_next_index;
            m_next_index += m_pre_data_size;
            m_buf_use_capacity -= m_pre_data_size;
        }
    };

    auto save_right_place = [&]() {
        auto begin = m_reassemble_buf.begin();
        auto end = m_reassemble_buf.end();
        bool the_last_data = false;

        for (; begin != end; begin++)
        {
            auto next_data = std::next(begin);
            if (next_data == end)
            {
                the_last_data = true;
            }

            if (first_index == begin->index)
            {
                m_buf_use_capacity -= begin->data_string.size() - data.size();
                begin->data_string = data;
            }

            else if (first_index < next_data->index)
            {
                m_buf_use_capacity += data.size();
                m_reassemble_buf.insert_after(begin, {first_index, data});
            }

            else if (first_index > next_data->index && !the_last_data)
            {
                continue;
            }
        }
    };

    switch (classify(first_index, data, output.available_capacity()))
    {
    case Beyond_Available_Capacity:
        break;

    case Direct_Push:
        m_pre_data_size = data.size();
        m_pre_index = m_next_index;
        m_next_index += data.size();
        output.push(std::move(data));
        if (0 != m_buf_use_capacity && m_next_index > m_buf_first_data.index)
        {
            repair_overlap();
        }
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
        m_next_index = m_next_index + data.size();
        m_pre_index = m_next_index;
        m_pre_data_size = data.size();
        output.push(move(data));
        if (0 != m_buf_use_capacity && m_next_index > m_buf_first_data.index)
        {
            repair_overlap();
        }
        continue_push();
        break;

    case Save_The_Buffer:
        if ( 0 != m_buf_use_capacity || first_index < m_buf_first_data.index)
        {
            m_buf_use_capacity += data.size();
            m_reassemble_buf.push_front({first_index, data});
            init_m_buf_first_data();
        }
        else if (first_index == m_buf_first_data.index)
        {
            m_buf_use_capacity += data.size() - m_buf_first_data.data_string.size();
            m_buf_first_data.data_string = data;
        }
        else
        {
            save_right_place();
        }
        break;
    }
    // switch case end

    if (is_last_substring)
    {

        m_write_close = true;
    }
    if (m_write_close && 0 == m_buf_use_capacity)
    {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    return m_buf_use_capacity;
}
