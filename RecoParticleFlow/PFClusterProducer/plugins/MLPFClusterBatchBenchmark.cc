#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "PhysicsTools/ONNXRuntime/interface/ONNXRuntime.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>

class MLPFClusterBatchBenchmark : public edm::one::EDAnalyzer<> {
public:
  explicit MLPFClusterBatchBenchmark(edm::ParameterSet const& config)
      : inputNames_(config.getParameter<std::vector<std::string>>("inputNames")),
        groupsPerEvent_(config.getParameter<unsigned int>("groupsPerEvent")),
        syntheticEvents_(config.getParameter<unsigned int>("syntheticEvents")),
        batchEvents_(config.getParameter<std::vector<unsigned int>>("batchEvents")),
        warmup_(config.getParameter<unsigned int>("warmup")),
        repetitions_(config.getParameter<unsigned int>("repetitions")) {
    auto sessionOptions = cms::Ort::ONNXRuntime::defaultSessionOptions(cms::Ort::Backend::cuda);
    onnx_ = std::make_unique<cms::Ort::ONNXRuntime>(
        config.getParameter<edm::FileInPath>("modelPath").fullPath(), &sessionOptions);
  }

  void analyze(edm::Event const&, edm::EventSetup const&) override {
    if (hasRun_)
      return;
    hasRun_ = true;

    for (auto const eventsPerBatch : batchEvents_) {
      if (eventsPerBatch == 0 || eventsPerBatch > syntheticEvents_) {
        edm::LogWarning("MLPFClusterBatchBenchmark")
            << "Skipping batchEvents=" << eventsPerBatch << " (syntheticEvents=" << syntheticEvents_ << ")";
        continue;
      }

      auto const numberOfBatches = syntheticEvents_ / eventsPerBatch;
      auto const groups = eventsPerBatch * groupsPerEvent_;
      auto const inputShapes = shapes(groups);
      auto inputs = makeInputs(groups);

      for (unsigned int i = 0; i < warmup_; ++i)
        onnx_->run(inputNames_, inputs, inputShapes, {}, groups);

      std::vector<double> milliseconds;
      milliseconds.reserve(repetitions_ * numberOfBatches);
      for (unsigned int repetition = 0; repetition < repetitions_; ++repetition) {
        for (unsigned int batch = 0; batch < numberOfBatches; ++batch) {
          auto const start = std::chrono::steady_clock::now();
          onnx_->run(inputNames_, inputs, inputShapes, {}, groups);
          auto const stop = std::chrono::steady_clock::now();
          milliseconds.emplace_back(std::chrono::duration<double, std::milli>(stop - start).count());
        }
      }

      std::sort(milliseconds.begin(), milliseconds.end());
      auto const mean = std::accumulate(milliseconds.begin(), milliseconds.end(), 0.) / milliseconds.size();
      auto const p95 = milliseconds[static_cast<size_t>(0.95 * (milliseconds.size() - 1))];
      auto const eventsPerSecond = 1000. * eventsPerBatch / mean;
      auto const groupsPerSecond = 1000. * groups / mean;
      auto const msPerEvent = mean / eventsPerBatch;
      edm::LogPrint("MLPFClusterBatchBenchmark")
          << "batchEvents=" << eventsPerBatch << " graphs=" << groups << " mean=" << mean << " ms p95=" << p95
          << " ms events/s=" << eventsPerSecond << " graphs/s=" << groupsPerSecond << " ms/event=" << msPerEvent;
    }
  }

private:
  std::vector<std::vector<int64_t>> shapes(unsigned int groups) const {
    return {{groups, 20, 7, 7}, {groups, 20, 2}, {groups, 20}, {groups, 20, 7, 7}};
  }

  cms::Ort::FloatArrays makeInputs(unsigned int groups) const {
    std::mt19937 generator(12345);
    std::uniform_real_distribution<float> distribution(0.f, 1.f);
    cms::Ort::FloatArrays inputs;
    for (auto const size : {groups * 20 * 7 * 7, groups * 20 * 2, groups * 20, groups * 20 * 7 * 7}) {
      inputs.emplace_back(size);
      for (auto& value : inputs.back())
        value = distribution(generator);
    }
    return inputs;
  }

  std::vector<std::string> inputNames_;
  unsigned int groupsPerEvent_;
  unsigned int syntheticEvents_;
  std::vector<unsigned int> batchEvents_;
  unsigned int warmup_;
  unsigned int repetitions_;
  bool hasRun_ = false;
  std::unique_ptr<cms::Ort::ONNXRuntime> onnx_;
};

DEFINE_FWK_MODULE(MLPFClusterBatchBenchmark);
