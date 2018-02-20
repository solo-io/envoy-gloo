#pragma once

#include "envoy/http/metadata_accessor.h"
#include "envoy/server/filter_config.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/protobuf/utility.h"

namespace Envoy {
namespace Http {

using Envoy::Server::Configuration::FactoryContext;

class FunctionalFilterBase : public StreamDecoderFilter,
                             public MetadataAccessor,
                             public Logger::Loggable<Logger::Id::filter> {
public:
  FunctionalFilterBase(FactoryContext &ctx, const std::string &childname)
      : cm_(ctx.clusterManager()), random_(ctx.random()),
        childname_(childname) {}
  virtual ~FunctionalFilterBase();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &headers,
                                    bool end_stream) override;
  FilterDataStatus decodeData(Buffer::Instance &data, bool end_stream) override;

  FilterTrailersStatus decodeTrailers(HeaderMap &trailers) override;

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
  }

  // MetadataAccessor
  Optional<const std::string*> getFunctionName() const override;
  Optional<const ProtobufWkt::Struct *> getFunctionSpec() const override;
  Optional<const ProtobufWkt::Struct *> getClusterMetadata() const override;
  Optional<const ProtobufWkt::Struct *> getRouteMetadata() const override;

protected:
  StreamDecoderFilterCallbacks *decoder_callbacks_{};
  bool is_reset_{};

  virtual FilterHeadersStatus functionDecodeHeaders(HeaderMap &m, bool e) PURE;
  virtual FilterDataStatus functionDecodeData(Buffer::Instance &, bool) PURE;
  virtual FilterTrailersStatus functionDecodeTrailers(HeaderMap &) PURE;
  virtual bool retrieveFunction(const MetadataAccessor& meta_accessor) PURE;
private:
  struct FunctionWeight {
    uint64_t weight;
    const std::string *name;
  };

  Upstream::ClusterManager &cm_;
  Envoy::Runtime::RandomGenerator &random_;
  const std::string &childname_;
  bool active_{false};

  Upstream::ClusterInfoConstSharedPtr cluster_info_{};
  const std::string *function_name_{}; // function name is here
  const ProtobufWkt::Struct *cluster_spec_{}; // function spec is here
  // mutable as these are modified in a const function. it is ok as the state of
  // the object doesnt change, it is for lazy loading.
  mutable const ProtobufWkt::Struct *child_spec_{}; // childfilter is here

  mutable Router::RouteConstSharedPtr route_info_{};
  mutable const ProtobufWkt::Struct *route_spec_{};
  bool error_{};

  void tryToGetSpec();

  Optional<const std::string *>
  findSingleFunction(const ProtobufWkt::Struct &filter_metadata_struct);
  Optional<const std::string *>
  findMultileFunction(const ProtobufWkt::Struct &filter_metadata_struct);

  Optional<FunctionWeight>
  getFuncWeight(const ProtobufWkt::Value &function_weight_value);

  void tryToGetSpecFromCluster(const std::string &funcname);
  void fetchClusterInfoIfOurs();
  void error();
  bool active() const { return active_; }
};

} // namespace Http
} // namespace Envoy
