#include "router.hh"
#include "address.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix, const uint8_t prefix_length, const optional<Address> next_hop,
                       const size_t interface_num)
{
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/"
         << static_cast<int>(prefix_length) << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)")
         << " on interface " << interface_num << "\n";
    m_route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

void Router::route()
{
    for (auto &items : interfaces_)
    {
        while (true)
        {
            auto dgram = items.maybe_receive();
            // 当前interface的queue没数据了
            if (!dgram.has_value())
            {
                break;
            }
            // ttl用完的话丢弃数据
            if (dgram->header.ttl <= 1)
            {
                continue;
            }
            --dgram->header.ttl;
            dgram->header.compute_checksum();
            // 选择最大匹配路由
            std::optional<route_struct> match_item;
            for (const auto &route : m_route_table)
            {
                uint8_t len = 32 - route.prefix_length;
                if ((static_cast<uint64_t>(route.route_prefix) >> len) ==
                    (static_cast<uint64_t>(dgram->header.dst) >> len))
                {
                    if (!match_item.has_value() ||
                        (match_item.has_value() && match_item->prefix_length < route.prefix_length))
                    {
                        match_item = route;
                        if (route.prefix_length == 32)
                        {
                            break;
                        }
                    }
                }
            }
            // 没找到匹配的选项，直接丢弃
            if (!match_item.has_value())
            {
                continue;
            }
            if (match_item->next_hop.has_value())
            {
                interface(match_item->interface_num).send_datagram(dgram.value(), match_item->next_hop.value());
                continue;
            }
            // 没有匹配的，直接发送
            interface(match_item->interface_num)
                .send_datagram(dgram.value(), Address::from_ipv4_numeric(dgram->header.dst));
        }
    }
}
