#include "pot.h"

#include <boost/none.hpp>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../util/floating_point_comparison.h"
#include "../../util/util.h"
#include "exception.h"

namespace Seiscomp {
namespace detect {
namespace detector {
namespace linker {

const double POT::tableDefault{std::numeric_limits<double>::max()};
const double POT::tolerance{1e-6};

POT::POT(const std::vector<POT::Entry> &entries) {
  auto sorted{entries};
  // sort by processor id
  std::sort(std::begin(sorted), std::end(sorted),
            [](const Entry &lhs, const Entry &rhs) {
              return lhs.templateWaveformProcessorId <
                     rhs.templateWaveformProcessorId;
            });

  for (auto i{std::begin(sorted)}; i != std::end(sorted); ++i) {
    std::vector<double> offsets;
    for (auto j{std::begin(sorted)}; j != std::end(sorted); ++j) {
      if (i->arrivalTime && j->arrivalTime) {
        offsets.push_back(
            std::abs(static_cast<double>(i->arrivalTime - j->arrivalTime)));
      } else {
        offsets.push_back(tableDefault);
      }
    }
    _offsets.push_back(offsets);

    _processorIdxMap.emplace(
        i->templateWaveformProcessorId,
        Item{static_cast<size_type>(std::distance(std::begin(sorted), i)),
             i->enabled});
  }
}

boost::optional<double> POT::operator()(
    const std::string &lhsProcessorId,
    const std::string &rhsProcessorId) const {
  try {
    auto &lhs{_processorIdxMap.at(lhsProcessorId)};
    auto &rhs{_processorIdxMap.at(rhsProcessorId)};
    if (lhs.enabled && rhs.enabled) {
      if (tableDefault == _offsets[lhs.idx][rhs.idx]) {
        return boost::none;
      }
      return _offsets[lhs.idx][rhs.idx];
    }
  } catch (std::out_of_range &) {
  }

  return boost::none;
}

bool POT::enabled(const std::string &processorId) const {
  try {
    return _processorIdxMap.at(processorId).enabled;
  } catch (std::out_of_range &) {
    return false;
  }
}

void POT::enable() { setEnableAll(true); }
void POT::enable(const std::string &processorId) {
  setEnable(processorId, true);
}

bool POT::disabled(const std::string &processorId) const {
  return !enabled(processorId);
}

void POT::disable() { setEnableAll(false); }

void POT::disable(const std::string &processorId) {
  setEnable(processorId, false);
}

std::vector<std::string> POT::processorIds() const {
  return util::map_keys(_processorIdxMap);
}

bool POT::validateEnabledOffsets(const POT &other,
                                 const Core::TimeSpan &thres) {
  if (size() != other.size()) {
    throw BaseException{"invalid POT size"};
  }

  if (util::map_keys(_processorIdxMap) !=
      util::map_keys(other._processorIdxMap)) {
    throw BaseException{"processor id mismatch"};
  }

  // create mask with common enabled processors
  std::vector<detail::ProcessorIdType> enabledProcessors;
  for (const auto &p : _processorIdxMap) {
    if (p.second.enabled && other.enabled(p.first)) {
      enabledProcessors.push_back(p.first);
    }
  }
  auto mask{createMask(enabledProcessors)};

  for (size_type i{0}; i < size(); ++i) {
    for (size_type j{0}; j < size(); ++j) {
      if (mask[i][j] && validEntry(_offsets[i][j]) &&
          validEntry(other._offsets[i][j]) &&
          util::greaterThan(std::abs(_offsets[i][j] - other._offsets[i][j]),
                            static_cast<double>(thres), tolerance)) {
        return false;
      }
    }
  }

  return true;
}

bool POT::validateEnabledOffsets(const std::string &processorId,
                                 const std::vector<double> &otherOffsets,
                                 const std::vector<bool> &otherMask,
                                 const Core::TimeSpan &thres) {
  if (otherOffsets.size() != size() || otherMask.size() != size()) {
    throw BaseException{"invalid sizes"};
  }

  auto it{_processorIdxMap.find(processorId)};
  if (it == _processorIdxMap.end()) {
    throw BaseException{"unknown processor id: " + processorId};
  }

  if (!enabled(processorId)) {
    return false;
  }

  // create mask with common enabled processors
  std::vector<bool> common_mask(size(), false);
  size_type i{0};
  for (const auto &p : _processorIdxMap) {
    if (p.second.enabled && otherMask[i]) {
      common_mask[i++] = true;
    }
  }

  const auto &offsets{_offsets[std::distance(_processorIdxMap.begin(), it)]};

  for (size_type i{0}; i < size(); ++i) {
    if (common_mask[i] && validEntry(otherOffsets[i]) &&
        validEntry(offsets[i]) &&
        util::greaterThan(std::abs(offsets[i] - otherOffsets[i]),
                          static_cast<double>(thres), tolerance)) {
      return false;
    }
  }
  return true;
}

bool POT::validEntry(double e) const { return e != tableDefault; }

void POT::setEnable(const std::string &processorId, bool enable) {
  try {
    _processorIdxMap.at(processorId).enabled = enable;
  } catch (std::out_of_range &) {
  }
}

void POT::setEnableAll(bool enable) {
  for (auto &p : _processorIdxMap) {
    p.second.enabled = enable;
  }
}

POT::Matrix<bool> POT::createMask(
    const std::vector<detail::ProcessorIdType> &enabledProcessors) {
  auto ret{Matrix<bool>(size(), std::vector<bool>(size(), false))};
  for (const auto &lhs : enabledProcessors) {
    for (const auto &rhs : enabledProcessors) {
      ret[_processorIdxMap[lhs].idx][_processorIdxMap[rhs].idx] = true;
    }
  }
  return ret;
}

}  // namespace linker
}  // namespace detector
}  // namespace detect
}  // namespace Seiscomp
