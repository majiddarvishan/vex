# API Documentation

## MetricsManager

### Initialization
```cpp
bool Init(bool enable_threading = false);
```

### Create Metrics
```cpp
Counter& CreateCounter(registry, name, help, labels);
Gauge& CreateGauge(registry, name, help, labels);
Histogram& CreateHistogram(registry, name, help, labels, buckets);
```

### Type-Safe Macros
```cpp
SAFE_CREATE_COUNTER(registry, name, help, labels);
SAFE_CREATE_GAUGE(registry, name, help, labels);
SAFE_CREATE_HISTOGRAM(registry, name, help, labels, buckets);
```

## RAII Guards
```cpp
ScopedCounter counter(family, labels);
ScopedGauge gauge(family, labels);
```

## Decorators
```cpp
ScopedTimer timer(histogram);
TimeFunction(histogram, lambda);
ResultTracker tracker(success, failure);
TimedResultTracker tracker(duration, success, failure);
```

## Health Checks
```cpp
HealthCheck::RegisterHealthMetrics(registry);
HealthCheck::SetHealthy(bool);
HealthCheck::UpdateUptime();