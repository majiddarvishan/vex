// Loopback HTTP integration test for Vex /metrics, using an ephemeral
// OS-assigned port. No curl or external Prometheus server needed.
#include <vex/monitoring/monitoring.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
class SocketHandle
{
public:
    explicit SocketHandle(int fd) : fd_(fd) {}
    ~SocketHandle()
    {
        if (fd_ >= 0)
            ::close(fd_);
    }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};

std::string http_get(int port)
{
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1)
        throw std::runtime_error("failed to build loopback address");

    // Exposer construction normally starts its listener immediately;
    // retries avoid depending on scheduler timing under sanitizer builds.
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        SocketHandle socket(::socket(AF_INET, SOCK_STREAM, 0));
        if (socket.get() < 0)
            throw std::runtime_error("socket creation failed");

        if (::connect(socket.get(),
                      reinterpret_cast<sockaddr*>(&address),
                      sizeof(address)) != 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        struct timeval timeout {};
        timeout.tv_sec = 2;
        (void)::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                           &timeout, sizeof(timeout));
        (void)::setsockopt(socket.get(), SOL_SOCKET, SO_SNDTIMEO,
                           &timeout, sizeof(timeout));

        const std::string request =
          "GET /metrics HTTP/1.1\r\n"
          "Host: 127.0.0.1\r\n"
          "Connection: close\r\n"
          "Accept-Encoding: identity\r\n\r\n";

        std::size_t written = 0;
        while (written < request.size())
        {
            const auto n = ::send(socket.get(),
              request.data() + written, request.size() - written, 0);
            if (n <= 0)
                throw std::runtime_error("HTTP send failed");
            written += static_cast<std::size_t>(n);
        }

        std::string response;
        char buffer[4096];
        for (;;)
        {
            const auto n = ::recv(socket.get(), buffer, sizeof(buffer), 0);
            if (n == 0)
                break;
            if (n < 0)
                throw std::runtime_error("HTTP recv failed");
            response.append(buffer, static_cast<std::size_t>(n));
            if (response.size() > 1024 * 1024)
                throw std::runtime_error("HTTP metrics response too large");
        }

        if (response.rfind("HTTP/1.1 200", 0) != 0 &&
            response.rfind("HTTP/1.0 200", 0) != 0)
            throw std::runtime_error("HTTP /metrics did not return 200");
        const auto split = response.find("\r\n\r\n");
        if (split == std::string::npos)
            throw std::runtime_error("invalid HTTP headers");
        return response.substr(split + 4);
    }
    throw std::runtime_error("loopback Exposer did not accept a connection");
}

bool contains(const std::string& body, const std::string& text)
{
    return body.find(text) != std::string::npos;
}
} // namespace

int main()
{
    using vex::MetricsManager;
    if (!MetricsManager::Init(true))
        return 1;

    auto main_registry = MetricsManager::GetRegistry();
    auto subsystem = MetricsManager::GetSubsystemRegistry("integration");
    auto& counter = MetricsManager::CreateCounter(
      main_registry, "vex_http_requests_total", "HTTP requests",
      {{"method", "GET"}});
    counter.Increment(3);
    auto& gauge = MetricsManager::CreateGauge(
      main_registry, "vex_http_inflight", "Inflight HTTP work");
    gauge.Set(2);
    auto& histogram = MetricsManager::CreateHistogram(
      subsystem, "vex_http_duration_seconds", "Request duration",
      {}, {0.1, 0.5, 1.0});
    histogram.Observe(0.2);

    {
        prometheus::Exposer exposer("127.0.0.1:0", 1);
        const auto ports = exposer.GetListeningPorts();
        if (ports.size() != 1 || ports[0] <= 0)
            return 2;
        const int port = ports.front();

        MetricsManager::RegisterAllWithExposer(exposer);
        const auto first = http_get(port);
        if (!contains(first, "vex_http_requests_total") ||
            !contains(first, "vex_http_inflight") ||
            !contains(first, "vex_http_duration_seconds_bucket") ||
            !contains(first, "vex_http_duration_seconds_count") ||
            !contains(first, "method=\"GET\""))
            return 3;

        auto transient = std::make_shared<prometheus::Registry>();
        auto& temporary = MetricsManager::CreateCounter(
          transient, "vex_http_transient_total", "Transient counter");
        temporary.Increment();
        exposer.RegisterCollectable(transient);
        if (!contains(http_get(port), "vex_http_transient_total"))
            return 4;

        transient.reset();
        const auto after_expiry = http_get(port);
        if (contains(after_expiry, "vex_http_transient_total") ||
            !contains(after_expiry, "vex_http_requests_total"))
            return 5;

        // An Exposer retains only weak Registry handles. After the
        // manager and all callers release their shared_ptr snapshots,
        // a future scrape must not touch deleted Registries.
        main_registry.reset();
        subsystem.reset();
        MetricsManager::Reset();
        const auto after_reset = http_get(port);
        if (contains(after_reset, "vex_http_requests_total") ||
            contains(after_reset, "vex_http_duration_seconds"))
            return 6;
    } // Exposer shutdown must be clean and bounded.
    return 0;
}
