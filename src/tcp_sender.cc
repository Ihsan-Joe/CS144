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
    return m_next_seq - m_ackno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
    // Your code here.
    return m_consecutive_retransmissions;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
    if (m_SYN && !m_need_send.empty())
    {
        if (!m_timer_start)
        {
            m_timer_start = true;
            m_remaining_RTO_ms = m_refrence_time = initial_RTO_ms_;
        }
        auto msg = m_need_send.front();
        m_outstanding.push(msg);
        m_need_send.pop();
        return msg;
    }
    return {};
}

void TCPSender::push(Reader &outbound_stream)
{
    if (m_FIN)
    {
        return;
    }
    uint64_t remaining_window_size = std::max(m_window, static_cast<uint16_t>(1)) - (m_next_seq - m_ackno);
    while (remaining_window_size && !m_FIN)
    {
        TCPSenderMessage msg;
        msg.seqno = Wrap32::wrap(m_next_seq, isn_);
        read(outbound_stream, std::min(remaining_window_size, TCPConfig::MAX_PAYLOAD_SIZE), msg.payload);
        remaining_window_size -= msg.payload.size();
        if (!m_SYN)
        {
            m_SYN = msg.SYN = true;
        }
        if (!m_FIN && outbound_stream.is_finished() && remaining_window_size)
        {
            m_FIN = msg.FIN = true;
        }
        if (!msg.sequence_length())
        {
            return;
        }
        m_need_send.push(msg);
        m_next_seq += msg.sequence_length();
    }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap(m_next_seq, isn_);
    return msg;
}

// 设置window大小，从m_outstanding中剔除msg.ackno的报文
void TCPSender::receive(const TCPReceiverMessage &msg)
{
    m_consecutive_retransmissions = 0;
    if (!msg.ackno.has_value())
    {
        return;
    }
    auto recv_ackno = msg.ackno->unwrap(isn_, m_ackno);
    if (recv_ackno < m_ackno || recv_ackno > m_next_seq)
    {
        return;
    }
    while (!m_outstanding.empty() &&
           recv_ackno >= (m_outstanding.front().seqno.unwrap(isn_, m_ackno) + m_outstanding.front().sequence_length()))
    {
        m_outstanding.pop();
    }
    if (!m_outstanding.empty() && recv_ackno > m_ackno)
    {
        m_refrence_time = m_remaining_RTO_ms = initial_RTO_ms_;
    }
    else
    {
        m_timer_start = false;
    }
    m_window = msg.window_size;
    m_ackno = recv_ackno;
}

void TCPSender::tick(const size_t ms_since_last_tick)
{
    m_remaining_RTO_ms = (m_remaining_RTO_ms > ms_since_last_tick) ? m_remaining_RTO_ms - ms_since_last_tick : 0;
    m_timer_start = !m_outstanding.empty();
    if (!m_timer_start || m_remaining_RTO_ms > 0)
    {
        return;
    }
    m_need_send.push(m_outstanding.front());
    ++m_consecutive_retransmissions;
    if (!m_ackno || m_window)
    {
        m_refrence_time = m_refrence_time * 2;
        m_remaining_RTO_ms = m_refrence_time;
        return;
    }
    m_remaining_RTO_ms = m_refrence_time = initial_RTO_ms_;
}