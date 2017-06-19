#include "common/platform.h"

#ifdef PLATFORM_WINDOWS
#include <WinSock2.h>
#undef s_addr
#else
using SOCKET = int;
static const int32_t INVALID_SOCKET = -1;
#include <sys/socket.h>
#include <errno.h>
#endif

#include "nsysnet.h"
#include "nsysnet_socket_lib.h"
#include "modules/coreinit/coreinit_ghs.h"

namespace nsysnet
{

static std::vector<SOCKET>
sSocketList;


static int32_t
registerSocket(SOCKET socket)
{
   for (auto i = 0u; i < sSocketList.size(); ++i) {
      if (sSocketList[i] == INVALID_SOCKET) {
         sSocketList[i] = socket;
         return i;
      }
   }

   sSocketList.push_back(socket);
   return static_cast<int32_t>(sSocketList.size() - 1);
}


static void
unregisterSocket(int32_t id)
{
   if (id >= 0 && id < sSocketList.size()) {
      sSocketList[id] = INVALID_SOCKET;
   }
}


static SOCKET
getSocket(int32_t id)
{
   if (id < 0 || id > sSocketList.size()) {
      return INVALID_SOCKET;
   }

   return sSocketList[id];
}


#ifdef PLATFORM_WINDOWS
static int
getLastSocketError()
{
   return WSAGetLastError();
}
#else
static int
getLastSocketError()
{
   return errno;
}
#endif


static void
copySockaddrIn(const nsysnet_sockaddr_in *src,
               uint32_t len,
               sockaddr_in *dst)
{
   decaf_check(len == sizeof(nsysnet_sockaddr_in));
   std::memset(dst, 0, sizeof(sockaddr_in));

#ifdef PLATFORM_WINDOWS
   dst->sin_addr.S_un.S_addr = src->sin_addr.s_addr;
#else
   dst->sin_addr.s_addr = src->sin_addr.s_addr;
#endif

   dst->sin_family = src->sin_family;
   dst->sin_port = src->sin_port;
}


static void
copySockaddrIn(const sockaddr_in *src,
               uint32_t len,
               nsysnet_sockaddr_in *dst)
{
   decaf_check(len == sizeof(sockaddr_in));
   std::memset(dst, 0, sizeof(nsysnet_sockaddr_in));

#ifdef PLATFORM_WINDOWS
   dst->sin_addr.s_addr = src->sin_addr.S_un.S_addr;
#else
   dst->sin_addr.s_addr = src->sin_addr.s_addr;
#endif

   dst->sin_family = src->sin_family;
   dst->sin_port = src->sin_port;
}


static void
copyFdSet(int32_t nfds,
          const nsysnet_fd_set *src,
          fd_set *dst)
{
   auto highestBit = -1;
   auto offset = 0u;

   for (auto i = 31; i >= 0; --i) {
      if (src->fd_bits & (1 << i)) {
         if (highestBit == -1) {
            highestBit = i;
            offset = (nfds - 1) - i;
         }

         auto socket = getSocket(i + offset);
         FD_SET(socket, dst);
      }
   }
}


static void
copyTimeval(const nsysnet_timeval *src,
            timeval *dst)
{
   dst->tv_sec = src->tv_sec;
   dst->tv_usec = src->tv_usec;
}


int32_t
socket_lib_init()
{
   decaf_warn_stub();
   return 0;
}


int32_t
socket_lib_finish()
{
   decaf_warn_stub();
   return 0;
}


int32_t
accept(int32_t sockfd,
       nsysnet_sockaddr *addr,
       be_val<uint32_t> *addrlen)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   // Copy in sockaddr
   sockaddr_in addr_in;
   int len = *addrlen;
   copySockaddrIn(reinterpret_cast<nsysnet_sockaddr_in *>(&addr),
                  len,
                  reinterpret_cast<sockaddr_in *>(&addr_in));

   auto result = ::accept(socket, reinterpret_cast<sockaddr *>(&addr_in), &len);

   // Copy out result sockaddr
   copySockaddrIn(reinterpret_cast<sockaddr_in *>(&addr_in),
                  len,
                  reinterpret_cast<nsysnet_sockaddr_in*>(addr));
   *addrlen = len;

#ifdef PLATFORM_WINDOWS
   if (result == INVALID_SOCKET) {
      coreinit::ghs_set_errno(getLastSocketError());
      return -1;
   } else {
      // Register the new socket returned from accept
      return registerSocket(result);
   }
#else
   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
      return result;
   } else {
      // Register the new socket returned from accept
      return registerSocket(result);
   }
#endif
}


int32_t
bind(int32_t sockfd,
     const nsysnet_sockaddr *addr,
     uint32_t addrlen)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   sockaddr_in addr_in;
   copySockaddrIn(reinterpret_cast<nsysnet_sockaddr_in *>(&addr),
                  addrlen,
                  reinterpret_cast<sockaddr_in *>(&addr_in));

   auto result = ::bind(socket, reinterpret_cast<const sockaddr *>(&addr_in), sizeof(sockaddr_in));

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
connect(int32_t sockfd,
        const nsysnet_sockaddr *addr,
        uint32_t addrlen)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   sockaddr_in addr_in;
   copySockaddrIn(reinterpret_cast<nsysnet_sockaddr_in *>(&addr),
                  addrlen,
                  reinterpret_cast<sockaddr_in *>(&addr_in));

   auto result = ::connect(socket, reinterpret_cast<const sockaddr *>(&addr_in), sizeof(sockaddr_in));

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
getpeername(int32_t sockfd,
            nsysnet_sockaddr *addr,
            be_val<uint32_t> *addrlen)
{
   decaf_abort("Unimplemented getpeername called.");
   return 0;
}


int32_t
getsockname(int32_t sockfd,
            nsysnet_sockaddr *addr,
            be_val<uint32_t> *addrlen)
{
   decaf_abort("Unimplemented getsockname called.");
   return 0;
}


int32_t
getsockopt(int32_t sockfd,
           int32_t level,
           NSSocketOptions optname,
           void *optval,
           be_val<uint32_t> *optlen)
{
   auto socket = getSocket(sockfd);
   auto result = -1;

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   // TODO: We need to handle each optname and byte_swap optval as appropriate
   switch (optname) {
   case NSSocketOptions::Broadcast:
   {
      int value;
      int len;
      result = ::getsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char *>(&value), &len);
      if (result > 0) {
         *reinterpret_cast<be_val<int32_t> *>(optval) = value;
         *optlen = 4;
      }
      break;
   }
   default:
      decaf_abort(fmt::format("Unsupported getsockopt option {}", optname));
   }

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
listen(int32_t sockfd,
       int32_t backlog)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   auto result = ::listen(socket, backlog);

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
recv(int32_t sockfd,
     char *buf,
     uint32_t len,
     int32_t flags)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   auto result = ::recv(socket, buf, len, flags);

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
recvfrom(int32_t sockfd,
         char *buf,
         uint32_t len,
         int32_t flags,
         nsysnet_sockaddr *src_addr,
         be_val<uint32_t> *addrlen)
{
   decaf_abort("Unimplemented recvfrom called.");
   return 0;
}


int32_t
select(int32_t nfds,
       nsysnet_fd_set *readfds,
       nsysnet_fd_set *writefds,
       nsysnet_fd_set *exceptfds,
       nsysnet_timeval *timeout)
{
   auto highestBit = -1;
   auto fdOffset = 0u;
   fd_set read_fds;
   fd_set write_fds;
   fd_set except_fds;
   timeval hostTimeout;

   FD_ZERO(&read_fds);
   FD_ZERO(&write_fds);
   FD_ZERO(&except_fds);

   copyFdSet(nfds, readfds, &read_fds);
   copyFdSet(nfds, writefds, &write_fds);
   copyFdSet(nfds, exceptfds, &except_fds);
   copyTimeval(timeout, &hostTimeout);

   auto result = ::select(nfds, &read_fds, &write_fds, &except_fds, &hostTimeout);

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
send(int32_t sockfd,
     const char *buf,
     uint32_t len,
     int32_t flags)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   auto result = ::send(socket, buf, len, flags);

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
sendto(int32_t sockfd,
       const void *buf,
       uint32_t len,
       int32_t flags,
       const nsysnet_sockaddr *dest_addr,
       uint32_t addrlen)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   sockaddr_in addr_in;
   copySockaddrIn(reinterpret_cast<const nsysnet_sockaddr_in *>(dest_addr),
                  addrlen,
                  &addr_in);

   auto result = ::sendto(socket, reinterpret_cast<const char *>(buf), len, flags, reinterpret_cast<sockaddr *>(&addr_in), sizeof(sockaddr_in));

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
setsockopt(int32_t sockfd,
           int32_t level,
           NSSocketOptions optname,
           const char *optval,
           uint32_t optlen)
{
   auto socket = getSocket(sockfd);
   auto result = -1;

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   // TODO: We need to handle each optname and byte_swap optval as appropriate
   switch (optname) {
   case NSSocketOptions::Broadcast:
   {
      auto value = static_cast<int>(reinterpret_cast<const be_val<uint32_t> *>(optval)->value());
      result = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&value), sizeof(int));
      break;
   }
   default:
      decaf_abort(fmt::format("Unsupported setsockopt option {}", optname));
   }

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
shutdown(int32_t sockfd,
         int32_t how)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

   auto result = ::shutdown(socket, how);

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
socket(int32_t domain,
       int32_t type,
       int32_t protocol)
{
   auto socket = ::socket(domain, type, protocol);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(getLastSocketError());
      return -1;
   }

   return registerSocket(socket);
}


int32_t
socketclose(int32_t sockfd)
{
   auto socket = getSocket(sockfd);

   if (socket == INVALID_SOCKET) {
      coreinit::ghs_set_errno(-1);
      return -1;
   }

#ifdef PLATFORM_WINDOWS
   auto result = ::closesocket(socket);
#else
   auto result = ::close(socket);
#endif

   if (result < 0) {
      coreinit::ghs_set_errno(getLastSocketError());
   }

   return result;
}


int32_t
socketlasterr()
{
   return coreinit::ghs_get_errno();
}

void
Module::registerSocketLibFunctions()
{
   RegisterKernelFunction(socket_lib_init);
   RegisterKernelFunction(socket_lib_finish);
   RegisterKernelFunction(accept);
   RegisterKernelFunction(bind);
   RegisterKernelFunction(connect);
   RegisterKernelFunction(getpeername);
   RegisterKernelFunction(getsockname);
   RegisterKernelFunction(getsockopt);
   RegisterKernelFunction(listen);
   RegisterKernelFunction(recv);
   RegisterKernelFunction(recvfrom);
   RegisterKernelFunction(select);
   RegisterKernelFunction(send);
   RegisterKernelFunction(sendto);
   RegisterKernelFunction(setsockopt);
   RegisterKernelFunction(shutdown);
   RegisterKernelFunction(socket);
   RegisterKernelFunction(socketclose);
   RegisterKernelFunction(socketlasterr);
}

} // namespace nsysnet
