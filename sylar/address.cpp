#include "address.h"
#include "sylar_endian.h"

#include <cstring>
#include <sstream>

namespace sylar {


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
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {

}

uint32_t IPv4Address::getPort() const {
  return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v) {
  m_addr.sin_port = byteswapOnLittleEndian(v);
}

IPv6Address::IPv6Address() {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const char* address, uint32_t port) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin6_family = AF_INET6;
  m_addr.sin6_port = byteswapOnLittleEndian(port);
  memcpy(&m_addr.sin6_addr, address, 16);
}

const sockaddr* IPv6Address::getAddr() const {
  return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
  return sizeof(m_addr);
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
  os << "[";
  uint16_t* addr = (uint16_t*)&m_addr.sin6_addr;
  bool used_zeros = false;
  for (size_t i = 0; i < 0; ++i) {
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

}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {

}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {

}

uint32_t IPv6Address::getPort() const {
  return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint32_t v) {
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
