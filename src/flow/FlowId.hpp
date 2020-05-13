#pragma once
#include "Utils.hpp"
#include "enum.h"
#include <arpa/inet.h>
#include <sstream>
#include <tins/ip.h>
#include <tins/tcp.h>
#include <tins/udp.h>

namespace flowstats {

using Port = uint16_t;
using IPv4 = uint32_t;
BETTER_ENUM(Transport, char, TCP, UDP);

struct FlowId {
    FlowId() = default;
    FlowId(FlowId const& flowId);
    FlowId(FlowId const&& flowId) noexcept;
    auto operator=(FlowId const& other) -> FlowId&;

    FlowId(std::array<uint16_t, 2> ports, std::array<IPv4, 2> pktIps,
        Transport transport);
    FlowId(const Tins::IP& ipv4Layer, const Tins::TCP& tcpLayer);
    FlowId(const Tins::IP& ipv4Layer, const Tins::UDP& udpLayer);
    explicit FlowId(const Tins::Packet& packet);

    [[nodiscard]] auto toString() const -> std::string;
    [[nodiscard]] auto getIps() const { return ips; };
    [[nodiscard]] auto getIp(uint8_t pos) const { return ips[pos]; };
    [[nodiscard]] auto getPorts() const { return ports; };
    [[nodiscard]] auto getPort(uint8_t pos) const { return ports[pos]; };
    [[nodiscard]] auto getTransport() const { return transport; };
    [[nodiscard]] auto getDirection() const { return direction; };
    [[nodiscard]] auto hash() const
    {
        return std::hash<Tins::IPv4Address>()(ips[0]) + std::hash<Tins::IPv4Address>()(ips[1]) + std::hash<uint16_t>()(ports[0]) + std::hash<uint16_t>()(ports[1]);
    };

private:
    std::array<Tins::IPv4Address, 2> ips = {};
    std::array<Port, 2> ports = {};
    Transport transport = Transport::TCP;
    Direction direction = FROM_CLIENT;
};
} // namespace flowstats

namespace std {

template <>
struct hash<flowstats::FlowId> {
    auto operator()(const flowstats::FlowId& flowId) const -> size_t
    {
        return flowId.hash();
        //+ std::hash<std::underlying_type<flowstats::Direction>::type>()(flowId.direction);
    }
};

} // namespace std
