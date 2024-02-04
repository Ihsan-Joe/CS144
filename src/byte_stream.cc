#include "byte_stream.hh"
#include <asm-generic/errno.h>
#include <cstdint>
#include <queue>
#include <string>
#include <string_view>
#include <unistd.h>

using namespace std;

ByteStream::ByteStream(uint64_t capacity)
    : capacity_(capacity), m_writer_close(false), m_error(false), m_pushed(0), m_poped(0), m_buffer_cur_data_number(0), m_buffer()
{
}

void Writer::push(string data)
{
    const auto data_size = data.size();
    const uint64_t avai_size = available_capacity();
    if (is_closed() || !avai_size || !data_size)
    {
        return;
    }

    // const auto n = min(data_size, avai_size);
    // if (data_size > n)
    // {
    //     data = data.substr(0, avai_size);
    // }
    // buffer.push(move(data));
    // buffer_cur_data_number += n;
    // pushed += n;

    uint64_t tmp_number = 0;
    if (data_size > avai_size)
    {
        m_buffer.push(data.substr(0, avai_size));
        tmp_number = avai_size;
    }
    else
    {
        m_buffer.push(std::move(data));
        tmp_number = data_size;
    }

    m_buffer_cur_data_number += tmp_number;
    m_pushed += tmp_number;
}

void Writer::close()
{
    m_writer_close = true;
}

void Writer::set_error()
{
    m_error = true;
}

bool Writer::is_closed() const
{
    return m_writer_close;
}

uint64_t Writer::available_capacity() const
{
    return capacity_ - m_buffer_cur_data_number;
}

uint64_t Writer::bytes_pushed() const
{
    return m_pushed;
}

string_view Reader::peek() const
{
    return m_buffer.front();
}

bool Reader::is_finished() const
{
    return m_writer_close && !m_buffer_cur_data_number;
}

bool Reader::has_error() const
{
    return m_error;
}

void Reader::pop(uint64_t len)
{
    if (m_buffer_cur_data_number < len || has_error())
    {
        return;
    }

    m_buffer_cur_data_number -= len;
    m_poped += len;

    std::string_view tmp_view = m_buffer.front();
    while (len > 0)
    {
        if (len < tmp_view.size())
        {
            m_buffer.front() = tmp_view.substr(len);
            break;
        }
        len -= tmp_view.size();
        m_buffer.pop();
        tmp_view = m_buffer.front();
    }
}

uint64_t Reader::bytes_buffered() const
{
    return m_buffer_cur_data_number;
}

uint64_t Reader::bytes_popped() const
{
    return m_poped;
}
