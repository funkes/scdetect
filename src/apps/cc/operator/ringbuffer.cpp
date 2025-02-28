#include "ringbuffer.h"

#include <seiscomp/core/datetime.h>
#include <seiscomp/core/recordsequence.h>

#include <exception>
#include <memory>

#include "../log.h"

namespace Seiscomp {
namespace detect {
namespace waveform_operator {

RingBufferOperator::RingBufferOperator(
    processing::WaveformProcessor *waveformProcessor)
    : _processor{waveformProcessor} {}

RingBufferOperator::RingBufferOperator(
    processing::WaveformProcessor *waveformProcessor, Core::TimeSpan bufferSize)
    : _bufferSize{bufferSize}, _processor{waveformProcessor} {}

RingBufferOperator::RingBufferOperator(
    processing::WaveformProcessor *waveformProcessor, Core::TimeSpan bufferSize,
    const std::vector<WaveformStreamID> &wfStreamIds)
    : _bufferSize{bufferSize}, _processor{waveformProcessor} {
  for (const auto &wfStreamId : wfStreamIds) {
    add(wfStreamId);
  }
}

processing::WaveformProcessor::Status RingBufferOperator::feed(
    const Record *record) {
  if (record->sampleCount() == 0)
    return processing::WaveformProcessor::Status::kWaitingForData;

  auto it{_streamConfigs.find(record->streamID())};
  if (it == _streamConfigs.end()) {
    return processing::WaveformProcessor::Status::kWaitingForData;
  }

  if (store(it->second.streamState, record)) {
    WaveformOperator::store(record);

    return processing::WaveformProcessor::Status::kInProgress;
  }

  return processing::WaveformProcessor::Status::kError;
}

void RingBufferOperator::reset() { _streamConfigs.clear(); }

void RingBufferOperator::add(WaveformStreamID wfStreamId) {
  add(wfStreamId, _bufferSize);
}

void RingBufferOperator::add(WaveformStreamID wfStreamId,
                             Core::TimeSpan bufferSize) {
  if (_streamConfigs.find(wfStreamId) != _streamConfigs.end()) {
    return;
  }

  if (!bufferSize) {
    bufferSize = _bufferSize;
  }

  _streamConfigs.emplace(
      wfStreamId, StreamConfig{processing::StreamState{},
                               std::make_shared<RingBuffer>(bufferSize)});
}

const std::shared_ptr<RingBuffer> &RingBufferOperator::get(
    WaveformStreamID wfStreamId) {
  return _streamConfigs.at(wfStreamId).streamBuffer;
}

bool RingBufferOperator::store(processing::StreamState &streamState,
                               const Record *record) {
  if (!record->data()) return false;

  DoubleArrayPtr data{
      dynamic_cast<DoubleArray *>(record->data()->copy(Array::DOUBLE))};

  if (streamState.lastRecord) {
    if (record == streamState.lastRecord) {
      return false;
    } else if (record->samplingFrequency() != streamState.samplingFrequency) {
      SCDETECT_LOG_WARNING_PROCESSOR(
          _processor,
          "%s: buffering operator: sampling frequency changed, resetting "
          "stream (sfreq_record != sfreq_stream): %f != %f",
          record->streamID().c_str(), record->samplingFrequency(),
          streamState.samplingFrequency);
      reset(streamState);
    } else if (!handleGap(streamState, record, data)) {
      return false;
    }

    streamState.dataTimeWindow.setEndTime(record->endTime());
  }

  if (!streamState.lastRecord) {
    try {
      setupStream(streamState, record);
    } catch (std::exception &e) {
      SCDETECT_LOG_WARNING_PROCESSOR(_processor,
                                     "%s: Failed to setup stream: %s",
                                     record->streamID().c_str(), e.what());
      return false;
    }

    auto &buffer{_streamConfigs.at(record->streamID()).streamBuffer};
    buffer->clear();
  }

  streamState.lastSample = (*data)[data->size() - 1];
  streamState.lastRecord = record;

  return fill(streamState, record, data);
}

bool RingBufferOperator::fill(processing::StreamState &streamState,
                              const Record *record, DoubleArrayPtr &data) {
  auto &buffer{_streamConfigs.at(record->streamID()).streamBuffer};
  // buffer record
  auto retval{buffer->feed(record)};
  if (!retval) {
    SCDETECT_LOG_WARNING_PROCESSOR(
        _processor,
        "%s: error while buffering data: start=%s, end=%s, samples=%d",
        record->streamID().c_str(), record->startTime().iso().c_str(),
        record->endTime().iso().c_str(), record->sampleCount());
  }
  return retval;
}

void RingBufferOperator::setupStream(processing::StreamState &streamState,
                                     const Record *record) {
  const auto &f{record->samplingFrequency()};
  streamState.samplingFrequency = f;

  if (gapInterpolation()) {
    setMinimumGapThreshold(streamState, record, _processor->id());
  }

  // update the received data timewindow
  streamState.dataTimeWindow = record->timeWindow();
}

void RingBufferOperator::reset(processing::StreamState &streamState) {
  streamState.lastRecord.reset();
}

}  // namespace waveform_operator
}  // namespace detect
}  // namespace Seiscomp
