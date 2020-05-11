#include "AggregatedTcpFlow.hpp"
#include <algorithm>

namespace flowstats {

auto AggregatedTcpFlow::updateFlow(Tins::Packet const& packet,
    FlowId const& flowId,
    Tins::IP const& ip,
    Tins::TCP const& tcp) -> void
{
    auto direction = flowId.getDirection();
    if (tcp.has_flags(Tins::TCP::RST)) {
        rsts[direction]++;
    }

    if (tcp.window() == 0 && !tcp.has_flags(Tins::TCP::RST)) {
        zeroWins[direction]++;
    }

    if (tcp.has_flags(Tins::TCP::SYN | Tins::TCP::ACK)) {
        synacks[direction]++;
    } else if (tcp.has_flags(Tins::TCP::SYN)) {
        syns[direction]++;
    } else if (tcp.has_flags(Tins::TCP::FIN)) {
        fins[direction]++;
    }
    mtu[direction] = std::max(mtu[direction],
        packet.pdu()->advertised_size());
}

auto AggregatedTcpFlow::fillValues(std::map<Field, std::string>& values,
    Direction direction, int duration) const -> void
{
    Flow::fillValues(values, direction, duration);
    values[Field::SYN] = std::to_string(syns[direction]);
    values[Field::SYNACK] = std::to_string(synacks[direction]);
    values[Field::FIN] = std::to_string(fins[direction]);
    values[Field::ZWIN] = std::to_string(zeroWins[direction]);
    values[Field::RST] = std::to_string(rsts[direction]);
    values[Field::MTU] = std::to_string(mtu[direction]);

    if (direction == FROM_CLIENT) {
        values[Field::ACTIVE_CONNECTIONS] = std::to_string(activeConnections);
        values[Field::FAILED_CONNECTIONS] = std::to_string(failedConnections);
        values[Field::CLOSE] = std::to_string(totalCloses);
        values[Field::CONN] = prettyFormatNumber(totalConnections);
        values[Field::CT_P95] = connections.getPercentileStr(0.95);
        values[Field::CT_P99] = connections.getPercentileStr(0.99);

        values[Field::SRT] = prettyFormatNumber(totalSrts);
        values[Field::SRT_P95] = srts.getPercentileStr(0.95);
        values[Field::SRT_P99] = srts.getPercentileStr(0.99);
        values[Field::SRTMAX] = srts.getPercentileStr(1);

        values[Field::DS_P95] = prettyFormatBytes(requestSizes.getPercentile(0.95));
        values[Field::DS_P99] = prettyFormatBytes(requestSizes.getPercentile(0.99));
        values[Field::DSMAX] = prettyFormatBytes(requestSizes.getPercentile(1));

        values[Field::FQDN] = getFqdn();
        values[Field::IP] = getSrvIp().to_string();
        values[Field::PORT] = std::to_string(getSrvPort());
        if (duration) {
            values[Field::CONN_RATE] = std::to_string(connections.getCount() / duration);
            values[Field::CLOSE_RATE] = std::to_string(totalCloses / duration);
            values[Field::SRT_RATE] = prettyFormatNumber(numSrts);
        } else {
            values[Field::CONN_RATE] = std::to_string(numConnections);
            values[Field::CLOSE_RATE] = std::to_string(closes);
            values[Field::SRT_RATE] = prettyFormatNumber(numSrts);
        }
    }
}

auto AggregatedTcpFlow::resetFlow(bool resetTotal) -> void
{
    Flow::resetFlow(resetTotal);
    srts.reset();
    requestSizes.reset();
    connections.reset();
    numConnections = 0;
    numSrts = 0;
    closes = 0;

    if (resetTotal) {
        totalCloses = 0;
        totalConnections = 0;
        totalSrts = 0;
    }
}

auto AggregatedTcpFlow::failConnection() -> void
{
    failedConnections++;
};

auto AggregatedTcpFlow::mergePercentiles() -> void
{
    srts.merge();
    connections.merge();
    requestSizes.merge();
}

auto AggregatedTcpFlow::ongoingConnection() -> void
{
    activeConnections++;
};

auto AggregatedTcpFlow::openConnection(int connectionTime) -> void
{
    connections.addPoint(connectionTime);
    numConnections++;
    activeConnections++;
    totalConnections++;
};

auto AggregatedTcpFlow::addSrt(int srt, int dataSize) -> void
{
    srts.addPoint(srt);
    requestSizes.addPoint(dataSize);
    numSrts++;
    totalSrts++;
};

auto AggregatedTcpFlow::closeConnection() -> void
{
    closes++;
    totalCloses++;
    activeConnections--;
};

auto AggregatedTcpFlow::getMetrics(std::vector<std::string> lst) const -> void
{
    DogFood::Tags tags = DogFood::Tags({ { "fqdn", getFqdn() },
        { "ip", getSrvIp().to_string() },
        { "port", std::to_string(getSrvPort()) } });
    for (auto& i : srts.getPoints()) {
        lst.push_back(DogFood::Metric("flowstats.tcp.srt", i,
            DogFood::Histogram, 1, tags));
    }
    for (auto& i : connections.getPoints()) {
        lst.push_back(DogFood::Metric("flowstats.tcp.ct", i,
            DogFood::Histogram, 1, tags));
    }
    if (activeConnections) {
        lst.push_back(DogFood::Metric("flowstats.tcp.activeConnections", activeConnections, DogFood::Counter, 1, tags));
    }
    if (failedConnections) {
        lst.push_back(DogFood::Metric("flowstats.tcp.failedConnections", failedConnections,
            DogFood::Counter, 1, tags));
    }
}

} // namespace flowstats
