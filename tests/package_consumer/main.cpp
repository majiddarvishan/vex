#include <vex/version.hpp>

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
    if (vex::version_major != 0 || vex::version_minor != 1 || vex::version_patch != 0)
        return 3;
    if (vex::version() != "0.1.0")
        return 4;
    if (vex::version_full() != "Vex v0.1.0 — Fast. Simple. Professional.")
        return 5;

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
