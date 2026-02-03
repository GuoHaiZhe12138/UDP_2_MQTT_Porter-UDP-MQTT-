#ifndef SENTINEL_GATE_HPP
#define SENTINEL_GATE_HPP

#include <mosquitto.h>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include "SafeQueue.hpp"

namespace sentinel {

class SentinelGate {
public:
    // 禁用拷贝构造和赋值
    SentinelGate(const SentinelGate&) = delete;
    SentinelGate& operator=(const SentinelGate&) = delete;

    SentinelGate(const std::string& mqtt_host, int mqtt_port);
    ~SentinelGate();

    void start(const std::string& group_ip, int udp_port, const std::string& device);
    void setMqttQos(const int qos);
    int  getMqttQos(void);
    void setMqttTopic(const std::string& topic);
    std::string getMqttTopic(void);
    void stop();

private:
    void udpListenerLoop(const std::string& ip, int port, const std::string& device);
    void mqttWorkerLoop();

private:
    struct mosquitto* mosq_{nullptr};

    std::atomic<bool> running_{false};

    std::atomic<int> mqtt_qos_{0};

    std::string mqtt_topic_{"sentinel/sensor/raw"};
    std::mutex topic_mtx_;

    std::thread udp_thread_;
    std::thread mqtt_thread_;
    int udp_fd_{-1};

    SafeQueue<std::vector<uint8_t>> data_queue_;
};

} // namespace sentinel

#endif // SENTINEL_GATE_HPP
