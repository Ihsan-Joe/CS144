#include "reassembler.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>

using namespace std;

size_t Reassembler::handle_data_string(std::string &src, std::string &dec)
{
    size_t index = dec.find_first_of(src.back());
    if (index == std::string::npos)
    {
        return -1;
    }
    return index + 1;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    if (output.is_closed() && first_index < m_pre_index)
    {
        return;
    }

    const auto avlb_capacity = output.available_capacity() - m_rasmblr_buf_cur_data_num;

    // 缓冲区清理
    auto clean_buffer = [&](const uint64_t &need_capacity, const uint64_t &the_need_space_data_index) {
        // 如果需要的空间比当前空buffer还大，那么直接丢弃。
        if (need_capacity > output.available_capacity())
        {
            return false;
        }

        // 先计算好需要的空间是否
        uint64_t length_from_end = m_the_last_index;
        uint64_t free_up_space = 0;

        while (length_from_end <= the_need_space_data_index)
        {
            if (free_up_space >= need_capacity)
            {
                // free space
                m_the_last_index = length_from_end - m_rasmblr_buffer[length_from_end].size();
                m_rasmblr_buf_cur_data_num -= free_up_space;
                while (length_from_end <= m_the_last_index)
                {
                    length_from_end += m_rasmblr_buffer[length_from_end].size();
                    m_rasmblr_buffer.erase(length_from_end);
                }
                return true;
            }
            length_from_end -= m_rasmblr_buffer[length_from_end].size();
            free_up_space += m_rasmblr_buffer[length_from_end].size();
        }
        // 踢出 当前需要被容纳的data 后面的data后，还是没法容纳的话 return false.
        return false;
    };

    // 获取上一次insert和这一次的capacity的差异，从而知道上一个data发送到哪儿了。
    const auto get_poped = [&]() { return output.available_capacity() - m_pre_capacity; };

    auto direct_push = [&]() {
        bool success_push = false;
        // data.size > 总available capacity,那就把它修成=总available capacity
        m_pre_data = data;
        if (data.size() > output.available_capacity())
        {
            // m_next_currect_index += output.available_capacity();
            data.erase(output.available_capacity());
        }
        if (data.size() <= avlb_capacity || clean_buffer(data.size(), first_index))
        {
            m_pre_index = m_next_currect_index;
            m_next_currect_index += data.size();
            output.push(std::move(data));
            success_push = true;
        }
        while (success_push && m_rasmblr_buffer.find(m_next_currect_index) != m_rasmblr_buffer.end())
        {
            std::cout << output.available_capacity() << ',' << m_next_currect_index << ',' << m_rasmblr_buf_cur_data_num
                      << ',' << m_rasmblr_buffer[m_next_currect_index] << ','
                      << m_rasmblr_buffer[m_next_currect_index].size() << '\n';
            auto tmp_size = m_rasmblr_buffer[m_next_currect_index].size();
            std::string tmp_data = std::move(m_rasmblr_buffer[m_next_currect_index]);
            m_rasmblr_buf_cur_data_num -= tmp_size;
            m_pre_index = m_next_currect_index;
            m_pre_data = tmp_data;
            m_next_currect_index += tmp_size;
            std::cout << "tmp_data" << tmp_data << '\n';
            m_rasmblr_buffer.erase(m_next_currect_index);
            std::cout << output.available_capacity() << ',' << m_next_currect_index << ',' << m_rasmblr_buf_cur_data_num
                      << '\n';
            output.push(std::move(tmp_data));
        }
        if (!m_rasmblr_buf_cur_data_num)
        {
            m_the_last_index = 0;
        }
    };

    auto different_index_overlap = [&]() {
        // reassembler buffer中有这个index的data
        if (m_rasmblr_buffer.find(first_index) != m_rasmblr_buffer.end())
        {
            // 有足够的空间替换new_data
            if (m_rasmblr_buffer[first_index].size() + avlb_capacity >= data.size())
            {
                m_rasmblr_buf_cur_data_num += data.size() - m_rasmblr_buffer[first_index].size();
                m_rasmblr_buffer[first_index] = std::move(data);
                m_the_last_index = max(m_the_last_index, first_index);
            }
            // 如果不是最后一个data，那么给他腾空间
            else if (first_index != m_the_last_index && clean_buffer(m_rasmblr_buffer[first_index].size(), first_index))
            {
                m_rasmblr_buf_cur_data_num += m_rasmblr_buffer[first_index].size();
                m_the_last_index = max(m_the_last_index, first_index);
            }
        }
        // 不存在当前buffer中, 看看存储空间能不能容得下current data
        else if (avlb_capacity >= data.size())
        {
            m_rasmblr_buf_cur_data_num += data.size();
            m_rasmblr_buffer[first_index] = std::move(data);
            m_the_last_index = max(m_the_last_index, first_index);
        }
    };

    auto pre_data_overlap = [&]() {
        // poped_number 肯定会 >= 0,
        // 因为available
        // capacity，上一次output.push后，如果又被output.push，那么这个分支不会被执行，因为m_last_index被改了。
        const auto poped_number = get_poped();
        std::cout << "pre_data_overlap : "
                  << "avlb_capacity:" << avlb_capacity << '\n';
        std::cout << "pre_data_overlap : "
                  << "poped_number:" << poped_number << '\n';
        std::cout << "pre_data_overlap : "
                  << "data.size():" << data.size() << '\n';

        if (data.size() > avlb_capacity + poped_number && clean_buffer(data.size() - poped_number, first_index))
        {
            // 调整缓冲区大小
            m_next_currect_index = m_pre_index + data.size();
            m_rasmblr_buf_cur_data_num += data.size();
            output.push(std::move(data));
        }
        else
        {
            std::cout << "pre_data_overlap: m_pre_data.size():" << m_pre_data.size() << '\n';
            output.push(data.substr(m_pre_data.size() - 1));
            std::cout << "pre_data_overlap: data.substr(data.size() - m_pre_data.size()):"
                      << data.substr(m_pre_data.size() - 1) << '\n';
        }
    };

    if (avlb_capacity)
    {
        // data index符合当前能push的index顺序
        if (first_index == m_next_currect_index)
        {
            direct_push();
        }
        // data index是上一个data的overlap
        else if (first_index == m_pre_index && data.size() > m_pre_data.size())
        {
            pre_data_overlap();
        }
        // data index是未来的data，因此有空间就存放到reassemble buffer中，没空间直接丢弃
        else
        {
            different_index_overlap();
        }
    }

    if (is_last_substring)
    {
        output.close();
    }
    m_pre_capacity = output.available_capacity();

    std::cout << "################################# " << count_called++ << " ###################################\n";
    std::cout << "m_rasmblr_buf_cur_data_num:" << m_rasmblr_buf_cur_data_num << '\n';
    int i = 0;
    for (const auto &pair : m_rasmblr_buffer)
    {
        std::cout << "m_rasmblr_buffer " << i << ":" << pair.first << "," << pair.second << '\n';
        i++;
    }
    std::cout << "m_next_currect_index:" << m_next_currect_index << '\n';
    std::cout << "m_pre_index:" << m_pre_index << '\n';
    std::cout << "m_pre_data:" << m_pre_data << '\n';
    std::cout << "m_the_last_index:" << m_the_last_index << '\n';
    std::cout << "avlb_capacity:" << avlb_capacity << '\n';
    std::cout << "m_pre_capacity:" << m_pre_capacity << '\n';
    std::cout << "####################################################################\n";
}

uint64_t Reassembler::bytes_pending() const
{
    // Your code here.
    return m_rasmblr_buf_cur_data_num;
}
