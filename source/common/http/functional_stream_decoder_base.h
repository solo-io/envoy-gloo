#pragma once

#include "envoy/http/metadata_accessor.h"
#include "envoy/server/filter_config.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/http/utility.h"
#include "common/protobuf/utility.h"

namespace Envoy {
namespace Http {

class FunctionRetrieverMetadataAccessor : public MetadataAccessor {
public:
  FunctionRetrieverMetadataAccessor(Server::Configuration::FactoryContext &ctx,
                                    const std::string &childname)
      : cm_(ctx.clusterManager()), random_(ctx.random()),
        childname_(childname) {}

  ~FunctionRetrieverMetadataAccessor();

  enum class Result {
    Error,
    Active,
  };

  absl::optional<Result> tryToGetSpec();

  // MetadataAccessor
  absl::optional<const std::string *> getFunctionName() const override;
  absl::optional<const ProtobufWkt::Struct *> getFunctionSpec() const override;
  absl::optional<const ProtobufWkt::Struct *>
  getClusterMetadata() const override;
  absl::optional<const ProtobufWkt::Struct *> getRouteMetadata() const override;

  void
  setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &decoder_callbacks) {
    decoder_callbacks_ = &decoder_callbacks;
  }

private:
  struct FunctionWeight {
    uint64_t weight;
    const std::string *name;
  };

  Upstream::ClusterManager &cm_;
  Runtime::RandomGenerator &random_;
  const std::string &childname_;

  Upstream::ClusterInfoConstSharedPtr cluster_info_{};
  const std::string *function_name_{};        // function name is here
  const ProtobufWkt::Struct *cluster_spec_{}; // function spec is here
  // mutable as these are modified in a const function. it is ok as the state of
  // the object doesnt change, it is for lazy loading.
  mutable const ProtobufWkt::Struct *child_spec_{}; // childfilter is here

  mutable Router::RouteConstSharedPtr route_info_{};
  mutable const ProtobufWkt::Struct *route_spec_{};

  StreamDecoderFilterCallbacks *decoder_callbacks_{};

  bool canPassthrough();

  absl::optional<const std::string *>
  findSingleFunction(const ProtobufWkt::Struct &filter_metadata_struct);
  absl::optional<const std::string *>
  findMultileFunction(const ProtobufWkt::Struct &filter_metadata_struct);

  absl::optional<FunctionWeight>
  getFuncWeight(const ProtobufWkt::Value &function_weight_value);

  void tryToGetSpecFromCluster(const std::string &funcname);
  void fetchClusterInfoIfOurs();
};

template <typename MixinBase> class FunctionalFilterMixin : public MixinBase {

  static_assert(std::is_base_of<StreamDecoderFilter, MixinBase>::value,
                "Base must be StreamDecoderFilter or StreamFilter");
  static_assert(std::is_base_of<FunctionalFilter, MixinBase>::value,
                "Base must be StreamDecoderFilter or StreamFilter");

public:
  template <class... Ts>
  FunctionalFilterMixin(Server::Configuration::FactoryContext &ctx,
                        const std::string &childname, Ts &&... args)
      : MixinBase(std::forward<Ts>(args)...),
        metadata_accessor_(ctx, childname) {}
  virtual ~FunctionalFilterMixin() {}

  // Http::StreamFilterBase
  void onDestroy() override {
    is_reset_ = true;
    MixinBase::onDestroy();
  }

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &headers,
                                    bool end_stream) override {
    auto mayberesult = metadata_accessor_.tryToGetSpec();
    // no function
    if (!mayberesult.has_value()) {
      return FilterHeadersStatus::Continue;
    }

    // get the function:
    auto result = mayberesult.value();

    switch (result) {
    case FunctionRetrieverMetadataAccessor::Result::Active: {
      // we should have a function!
      active_ = MixinBase::retrieveFunction(metadata_accessor_);
      if (active_) {
        return MixinBase::decodeHeaders(headers, end_stream);
      } else {
        // we found a function but we are not active.
        // this means retrieval failed.
        // return internal server error
        // TODO(yuval-k): do we want to return a different error type here?

        error();
        return FilterHeadersStatus::StopIteration;
      }
    }
    default:
      // Not active, this means some sort of error..
      error();
      return FilterHeadersStatus::StopIteration;
    }
  }

  FilterDataStatus decodeData(Buffer::Instance &data,
                              bool end_stream) override {
    if (active_) {
      return MixinBase::decodeData(data, end_stream);
    }
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus decodeTrailers(HeaderMap &trailers) override {
    if (active_) {
      return MixinBase::decodeTrailers(trailers);
    }
    return FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
    metadata_accessor_.setDecoderFilterCallbacks(decoder_callbacks);
    MixinBase::setDecoderFilterCallbacks(decoder_callbacks);
  }

  StreamDecoderFilterCallbacks *decoder_callbacks_{};

private:
  FunctionRetrieverMetadataAccessor metadata_accessor_;
  bool is_reset_{false};
  bool active_{false};

  void error() {
    Utility::sendLocalReply(*decoder_callbacks_, is_reset_, Code::NotFound,
                            "Function not found");
  }
};

} // namespace Http
} // namespace Envoy
