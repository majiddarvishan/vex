#ifdef VEX_HAS_THREAD_POOL
#include <vex/thread/threadpool.hpp>
#endif
#ifdef VEX_HAS_OBJECT_POOL
#include <vex/object_pool/object_pool.hpp>
#endif
#ifdef VEX_HAS_MONITORING
#include <vex/monitoring/monitoring.hpp>
#endif

int main()
{
#ifdef VEX_HAS_THREAD_POOL
    vex::ThreadPool pool(1);
    pool.enqueue([] {});
#endif
#ifdef VEX_HAS_OBJECT_POOL
    auto object_pool = std::make_shared<vex::ObjectPool>();
    auto value = object_pool->create<int>(7);
    if (!value || *value != 7)
        return 1;
#endif
#ifdef VEX_HAS_MONITORING
    (void)vex::MetricsManager::Init();
    if (!vex::MetricsManager::GetRegistry())
        return 2;
#endif
    return 0;
}
