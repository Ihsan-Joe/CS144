#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender(uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn)
    : isn_(fixed_isn.value_or(Wrap32{random_device()()})), initial_RTO_ms_(initial_RTO_ms),
      m_remaining_RTO_ms(initial_RTO_ms)
{
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
    uint64_t count = 0;
    for (const auto &[ab_seqno, segment] : m_outstanding)
    {
        count += segment.sequence_length();
    }
    return count;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
    // Your code here.
    return m_consecutive_retransmissions;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
    m_timer_start = true;
    if (!m_start_send)
    {
        TCPSenderMessage msg;
        msg.seqno = Wrap32::wrap(m_ab_seqno, isn_);
        msg.SYN = m_syn = true;
        m_ab_seqno++;
        m_outstanding.push_front({m_ab_seqno, msg});
        m_start_send = true;
        return msg;
    }
    if (!m_need_send.empty())
    {
        m_outstanding.push_front({m_need_send.front().seqno.unwrap(isn_, m_ab_seqno), m_need_send.front()});
        m_ab_seqno += m_need_send.front().sequence_length();
        m_need_send.pop();
        return m_outstanding.front().second;
    }
    return {};
}

void TCPSender::push(Reader &outbound_stream)
{
    if (!outbound_stream.bytes_buffered())
    {
        return;
    }
    auto segment_size = outbound_stream.bytes_buffered();
    TCPSenderMessage msg;
    if (!m_syn)
    {
        m_ab_seqno = 0;
        msg.SYN = m_syn = true;
        segment_size++;
    }
    segment_size = outbound_stream.is_finished() ? segment_size + 1 : segment_size; // 加不加FIN

    uint64_t seqno = m_ab_seqno;
    uint64_t mod = std::min(std::min(TCPConfig::MAX_PAYLOAD_SIZE, static_cast<uint64_t>(m_window)), segment_size);
    
    if (mod == segment_size) // 如果字符串可以一次性发过去
    {
        read(outbound_stream, segment_size, msg.payload);
        outbound_stream.pop(segment_size);
        msg.seqno = Wrap32::wrap(seqno, isn_);
        msg.FIN = outbound_stream.is_finished();
        m_need_send.push(msg);
        return;
    }

    auto quotient = segment_size / mod;
    for (size_t i = 1; i <= quotient; i++)
    {
        msg.seqno = Wrap32::wrap(seqno, isn_);
        read(outbound_stream, mod, msg.payload);
        outbound_stream.pop(mod);
        m_need_send.push(msg);
        // m_outstanding.push_front({m_need_send.front().seqno.unwrap(isn_, m_ab_seqno), m_need_send.front()});
        seqno += msg.sequence_length();
    }
    auto remainder = segment_size % mod;
    if (remainder)
    {
        read(outbound_stream, remainder, msg.payload);
        outbound_stream.pop(remainder);
    }
    else
    {
        msg.payload = static_cast<string>("");
    }
    msg.seqno = Wrap32::wrap(seqno, isn_);
    msg.FIN = outbound_stream.is_finished();
    m_need_send.push(msg);
    // m_outstanding.push_front({m_need_send.front().seqno.unwrap(isn_, m_ab_seqno), m_need_send.front()});
}

TCPSenderMessage TCPSender::send_empty_message() const
{
    // Your code here.
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap(m_ab_seqno, isn_);
    return msg;
}

// 设置window大小，从m_outstanding中剔除msg.ackno的报文
void TCPSender::receive(const TCPReceiverMessage &msg)
{
    m_window = msg.window_size ? msg.window_size : 1;

    if (!msg.ackno.has_value())
    {
        return;
    }

    while (msg.ackno.value().unwrap(isn_, m_ab_seqno) >= m_outstanding.front().first)
    {
        m_outstanding.pop_front();
    }

    m_timer_start = !m_outstanding.empty();
}

void TCPSender::tick(const size_t ms_since_last_tick)
{
    if (!m_timer_start)
    {
        return;
    }
    m_remaining_RTO_ms -= ms_since_last_tick;
    if (m_remaining_RTO_ms <= 0)
    {
        m_need_send.push(m_outstanding.front().second);
        ++m_consecutive_retransmissions;
        m_remaining_RTO_ms = static_cast<uint64_t>(std::pow(2, m_consecutive_retransmissions)) * initial_RTO_ms_;
        return;
    }
}
