#ifndef TUN_TAP_HPP
#define TUN_TAP_HPP

#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

#include <fcntl.h>
#include <linux/if_tun.h>
#include <memory.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>


namespace tun_tap {

enum class mode {
    TUN, TAP
};

class iface {
    class fd {
    public:
        fd() : _fd { -1 } { }

        fd(std::string_view name, int flags) : _fd { -1 } { 
            using namespace std::string_literals;
            if((_fd = open(name.data(), flags)) < 0) {
                throw std::runtime_error("can't open "s + std::string { name });
            }
        }

        ~fd() { if (_fd != -1) close(_fd);}

        fd(fd&& o) { _fd = o._fd; o._fd = -1; }
        fd& operator=(fd&& o) { 
            _fd = o._fd; o._fd = -1;
            return *this;
        }

        operator int() const { return _fd; }

    private:
        fd(const fd&) = delete;
        fd& operator=(const fd&) = delete;

    private:
        int _fd;
    };

public:
    iface(mode mode, bool packet_info = true, size_t n_queues = 1);
    iface(std::string name, mode mode, bool packet_info = true, size_t n_queues = 1);

    iface(iface&&) = default;
    iface& operator=(iface&&) = default;

    ~iface() = default;

    size_t read(void* buf, size_t n_bytes, size_t queue = 0) const;

    size_t write(const void* buf, size_t n_bytes, size_t queue = 0) const;

    size_t get_n_queues() const noexcept { return _queues.size(); }

    std::string_view get_name() const noexcept { return _name; } 

private:
    iface(const iface&) = delete;
    iface& operator=(const iface&) = delete;

private:
    mode            _mode;
    std::string     _name;
    std::vector<fd> _queues;
};

iface::iface(mode mode, bool packet_info, size_t n_queues) 
    : iface::iface("", mode, packet_info, n_queues) 
{ }

iface::iface(std::string name, mode mode, bool packet_info, size_t n_queues)
    : _mode { mode }, _name { std::move(name) }, _queues { n_queues }
{
    using namespace std::string_literals;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof (ifr));


    switch (mode) {
    case mode::TUN: ifr.ifr_flags = IFF_TUN; break;
    case mode::TAP: ifr.ifr_flags = IFF_TAP; break;
    }

    ifr.ifr_flags |= !packet_info ? IFF_NO_PI : 0;
    ifr.ifr_flags |= n_queues > 1 ? IFF_MULTI_QUEUE : 0;

    if (IFNAMSIZ < _name.size()) {
       throw std::runtime_error("name is too long. Maximum lenght is "s + std::to_string(IFNAMSIZ) + " bytes"s);
    }

    if (_name != "") {
        strncpy(ifr.ifr_name, _name.c_str(), _name.size());
    }

    for (auto& queue : _queues) {
        fd fd { "/dev/net/tun", O_RDWR };
        if(int err; (err = ioctl(fd, TUNSETIFF, &ifr)) < 0) {
            close(fd);
            throw std::runtime_error("ioctl returned "s + std::to_string(err));
        }

        if (_name.empty()) {
            _name = ifr.ifr_name;
        }
        queue = std::move(fd);
    }
}

size_t iface::read(void* buf, size_t n_bytes, size_t queue) const
{
    ssize_t res = ::read(_queues[queue], buf, n_bytes);
    if (res < 0) {
        throw std::runtime_error("n bytes read < 0");
    }
    return static_cast<size_t>(res);
}

size_t iface::write(const void* buf, size_t n_bytes, size_t queue) const
{
    ssize_t res = ::write(_queues[queue], buf, n_bytes);
    if (res < 0) {
        throw std::runtime_error("n bytes written < 0");
    }
    return static_cast<size_t>(res);
}

};

#endif // TUN_TAP_HPP