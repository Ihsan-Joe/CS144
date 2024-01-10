#include "reassembler.hh"

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    auto rsmbl_func = [&]() {
        if (first_index != m_next_currect_index)
        {
            // 有空间放置&&当前buffer不存在此数据包
            if (output.available_capacity() - m_rasmblr_buf_cur_data_num >= data.size() &&
                m_rasmblr_buffer.find(first_index) == m_rasmblr_buffer.end())
            {
                m_rasmblr_buf_cur_data_num += data.size();
                m_rasmblr_buffer[first_index] = std::move(data);
            }
            return;
        }

        m_next_currect_index = first_index + 1;
        output.push(std::move(data));

        while (!m_rasmblr_buffer.empty())
        {
            // 没空间 || 没有顺序合适的数据包
            if (!output.available_capacity() || m_rasmblr_buffer.find(m_next_currect_index) == m_rasmblr_buffer.end())
            {
                break;
            }
            // 从 reassembler buffer 总字符数 减去 当前被推到 Write buffer的字符数量
            m_rasmblr_buf_cur_data_num -= m_rasmblr_buffer[m_next_currect_index].size();

            // 推过去下一个数据包，比如这次给送过来的[index 5],
            // 本来rsmbl_buffer中有 [index 6]那么它也会推过去，在这个循环中
            output.push(std::move(m_rasmblr_buffer[m_next_currect_index]));
            
            // 清除当前被推到 Write buffer 的键值对
            m_rasmblr_buffer.erase(m_next_currect_index++);

        }
    };

    if (is_last_substring)
    {
        rsmbl_func();
        output.close();
        return;
    }
    rsmbl_func();
}

uint64_t Reassembler::bytes_pending() const
{
    // Your code here.
    return {};
}
