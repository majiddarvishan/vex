#include <vex/monitoring/raii_guards.hpp>

#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <utility>
#include <type_traits>

namespace vex
{
namespace
{
    using GuardKey = std::pair<const void*, const void*>;

    struct GuardKeyLess
    {
        bool operator()(const GuardKey& a, const GuardKey& b) const
        {
            const auto less = std::less<const void*> {};
            if (less(a.first, b.first))
                return true;
            if (less(b.first, a.first))
                return false;
            return less(a.second, b.second);
        }
    };

    std::mutex scoped_guard_mutex;

    // Separate reference counts for each Prometheus metric type.
    // A Family::Add call may return an existing series when labels match;
    // never remove that series while another guard still uses it.
    template <typename Metric>
    std::map<GuardKey, std::size_t, GuardKeyLess>& active_guards()
    {
        static std::map<GuardKey, std::size_t, GuardKeyLess> refs;
        return refs;
    }

    template <typename Metric>
    Metric* acquire(
      prometheus::Family<Metric>& family,
      const std::map<std::string, std::string>& labels
    )
    {
        std::lock_guard<std::mutex> guard(scoped_guard_mutex);

        // Family::Add can return an existing metric when labels match.
        // Do not claim ownership of a series created outside the RAII
        // helpers: removing it at guard exit would erase somebody else's
        // persistent counters/gauges.
        // Resolve and validate a single immutable label snapshot before
        // both Has and Add. Calling add_counter/add_gauge here used to
        // MERGE AGAIN, allowing SetDefaultLabels to change identity between
        // our ownership check and actual Family::Add.
        const auto effective_labels = MetricsManager::MergeLabels(labels);
        const bool existed = family.Has(effective_labels);
        Metric* metric = &family.Add(effective_labels);
        auto& refs = active_guards<Metric>();
        const GuardKey key{&family, metric};
        const auto current = refs.find(key);
        if (existed && current == refs.end())
        {
            throw std::invalid_argument(
              "Scoped metric labels already belong to an external series"
            );
        }

        try
        {
            if (current == refs.end())
                refs.emplace(key, 1);
            else
                ++current->second;
        }
        catch (...)
        {
            // If bookkeeping fails after a fresh Family::Add, clean up
            // our newly added series. Do not remove externally owned ones.
            if (!existed)
            {
                try
                {
                    family.Remove(metric);
                }
                catch (...) {}
            }
            throw;
        }
        return metric;
    }

    template <typename Metric>
    void release(
      prometheus::Family<Metric>* family, Metric* metric
    ) noexcept
    {
        if (!family || !metric)
            return;

        std::lock_guard<std::mutex> guard(scoped_guard_mutex);
        auto& refs = active_guards<Metric>();
        const auto it = refs.find({family, metric});
        if (it == refs.end())
            return;

        if (--it->second == 0)
        {
            refs.erase(it);
            // No guard can acquire while the old series is being removed.
            // Family removal is safe only while the caller retains its
            // Family/Registry (required lifetime contract).
            try
            {
                if constexpr (std::is_same_v<Metric, prometheus::Counter>)
                    MetricsManager::remove_counter(*family, metric);
                else
                    MetricsManager::remove_gauge(*family, metric);
            }
            catch (...)
            {
                // Destruction/assignment must not throw.
            }
        }
    }
} // namespace

    ScopedCounter::ScopedCounter(
      prometheus::Family<prometheus::Counter>& family,
      const std::map<std::string, std::string>& labels
    )
        : family_(&family),
          metric_(acquire(family, labels))
    {
    }

    ScopedCounter::~ScopedCounter()
    {
        release(family_, metric_);
    }

    ScopedCounter::ScopedCounter(ScopedCounter&& other) noexcept
        : family_(std::exchange(other.family_, nullptr)),
          metric_(std::exchange(other.metric_, nullptr))
    {
    }

    ScopedCounter& ScopedCounter::operator=(ScopedCounter&& other) noexcept
    {
        if (this != &other)
        {
            release(family_, metric_);
            family_ = std::exchange(other.family_, nullptr);
            metric_ = std::exchange(other.metric_, nullptr);
        }
        return *this;
    }

    ScopedGauge::ScopedGauge(
      prometheus::Family<prometheus::Gauge>& family,
      const std::map<std::string, std::string>& labels
    )
        : family_(&family),
          metric_(acquire(family, labels))
    {
    }

    ScopedGauge::~ScopedGauge()
    {
        release(family_, metric_);
    }

    ScopedGauge::ScopedGauge(ScopedGauge&& other) noexcept
        : family_(std::exchange(other.family_, nullptr)),
          metric_(std::exchange(other.metric_, nullptr))
    {
    }

    ScopedGauge& ScopedGauge::operator=(ScopedGauge&& other) noexcept
    {
        if (this != &other)
        {
            release(family_, metric_);
            family_ = std::exchange(other.family_, nullptr);
            metric_ = std::exchange(other.metric_, nullptr);
        }
        return *this;
    }

    ScopedInFlightGauge::ScopedInFlightGauge(prometheus::Gauge& gauge)
        : gauge_(&gauge)
    {
        gauge_->Increment();
    }

    ScopedInFlightGauge::~ScopedInFlightGauge() noexcept
    {
        if (gauge_)
            gauge_->Decrement();
    }

    ScopedInFlightGauge::ScopedInFlightGauge(
      ScopedInFlightGauge&& other
    ) noexcept
        : gauge_(std::exchange(other.gauge_, nullptr))
    {
    }

    ScopedInFlightGauge& ScopedInFlightGauge::operator=(
      ScopedInFlightGauge&& other
    ) noexcept
    {
        if (this != &other)
        {
            if (gauge_)
                gauge_->Decrement();
            gauge_ = std::exchange(other.gauge_, nullptr);
        }
        return *this;
    }

} // namespace vex
