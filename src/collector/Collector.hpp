#pragma once

#include "AggregatedFlow.hpp"
#include "CollectorOutput.hpp"
#include "Configuration.hpp"
#include "DogFood.hpp"
#include "Flow.hpp"
#include "FlowFormatter.hpp"
#include "Utils.hpp"
#include <fmt/format.h>
#include <map>
#include <mutex>
#include <sys/time.h>

namespace flowstats {

enum CollectorProtocol {
    TCP,
    DNS,
    SSL,
};
auto collectorProtocolToString(CollectorProtocol proto) -> std::string;

class Collector {
public:
    Collector(FlowstatsConfiguration const& conf, DisplayConfiguration const& displayConf)
        : conf(conf)
        , displayConf(displayConf) {};
    virtual ~Collector();

    virtual auto processPacket(Tins::Packet const& pdu) -> void = 0;
    virtual auto advanceTick(timeval now) -> void {};
    auto resetMetrics() -> void;

    auto getStatsdMetrics() const -> std::vector<std::string>;
    auto sendMetrics() -> void;
    auto mergePercentiles() -> void;

    virtual auto toString() const -> std::string = 0;
    virtual auto getProtocol() const -> CollectorProtocol = 0;

    auto getDisplayPairs() const { return displayPairs; };
    auto getSortFields() const { return sortFields; };
    auto getSelectedSortField() const { return selectedSortField; };
    virtual auto getSortFun(Field field) const -> Flow::sortFlowFun;
    auto outputStatus(int duration) -> CollectorOutput;

    auto updateDisplayType(int displayIndex) -> void { flowFormatter.setDisplayValues(displayPairs[displayIndex].second); };
    auto updateSort(int sortIndex) -> void { selectedSortField = sortFields.at(sortIndex); };
    [[nodiscard]] auto const& getAggregatedMap() const { return aggregatedMap; }
    [[nodiscard]] auto getAggregatedMap() { return &aggregatedMap; }

protected:
    [[nodiscard]] auto getAggregatedFlows() const -> std::vector<Flow const*>;
    auto fillOutputs(std::vector<Flow const*> const& aggregatedFlows,
        std::vector<std::string>* keyLines,
        std::vector<std::string>* valueLines, int duration);

    auto outputFlow(Flow const* flow,
        std::vector<std::string>* keyLines,
        std::vector<std::string>* valueLines,
        int duration, int position) const -> void;

    auto getDataMutex() -> std::mutex* { return &dataMutex; };
    auto getFlowFormatter() -> FlowFormatter& { return flowFormatter; };
    auto getFlowFormatter() const -> FlowFormatter const& { return flowFormatter; };
    [[nodiscard]] auto getDisplayConf() const -> DisplayConfiguration const& { return displayConf; };
    [[nodiscard]] auto getFlowstatsConfiguration() const -> FlowstatsConfiguration const& { return conf; };

    auto setDisplayKeys(std::vector<Field> const& keys) -> void { getFlowFormatter().setDisplayKeys(keys); };
    auto setDisplayPairs(std::vector<DisplayPair> pairs) -> void { displayPairs = std::move(pairs); };
    auto fillSortFields() -> void;
    auto setTotalFlow(Flow* flow) -> void { totalFlow = flow; };

private:
    std::mutex dataMutex;
    FlowFormatter flowFormatter;
    FlowstatsConfiguration const& conf;
    DisplayConfiguration const& displayConf;
    Flow* totalFlow = nullptr;
    std::vector<DisplayPair> displayPairs;
    std::vector<Field> sortFields;
    Field selectedSortField = Field::FQDN;
    std::map<AggregatedKey, Flow*> aggregatedMap;
};
} // namespace flowstats
