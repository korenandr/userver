#pragma once

#include <storages/redis/parse_reply.hpp>
#include <storages/redis/request.hpp>

namespace storages {
namespace redis {
template <typename ScriptResult,
          typename ReplyType = impl::DefaultReplyType<ScriptResult>>
class USERVER_NODISCARD RequestEval final {
 public:
  explicit RequestEval(RequestEvalCommon&& request)
      : request_(std::move(request)) {}

  void Wait() { request_.Wait(); }

  void IgnoreResult() const { request_.IgnoreResult(); }

  ReplyType Get(const std::string& request_description = {}) {
    return ParseReply<ScriptResult, ReplyType>(request_.GetRaw(),
                                               request_description);
  }

 private:
  RequestEvalCommon request_;
};

}  // namespace redis
}  // namespace storages
