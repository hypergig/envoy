#include "common/ratelimit/ratelimit_impl.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "common/common/assert.h"
#include "common/grpc/async_client_impl.h"
#include "common/http/headers.h"

#include "fmt/format.h"

namespace Envoy {
namespace RateLimit {

GrpcClientImpl::GrpcClientImpl(RateLimitAsyncClientPtr&& async_client,
                               const Optional<std::chrono::milliseconds>& timeout)
    : service_method_(*Protobuf::DescriptorPool::generated_pool()->FindMethodByName(
          "pb.lyft.ratelimit.RateLimitService.ShouldRateLimit")),
      async_client_(std::move(async_client)), timeout_(timeout) {
}

GrpcClientImpl::~GrpcClientImpl() { ASSERT(!callbacks_); }

void GrpcClientImpl::cancel() {
  ASSERT(callbacks_ != nullptr);
  request_->cancel();
  callbacks_ = nullptr;
}

void GrpcClientImpl::createRequest(pb::lyft::ratelimit::RateLimitRequest& request,
                                   const std::string& domain,
                                   const std::vector<Descriptor>& descriptors) {
  request.set_domain(domain);
  for (const Descriptor& descriptor : descriptors) {
    pb::lyft::ratelimit::RateLimitDescriptor* new_descriptor = request.add_descriptors();
    for (const DescriptorEntry& entry : descriptor.entries_) {
      pb::lyft::ratelimit::RateLimitDescriptor::Entry* new_entry = new_descriptor->add_entries();
      new_entry->set_key(entry.key_);
      new_entry->set_value(entry.value_);
    }
  }
}

void GrpcClientImpl::limit(RequestCallbacks& callbacks, const std::string& domain,
                           const std::vector<Descriptor>& descriptors,
                           const std::string& request_id, Tracing::Span& parent_span) {
  ASSERT(callbacks_ == nullptr);
  callbacks_ = &callbacks;

  pb::lyft::ratelimit::RateLimitRequest request;
  createRequest(request, domain, descriptors);

  request_ = async_client_->send(service_method_, request, *this, parent_span, timeout_);
}

void GrpcClientImpl::onCreateInitialMetadata(Http::HeaderMap& metadata) {
  if (!request_id_.empty()) {
    metadata.insertRequestId().value(request_id_);
  }
}

void GrpcClientImpl::onSuccess(std::unique_ptr<pb::lyft::ratelimit::RateLimitResponse>&& response) {
  LimitStatus status = LimitStatus::OK;
  ASSERT(response->overall_code() != pb::lyft::ratelimit::RateLimitResponse_Code_UNKNOWN);
  if (response->overall_code() == pb::lyft::ratelimit::RateLimitResponse_Code_OVER_LIMIT) {
    status = LimitStatus::OverLimit;
  }
  callbacks_->complete(status);
  callbacks_ = nullptr;
}

void GrpcClientImpl::onFailure(Grpc::Status::GrpcStatus status, const std::string&) {
  ASSERT(status != Grpc::Status::GrpcStatus::Ok);
  UNREFERENCED_PARAMETER(status);
  callbacks_->complete(LimitStatus::Error);
  callbacks_ = nullptr;
}

GrpcFactoryImpl::GrpcFactoryImpl(const envoy::api::v2::RateLimitServiceConfig& config,
                                 Upstream::ClusterManager& cm)
    : cluster_name_(config.cluster_name()), cm_(cm) {
  if (!cm_.get(cluster_name_)) {
    throw EnvoyException(fmt::format("unknown rate limit service cluster '{}'", cluster_name_));
  }
}

ClientPtr GrpcFactoryImpl::create(const Optional<std::chrono::milliseconds>& timeout) {
  return ClientPtr{new GrpcClientImpl(
      RateLimitAsyncClientPtr{
          new Grpc::AsyncClientImpl<pb::lyft::ratelimit::RateLimitRequest,
                                    pb::lyft::ratelimit::RateLimitResponse>(cm_, cluster_name_)},
      timeout)};
}

void RateLimitSpanFinalizer::finalize(Tracing::Span& span) {
  if (response_ != nullptr) {
    if (response_->overall_code() == pb::lyft::ratelimit::RateLimitResponse_Code_OVER_LIMIT) {
      span.setTag("ratelimit_status", "over_limit");
    } else {
      span.setTag("ratelimit_status", "ok");
    }
  }
}

Tracing::SpanFinalizerPtr
RateLimitSpanFinalizerFactoryImpl::create(const pb::lyft::ratelimit::RateLimitRequest&,
                                          const pb::lyft::ratelimit::RateLimitResponse* response) {
  Tracing::SpanFinalizerPtr finalizer{new RateLimitSpanFinalizer(response)};
  return finalizer;
}

} // namespace RateLimit
} // namespace Envoy
