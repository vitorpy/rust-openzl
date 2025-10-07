// Copyright (c) Meta Platforms, Inc. and affiliates.

#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

// Folly doesn't play nice with -Wsign-conversion and -Wfloat-equal
#if defined(__clang__) && __clang_major__ >= 5
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wsign-conversion"
#    pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include <folly/Synchronized.h>
#include <folly/dynamic.h>

#if defined(__clang__) && __clang_major__ >= 5
#    pragma clang diagnostic pop
#endif

#include "openzl/zl_data.h"
#include "tools/gbt_predictor/zstrong_gbt_predictor.h"
#include "tools/zstrong_cpp.h"

namespace zstrong {
namespace ml {

using FeatureMap = std::unordered_map<std::string, double>;
using TargetsMap =
        std::unordered_map<std::string, std::unordered_map<std::string, float>>;

/// A base class that defines the interface of any ML based
/// model used by Zstrong's selectors.
class MLModel {
   public:
    virtual Label predict(
            const ZL_Input* input,
            FeatureGenerator fgen,
            const void* featureCxt) const = 0;

    virtual size_t predict(const FeatureMap& features) const = 0;
    std::string predictLabel(const FeatureMap& features) const
    {
        return getLabels()[predict(features)];
    }
    virtual std::span<const std::string> getLabels() const = 0;
    virtual ~MLModel()                                     = default;
};

/// A Gradient Boosted Tree base Model for Zstrong selectors
class GBTModel : public MLModel {
   public:
    explicit GBTModel(folly::dynamic const& model);
    explicit GBTModel(std::string_view model);
    Label predict(
            const ZL_Input* input,
            FeatureGenerator fgen,
            const void* featureCxt) const override;
    size_t predict(const FeatureMap& features) const override;
    std::span<const std::string> getLabels() const override
    {
        return labels_str_;
    }

   private:
    const std::vector<std::string> labels_str_;
    const std::vector<Label> labels_;
    const std::vector<std::string> features_str_;
    const std::vector<Label> features_;

    const gbt_predictor::GBTPredictor predictor_;
    const ::GBTModel gbtModel_;
};

/// FeatureGenerators are used to create a FeatureMaps from a Zstrong Streams
/// Currently they are not very stateful, but it the future we will want
/// them to be configurable based on the selector and trained model
class FeatureGenerator {
   public:
    explicit FeatureGenerator(
            const std::unordered_set<std::string>& featureNames)
            : featureNames_(featureNames)
    {
    }

    virtual std::unordered_set<std::string> getFeatureNames() const = 0;

    virtual void getFeatures(
            FeatureMap& featuresMap,
            const void* data,
            ZL_Type type,
            size_t eltWidth,
            size_t nbElts) const = 0;

    virtual void getFeatures(FeatureMap& featuresMap, ZL_Input const* data)
            const
    {
        getFeatures(
                featuresMap,
                ZL_Input_ptr(data),
                ZL_Input_type(data),
                ZL_Input_eltWidth(data),
                ZL_Input_numElts(data));
    }

    void getCFeatures(VECTOR(LabeledFeature) * features, ZL_Input const* data)
            const
    {
        FeatureMap featuresMap;
        getFeatures(
                featuresMap,
                ZL_Input_ptr(data),
                ZL_Input_type(data),
                ZL_Input_eltWidth(data),
                ZL_Input_numElts(data));
        bool badAlloc = false;
        for (auto it : featuresMap) {
            LabeledFeature lf = { getLabel(it.first), (float)it.second };
            badAlloc |= !VECTOR_PUSHBACK(*features, lf);
        }
        if (badAlloc) {
            throw std::runtime_error("Failed to add features to vector");
        }
    }

    virtual ~FeatureGenerator() = default;

   private:
    char const* getLabel(std::string const& label) const
    {
        auto it = featureNames_.find(label);
        if (it == featureNames_.end()) {
            throw std::runtime_error(
                    "FeatureGenerator doesn't expect a label " + label);
        }
        return it->c_str();
    }

    const std::unordered_set<std::string> featureNames_;
};

namespace features {

/// The features namespace includes some basic FeatureGenerators that can
/// be used as a starting point:

/// IntFeatureGenerator calculates basic features for numeric data, it assumes
/// the data is unsigned integers
class IntFeatureGenerator : public FeatureGenerator {
   public:
    IntFeatureGenerator() : FeatureGenerator(getFeatureNames()) {}
    virtual void getFeatures(
            FeatureMap& featuresMap,
            const void* data,
            ZL_Type type,
            size_t eltWidth,
            size_t nbElts) const override;

    virtual std::unordered_set<std::string> getFeatureNames() const override;
};

/// DeltaIntFeatureGenerator calculates basic integer features on the deltas
/// of items in the stream
class DeltaIntFeatureGenerator : public FeatureGenerator {
   public:
    DeltaIntFeatureGenerator() : FeatureGenerator(getFeatureNames()) {}
    virtual void getFeatures(
            FeatureMap& featuresMap,
            const void* data,
            ZL_Type type,
            size_t eltWidth,
            size_t nbElts) const override;

    virtual std::unordered_set<std::string> getFeatureNames() const override;
};

/// TokenizeIntFeatureGenerator calculates features that should help in a
/// decision about tokenization
class TokenizeIntFeatureGenerator : public FeatureGenerator {
   public:
    TokenizeIntFeatureGenerator() : FeatureGenerator(getFeatureNames()) {}
    virtual void getFeatures(
            FeatureMap& featuresMap,
            const void* data,
            ZL_Type type,
            size_t eltWidth,
            size_t nbElts) const override;

    virtual std::unordered_set<std::string> getFeatureNames() const override;
};
} // namespace features

/// An MLSelector is a custom Zstrong selector that uses a given trained MLModel
/// and FeatureGenerator to decide on successors. It's highly recommended to
/// provide the labels of the successors to avoid errors caused by mismatch of
/// ordering between model and code.
class MLSelector : public CustomSelector {
   public:
    MLSelector(
            ZL_Type inputType,
            std::shared_ptr<MLModel> model,
            std::shared_ptr<FeatureGenerator> featureGenerator,
            std::vector<std::string> labels = {})
            : inputType_(inputType),
              model_(model),
              featureGenerator_(featureGenerator)
    {
        if (!model) {
            throw std::runtime_error(
                    "MLSelector must be constructed with a model.");
        }
        if (!featureGenerator) {
            throw std::runtime_error(
                    "MLSelector must be constructed with a featureGenerator.");
        }
        if (labels.size()) {
            for (size_t i = 0; i < labels.size(); i++) {
                labelsIdx_.emplace(labels[i], i);
            }
            for (auto label : model.get()->getLabels()) {
                if (!labelsIdx_.contains(label)) {
                    throw std::runtime_error(
                            "MLSelector doesn't expect a model with label "
                            + label);
                }
            }
        }
    }

    static ZL_Report featureGen_MLSelector(
            const ZL_Input* inputStream,
            VECTOR(LabeledFeature) * features,
            const void* featureContext)
    {
        try {
            // TODO: Not efficient to use cpp implementation to get features,
            // since the cpp implementation uses the c implementation, this
            // means that we are turning VECTOR into FeatureMap then back to
            // VECTOR.

            const FeatureGenerator* cppFeatureGen =
                    (const FeatureGenerator*)featureContext;

            cppFeatureGen->getCFeatures(features, inputStream);

            return ZL_returnSuccess();
        } catch (const std::exception& e) {
            ZL_RET_R_ERR(GENERIC, "ML selector error %s", e.what());
        }
    }

    ZL_GraphID select(
            ZL_Selector const* selCtx,
            ZL_Input const* input,
            std::span<ZL_GraphID const> successors) const override;

    std::optional<size_t> expectedNbSuccessors() const override
    {
        return model_.get()->getLabels().size();
    }

    ZL_Type inputType() const override
    {
        return inputType_;
    }

   private:
    ZL_Type inputType_;
    std::shared_ptr<MLModel> model_;
    std::shared_ptr<FeatureGenerator> featureGenerator_;
    std::unordered_map<std::string, size_t> labelsIdx_;
};

struct MLTrainingSampleData {
    std::vector<uint8_t> data;
    size_t eltWidth;
    ZL_Type streamType;

    folly::dynamic toDynamic() const;
};

struct MLTrainingSample {
    std::optional<MLTrainingSampleData> data = std::nullopt;
    FeatureMap features                      = {};
    TargetsMap targets                       = {};

    explicit MLTrainingSample(
            std::optional<MLTrainingSampleData> data,
            FeatureMap features,
            TargetsMap targets)
            : data(std::move(data)),
              features(std::move(features)),
              targets(std::move(targets))
    {
    }
    explicit MLTrainingSample(folly::dynamic const& dynamic);

    folly::dynamic toDynamic() const;
    std::string toJson() const;
};

/// Serializes a vector of `MLTrainingSample`s into JSON
std::string MLTrainingSamplesToJson(
        const std::vector<MLTrainingSample>& samples);
/// Deserializes a vector of `MLTrainingSample`s from JSON, the inverse of
/// `MLTrainingSamplesToJson`
std::vector<MLTrainingSample> MLTrainingSamplesFromJson(std::string_view json);

/// A base selector that collects samples for ML training. Labels should match
/// the successors given to the selector.
class MLTrainingSelector : public CustomSelector {
   public:
    MLTrainingSelector(
            ZL_Type inputType,
            std::vector<std::string> labels,
            bool collectInputs                                 = true,
            std::shared_ptr<FeatureGenerator> featureGenerator = nullptr)
            : inputType_(inputType),
              labels_(std::move(labels)),
              collectInputs_(collectInputs),
              featureGenerator_(featureGenerator)
    {
    }

    ZL_GraphID select(
            ZL_Selector const* selCtx,
            ZL_Input const* input,
            std::span<ZL_GraphID const> successors) const override;

    std::optional<size_t> expectedNbSuccessors() const override
    {
        return labels_.size();
    }

    ZL_Type inputType() const override
    {
        return inputType_;
    }

   protected:
    virtual void collectSample(MLTrainingSample&& sample) const
    {
        (void)sample;
    }

   private:
    void collectSample(ZL_Input const* data, TargetsMap targets) const;
    ZL_Type inputType_;
    std::vector<std::string> labels_;
    bool collectInputs_;
    std::shared_ptr<FeatureGenerator> featureGenerator_;
};

/// A selector that records samples for ML training in memory and provides
/// multiple ways to access this data. Labels should match the successors
/// given to the selector.
class MemMLTrainingSelector : public MLTrainingSelector {
   public:
    using MLTrainingSelector::MLTrainingSelector;

    std::vector<MLTrainingSample> getCollected() const;
    size_t getCollectedSize() const;
    std::vector<MLTrainingSample> flushCollected() const;
    std::string getCollectedJson() const;
    void clearCollected();

   protected:
    void collectSample(MLTrainingSample&& sample) const override;

   private:
    mutable folly::Synchronized<std::vector<MLTrainingSample>>
            results_; // mutable because we need to mutate it from select which
                      // is a const function
};

/// A selector that records samples for ML training to a file stream. Each
/// sample is a json encoded line in the output. Labels should match the
/// successors given to the selector.
class FileMLTrainingSelector : public MLTrainingSelector {
   public:
    FileMLTrainingSelector(
            ZL_Type inputType,
            const std::vector<std::string>& labels,
            std::ofstream&& output,
            bool collectInputs                                 = true,
            std::shared_ptr<FeatureGenerator> featureGenerator = nullptr)
            : MLTrainingSelector(
                      inputType,
                      labels,
                      collectInputs,
                      std::move(featureGenerator)),
              firstSample_(true),
              output_(std::move(output))
    {
        (*output_.wlock()) << "[";
    }
    virtual ~FileMLTrainingSelector() override
    {
        (*output_.wlock()) << "]";
        output_.wlock()->close();
    }

   protected:
    void collectSample(MLTrainingSample&& sample) const override;

   private:
    mutable std::atomic<bool> firstSample_;
    mutable folly::Synchronized<std::ofstream>
            output_; // mutable because we need to mutate it from select which
                     // is a const function
};

} // namespace ml
} // namespace zstrong
