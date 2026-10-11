#pragma once
#include <gungnir/cache/lock.hpp>
#include <gungnir/scheduler/redis_lock.hpp>

namespace gungnir::cache {
using RedisLockSettings = scheduler::RedisLockSettings;
using RedisLockStore = scheduler::RedisLockStore;
}
