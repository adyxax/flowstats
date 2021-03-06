#include "Collector.hpp"
#include "DnsStatsCollector.hpp"
#include "MainTest.hpp"
#include "TcpStatsCollector.hpp"
#include "Utils.hpp"
#include <catch2/catch.hpp>

using namespace flowstats;

TEST_CASE("Tcp simple", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("Tcp aggregated stats are computed")
    {
        tester.readPcap("tcp_simple.pcap", "port 53");
        tester.readPcap("tcp_simple.pcap", "port 80", false);

        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("google.com", 0, 80);
        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto it = aggregatedMap.find(tcpKey);
        REQUIRE(it != aggregatedMap.end());

        auto aggregatedFlow = it->second;
        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);
        std::map<Field, std::string> srvValues;
        aggregatedFlow->fillValues(&srvValues, FROM_SERVER);

        CHECK(cltValues[Field::SYN] == "1");
        CHECK(cltValues[Field::FIN] == "1");
        CHECK(cltValues[Field::CLOSE] == "1");
        CHECK(cltValues[Field::ACTIVE_CONNECTIONS] == "0");
        CHECK(cltValues[Field::CONN] == "1");
        CHECK(cltValues[Field::CONN_RATE] == "1");
        CHECK(cltValues[Field::CT_P99] == "50ms");
        CHECK(cltValues[Field::MTU] == "140");
        CHECK(srvValues[Field::MTU] == "594");

        auto flows = tcpStatsCollector.getTcpFlow();
        CHECK(flows.size() == 1);
        CHECK(flows.begin()->second.getGap() == 0);

        AggregatedKey totalKey = AggregatedKey::aggregatedIpv4TcpKey("Total", 0, 0);
        std::map<Field, std::string> totalValues;
        aggregatedFlow->fillValues(&totalValues, FROM_CLIENT);
        CHECK(totalValues[Field::SYN] == "1");
    }
}

TEST_CASE("Tcp sort", "[tcp]")
{
    auto tester = Tester();
    auto& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("Fqdn sort works")
    {
        tester.readPcap("testcom.pcap");
        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap->size() == 4);

        auto flows = tcpStatsCollector.getAggregatedFlows();
        CHECK(flows[0]->getFqdn() == "news.ycombinator.com");
        CHECK(flows[1]->getFqdn() == "Unknown");
        CHECK(flows[2]->getFqdn() == "www.test.com");
        CHECK(flows[3]->getFqdn() == "www.test.com");

        tcpStatsCollector.setSortField(Field::FQDN, true);
        flows = tcpStatsCollector.getAggregatedFlows();
        CHECK(flows[0]->getFqdn() == "www.test.com");
        CHECK(flows[1]->getFqdn() == "www.test.com");
        CHECK(flows[2]->getFqdn() == "Unknown");
        CHECK(flows[3]->getFqdn() == "news.ycombinator.com");

        tcpStatsCollector.setSortField(Field::PORT, false);
        flows = tcpStatsCollector.getAggregatedFlows();
        CHECK(flows[0]->getSrvPort() == 80);
        CHECK(flows[1]->getSrvPort() == 443);
        CHECK(flows[2]->getSrvPort() == 443);
        CHECK(flows[3]->getSrvPort() == 443);
    }
}

TEST_CASE("https pcap", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("Active connections are correctly counted")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 443);
        tester.readPcap("https.pcap", "port 443", false);

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);
        auto aggregatedFlow = aggregatedMap[tcpKey];

        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);

        CHECK(cltValues[Field::SYN] == "1");
        CHECK(cltValues[Field::FIN] == "1");
        CHECK(cltValues[Field::CLOSE] == "1");
        CHECK(cltValues[Field::ACTIVE_CONNECTIONS] == "0");
        CHECK(cltValues[Field::CONN] == "1");
        CHECK(cltValues[Field::CT_P99] == "1ms");

        auto flows = tcpStatsCollector.getTcpFlow();
        REQUIRE(flows.size() == 1);
        CHECK(flows.begin()->second.getGap() == 0);
    }
}

TEST_CASE("Tcp reused port", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();
    SECTION("Reused connections")
    {
        tester.readPcap("reuse_port.pcap");

        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 1234);
        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);
        auto aggregatedFlow = aggregatedMap[tcpKey];

        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);

        CHECK(cltValues[Field::SYN] == "6");
        CHECK(cltValues[Field::FIN] == "5");
        CHECK(cltValues[Field::CLOSE] == "5");
        CHECK(cltValues[Field::ACTIVE_CONNECTIONS] == "0");
        CHECK(cltValues[Field::CONN] == "5");
        CHECK(cltValues[Field::CT_P99] == "0ms");
        CHECK(cltValues[Field::SRT_P99] == "0ms");

        auto flows = tcpStatsCollector.getTcpFlow();
        REQUIRE(flows.size() == 0);
    }
}

TEST_CASE("Ssl stream ack + srt", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();
    SECTION("Only payload from client starts SRT")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 443);
        tester.readPcap("ssl_ack_srt.pcap");

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto aggregatedFlow = aggregatedMap[tcpKey];

        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);

        REQUIRE(cltValues[Field::SRT] == "2");
        REQUIRE(cltValues[Field::SRT_P99] == "2ms");
        REQUIRE(cltValues[Field::SRT_P95] == "2ms");
    }
}

TEST_CASE("Ssl stream multiple srts", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();
    SECTION("Srts are correctly computed from single flow")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 443);
        tester.readPcap("tls_stream_extract.pcap");

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);
        auto aggregatedFlow = aggregatedMap[tcpKey];

        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);

        REQUIRE(cltValues[Field::SRT] == "11");
        REQUIRE(cltValues[Field::SRT_P99] == "9ms");
        REQUIRE(cltValues[Field::SRT_P95] == "3ms");
    }
}

TEST_CASE("Tcp double", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("Srts are correctly computed from multiple flows")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 3834);
        tester.readPcap("tcp_double.pcap");

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);
        auto aggregatedFlow = aggregatedMap[tcpKey];

        std::map<Field, std::string> cltValues;
        aggregatedFlow->fillValues(&cltValues, FROM_CLIENT);

        CHECK(cltValues[Field::SRT] == "2");
        CHECK(cltValues[Field::SRT_P99] == "499ms");
        CHECK(cltValues[Field::SRT_P95] == "499ms");
    }
}

TEST_CASE("Tcp 0 win", "[tcp]")
{
    auto tester = Tester(true);
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("0 wins are correctly counted")
    {
        tester.readPcap("0_win.pcap", "");

        auto ipFlows = tcpStatsCollector.getAggregatedMap();
        REQUIRE(ipFlows.size() == 1);

        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", Tins::IPv4Address("127.0.0.1"), 443);
        auto flow = ipFlows[tcpKey];

        std::map<Field, std::string> srvValues;
        flow->fillValues(&srvValues, FROM_SERVER);
        REQUIRE(flow != nullptr);

        CHECK(srvValues[Field::ZWIN] == "3");
        CHECK(srvValues[Field::RST] == "1");
    }
}

TEST_CASE("Tcp rst", "[tcp]")
{

    auto tester = Tester(true);
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    auto& ipToFqdn = tester.getIpToFqdn();
    Tins::IPv4Address ip("10.142.226.42");
    ipToFqdn.updateFqdn("whatever", { ip }, {});

    SECTION("Rst only close once")
    {
        tester.readPcap("rst_close.pcap", "");

        auto ipFlows = tcpStatsCollector.getAggregatedMap();
        REQUIRE(ipFlows.size() == 1);

        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("whatever", ip, 3834);
        auto flow = ipFlows[tcpKey];

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);

        CHECK(cltValues[Field::RST] == "2");
        CHECK(cltValues[Field::CLOSE] == "1");
    }
}

TEST_CASE("Inversed srt", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("We correctly detect the server")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 9000);
        tester.readPcap("inversed_srv.pcap", "");

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto aggregatedFlow = aggregatedMap[tcpKey];
        REQUIRE(aggregatedFlow != NULL);
        REQUIRE(aggregatedFlow->getSrvIp() == "10.8.109.46");
    }
}

TEST_CASE("Request size", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("We correctly detect the server")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 9000);
        tester.readPcap("6_sec_srt_extract.pcap", "");

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto flow = aggregatedMap[tcpKey];
        REQUIRE(flow != nullptr);

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);

        REQUIRE(cltValues[Field::DS_MAX] == "183.5 KB");
    }
}

TEST_CASE("Srv port detection", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("We correctly detect srv port")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 9000);
        tester.readPcap("port_detection.pcap", "", false);

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto flow = aggregatedMap[tcpKey];
        REQUIRE(flow != nullptr);

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);

        REQUIRE(cltValues[Field::ACTIVE_CONNECTIONS] == "3");
    }
}

TEST_CASE("Gap in capture", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();

    SECTION("We don't compute SRT on gap")
    {
        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 80);
        tester.readPcap("tcp_gap.pcap", "", false);

        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto flow = aggregatedMap[tcpKey];
        REQUIRE(flow != nullptr);

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);

        CHECK(cltValues[Field::SRT] == "1");
        CHECK(cltValues[Field::SRT_P99] == "26ms");

        auto flows = tcpStatsCollector.getTcpFlow();
        CHECK(flows.size() == 1);
        CHECK(flows.begin()->second.getGap() == 1);
    }
}

TEST_CASE("Mtu is correctly computed", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();
    tester.readPcap("tcp_mtu.pcap", "");

    SECTION("We correctly compute mtu")
    {
        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto tcpKey = AggregatedKey::aggregatedIpv4TcpKey("Unknown", 0, 80);
        auto flow = aggregatedMap[tcpKey];
        REQUIRE(flow != nullptr);

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);
        std::map<Field, std::string> srvValues;
        flow->fillValues(&srvValues, FROM_SERVER);

        CHECK(cltValues[Field::MTU] == "15346");
        CHECK(srvValues[Field::MTU] == "413");
    }
}

TEST_CASE("Ipv6", "[tcp]")
{
    auto tester = Tester();
    auto const& tcpStatsCollector = tester.getTcpStatsCollector();
    auto const& dnsStatsCollector = tester.getDnsStatsCollector();
    tester.readPcap("ipv6.pcap", "");

    SECTION("We build ipv6 to fqdn mapping")
    {
        auto aggregatedMap = dnsStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 2);
        auto dnsKey = AggregatedKey::aggregatedDnsKey("google.fr",
            Tins::DNS::AAAA, Transport::UDP);
        auto flow = aggregatedMap[dnsKey];
        REQUIRE(flow != nullptr);
    }

    SECTION("We correctly compute ipv6 tcp traffic")
    {
        auto aggregatedMap = tcpStatsCollector.getAggregatedMap();
        REQUIRE(aggregatedMap.size() == 1);

        auto tcpKey = AggregatedKey::aggregatedIpv6TcpKey("google.fr", {}, 80);
        auto flow = aggregatedMap[tcpKey];
        REQUIRE(flow != nullptr);

        std::map<Field, std::string> cltValues;
        flow->fillValues(&cltValues, FROM_CLIENT);
        std::map<Field, std::string> srvValues;
        flow->fillValues(&srvValues, FROM_SERVER);

        CHECK(cltValues[Field::BYTES] == "609 B");
        CHECK(srvValues[Field::BYTES] == "886 B");
    }
}
