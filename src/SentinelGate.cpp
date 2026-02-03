#include "SentinelGate.hpp"
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace sentinel {

SentinelGate::SentinelGate(const std::string& mqtt_host, int mqtt_port)
    : mosq_(nullptr),
      running_(false),
      udp_fd_(-1),
      data_queue_(1000) {

    mosquitto_lib_init();

    mosq_ = mosquitto_new("SentinelGate_Client", true, nullptr);
    if (!mosq_) {
        perror("mosquitto_new failed");
        stop();
        return;
    }

    mosquitto_threaded_set(mosq_, true);

    // Connect to MQTT broker
    int rc = mosquitto_connect(mosq_, mqtt_host.c_str(), mqtt_port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        mosquitto_lib_cleanup();
        throw std::runtime_error(mosquitto_strerror(rc));
    }

    running_.store(false);

    mosquitto_loop_start(mosq_);
}

SentinelGate::~SentinelGate() {
    stop();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
    mosquitto_lib_cleanup();
}

void SentinelGate::start(const std::string& ip, int port, const std::string& device) {
    running_.store(true);

    udp_thread_ = std::thread(
        &SentinelGate::udpListenerLoop, this, ip, port, device
    );

    mqtt_thread_ = std::thread(
        &SentinelGate::mqttWorkerLoop, this
    );
}

void SentinelGate::stop() {
    if (!running_.exchange(false))
        return;

    // 等待 MQTT 循环停止
    mosquitto_loop_stop(mosq_, true);

    // 唤醒等待的线程
    data_queue_.close();

    if (udp_thread_.joinable())
        udp_thread_.join();

    if (mqtt_thread_.joinable())
        mqtt_thread_.join();
}

void SentinelGate::setMqttQos(const int qos)
{
    mqtt_qos_.store(qos);
}

int SentinelGate::getMqttQos(void)
{
    return mqtt_qos_.load();
}

void SentinelGate::setMqttTopic(const std::string& topic)
{
    std::lock_guard<std::mutex> lock(topic_mtx_);
    mqtt_topic_ = topic;
}

std::string SentinelGate::getMqttTopic(void)
{
    std::lock_guard<std::mutex> lock(topic_mtx_);
    return mqtt_topic_;
}

void SentinelGate::udpListenerLoop(const std::string& ip, int port, const std::string& device) {
    // 创建 UDP 套接字并加入组播组
    udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd_ < 0) {
        perror("socket");
        stop();
        return;
    }

    // 绑定到本地地址和端口
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    // 允许ip和端口重复绑定
    int reuse = 1;
    setsockopt(udp_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(udp_fd_, (sockaddr*)&local, sizeof(local)) < 0) {
        perror("bind");
        close(udp_fd_);
        stop();
        return;
    }

    // 加入组播组
    ip_mreqn mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str()); // 224.0.1.100
    mreq.imr_ifindex = if_nametoindex(device.c_str());
    if(mreq.imr_ifindex == 0) {
        perror("Invalid network device");
        close(udp_fd_);
        stop();
        return;
    }
    setsockopt(udp_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    // 设置接收超时，以便能够定期检查 running_ 标志
    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 0.5 秒
    setsockopt(udp_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buffer[2048];

    while (running_.load()) {

        std::cout << "[udp loop] running" << std::endl;

        // 接收数据
        ssize_t len = recvfrom(udp_fd_, buffer, sizeof(buffer), 0, nullptr, nullptr);
        if (len <= 0) {
            if (!running_.load())
                break; 
            continue;
        }
        // 将数据放入队列
        data_queue_.push(
            std::vector<uint8_t>(buffer, buffer + len)
        );
    }
    close(udp_fd_);
}

void SentinelGate::mqttWorkerLoop() {
    std::vector<uint8_t> data;
    while (data_queue_.pop(data)) {

        std::cout << "[mqtt loop] running" << std::endl;

        int ret = mosquitto_publish(
            mosq_,
            nullptr,
            getMqttTopic().c_str(),
            data.size(),
            data.data(),
            getMqttQos(),
            false
        );

        if (ret != MOSQ_ERR_SUCCESS) {
            perror("mosquitto_publish failed");
            stop();
            return;
        }
    }
}

}
