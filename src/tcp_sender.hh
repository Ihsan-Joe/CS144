#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <queue>
#include <string>
#include <utility>

class TCPSender
{
    Wrap32 isn_;
    uint64_t initial_RTO_ms_;

private: 
    // 保存发送过去但是还没收到ackno的报文
    std::deque<std::pair<uint64_t, TCPSenderMessage>> m_outstanding{};

    // 需要发送的数据
    std::queue<TCPSenderMessage> m_need_send{};

    // 有没有发送第一个报文
    bool m_start_send{false};

    // window大小
    uint16_t m_window{0};

    // 连续发送了几次
    uint64_t m_consecutive_retransmissions{0};

    // 同步标志
    bool m_syn{false};

    // 绝对序列号
    uint64_t m_ab_seqno{0};

    // 计数器
    bool m_timer_start{false};
    // 计数器剩余时间
    uint64_t m_remaining_RTO_ms;

public:
    /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
    TCPSender(uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn);

    /* Push bytes from the outbound stream */
    void push(Reader &outbound_stream);

    /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
    std::optional<TCPSenderMessage> maybe_send();

    /* Generate an empty TCPSenderMessage */
    TCPSenderMessage send_empty_message() const;

    /* Receive an act on a TCPReceiverMessage from the peer's receiver */
    void receive(const TCPReceiverMessage &msg);

    /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
    void tick(uint64_t ms_since_last_tick);

    /* Accessors for use in testing */
    uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
    uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
