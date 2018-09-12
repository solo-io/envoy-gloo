#pragma once

#include "envoy/grpc/async_client.h"

namespace Envoy {
namespace Grpc {

template <class ResponseType>
class TypedAsyncRequestCallbacksShim
    : public TypedAsyncRequestCallbacks<ResponseType> {
public:
  typedef std::unique_ptr<ResponseType> ResponseTypePtr;

  TypedAsyncRequestCallbacksShim(
      std::function<void(ResponseTypePtr &&, Tracing::Span &)> &&on_success,
      std::function<void(Status::GrpcStatus, const std::string &,
                         Tracing::Span &)> &&on_failure)
      : on_success_(on_success), on_failure_(on_failure) {}

  // Grpc::AsyncRequestCallbacks
  void onCreateInitialMetadata(Http::HeaderMap &) override {}
  void onFailure(Status::GrpcStatus status, const std::string &message,
                 Tracing::Span &span) override {
    on_failure_(status, message, span);
  }

  // Grpc::TypedAsyncRequestCallbacks<ResponseType>
  void onSuccess(ResponseTypePtr &&response, Tracing::Span &span) override {
    on_success_(std::forward<ResponseTypePtr>(response), span);
  }

private:
  std::function<void(ResponseTypePtr &&, Tracing::Span &)> on_success_;
  std::function<void(Status::GrpcStatus, const std::string &, Tracing::Span &)>
      on_failure_;
};

} // namespace Grpc
} // namespace Envoy
