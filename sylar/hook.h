//
// Created by changyuli on 11/3/21.
//

#ifndef SYLAR_SYLAR_HOOK_H_
#define SYLAR_SYLAR_HOOK_H_

#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

namespace sylar {

bool is_hook_enable();
void set_hook_enable(bool flag);

}

extern "C" {

  // sleep 部分
  typedef unsigned int (*sleep_fun)(unsigned int seconds);
  extern sleep_fun sleep_f;

  typedef int (*usleep_fun)(useconds_t usec);
  extern usleep_fun usleep_f;

  typedef int (*nanosleep_fun)(const struct timespec *rqtp, struct timespec *rmtp);
  extern nanosleep_fun nanosleep_f;

  // socket 部分
  typedef int (*socket_fun)(int domain, int type, int protocol);
  extern socket_fun socket_f;

  typedef int (*connect_fun)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
  extern connect_fun connect_f;

  typedef int (*accept_fun)(int s, struct sockaddr* addr, socklen_t* addrlen);
  extern accept_fun accept_f;

  // read部分
  typedef ssize_t (*read_fun) (int fildes, void *buf, size_t nbyte);
  extern read_fun read_f;

  typedef ssize_t (*readv_fun) (int fildes, const struct iovec *iov, int iovcnt);
  extern readv_fun readv_f;

  typedef ssize_t (*recv_fun) (int socket, void *buffer, size_t length, int flags);
  extern recv_fun recv_f;

  typedef ssize_t (*recvfrom_fun) (int socket, void * buffer, size_t length,
				 int flags, struct sockaddr * address,
				 socklen_t * address_len);
  extern recvfrom_fun recvfrom_f;

  typedef ssize_t (*recvmsg_fun) (int socket, struct msghdr *message, int flags);
  extern recvmsg_fun recvmsg_f;

  // write 部分
  typedef ssize_t (*write_fun) (int fildes, const void *buf, size_t nbyte);
  extern write_fun write_f;

  typedef ssize_t (*writev_fun) (int fd, const struct iovec* iov, int iovcnt);
  extern writev_fun writev_f;

  typedef ssize_t (*send_fun) (int s, const void* msg, size_t len, int flags);
  extern send_fun send_f;

  typedef ssize_t (*sendto_fun) (int s, const void* msg, size_t len, int flags, const struct sockaddr* to, socklen_t tolen);
  extern sendto_fun sendto_f;

  typedef ssize_t (*sendmsg_fun) (int s, const struct msghdr* msg, int flags);
  extern sendmsg_fun sendmsg_f;

  typedef int (*close_fun) (int fd);
  extern close_fun close_f;

  typedef int (*fcntl_fun) (int fd, int cmd, ...);
  extern fcntl_fun fcntl_f;

  typedef int (*ioctl_fun) (int d, int request, ...);
  extern ioctl_fun ioctl_f;

  typedef int (*getsockopt_fun) (int socket, int level, int option_name,
                 void *option_value, socklen_t *option_len);
  extern getsockopt_fun getsockopt_f;

  typedef int (*setsockopt_fun) (int sockfd, int level, int optname, const void* optval, socklen_t optlen);
  extern setsockopt_fun setsockopt_f;
}

#endif //SYLAR_SYLAR_HOOK_H_
