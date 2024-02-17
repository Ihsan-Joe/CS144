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
    std::queue<TCPSenderMessage> m_outstanding{}; // 保存发送过去但是还没收到ackno的报文
    std::queue<TCPSenderMessage> m_need_send{}; // 需要发送的数据
    uint16_t m_window{0}; // window大小
    uint64_t m_next_seq{0};
    uint64_t m_ackno{0};
    uint64_t m_consecutive_retransmissions{0}; // 连续发送了几次
    uint64_t m_refrence_time{0};
    bool m_SYN{false}; // 同步标志
    bool m_FIN{false};
    bool m_timer_start{false}; // 计数器
    uint64_t m_remaining_RTO_ms; // 计数器剩余时间
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
