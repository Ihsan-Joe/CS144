#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <system_error>

using namespace std;

void tmp_traverse_func(std::forward_list<Reassembler::m_data_struct> &m_reassemble_buf)
{
    std::cerr << "****************************" << '\n';
    for (auto &emp_val : m_reassemble_buf)
    {
        std::cerr << "emp_val.index: " << emp_val.index << ", emp_val.data_string: " << emp_val.data_string << '\n';
    }
    std::cerr << "****************************" << '\n';
}

Reassembler::Classify_Returns Reassembler::classify(uint64_t first_index, const std::string_view &data,
                                                    uint64_t available_capacity) const
{
    // ####################################################################

    std::cerr << "available_capacity " << available_capacity << '\n';
    std::cerr << "m_pre_index " << m_pre_index << " m_pre_data_size:" << m_pre_data_size << '\n';
    std::cerr << "m_buf_use_capacity " << m_buf_use_capacity << '\n';
    std::cerr << "m_next_index " << m_next_index << '\n';

    // ####################################################################

    if (!available_capacity || first_index < m_pre_index || (first_index > m_pre_index && first_index < m_next_index))
    {
        std::cerr << "in Beyond_Available_Capacity\n";
        return Beyond_Available_Capacity;
    }
    if (first_index == m_next_index)
    {
        if (data.size() > available_capacity)
        {
            std::cerr << "in Need_Cut_And_Push\n";

            return Need_Cut_And_Push;
        }

        std::cerr << "in Direct_Push\n";

        return Direct_Push;
    }
    if (first_index == m_pre_index)
    {
        if (data.size() < m_pre_data_size)
        {
            std::cerr << "in Beyond_Available_Capacity\n";
            return Beyond_Available_Capacity;
        }
        std::cerr << "in overlap\n";
        return Overlap;
    }
    if (data.size() <= available_capacity - m_buf_use_capacity)
    {
        std::cerr << "in Save_The_Buffer\n";
        return Save_The_Buffer;
    }
    std::cerr << "in Beyond_Available_Capacity\n";

    return Beyond_Available_Capacity;
}

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring, Writer &output)
{
    std::cerr << "################################# " << m_func_calld << "data size: " << data.size()
              << " first_index: " << first_index << " ###################################\n";
    std::cerr << "in main function\n";
    //tmp_traverse_func(m_reassemble_buf);

    if (output.is_closed())
    {
        return;
    }

    auto init_m_buf_first_data = [&]() {
        std::cerr << "in init_m_buf_first_data\n";
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
    auto continue_push = [&]() {
        std::cerr << "in continue_push\n";

        //tmp_traverse_func(m_reassemble_buf);

        while (0 != m_buf_use_capacity && m_next_index == m_buf_first_data.index)
        {
            m_pre_data_size = m_buf_first_data.data_string.size();

            std::cerr << "output.push : " << m_buf_first_data.data_string << '\n';

            output.push(std::move(m_buf_first_data.data_string));
            m_reassemble_buf.pop_front();

            std::cerr << "m_reassemble_buf.empty(): " << m_reassemble_buf.empty() << '\n';

            m_buf_use_capacity -= m_pre_data_size;
            m_pre_index = m_next_index;
            m_next_index += m_pre_data_size;

            std::cerr << "m_buf_use_capacity: " << m_buf_use_capacity << '\n';

            init_m_buf_first_data();
        }
    };

    auto repair_overlap = [&]() {
        std::cerr << "in repair_overlap\n";

        if (0 == m_buf_use_capacity)
        {
            return;
        }
        auto begin = m_reassemble_buf.begin();
        auto end = m_reassemble_buf.end();

        for (; begin != end; begin++)
        {

            std::cerr << "m_next_index: " << m_next_index << ','
                      << "begin->index + begin->data_string.size(): " << begin->index + begin->data_string.size()
                      << '\n';

            // 需要截取，调整index的只有这个
            if (m_next_index < begin->index + begin->data_string.size())
            {
                begin->data_string = begin->data_string.substr(m_next_index - begin->index);
                m_buf_use_capacity -= m_next_index - begin->index;
                begin->index += m_next_index - begin->index;
                break;
            }

            auto next_data = std::next(begin);
            m_buf_use_capacity -= begin->data_string.size();
            m_reassemble_buf.pop_front();

            if (!(next_data == end) && m_next_index > next_data->index)
            {
                continue;
            }
            // if:
            //  1.m_next_index > begin->index + begin->data_string.size()) 比这个data大，但是下一个data小
            //  2.m_next_index == next_index
            //  3.m_next_index == begin->index + begin->data_string.size()
            break;
        }
        init_m_buf_first_data();
    };

    auto reapir_unassembler_overlap = [&](uint64_t index) {
        std::cerr << "in reapir_unassembler_overlap\n";
        //tmp_traverse_func(m_reassemble_buf);

        auto new_data_position = m_reassemble_buf.begin();
        std::advance(new_data_position, index);

        auto begin = m_reassemble_buf.before_begin();
        auto end = m_reassemble_buf.end();

        uint64_t data_end_index = first_index + data.size();

        // 跟自己前面的data有重叠部分的话这个函数处理，后面的还是需要
        while (true)
        {
            // std::cerr << "in 第一个while\n";
            auto next_data = std::next(begin);
            if (next_data == new_data_position)
            {
                std::cerr << "in 第一个while的next_data == new_data_position\n";
                break;
            }
            // 如果跟当前data有重叠部分,
            if (first_index > next_data->index && first_index < next_data->index + next_data->data_string.size())
            {

                std::cerr << "进入第一个while的处理部分\n";
                std::cerr << "first_index: " << first_index << " next_data->index: " << next_data->index << '\n';
                std::cerr << "data.size(): " << data.size() << " next_data->data_string.size(): " << next_data->data_string.size() << '\n';

                if (first_index + data.size() < next_data->index + next_data->data_string.size())
                {
                    std::cerr << "in insert_after" << '\n';

                    m_reassemble_buf.insert_after(
                        new_data_position,
                        {first_index + data.size(), next_data->data_string.substr((first_index - next_data->index) + data.size())});

                    m_buf_use_capacity += std::next(new_data_position)->data_string.size();
                }

                m_buf_use_capacity -= next_data->data_string.size();
                next_data->data_string.erase(first_index - next_data->index);
                m_buf_use_capacity += next_data->data_string.size();

                //tmp_traverse_func(m_reassemble_buf);

                break;
            }
            // std::cerr << "跟当前data无重叠\n";
            begin++;
        }

        while (true)
        {
            // std::cerr << "in 第二个while\n";

            auto next = std::next(new_data_position);

            // 如果已是最后面的一个元素 或 如果new data跟后面存放的data没重叠
            if (next == end || data_end_index <= next->index)
            {
                std::cerr << "in 第二个while的 next == end || data_end_index <= next->index \n";

                break;
            }

            // 如果new data跟后面的data有重叠
            if (data_end_index < next->index + next->data_string.size())
            {
                std::cerr << "进入第二个while的处理部分\n";

                std::cerr << "m_buf_use_capacity before: " << m_buf_use_capacity << '\n';
                m_buf_use_capacity -= data_end_index - next->index;
                std::cerr << "m_buf_use_capacity after: " << m_buf_use_capacity << '\n';
                next->data_string = next->data_string.substr(data_end_index - next->index);

                next->index += data_end_index - next->index; // 对
                break;
            }
            std::cerr << "完全覆盖\n";
            // 如果new data完全覆盖后面的data
            m_buf_use_capacity -= next->data_string.size();
            m_reassemble_buf.erase_after(new_data_position);
        }

        //tmp_traverse_func(m_reassemble_buf);
    };

    auto save_right_place = [&]() {
        std::cerr << "in save_right_place\n";

        auto begin = m_reassemble_buf.begin();
        auto end = m_reassemble_buf.end();

        uint64_t counter = 0;

        for (; begin != end; begin++)
        {
            if (first_index == begin->index)
            {
                // std::cerr << "**************m_buf_use_capacity before: " << m_buf_use_capacity << '\n';
                auto tmp = begin->data_string.size();
                begin->data_string.replace(0, data.size(), data);
                if (tmp < begin->data_string.size())
                {
                    m_buf_use_capacity += begin->data_string.size() - tmp;
                }
                break;
            }

            auto next_data = std::next(begin);
            if (next_data == end)
            {
                m_buf_use_capacity += data.size();
                m_reassemble_buf.insert_after(begin, {first_index, data});
                counter++;
                break;
            }

            if (first_index < next_data->index)
            {
                m_buf_use_capacity += data.size();
                m_reassemble_buf.insert_after(begin, {first_index, data});
                counter++;
                break;
            }

            if (first_index > next_data->index)
            {
                counter++;
                continue;
            }
        }
        std::cerr << "m_buf_use_capacity: " << m_buf_use_capacity << '\n';
        std::cerr << "counter: " << counter << '\n';
        return counter;
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
        if (0 != m_buf_use_capacity && m_next_index >= m_buf_first_data.index)
        {
            repair_overlap();
            std::cerr << "m_buf_use_capacity " << m_buf_use_capacity << " m_reassemble_buf: " << m_buf_first_data.index
                      << ',' << m_buf_first_data.data_string << '\n';
        }

        continue_push();
        break;

    case Need_Cut_And_Push:
        data = data.substr(0, output.available_capacity());
        m_pre_data_size = data.size();
        output.push(std::move(data));
        m_pre_index = m_next_index;
        m_next_index = m_next_index + m_pre_data_size;
        break;

    case Overlap:
        data = data.substr(m_pre_data_size);
        m_pre_index = m_next_index;
        m_pre_data_size = data.size();
        m_next_index = m_next_index + data.size();
        output.push(move(data));
        if (0 != m_buf_use_capacity && m_next_index > m_buf_first_data.index)
        {
            repair_overlap();
        }
        continue_push();
        break;

    case Save_The_Buffer:
        // 这里声明一个变量，存储新保存的data位置，然后最后面，break前面写一个函数，来处理新data和新data后面的那个data
        // 重叠多，可能需要写一个while循环去处理，处理过程类似repair_overlap
        if (first_index + data.size() > output.available_capacity())
        {
            data = data.substr(0, output.available_capacity() - 1);

            std::cerr << data << '\n';
        }
        if (0 == m_buf_use_capacity)
        {
            m_buf_use_capacity += data.size();
            m_reassemble_buf.push_front({first_index, data});
        }
        else if (first_index < m_buf_first_data.index)
        {
            m_buf_use_capacity += data.size();
            m_reassemble_buf.push_front({first_index, data});
            reapir_unassembler_overlap(0);
        }
        else
        {
            reapir_unassembler_overlap(save_right_place());
        }
        break;
    }
    // switch case end

    std::cerr << "out the switch\n";
    //tmp_traverse_func(m_reassemble_buf);
    std::cerr << "m_buf_use_capacity: " << m_buf_use_capacity << '\n';

    if (is_last_substring)
    {
        m_write_close = true;
    }
    if (m_write_close && 0 == m_buf_use_capacity)
    {
        output.close();
    }
    m_func_calld++;
}

uint64_t Reassembler::bytes_pending() const
{
    return m_buf_use_capacity;
}
