#include <functional>

#include "common/grpc/async_client_impl.h"

#include "test/mocks/grpc/mocks.h"
#include "test/mocks/tracing/mocks.h"
#include "test/proto/helloworld.pb.h"

#include "gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Ref;

namespace Envoy {
namespace Grpc {

using namespace std::placeholders;

class TypedAsyncRequestCallbacksShimTest : public testing::Test {
public:
  TypedAsyncRequestCallbacksShimTest()
      : shim_(std::bind(
                  &MockAsyncRequestCallbacks<helloworld::HelloReply>::onSuccess,
                  &callbacks_, _1, _2),
              std::bind(
                  &MockAsyncRequestCallbacks<helloworld::HelloReply>::onFailure,
                  &callbacks_, _1, _2, _3)) {}

  MockAsyncRequestCallbacks<helloworld::HelloReply> callbacks_;
  TypedAsyncRequestCallbacksShim<helloworld::HelloReply> shim_;
};

TEST_F(TypedAsyncRequestCallbacksShimTest, OnSuccess) {
  std::string message{"Hello, world!"};
  Tracing::MockSpan span;
  EXPECT_CALL(callbacks_, onSuccess_(_, Ref(span)))
      .Times(1)
      .WillOnce(Invoke(
          [&message](const helloworld::HelloReply &response, Tracing::Span &) {
            EXPECT_EQ(message, response.message());
          }));

  std::unique_ptr<helloworld::HelloReply> response =
      std::make_unique<helloworld::HelloReply>();
  response->set_message(message);
  shim_.onSuccess(std::move(response), span);
}

TEST_F(TypedAsyncRequestCallbacksShimTest, OnFailure) {
  Status::GrpcStatus status{};
  std::string message{"Error"};
  Tracing::MockSpan span;
  EXPECT_CALL(callbacks_, onFailure(status, message, Ref(span))).Times(1);

  shim_.onFailure(status, message, span);
}

} // namespace Grpc
} // namespace Envoy
