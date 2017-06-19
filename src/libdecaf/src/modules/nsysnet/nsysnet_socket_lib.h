#pragma once
#include "nsysnet_enum.h"

#include <cstdint>
#include <common/be_val.h>
#include <common/structsize.h>

namespace nsysnet
{

#pragma pack(push, 1)

struct nsysnet_fd_set
{
   be_val<uint32_t> fd_bits;
};
CHECK_OFFSET(nsysnet_fd_set, 0x00, fd_bits);
CHECK_SIZE(nsysnet_fd_set, 0x04);

struct nsysnet_in_addr
{
   be_val<int> s_addr;
};
CHECK_OFFSET(nsysnet_in_addr, 0x00, s_addr);
CHECK_SIZE(nsysnet_in_addr, 0x04);

struct nsysnet_sockaddr
{
   be_val<uint16_t> sa_family;
   uint8_t sa_data[14];
};
CHECK_OFFSET(nsysnet_sockaddr, 0x00, sa_family);
CHECK_OFFSET(nsysnet_sockaddr, 0x02, sa_data);
CHECK_SIZE(nsysnet_sockaddr, 0x10);

struct  nsysnet_sockaddr_in
{
   be_val<uint16_t> sin_family;
   be_val<uint16_t> sin_port;
   nsysnet_in_addr sin_addr;
   uint8_t sin_zero[8];
};
CHECK_OFFSET(nsysnet_sockaddr_in, 0x00, sin_family);
CHECK_OFFSET(nsysnet_sockaddr_in, 0x02, sin_port);
CHECK_OFFSET(nsysnet_sockaddr_in, 0x04, sin_addr);
CHECK_OFFSET(nsysnet_sockaddr_in, 0x08, sin_zero);
CHECK_SIZE(nsysnet_sockaddr_in, 0x10);

struct  nsysnet_timeval
{
   be_val<int32_t> tv_sec;
   be_val<int32_t> tv_usec;
};
CHECK_OFFSET(nsysnet_timeval, 0x00, tv_sec);
CHECK_OFFSET(nsysnet_timeval, 0x04, tv_usec);
CHECK_SIZE(nsysnet_timeval, 0x08);

#pragma pack(pop)

int32_t
socket_lib_init();

int32_t
socket_lib_finish();

int32_t
accept(int32_t sockfd,
       nsysnet_sockaddr *addr,
       be_val<uint32_t> *addrlen);

int32_t
bind(int32_t sockfd,
     const nsysnet_sockaddr *addr,
     uint32_t addrlen);

int32_t
connect(int32_t sockfd,
        const nsysnet_sockaddr *addr,
        uint32_t addrlen);

int32_t
getpeername(int32_t sockfd,
            nsysnet_sockaddr *addr,
            be_val<uint32_t> *addrlen);

int32_t
getsockname(int32_t sockfd,
            nsysnet_sockaddr *addr,
            be_val<uint32_t> *addrlen);

int32_t
getsockopt(int32_t sockfd,
           int32_t level,
           NSSocketOptions optname,
           void *optval,
           be_val<uint32_t> *optlen);

int32_t
listen(int32_t sockfd,
       int32_t backlog);

int32_t
recv(int32_t sockfd,
     char *buf,
     uint32_t len,
     int32_t flags);

int32_t
recvfrom(int32_t sockfd,
         char *buf,
         uint32_t len,
         int32_t flags,
         nsysnet_sockaddr *src_addr,
         be_val<uint32_t> *addrlen);

int32_t
select(int32_t nfds,
       nsysnet_fd_set *readfds,
       nsysnet_fd_set *writefds,
       nsysnet_fd_set *exceptfds,
       nsysnet_timeval *timeout);

int32_t
send(int32_t sockfd,
     const char *buf,
     uint32_t len,
     int32_t flags);

int32_t
sendto(int32_t sockfd,
       const void *buf,
       uint32_t len,
       int32_t flags,
       const nsysnet_sockaddr *dest_addr,
       uint32_t addrlen);

int32_t
setsockopt(int32_t sockfd,
           int32_t level,
           NSSocketOptions optname,
           const char *optval,
           uint32_t optlen);

int32_t
shutdown(int32_t sockfd,
         int32_t how);

int32_t
socket(int32_t domain,
       int32_t type,
       int32_t protocol);

int32_t
socketclose(int32_t sockfd);

int32_t
socketlasterr();

} // namespace nsysnet
