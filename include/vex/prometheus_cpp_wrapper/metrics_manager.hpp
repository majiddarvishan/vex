
// ============================================================================
// Core metrics manager
// ============================================================================
#pragma once

#include <prometheus/gauge.h>
#include <prometheus/family.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

#include <map>
#include <string>
#include <memory>
#include <stdexcept>
#include <regex>
#include <chrono>
#include <unordered_map>

#ifdef METRICS_MULTI_THREADED
    #include <mutex>
    #define METRICS_LOCK_GUARD std::lock_guard<std::mutex> lock(mutex_)
#else
    #define METRICS_LOCK_GUARD
#endif

namespace vex::metrics {

class MetricsManager {
public:
/// Initialize the metrics manager. Must be called before any other methods.
/// Thread-safe only if compiled with METRICS_MULTI_THREADED.
/// Idempotent (safe to call multiple times).
/// @param enable_threading Enable runtime thread safety (only works if compiled with METRICS_MULTI_THREADED)
/// @return true if initialization succeeded, false if already initialized
    [[nodiscard]] static bool Init(bool enable_threading = false);

    [[nodiscard]] static std::shared_ptr<prometheus::Registry> GetRegistry();

    /// Get or create a subsystem-specific registry.
    /// @param ns Namespace for the subsystem (e.g., "db", "cache")
    /// @return Shared pointer to the subsystem registry
    [[nodiscard]] static std::shared_ptr<prometheus::Registry> GetSubsystemRegistry(const std::string& ns);

    /// Reset the metrics manager (primarily for testing).
    /// Warning: Invalidates all existing metric references.
    static void Reset();

    /// Register all registries (main + subsystems) with a Prometheus exposer.
    static void RegisterAllWithExposer(prometheus::Exposer& exposer);

        /// Set default labels that will be applied to all metrics.
    /// @throws std::invalid_argument if any label name is invalid
    static void SetDefaultLabels(const std::map<std::string,std::string>& labels);
    [[nodiscard]] static std::map<std::string,std::string> GetDefaultLabels();
    [[nodiscard]] static std::map<std::string,std::string> GetDynamicLabels();

    /// Merge default labels with provided labels.
    /// Note: Provided labels override defaults if keys conflict.
    /// @param labels Additional labels to merge
    /// @return Merged label map
    [[nodiscard]] static std::map<std::string,std::string> MergeLabels(const std::map<std::string,std::string>& labels);

    /// Add a counter metric to a family with merged labels.
    /// @return Reference valid until family is destroyed or metric is removed
    [[nodiscard]] static prometheus::Counter& add_counter(
        prometheus::Family<prometheus::Counter>& family,
        const std::map<std::string, std::string>& labels);

        /// Add a gauge metric to a family with merged labels.
    /// @return Reference valid until family is destroyed or metric is removed
    [[nodiscard]] static prometheus::Gauge& add_gauge(
        prometheus::Family<prometheus::Gauge>& family,
        const std::map<std::string, std::string>& labels);

        /// Remove a counter metric from its family.
    /// @param metric Pointer to metric; becomes invalid after removal
    static void remove_counter(prometheus::Family<prometheus::Counter>& family, prometheus::Counter* metric);

    /// Remove a gauge metric from its family.
    /// @param metric Pointer to metric; becomes invalid after removal
    static void remove_gauge(prometheus::Family<prometheus::Gauge>& family, prometheus::Gauge* metric);

    /// Create a counter metric in a specific registry.
    /// Useful for subsystem-specific metrics.
    /// @return Reference valid until registry/family is destroyed
    [[nodiscard]] static prometheus::Counter& CreateCounter(
        const std::shared_ptr<prometheus::Registry>& registry,
        const std::string& name,
        const std::string& help,
        const std::map<std::string,std::string>& labels = {});

        /// Create a gauge metric in a specific registry.
    /// @return Reference valid until registry/family is destroyed
    [[nodiscard]] static prometheus::Gauge& CreateGauge(
        const std::shared_ptr<prometheus::Registry>& registry,
        const std::string& name,
        const std::string& help,
        const std::map<std::string,std::string>& labels = {});

        /// Create a histogram metric in a specific registry.
    /// @param buckets Custom bucket boundaries (empty = default buckets)
    /// @return Reference valid until registry/family is destroyed
    [[nodiscard]] static prometheus::Histogram& CreateHistogram(
        const std::shared_ptr<prometheus::Registry>& registry,
        const std::string& name,
        const std::string& help,
        const std::map<std::string,std::string>& labels = {},
        const std::vector<double>& buckets = {});

        /// Check if the manager has been initialized.
    [[nodiscard]] static bool IsInitialized();

    /// Check if threading is enabled.
    [[nodiscard]] static bool IsThreadingEnabled();
    [[nodiscard]] static double GetUptimeSeconds();

private:
/// Ensure the manager has been initialized, throw if not.
    static void EnsureInitialized();

    /// Validate metric name according to Prometheus naming conventions.
    /// Must match [a-zA-Z_:][a-zA-Z0-9_:]* and not start with __
    static void ValidateMetricName(const std::string& name);

    /// Validate label names and values.
    static void ValidateLabels(const std::map<std::string,std::string>& labels);

    /// Validate label value (basic check for printable characters).
    [[nodiscard]] static bool ValidateLabelValue(const std::string& value);

    inline static std::shared_ptr<prometheus::Registry> registry_;
    inline static std::unordered_map<std::string, std::shared_ptr<prometheus::Registry>> subsystems_;
    inline static std::map<std::string,std::string> default_labels_;
    inline static std::map<std::string,std::string> dynamic_labels_;
    inline static bool initialized_ = false;
    inline static bool threading_enabled_ = false;
    inline static std::chrono::steady_clock::time_point start_time_{};

#ifdef METRICS_MULTI_THREADED
    inline static std::mutex mutex_;
#endif
};

}