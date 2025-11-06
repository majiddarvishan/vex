#pragma once

#ifdef BUILD_LOGGER
    #include "vex/logger.h"
#endif

#ifdef BUILD_THREADPOOL
    #include "vex/threadpool.h"
#endif

#ifdef BUILD_NETWORKING
    #include "vex/networking.h"
#endif

#ifdef BUILD_OBJECT_POOL
    #include "vex/object_pool/thread_local_object_pool.h"
#endif
