#include "address.h"
#include "sylar_endian.h"
#include "log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <cstring>
#include <memory>
#include <sstream>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

template <typename T>
static T createMask(uint32_t bits) {
  // 例如T == uint32_t bits == 8 
  // 1 00000000 00000000 00000000
  // 0 11111111 11111111 11111111
  return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <typename T>
static uint32_t CountBytes(T value) {
  uint32_t result = 0;
  // 统计value中为1的位的个数
  for(; value; ++result) {
    value &= value - 1; // 从1->0与自己按位与消掉这个1,从0->1与自己按位与没变化
  }
  return result;
}

int Address::getFamily() const {
  return getAddr()->sa_family;
}

std::string Address::toString() {
  std::stringstream ss;
  insert(ss);
  return ss.str();
}

bool Address::operator<(const Address& rhs) const {
  socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
  int result = memcmp(getAddr(), rhs.getAddr(), minlen);
  if (result < 0) {
    return true;
  } else if (result > 0) {
    return false;
  } else if (getAddrLen() < rhs.getAddrLen()) {
    return true;
  }
  return false;
}

bool Address::operator==(const Address& rhs) const {
  return getAddrLen() == rhs.getAddrLen()
    && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
  return !(*this == rhs);
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
  if (addr == nullptr) {
    return nullptr;
  }

  Address::ptr result;
  switch(addr->sa_family) {
    case AF_INET:
      result.reset(new IPv4Address(*(const sockaddr_in*)addr));
      break;
    case AF_INET6:
      result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
      break;
    default:
      result.reset(new UnknowAddress(*addr));
      break;
  }
  return result;
}

Address::ptr Address::LookupAny(const std::string& host, int family, 
        int type, int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    return result[0];
  }
  return nullptr;
}

std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host, int family, 
        int type, int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    for (auto& e : result) {
      IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(e);
      if (v) {
        return v;
      }
    }
  }
  return nullptr;
}

bool Address::GetInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
        int family) {
  struct ifaddrs* next, *results;
  if (getifaddrs(&results) != 0) {
    SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress getifaddrs "
      << " err=" << errno << " errstr=" << strerror(errno);
    return false;
  }

  try {
    for (next = results; next != nullptr; next = next->ifa_next) {
      Address::ptr addr;
      uint32_t prefix_length = ~0u;
      if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
        continue;
      }
      switch (next->ifa_addr->sa_family) {
        case AF_INET:
          {
            addr = Create(next->ifa_addr, sizeof(sockaddr_in));
            uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
            prefix_length = CountBytes(netmask);
          }
          break;
        case AF_INET6:
          {
            addr = Create(next->ifa_addr, sizeof(sockaddr_in6));
            in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
            prefix_length = 0;
            for (int i = 0; i < 16; ++i) {
              prefix_length += CountBytes(netmask.s6_addr[i]);
            }
          }
          break;
        default:
          break;
      }

      if (addr) {
        result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_length)));
      }
    }
  } catch (...) {
    SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddress exception";
    freeifaddrs(results);
    return false;
  }
  freeifaddrs(results);
  return true;
}

bool Address::GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>>& result,
        const std::string& iface, int family) {
  if (iface.empty() || iface == "*") {
    if (family == AF_INET || family == AF_UNSPEC) {
      result.emplace_back(Address::ptr(new IPv4Address()), 0u);
    }
    if (family == AF_INET6 || family == AF_UNSPEC) {
      result.emplace_back(Address::ptr(new IPv6Address()), 0u);
    }
    return true;
  }

  std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
  if (!GetInterfaceAddress(results, family)) {
    return false;
  }

  auto its = results.equal_range(iface);
  for (; its.first != its.second; ++its.first) {
    result.push_back(its.first->second);
  }
  return true;
} 

bool Address::Lookup(std::vector<Address::ptr>& result, 
    const std::string& host, int family, int type, int protocol) {
  addrinfo hints, *results, *next;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = 0;
  hints.ai_family = family;
  hints.ai_socktype = type;
  hints.ai_protocol = protocol;
  hints.ai_addrlen = 0;
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  std::string node;
  const char* service = nullptr;

  // 检查 ipv6address service
  if (!host.empty() && host[0] == '[') {
    const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
    if (endipv6) {
      if (*(endipv6 + 1) == ':') {
        service = endipv6 + 2;
      }
      node = host.substr(1, endipv6 - host.c_str() - 1);
    }
  }

  // 检查 node service
  if (node.empty()) {
    service = (const char*)memchr(host.c_str(), ':', host.size());
    if (service) {
      if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
        node = host.substr(0, service - host.c_str());
        ++service;
      }
    }
  }
  
  if (node.empty()) {
    node = host;
  }
  int error = getaddrinfo(node.c_str(), service, &hints, &results);
  if (error) {
    SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddress(" << host
      << ", " << family << ", " << type << ") err=" << error << " errstr="
      << strerror(errno);
    return false;
  }

  next = results;
  while (next) {
    result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
    next = next->ai_next;
  }

  freeaddrinfo(results);
  return true;
}

IPAddress::ptr IPAddress::Create(const char* address, uint32_t port) {
  addrinfo hints, *results;
  memset(&hints, 0, sizeof(hints));

  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = AF_UNSPEC;

  int error = getaddrinfo(address, nullptr, &hints, &results);
  if (error) {
    SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address
      << ", " << port << ") error=" << error << " errno="
      << errno << " errstr=" << strerror(errno);
    return nullptr;
  }

  try {
    IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
        Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
    if (result) {
      result->setPort(port);
    }
    freeaddrinfo(results);
    return result;
  } catch (...) {
    freeaddrinfo(results);
    return nullptr;
  }
}

IPv4Address::IPv4Address(const sockaddr_in& address) {
  m_addr = address;
}

IPv4Address::IPv4Address(uint32_t address, uint32_t port) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin_family = AF_INET;
  m_addr.sin_port = byteswapOnLittleEndian(port); 
  m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

const sockaddr* IPv4Address::getAddr() const {
  return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const {
  return sizeof(m_addr);
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
  uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
  os << ((addr >> 24) & 0xff) << "."
     << ((addr >> 16) & 0xff) << "."
     << ((addr >> 8) & 0xff) << "."
     << ((addr) & 0xff);
  os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
  return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(m_addr);
  baddr.sin_addr.s_addr |= byteswapOnLittleEndian(createMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(baddr);
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(m_addr);
  baddr.sin_addr.s_addr &= byteswapOnLittleEndian(createMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(baddr);
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
  sockaddr_in subnet;
  memset(&subnet, 0, sizeof(subnet));
  subnet.sin_family = AF_INET;
  subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(createMask<uint32_t>(prefix_len));
  return std::make_shared<IPv4Address>(subnet);
}

uint32_t IPv4Address::getPort() const {
  return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
  m_addr.sin_port = byteswapOnLittleEndian(v);
}

IPv4Address::ptr IPv4Address::Create(const char* address, uint32_t port) {
  auto rt = std::make_shared<IPv4Address>();
  rt->m_addr.sin_port = byteswapOnLittleEndian(port);
  int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
  if (result <= 0) {
    SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", " << port << ") rt=" << result << " errno=" << errno
      << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

IPv6Address::IPv6Address() {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint32_t port) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin6_family = AF_INET6;
  m_addr.sin6_port = byteswapOnLittleEndian(port);
  memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

IPv6Address::IPv6Address(const sockaddr_in6& address) {
  m_addr = address;
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint32_t port) {
  auto rt = std::make_shared<IPv6Address>();
  rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
  int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
  if (result <= 0) {
    SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", " << port << ") rt=" << result << " errno=" << errno
      << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

const sockaddr* IPv6Address::getAddr() const {
  return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
  return sizeof(m_addr);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
  os << "[";
  auto* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
  bool used_zeros = false;
  // 一次取16位解读，一共128位，需要取8次
  for (size_t i = 0; i < 8; ++i) {
    if (addr[i] == 0 && !used_zeros) {
      continue;
    }
    if (i && addr[i - 1] == 0 && !used_zeros) {
      os << ":";
      used_zeros = true;
    }
    if (i) {
      os << ":";
    }
    os << std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
  }
  if (!used_zeros && addr[7] == 0) {
    os << "::";
  }
  os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
  return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
  sockaddr_in6 baddr(m_addr);
  baddr.sin6_addr.s6_addr[prefix_len / 8] |= createMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
    baddr.sin6_addr.s6_addr[i] = 0xff;
  }
  return std::make_shared<IPv6Address>(baddr);
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
  sockaddr_in6 baddr(m_addr);
  baddr.sin6_addr.s6_addr[prefix_len / 8] &= createMask<uint8_t>(prefix_len % 8);
  // for (size_t i = prefix_len / 8 + 1; i < 16; ++i) {
    // baddr.sin6_addr.s6_addr[i] = 0xff;
  // }
  return std::make_shared<IPv6Address>(baddr);
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
  sockaddr_in6 subnet;
  memset(&subnet, 0, sizeof(subnet));
  subnet.sin6_family = AF_INET6;
  subnet.sin6_addr.s6_addr[prefix_len / 8] = ~createMask<uint8_t>(prefix_len % 8);

  for (size_t i = 0; i < prefix_len / 8; ++i) {
    subnet.sin6_addr.s6_addr[i] = 0xFF;
  }
  return std::make_shared<IPv6Address>(subnet);
}

uint32_t IPv6Address::getPort() const {
  return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
  m_addr.sin6_port = byteswapOnLittleEndian(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)nullptr)->sun_path) - 1;

UnixAddress::UnixAddress() {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sun_family = AF_UNIX;
  m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sun_family = AF_UNIX;
  m_length = path.size() + 1; // path长度加上'\0'

  if (!path.empty() && path[0] == '\0') {
    // 字符串只有一个'\0',不需要再加一个'\0'
    --m_length;
  }

  if (m_length > sizeof(m_addr.sun_path)) {
    // 字符串长度大于最大buffer(108bytes)
    throw std::logic_error("path too long");
  }
  memcpy(m_addr.sun_path, path.c_str(), m_length);
  m_length += offsetof(sockaddr_un, sun_path); // 地址大小还需要加上从sockaddr_un头地址到sun_path成员的长度
}

const sockaddr* UnixAddress::getAddr() const {
  return (sockaddr*)&m_addr;
} 

socklen_t UnixAddress::getAddrLen() const {
  return sizeof(m_addr);
} 

std::ostream& UnixAddress::insert(std::ostream& os) const {
  if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
    return os << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
  }
  return os << m_addr.sun_path;
}

UnknowAddress::UnknowAddress(int family) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sa_family = family;
}

UnknowAddress::UnknowAddress(const sockaddr& addr) {
  m_addr = addr;
}

const sockaddr* UnknowAddress::getAddr() const {
  return &m_addr;
}

socklen_t UnknowAddress::getAddrLen() const {
  return sizeof(m_addr);
}

std::ostream& UnknowAddress::insert(std::ostream& os) const {
  os << "[UnknowAddress family=" << m_addr.sa_family << "]";
  return os;
}

}
