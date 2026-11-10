#include <vex/monitoring/metrics_manager.hpp>

#include <cstdlib>
#include <vector>

namespace vex
{

    bool MetricsManager::Init(bool enable_threading)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!registry_)
        {
#ifdef METRICS_MULTI_THREADED
            threading_enabled_ = enable_threading;
#else
            threading_enabled_ = false;
            (void) enable_threading;
#endif
            registry_    = std::make_shared<prometheus::Registry>();
            initialized_ = true;
            start_time_  = std::chrono::steady_clock::now();

            const char* container = std::getenv("CONTAINER_ID");
            if (container && ValidateLabelValue(container))
            {
                dynamic_labels_["container_id"] = container;
            }

            // const char* pod = std::getenv("POD_NAME");
            // if (pod && ValidateLabelValue(pod)) {
            //     dynamic_labels_["pod_name"] = pod;
            // }

            // Add hostname and PID if needed for your use case
            // Uncomment and link with appropriate system libraries

            // char hostname[256];
            // if (gethostname(hostname, sizeof(hostname)) == 0) {
            //     dynamic_labels_["hostname"] = hostname;
            // }
            // dynamic_labels_["pid"] = std::to_string(getpid());

            return true;
        }
        return false; // Already initialized
    }

    std::shared_ptr<prometheus::Registry> MetricsManager::GetRegistry()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!initialized_ || !registry_)
        {
            throw std::runtime_error(
              "MetricsManager not initialized. Call MetricsManager::Init() first."
            );
        }
        return registry_;
    }

    void MetricsManager::Reset()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        registry_.reset();
        subsystems_.clear();
        default_labels_.clear();
        dynamic_labels_.clear();
        initialized_       = false;
        threading_enabled_ = false;
        start_time_ = {};
    }

    std::shared_ptr<prometheus::Registry>
      MetricsManager::GetSubsystemRegistry(const std::string& ns)
    {
        if (ns.empty())
        {
            throw std::invalid_argument("Subsystem namespace cannot be empty");
        }

        std::lock_guard<std::mutex> guard(mutex_);
        if (!initialized_ || !registry_)
            throw std::runtime_error("MetricsManager not initialized");
        auto it = subsystems_.find(ns);
        if (it != subsystems_.end())
            return it->second;

        auto sub_registry = std::make_shared<prometheus::Registry>();
        subsystems_[ns]   = sub_registry;
        return sub_registry;
    }

    void MetricsManager::RegisterAllWithExposer(prometheus::Exposer& exposer)
    {
        // Snapshot shared ownership under lock, then invoke the external
        // Exposer API without holding the manager mutex.
        std::vector<std::shared_ptr<prometheus::Registry>> snapshots;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (!initialized_ || !registry_)
                throw std::runtime_error("MetricsManager not initialized");
            snapshots.reserve(subsystems_.size() + 1);
            snapshots.push_back(registry_);
            for (const auto& kv: subsystems_)
                snapshots.push_back(kv.second);
        }
        for (const auto& registry: snapshots)
            exposer.RegisterCollectable(registry);
    }

    void MetricsManager::SetDefaultLabels(
      const std::map<std::string, std::string>& labels
    )
    {
        ValidateLabels(labels);
        std::lock_guard<std::mutex> guard(mutex_);
        default_labels_ = labels;
    }

    std::map<std::string, std::string> MetricsManager::GetDefaultLabels()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return default_labels_;
    }

    std::map<std::string, std::string> MetricsManager::GetDynamicLabels()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return dynamic_labels_;
    }

    std::map<std::string, std::string> MetricsManager::MergeLabels(
      const std::map<std::string, std::string>& labels
    )
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto merged = default_labels_;
        merged.insert(dynamic_labels_.begin(), dynamic_labels_.end());

        // Explicit per-metric labels override the defaults.
        for (const auto& [key, value]: labels)
            merged[key] = value;
        return merged;
    }

    prometheus::Counter& MetricsManager::add_counter(
      prometheus::Family<prometheus::Counter>& family,
      const std::map<std::string, std::string>& labels
    )
    {
        ValidateLabels(labels);
        return family.Add(MergeLabels(labels));
    }

    prometheus::Gauge& MetricsManager::add_gauge(
      prometheus::Family<prometheus::Gauge>& family,
      const std::map<std::string, std::string>& labels
    )
    {
        ValidateLabels(labels);
        return family.Add(MergeLabels(labels));
    }

    void MetricsManager::remove_counter(
      prometheus::Family<prometheus::Counter>& family,
      prometheus::Counter* metric
    )
    {
        if (metric == nullptr)
        {
            throw std::invalid_argument("Cannot remove null metric");
        }
        family.Remove(metric);
    }

    void MetricsManager::remove_gauge(
      prometheus::Family<prometheus::Gauge>& family, prometheus::Gauge* metric
    )
    {
        if (metric == nullptr)
        {
            throw std::invalid_argument("Cannot remove null metric");
        }
        family.Remove(metric);
    }

    prometheus::Counter& MetricsManager::CreateCounter(
      const std::shared_ptr<prometheus::Registry>& registry,
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& labels
    )
    {
        if (!registry)
        {
            throw std::invalid_argument("Registry cannot be null");
        }
        ValidateMetricName(name);
        ValidateLabels(labels);

        auto& family = prometheus::BuildCounter().Name(name).Help(help).Register(
          *registry
        );

        return family.Add(MergeLabels(labels));
    }

    prometheus::Gauge& MetricsManager::CreateGauge(
      const std::shared_ptr<prometheus::Registry>& registry,
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& labels
    )
    {
        if (!registry)
        {
            throw std::invalid_argument("Registry cannot be null");
        }
        ValidateMetricName(name);
        ValidateLabels(labels);

        auto& family = prometheus::BuildGauge().Name(name).Help(help).Register(
          *registry
        );

        return family.Add(MergeLabels(labels));
    }

    prometheus::Histogram& MetricsManager::CreateHistogram(
      const std::shared_ptr<prometheus::Registry>& registry,
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& labels,
      const std::vector<double>& buckets
    )
    {
        if (!registry)
        {
            throw std::invalid_argument("Registry cannot be null");
        }
        ValidateMetricName(name);
        ValidateLabels(labels);

        auto&
          family = prometheus::BuildHistogram().Name(name).Help(help).Register(
            *registry
          );

        return family.Add(MergeLabels(labels), buckets);
    }

    bool MetricsManager::IsInitialized()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return initialized_;
    }

    bool MetricsManager::IsThreadingEnabled()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        return threading_enabled_;
    }

    double MetricsManager::GetUptimeSeconds()
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (start_time_ == std::chrono::steady_clock::time_point {})
        {
            start_time_ = std::chrono::steady_clock::now();
        }
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time_).count();
    }

    void MetricsManager::ValidateMetricName(const std::string& name)
    {
        if (name.empty())
        {
            throw std::invalid_argument("Metric name cannot be empty");
        }
        if (name.size() >= 2 && name[0] == '_' && name[1] == '_')
        {
            throw std::invalid_argument(
              "Metric name '" + name +
              "' cannot start with '__' (reserved prefix)"
            );
        }

        static const std::regex name_regex("^[a-zA-Z_:][a-zA-Z0-9_:]*$");
        if (!std::regex_match(name, name_regex))
        {
            throw std::invalid_argument(
              "Invalid metric name '" + name +
              "'. Must match [a-zA-Z_:][a-zA-Z0-9_:]*"
            );
        }
    }

    void MetricsManager::ValidateLabels(
      const std::map<std::string, std::string>& labels
    )
    {
        static const std::regex label_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");

        for (const auto& [key, value]: labels)
        {
            if (key.empty())
            {
                throw std::invalid_argument("Label name cannot be empty");
            }
            if (key.size() >= 2 && key[0] == '_' && key[1] == '_')
            {
                throw std::invalid_argument(
                  "Label name '" + key +
                  "' cannot start with '__' (reserved prefix)"
                );
            }
            if (!std::regex_match(key, label_regex))
            {
                throw std::invalid_argument(
                  "Invalid label name '" + key +
                  "'. Must match [a-zA-Z_][a-zA-Z0-9_]*"
                );
            }
            if (!ValidateLabelValue(value))
            {
                throw std::invalid_argument(
                  "Label value for '" + key + "' contains invalid characters"
                );
            }
        }
    }

    bool MetricsManager::ValidateLabelValue(const std::string& value)
    {
        // Allow empty values and printable ASCII/UTF-8
        for (char c: value)
        {
            if (c < 32 && c != '\t')
            { // Reject control chars except tab
                return false;
            }
        }
        return true;
    }

} // namespace vex
