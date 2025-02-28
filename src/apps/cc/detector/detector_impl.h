#ifndef SCDETECT_APPS_CC_DETECTOR_DETECTORIMPL_H_
#define SCDETECT_APPS_CC_DETECTOR_DETECTORIMPL_H_

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/record.h>
#include <seiscomp/core/timewindow.h>
#include <seiscomp/datamodel/origin.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../exception.h"
#include "../processing/processor.h"
#include "../processing/waveform_operator.h"
#include "arrival.h"
#include "detail.h"
#include "linker.h"
#include "linker/association.h"
#include "template_waveform_processor.h"

namespace Seiscomp {
namespace detect {
namespace detector {

namespace detail {
struct SensorLocation {
  double latitude;
  double longitude;

  // The unique station's identifier (usually the station's public id)
  std::string stationId;
};

struct ProcessorState {
  ProcessorState(ProcessorState &&other) = default;
  ProcessorState &operator=(ProcessorState &&other) = default;
  ~ProcessorState() = default;

  // The sensor location w.r.t. the template waveform `processor`
  SensorLocation sensorLocation;
  // The time window fed
  // XXX(damb): The data time window fed might be different from the data
  // time window processed (e.g. due to the usage of certain waveform
  // operators). Therefore, keep track of the time window fed, too.
  Core::TimeWindow dataTimeWindowFed;
  // The template waveform reference time w.r.t. the template waveform
  // `processor`
  Core::Time templateWaveformReferenceTime;

  std::unique_ptr<TemplateWaveformProcessor> processor;
};

using ProcessorStatesType = std::unordered_map<ProcessorIdType, ProcessorState>;

struct TemplateWaveformProcessorIterator
    : public ProcessorStatesType::const_iterator {
  explicit TemplateWaveformProcessorIterator(
      ProcessorStatesType::const_iterator it);
  const TemplateWaveformProcessor &operator*() const;
};

}  // namespace detail

class DetectorImpl : public detect::processing::Processor {
 public:
  explicit DetectorImpl(const DataModel::OriginCPtr &origin);

  class BaseException : public Processor::BaseException {
   public:
    using Processor::BaseException::BaseException;
    BaseException();
  };

  class ProcessingError : public BaseException {
   public:
    using BaseException::BaseException;
    ProcessingError();
  };

  class TemplateMatchingError : public ProcessingError {
   public:
    using ProcessingError::ProcessingError;
    TemplateMatchingError();
  };

  using SensorLocation = detail::SensorLocation;

  struct Result {
    Core::Time originTime;
    double score;

    struct TemplateResult {
      Arrival arrival;
      SensorLocation sensorLocation;

      Core::Time templateWaveformStartTime;
      Core::Time templateWaveformEndTime;
      // The template waveform reference time
      Core::Time templateWaveformReferenceTime;

      // The unique identifier of the underlying processor
      std::string processorId;
    };

    using WaveformStreamId = std::string;
    using TemplateResults =
        std::unordered_multimap<WaveformStreamId, TemplateResult>;
    TemplateResults templateResults;

    size_t numChannelsUsed;
    size_t numChannelsAssociated;
    size_t numStationsUsed;
    size_t numStationsAssociated;
  };

  void setGapInterpolation(bool gapInterpolation);
  void setGapThreshold(const Core::TimeSpan &duration);
  void setGapTolerance(const Core::TimeSpan &duration);

  // Returns the overall time window processed
  const Core::TimeWindow &processed() const;
  // Returns `true` if the detector is currently triggered, else `false`
  bool triggered() const;
  // Enables trigger duration facilities with `duration`
  void enableTrigger(const Core::TimeSpan &duration);
  // Disables trigger duration facilities
  void disableTrigger();
  // Set the trigger thresholds
  void setTriggerThresholds(double triggerOn, double triggerOff = 1);
  // Set the maximum arrival offset threshold
  void setArrivalOffsetThreshold(const boost::optional<Core::TimeSpan> &thres);
  // Returns the arrival offset threshold configured
  boost::optional<Core::TimeSpan> arrivalOffsetThreshold() const;
  // Configures the detector with a minimum number of arrivals required to
  // declare an event as a detection
  void setMinArrivals(const boost::optional<size_t> &n);
  // Returns the minimum number of arrivals required in order to declare an
  // event as a detection
  boost::optional<size_t> minArrivals() const;
  // Sets the merging strategy applied while linking
  void setMergingStrategy(Linker::MergingStrategy mergingStrategy);
  // Sets the maximum data latency w.r.t. `NOW`. If configured with
  // `boost::none` latency is not taken into account and thus not validated
  void setMaxLatency(const boost::optional<Core::TimeSpan> &latency);
  // Returns the maximum allowed data latency configured
  boost::optional<Core::TimeSpan> maxLatency() const;
  // Returns the number of registered template processors
  size_t processorCount() const;

  // Returns the template waveform processor identified by `processorId`
  //
  // - returns `nullptr` if there is no processor with `processorId` registered
  const TemplateWaveformProcessor *processor(
      const std::string &processorId) const;

  using const_iterator = detail::TemplateWaveformProcessorIterator;
  const_iterator begin() const { return const_iterator{_processors.begin()}; }
  const_iterator end() const { return const_iterator{_processors.end()}; }
  const_iterator cbegin() const { return const_iterator{_processors.cbegin()}; }
  const_iterator cend() const { return const_iterator{_processors.cend()}; }

  // Register the template waveform processor `proc`. Records are identified by
  // the waveform stream identifier `waveformStreamId`. `proc` is registered
  // together with the template arrival `arrival` and the sensor location
  // `loc`.
  void add(std::unique_ptr<TemplateWaveformProcessor> proc,
           const std::string &waveformStreamId, const Arrival &arrival,
           const DetectorImpl::SensorLocation &loc,
           const boost::optional<double> &mergingThreshold);
  // Removes the processors processing streams identified by `waveformStreamId`
  void remove(const std::string &waveformStreamId);

  // Feeds `record` to the detector
  void feed(const Record *record);
  // Reset the detector
  void reset();
  // Flushes pending detections
  void flush();

  using PublishResultCallback = std::function<void(const Result &result)>;
  void setResultCallback(const PublishResultCallback &callback);

 protected:
  // Process data with underlying template processors
  bool process(const Record *record);
  // Returns `true` if `record` has an acceptable latency, else `false`
  bool hasAcceptableLatency(const Record *record);

  void processResultQueue();

  void processLinkerResult(const linker::Association &result);

  void disableProcessorsNotContributing(const linker::Association &result);

  std::string triggerProcessorId(const linker::Association &result);

  // Prepare detection
  void prepareResult(const linker::Association &linkerResult,
                     Result &result) const;
  // Emit the detection result
  void emitResult(const Result &result);

  // Reset the processor's processing facilities
  void resetProcessing();
  // Reset the trigger
  void resetTrigger();
  // Reset the currently enabled processors
  void resetProcessors();

 private:
  // Callback storing results from `TemplateWaveformProcessor`
  void storeTemplateResult(
      const TemplateWaveformProcessor *processor, const Record *record,
      std::unique_ptr<const TemplateWaveformProcessor::MatchResult> result);

  // Callback storing results from the linker
  void storeLinkerResult(const linker::Association &linkerResult);

  struct TemplateResult {
    linker::Association::TemplateResult result;
    detail::ProcessorIdType processorId;
  };
  static std::vector<TemplateResult> sortByArrivalTime(
      const linker::Association &linkerResult);

  // Safety margin for linker on hold duration
  static const Core::TimeSpan _linkerSafetyMargin;

  detail::ProcessorStatesType _processors;
  using ProcessorIdx =
      std::unordered_multimap<std::string, detail::ProcessorIdType>;
  ProcessorIdx _processorIdx;

  // The overall time window processed
  Core::TimeWindow _processed;

  // The current linker result
  boost::optional<linker::Association> _currentResult;

  // The result callback function
  boost::optional<PublishResultCallback> _resultCallback;

  boost::optional<double> _thresTriggerOn;
  boost::optional<double> _thresTriggerOff;
  boost::optional<Core::TimeSpan> _triggerDuration;
  boost::optional<Core::Time> _triggerEnd;
  // The processor used for triggering (i.e. the current reference processor)
  boost::optional<std::string> _triggerProcId;

  // The linker required for associating arrivals
  Linker _linker;
  using ResultQueue = std::deque<linker::Association>;
  ResultQueue _resultQueue;

  // Maximum data latency
  boost::optional<Core::TimeSpan> _maxLatency;
  // The configured processing chunk size
  boost::optional<Core::TimeSpan> _chunkSize;

  DataModel::OriginCPtr _origin;
};

}  // namespace detector
}  // namespace detect
}  // namespace Seiscomp

#endif  // SCDETECT_APPS_CC_DETECTOR_DETECTORIMPL_H_
