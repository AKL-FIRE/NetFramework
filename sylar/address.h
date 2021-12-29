#ifndef SYLAR_SYLAR_ADDRESS_H_
#define SYLAR_SYLAR_ADDRESS_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <map>

namespace sylar {

class IPAddress;

class Address {
  public:
    using ptr = std::shared_ptr<Address>;
    virtual ~Address() = default;

    int getFamily() const;

    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr* getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;

    virtual std::ostream& insert(std::ostream& os) const = 0;
    std::string toString();

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;

    static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);
    static bool Lookup(std::vector<Address::ptr>& result, 
        const std::string& host, int family = AF_INET, 
        int type = 0, int protocol = 0);
    static Address::ptr LookupAny(const std::string& host, int family = AF_INET, 
        int type = 0, int protocol = 0);
    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host, 
        int family = AF_INET, int type = 0, int protocol = 0);
    static bool GetInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
        int family = AF_INET);
    static bool GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>>& result,
        const std::string& iface, int family = AF_INET);
};

class IPAddress : public Address {
  public:
    using ptr = std::shared_ptr<IPAddress>;

    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint16_t v) = 0;

    static IPAddress::ptr Create(const char* address, uint16_t port);
};

class IPv4Address : public IPAddress {
  public:
    using ptr = std::shared_ptr<IPv4Address>;
    IPv4Address(const sockaddr_in& address);
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;

    static IPv4Address::ptr Create(const char* address, uint16_t port = 0);
  private:
    sockaddr_in m_addr;
};


class IPv6Address : public IPAddress {
  public:
    using ptr = std::shared_ptr<IPv6Address>;
    IPv6Address();
    IPv6Address(const uint8_t address[16], uint16_t port);
    IPv6Address(const sockaddr_in6& address);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;
    std::ostream& insert(std::ostream& os) const override;

    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void setPort(uint16_t v) override;

    static IPv6Address::ptr Create(const char* address, uint16_t port = 0);
  private:
    sockaddr_in6 m_addr;
};

class UnixAddress : public Address {
  public:
    using ptr = std::shared_ptr<UnixAddress>;
    UnixAddress();
    UnixAddress(const std::string& path);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    void setAddrLen(uint32_t v);
    socklen_t getAddrLen() const override;

    std::ostream& insert(std::ostream& os) const override;
  private:
    sockaddr_un m_addr;
    socklen_t m_length;
};

class UnknowAddress : public Address {
  public:
    using ptr = std::shared_ptr<UnknowAddress>;
    UnknowAddress(int family);
    UnknowAddress(const sockaddr& addr);

    const sockaddr* getAddr() const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen() const override;

    std::ostream& insert(std::ostream& os) const override;
private:
    sockaddr m_addr;
};

}

#endif
