#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message, Reassembler &reassembler, Writer &inbound_stream)
{
    // current Absolute sequence
    if (message.SYN)
    {
        m_start_to_accept = true;
        m_zero_point = message.seqno;
        m_checkpoint = 0;
    }
    // 还没设置ISN或者已经接收到FIN(收到FIN的时候会把Write关闭只有reassembler buffer没有数据的时候)
    if (!m_start_to_accept || inbound_stream.is_closed())
    {
        return;
    }
    uint64_t abs_seqno = message.seqno.unwrap(m_zero_point, m_checkpoint);
    // 加上message.SYN是因为它会占用一个byte
    uint64_t first_index = abs_seqno - 1 + message.SYN + message.FIN;
    reassembler.insert(first_index, std::move(message.payload), message.FIN, inbound_stream);
    send(inbound_stream);
}

TCPReceiverMessage TCPReceiver::send(const Writer &inbound_stream) const
{
    // (void)inbound_stream;
    // return{};
    TCPReceiverMessage message;
    uint64_t capacity = inbound_stream.available_capacity();
    message.window_size = (capacity <= UINT16_MAX) ? static_cast<uint16_t>(capacity) : UINT16_MAX;
    if (m_start_to_accept)
    {
        message.ackno = inbound_stream.is_closed() ? m_zero_point + 2 + inbound_stream.bytes_pushed()
                                                   : m_zero_point + 1 + inbound_stream.bytes_pushed();
    }
    return message;
}
