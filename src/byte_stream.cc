#include "byte_stream.hh"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unistd.h>

using namespace std;

ByteStream::ByteStream(uint64_t capacity)
    : capacity_(capacity), writer_close(false), error(false), pushed(0), poped(0), buffer_cur_data_number(0), buffer()
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

    const auto n = min(data_size, avai_size);
    if (data_size > n)
    {
        data = data.substr(0, avai_size);
    }
    buffer.push(move(data));
    buffer_cur_data_number += n;
    pushed += n;
}

void Writer::close()
{
    writer_close = true;
}

void Writer::set_error()
{
    error = true;
}

bool Writer::is_closed() const
{
    return writer_close;
}

uint64_t Writer::available_capacity() const
{
    return capacity_ - buffer_cur_data_number;
}

uint64_t Writer::bytes_pushed() const
{
    return pushed;
}

string_view Reader::peek() const
{
    return buffer.front();
}

bool Reader::is_finished() const
{
    return writer_close && !buffer_cur_data_number;
}

bool Reader::has_error() const
{
    return error;
}

void Reader::pop(uint64_t len)
{
    if (buffer_cur_data_number < len || has_error())
    {
        return;
    }

    buffer_cur_data_number -= len;
    poped += len;

    std::string_view tmp_view = buffer.front();
    while (len > 0)
    {
        if (len < tmp_view.size())
        {
            buffer.front() = tmp_view.substr(len);
            break;
        }
        len -= tmp_view.size();
        buffer.pop();
        tmp_view = buffer.front();
    }
}

uint64_t Reader::bytes_buffered() const
{
    return buffer_cur_data_number;
}

uint64_t Reader::bytes_popped() const
{
    return poped;
}
