#include "reassembler.hh"
#include <cstdint>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (first_index < m_next_currect_index)
    {
        return;
    }

    uint64_t avbl_cap = output.available_capacity();

    auto l_push = [&]() {
        m_next_currect_index = first_index + data.size();
        output.push(std::move(data));
        while (m_rasmblr_buffer.find(m_next_currect_index) != m_rasmblr_buffer.end())
        {
            // 这样编写比起直接调用两次size更容易被编译器优化
            auto tmp_size = m_rasmblr_buffer[m_next_currect_index].size();
            m_rasmblr_buf_cur_data_num -= tmp_size;
            m_next_currect_index += tmp_size;

            output.push(std::move(m_rasmblr_buffer[m_next_currect_index]));
            m_rasmblr_buffer.erase(m_next_currect_index);
        }
    };

    // 直接push的情况集合
    if (first_index == m_next_currect_index)
    {
        l_push();
    }

    auto overlapping = [&]() {
        if (m_rasmblr_buffer.find(first_index) != m_rasmblr_buffer.end())
        {
            m_rasmblr_buffer[first_index] = std::move(data);
            return;
        }
        m_rasmblr_buf_cur_data_num += data.size();
        m_rasmblr_buffer[first_index] = std::move(data);
    };

    // 有空间 && 这个index不存在当前buffer
    if (avbl_cap - m_rasmblr_buf_cur_data_num >= data.size())
    {
        overlapping();
    }

    if (is_last_substring)
    {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const
{
    // Your code here.
    return m_rasmblr_buf_cur_data_num;
}
