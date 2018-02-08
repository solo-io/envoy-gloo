#pragma once

#include "envoy/server/filter_config.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/protobuf/utility.h"

namespace Envoy {
namespace Http {

using Envoy::Server::Configuration::FactoryContext;

class FunctionalFilterBase : public StreamDecoderFilter,
                             public Logger::Loggable<Logger::Id::filter> {
public:
  FunctionalFilterBase(FactoryContext &ctx, const std::string &childname)
      : decoder_callbacks_(nullptr), cm_(ctx.clusterManager()),
        random_(ctx.random()), childname_(childname), cluster_info_(nullptr),
        spec_(nullptr) {}
  virtual ~FunctionalFilterBase();

  // Http::StreamFilterBase

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &headers,
                                    bool end_stream) override;
  FilterDataStatus decodeData(Buffer::Instance &data, bool end_stream) override;

  FilterTrailersStatus decodeTrailers(HeaderMap &trailers) override;

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
  }

protected:
  const ProtobufWkt::Struct &getFunctionSpec();
  StreamDecoderFilterCallbacks *decoder_callbacks_;

  virtual FilterHeadersStatus functionDecodeHeaders(HeaderMap &m, bool e) PURE;
  virtual FilterDataStatus functionDecodeData(Buffer::Instance &, bool) PURE;
  virtual FilterTrailersStatus functionDecodeTrailers(HeaderMap &) PURE;

private:
  Upstream::ClusterManager &cm_;
  Envoy::Runtime::RandomGenerator &random_;
  const std::string &childname_;

  Upstream::ClusterInfoConstSharedPtr cluster_info_;
  const ProtobufWkt::Struct *spec_;

  void tryToGetSpec();
  void findSingleFunction(Upstream::ClusterInfoConstSharedPtr &&info,
                          const ProtobufWkt::Struct &filter_metadata_struct);
  void
  tryToGetSpecFromCluster(const std::string &funcname,
                          Upstream::ClusterInfoConstSharedPtr &&clusterinfo);
  bool isOurCluster(const Upstream::ClusterInfoConstSharedPtr &clusterinfo);
  bool active() { return spec_ != nullptr; }
  void error();
};

} // namespace Http
} // namespace Envoy
