#include "../include/discovery.h"
#include "../vendor/mdns/mdns.h"

#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <iostream>
#include <ifaddrs.h>
#include <net/if.h>
#include <thread>

bool DEBUG = false;

struct DeviceRecord {
    std::string name;
    std::string hostname;
    std::string ip;
    uint16_t port = 0;
    std::string mac;

    bool password_required = false;
    int encryption_types = 0;
    int codec_support = 0;
};

struct CallbackContext {
    std::map<std::string, DeviceRecord> devices;     
    std::map<std::string, std::string>  hostname_to_instance;
};

static int query_callback(
    int sock,
    const struct sockaddr* from,
    size_t addrlen,
    mdns_entry_type_t entry,
    uint16_t query_id,
    uint16_t rtype,
    uint16_t rclass,
    uint32_t ttl,
    const void* data,
    size_t size,
    size_t name_offset,
    size_t name_length,   
    size_t record_offset,
    size_t record_length,
    void* user_data)
{
    
    if (DEBUG) std::cout << "[callback] fired rtype=" << rtype << " entry=" << entry << "\n";

    char src_ip[INET_ADDRSTRLEN] = {0};
    const struct sockaddr_in* from4 = (const struct sockaddr_in*)from;
    inet_ntop(AF_INET, &from4->sin_addr, src_ip, sizeof(src_ip));
    if (DEBUG) std::cout << "[callback] from=" << src_ip << " rtype=" << rtype << std::endl;

    CallbackContext* ctx = static_cast<CallbackContext*>(user_data);

    char name_buf[256] = {0};
    size_t offset = name_offset;
    mdns_string_t name = mdns_string_extract(data, size, &offset, name_buf, sizeof(name_buf));
    std::string record_name(name.str, name.length);

    if (rtype == MDNS_RECORDTYPE_PTR) {
        char ptr_buf[256] = {0};
        mdns_string_t ptr = mdns_record_parse_ptr(data, size, record_offset, record_length, ptr_buf, sizeof(ptr_buf));
        std::string instance(ptr.str, ptr.length);
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) raw instance='" << instance << "', but record_name='" << record_name  << "'. Length of instance=" << sizeof(instance) << ", length of record_name=" << sizeof(record_name) << std::endl;

        if (DEBUG) std::cout << "[discovery.cpp] (PTR) raw ptr='" << ptr.str << "'\n"; 

        // Only care about actual RAOP instance records, not service listing PTRs
        size_t at   = instance.find('@');
        size_t raop = instance.find("_raop._tcp");
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) at=" << at << std::endl;
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) raop=" << raop << std::endl;
        if (at == std::string::npos || raop == std::string::npos) {
            if (DEBUG) std::cout << "[discovery.cpp] (PTR) skipping (no @ or _raop._tcp)" << std::endl;
            return 0;
        }

        auto& dev  = ctx->devices[instance];
        dev.mac    = instance.substr(0, at);
        dev.name   = instance.substr(at + 1, raop - at - 1);
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) dev.mac=" << dev.mac << std::endl;
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) dev.name=" << dev.name << std::endl;
        
        if (dev.ip.empty()) {
            dev.ip = std::string(src_ip);
        }

        if (dev.port == 0) {
            dev.port = 5000; // AirPlay 1 default, will be overwritten if SRV fires
        }
        if (DEBUG) std::cout << "[discovery.cpp] (PTR) found device: name='" << dev.name << "' mac='" << dev.mac << "'\n";
    
    } else if (rtype == MDNS_RECORDTYPE_SRV) {
        char srv_buf[256] = {0};
        mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length,
                                                    srv_buf, sizeof(srv_buf));
        auto& dev    = ctx->devices[record_name];
        dev.hostname = std::string(srv.name.str, srv.name.length);
        dev.port     = srv.port;
        ctx->hostname_to_instance[dev.hostname] = record_name;

        if (dev.ip.empty()) {
            dev.ip = std::string(src_ip);  // src_ip already computed at top of callback
        }

        // Extract name and MAC from record_name which is the full instance string
        // e.g. "9C783EB55E41@Legacy PiPod._raop._tcp.local."
        if (dev.name.empty()) {
            auto at   = record_name.find('@');
            auto raop = record_name.find("._raop._tcp");
            if (at != std::string::npos && raop != std::string::npos) {
                dev.mac  = record_name.substr(0, at);
                dev.name = record_name.substr(at + 1, raop - at - 1);
            }
        }

    } else if (rtype == MDNS_RECORDTYPE_TXT) {
        if (DEBUG) std::cout << "[discovery.cpp] (TXT) record_name='" << record_name << "'\n";
        mdns_record_txt_t txt_records[32];
        size_t count = mdns_record_parse_txt(data, size, record_offset, record_length, txt_records, 32);
        auto& dev = ctx->devices[record_name];
        for (size_t i = 0; i < count; ++i) {
            std::string key(txt_records[i].key.str, txt_records[i].key.length);
            std::string val(txt_records[i].value.str, txt_records[i].value.length);
            if (key == "pw") dev.password_required = (val == "true");
            if (key == "et") dev.encryption_types = std::stoi(val);
            if (key == "cn") dev.codec_support = std::stoi(val);
        }

    } else if (rtype == MDNS_RECORDTYPE_A) {
        if (DEBUG) std::cout << "[discovery.cpp] (A) record_name='" << record_name << "'\n";
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);

        char ip_buf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &addr.sin_addr, ip_buf, sizeof(ip_buf));
        std::string ip(ip_buf);

        auto it = ctx->hostname_to_instance.find(record_name);
        if (it != ctx->hostname_to_instance.end()) {
            ctx->devices[it->second].ip = ip;
        }
    }

    return 0;
}

std::vector<AirPlayDevice> discover_devices(int timeout_ms, bool debug_input) {
    if (debug_input) DEBUG = true;
    
    CallbackContext ctx;

    uint8_t buffer[2048];

    std::vector<int> sockets;

    struct ifaddrs* ifaddr;
    getifaddrs(&ifaddr);

    for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;

        sockaddr_in* addr = (sockaddr_in*)ifa->ifa_addr;

        addr->sin_port = htons(5353);

        int sock = mdns_socket_open_ipv4(addr);
        if (sock < 0) continue;

        ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
        mreq.imr_interface = addr->sin_addr;

        setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

        sockets.push_back(sock);
    }

    freeifaddrs(ifaddr);

    for (int sock : sockets) {
        mdns_query_send(
            sock,
            MDNS_RECORDTYPE_PTR,
            "_raop._tcp.local",
            strlen("_raop._tcp.local"),
            buffer,
            sizeof(buffer),
            0
        );
        mdns_discovery_send(sock);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    for (int sock : sockets) {
        mdns_query_send(
            sock,
            MDNS_RECORDTYPE_PTR,
            "_raop._tcp.local",
            strlen("_raop._tcp.local"),
            buffer,
            sizeof(buffer),
            0
        );
        mdns_discovery_send(sock);
    }

    if (DEBUG) std::cout << "[discovery.cpp] query sent, listening...\n";

    auto start = std::chrono::steady_clock::now(); 

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= timeout_ms) break;

        int remaining = timeout_ms - (int)elapsed;

        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        for (int sock : sockets) {
            FD_SET(sock, &readfds);
            if (sock > max_fd) max_fd = sock;
        }

        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 100000; // 100ms poll

        int ready = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        for (int sock : sockets) {
            if (FD_ISSET(sock, &readfds)) {
                mdns_query_recv(sock, buffer, sizeof(buffer), query_callback, &ctx, 0);
            }
        }
    }

    for (int sock : sockets) {
        mdns_socket_close(sock);
    }

    std::vector<AirPlayDevice> results;
    if (DEBUG) std::cout << "[discovery.cpp] Raw device map:" << std::endl;
    for (auto& [key, rec] : ctx.devices) {
        if (DEBUG) std::cout << "  key='" << key << "' ip'" << rec.ip << "' port=" << rec.port << " name='" << rec.name << "'\n";
        if (rec.ip.empty() || rec.port == 0) continue;

        AirPlayDevice dev;
        dev.name              = rec.name;
        dev.hostname          = rec.hostname;
        dev.ip                = rec.ip;
        dev.port              = rec.port;
        dev.mac               = rec.mac;
        dev.password_required = rec.password_required;
        dev.encryption_types  = rec.encryption_types;
        dev.codec_support     = rec.codec_support;
        results.push_back(dev);
    }

    return results;
}