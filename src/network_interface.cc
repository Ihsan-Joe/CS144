#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "parser.hh"
#include <cstdint>
#include <queue>
#include <unordered_map>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : ethernet_address_(ethernet_address), ip_address_(ip_address)
{
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address_) << " and IP address "
         << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop)
{
    EthernetFrame frame;
    Serializer ser;
    uint32_t ip = next_hop.ipv4_numeric();
    // 这两个事固定的

    // 如果映射不存在那么发送一个广播，把当前报文存到等待回复的队列中
    // 可以查询到ip->mac的映射,且超时(30s)
    if (m_ip_mac_map.find(ip) != m_ip_mac_map.end() && m_ip_mac_map[ip].first >= m_time_point)
    {
        frame.header.type = EthernetHeader::TYPE_IPv4;
        frame.header.src = ethernet_address_;
        frame.header.dst = m_ip_mac_map[ip].second;
        dgram.serialize(ser);
    }
    // 未能查询到ip->mac映射
    else
    {
        // 存入ARP请求等待队列, 直接以EthernetFrame格式保存，之后收到MAC之后直接发送
        EthernetFrame tmp_frame;
        Serializer tmp_ser;
        tmp_frame.header.type = EthernetHeader::TYPE_IPv4;
        tmp_frame.header.src = ethernet_address_;
        tmp_frame.header.dst = {0, 0, 0, 0, 0, 0};
        dgram.serialize(tmp_ser);
        tmp_frame.payload = tmp_ser.output();
        m_ip_arpfram_map[ip].push(tmp_frame);

        // 上一个一样的ip发送的ARP还没收到回应,没超过5秒
        if (m_arp_req.find(ip) != m_arp_req.end() && m_arp_req[ip] >= m_time_point)
        {
            return;
        }
        // 如果超过5秒需要重新发送ARP请求
        // 设置时间为当前时间点 + 5s
        m_arp_req[ip] = m_time_point + 5000;
        // arp请求添加到队列中
        ARPMessage arp_msg;
        frame.header.type = EthernetHeader::TYPE_ARP;
        frame.header.src = ethernet_address_;
        frame.header.dst = ETHERNET_BROADCAST;
        arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
        arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
        arp_msg.sender_ethernet_address = ethernet_address_;
        arp_msg.target_ip_address = ip;
        arp_msg.target_ethernet_address = EthernetAddress{0, 0, 0, 0, 0, 0};
        arp_msg.serialize(ser);
    }
    frame.payload = ser.output();
    m_need_send_frame.push(frame);
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    // invalid situation
    if ((frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) ||
        (frame.header.type != EthernetHeader::TYPE_ARP && frame.header.type != EthernetHeader::TYPE_IPv4))
    {
        return {};
    }
    // type is arp
    if (frame.header.type == EthernetHeader::TYPE_ARP)
    {
        ARPMessage arp_msg_recv;
        // 解析失败或者arp地址不对，直接返回
        if (!parse(arp_msg_recv, frame.payload) || arp_msg_recv.target_ip_address != ip_address_.ipv4_numeric())
        {
            return {};
        }
        auto recv_ip = arp_msg_recv.sender_ip_address;
        m_ip_mac_map[recv_ip].first = m_time_point + 30000;
        m_ip_mac_map[recv_ip].second = arp_msg_recv.sender_ethernet_address;
        if (arp_msg_recv.opcode == ARPMessage::OPCODE_REQUEST)
        {
            EthernetFrame ead;
            Serializer ser;
            ARPMessage arp_msg_send;
            ead.header.type = EthernetHeader::TYPE_ARP;
            ead.header.src = ethernet_address_;
            ead.header.dst = arp_msg_recv.sender_ethernet_address;
            arp_msg_send.opcode = ARPMessage::OPCODE_REPLY;
            arp_msg_send.sender_ip_address = ip_address_.ipv4_numeric();
            arp_msg_send.sender_ethernet_address = ethernet_address_;
            arp_msg_send.target_ip_address = recv_ip;
            arp_msg_send.target_ethernet_address = frame.header.src;
            arp_msg_send.serialize(ser);
            ead.payload = ser.output();
            m_need_send_frame.push(ead);
        }

        // 等待ARP回应的报文如果匹配,那么转存到need_send_frame中,然后从两外两个表中删除
        if (m_arp_req.find(recv_ip) != m_arp_req.end())
        {
            while (!m_ip_arpfram_map[recv_ip].empty())
            {
                m_ip_arpfram_map[recv_ip].front().header.dst = frame.header.src;
                m_need_send_frame.push(m_ip_arpfram_map[recv_ip].front());
                m_ip_arpfram_map[recv_ip].pop();
            }
            m_ip_arpfram_map.erase(recv_ip);
            m_arp_req.erase(recv_ip);
        }
        return{};
    }
    // type is ipv4
    InternetDatagram dgram;
    if (!parse(dgram, frame.payload))
    {
        return {};
    }
    return dgram;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    m_time_point += static_cast<uint64_t>(ms_since_last_tick);
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
    if (m_need_send_frame.empty())
    {
        return {};
    }
    auto send_frame = m_need_send_frame.front();
    m_need_send_frame.pop();
    return send_frame;
}
