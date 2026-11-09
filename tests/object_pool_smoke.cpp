#include <vex/object_pool/object_pool.hpp>

#include <memory>

int main()
{
    auto pool = std::make_shared<vex::ObjectPool>();
    {
        auto obj = pool->create<int>(42);
        if (!obj || *obj != 42)
            return 1;
    }
    if (pool->available() != 1)
        return 2;
    auto reused = pool->create<int>(17);
    return reused && *reused == 17 ? 0 : 3;
}
