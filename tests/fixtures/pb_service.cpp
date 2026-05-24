#include "pb_stub.hpp"

#include <cstdint>
#include <functional>
#include <string>

// Commit 4: [[pb::service("Name")]] — a record marked this way emits as
// a proto3 `service Name { ... }` block. Each std::function<Resp(Req)>
// member becomes one `rpc <member-name>(Req) returns (Resp);` line.
// Non-function fields would be skipped with a diagnostic; this fixture
// keeps the struct RPC-handler-only.
//
// The Request / Response struct types are walked as dependencies so
// they emit as proto3 `message` blocks BEFORE the service block.

namespace acme::users {

// Request / response messages. Plain pb::field-only structs.
struct [[pb::doc("user-id lookup request")]] get_user_req_t {
    [[pb::field(1)]] std::int64_t user_id;
};
struct [[pb::doc("user-id lookup response")]] get_user_resp_t {
    [[pb::field(1)]] std::string display_name;
    [[pb::field(2)]] std::int64_t created_ns;
};

struct list_users_req_t {
    [[pb::field(1)]] std::int32_t page;
    [[pb::field(2)]] std::int32_t page_size;
};
struct list_users_resp_t {
    [[pb::field(1)]] std::int32_t total;
};

struct auth_req_t  { [[pb::field(1)]] std::string token; };
struct auth_resp_t { [[pb::field(1)]] bool ok; };

// The service definition. No data fields — only RPC handlers.
struct [[pb::service("UserService"),
        pb::doc("user-account RPC surface")]]
user_service_t {
    std::function<get_user_resp_t(get_user_req_t)>     get_user;
    std::function<list_users_resp_t(list_users_req_t)> list_users;
    std::function<auth_resp_t(auth_req_t)>             authenticate;
};

} // namespace acme::users

void use() {
    // Touching the messages teaches h5cpp-compiler the requests/responses
    // exist as user records — they emit as `message` blocks before the
    // service that references them.
    acme::users::get_user_req_t  q1{}; (void)pb::encode(q1);
    acme::users::get_user_resp_t r1{}; (void)pb::encode(r1);
    acme::users::list_users_req_t  q2{}; (void)pb::encode(q2);
    acme::users::list_users_resp_t r2{}; (void)pb::encode(r2);
    acme::users::auth_req_t  q3{}; (void)pb::encode(q3);
    acme::users::auth_resp_t r3{}; (void)pb::encode(r3);
    acme::users::user_service_t svc{}; (void)pb::encode(svc);
}
