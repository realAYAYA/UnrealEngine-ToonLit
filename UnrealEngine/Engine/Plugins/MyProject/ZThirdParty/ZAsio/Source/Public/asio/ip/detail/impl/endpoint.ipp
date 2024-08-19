//
// ip/detail/impl/endpoint.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IP_DETAIL_IMPL_ENDPOINT_IPP
#define ASIO_IP_DETAIL_IMPL_ENDPOINT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include <cstring>
#if !defined(ASIO_NO_IOSTREAM)
# include <sstream>
#endif // !defined(ASIO_NO_IOSTREAM)
#include "asio/detail/socket_ops.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/error.hpp"
#include "asio/ip/detail/endpoint.hpp"

#include "asio/detail/push_options.hpp"

namespace zasio {
namespace ip {
namespace detail {

endpoint::endpoint() ASIO_NOEXCEPT
  : data_()
{
  data_.v4.sin_family = ASIO_OS_DEF(AF_INET);
  data_.v4.sin_port = 0;
  data_.v4.sin_addr.s_addr = ASIO_OS_DEF(INADDR_ANY);
}

endpoint::endpoint(int family, unsigned short port_num) ASIO_NOEXCEPT
  : data_()
{
  using namespace std; // For memcpy.
  if (family == ASIO_OS_DEF(AF_INET))
  {
    data_.v4.sin_family = ASIO_OS_DEF(AF_INET);
    data_.v4.sin_port =
      zasio::detail::socket_ops::host_to_network_short(port_num);
    data_.v4.sin_addr.s_addr = ASIO_OS_DEF(INADDR_ANY);
  }
  else
  {
    data_.v6.sin6_family = ASIO_OS_DEF(AF_INET6);
    data_.v6.sin6_port =
      zasio::detail::socket_ops::host_to_network_short(port_num);
    data_.v6.sin6_flowinfo = 0;
    data_.v6.sin6_addr.s6_addr[0] = 0; data_.v6.sin6_addr.s6_addr[1] = 0;
    data_.v6.sin6_addr.s6_addr[2] = 0; data_.v6.sin6_addr.s6_addr[3] = 0;
    data_.v6.sin6_addr.s6_addr[4] = 0; data_.v6.sin6_addr.s6_addr[5] = 0;
    data_.v6.sin6_addr.s6_addr[6] = 0; data_.v6.sin6_addr.s6_addr[7] = 0;
    data_.v6.sin6_addr.s6_addr[8] = 0; data_.v6.sin6_addr.s6_addr[9] = 0;
    data_.v6.sin6_addr.s6_addr[10] = 0; data_.v6.sin6_addr.s6_addr[11] = 0;
    data_.v6.sin6_addr.s6_addr[12] = 0; data_.v6.sin6_addr.s6_addr[13] = 0;
    data_.v6.sin6_addr.s6_addr[14] = 0; data_.v6.sin6_addr.s6_addr[15] = 0;
    data_.v6.sin6_scope_id = 0;
  }
}

endpoint::endpoint(const zasio::ip::address& addr,
    unsigned short port_num) ASIO_NOEXCEPT
  : data_()
{
  using namespace std; // For memcpy.
  if (addr.is_v4())
  {
    data_.v4.sin_family = ASIO_OS_DEF(AF_INET);
    data_.v4.sin_port =
      zasio::detail::socket_ops::host_to_network_short(port_num);
    data_.v4.sin_addr.s_addr =
      zasio::detail::socket_ops::host_to_network_long(
        addr.to_v4().to_uint());
  }
  else
  {
    data_.v6.sin6_family = ASIO_OS_DEF(AF_INET6);
    data_.v6.sin6_port =
      zasio::detail::socket_ops::host_to_network_short(port_num);
    data_.v6.sin6_flowinfo = 0;
    zasio::ip::address_v6 v6_addr = addr.to_v6();
    zasio::ip::address_v6::bytes_type bytes = v6_addr.to_bytes();
    memcpy(data_.v6.sin6_addr.s6_addr, bytes.data(), 16);
    data_.v6.sin6_scope_id =
      static_cast<zasio::detail::u_long_type>(
        v6_addr.scope_id());
  }
}

void endpoint::resize(std::size_t new_size)
{
  if (new_size > sizeof(zasio::detail::sockaddr_storage_type))
  {
    zasio::error_code ec(zasio::error::invalid_argument);
    zasio::detail::throw_error(ec);
  }
}

unsigned short endpoint::port() const ASIO_NOEXCEPT
{
  if (is_v4())
  {
    return zasio::detail::socket_ops::network_to_host_short(
        data_.v4.sin_port);
  }
  else
  {
    return zasio::detail::socket_ops::network_to_host_short(
        data_.v6.sin6_port);
  }
}

void endpoint::port(unsigned short port_num) ASIO_NOEXCEPT
{
  if (is_v4())
  {
    data_.v4.sin_port
      = zasio::detail::socket_ops::host_to_network_short(port_num);
  }
  else
  {
    data_.v6.sin6_port
      = zasio::detail::socket_ops::host_to_network_short(port_num);
  }
}

zasio::ip::address endpoint::address() const ASIO_NOEXCEPT
{
  using namespace std; // For memcpy.
  if (is_v4())
  {
    return zasio::ip::address_v4(
        zasio::detail::socket_ops::network_to_host_long(
          data_.v4.sin_addr.s_addr));
  }
  else
  {
    zasio::ip::address_v6::bytes_type bytes;
#if defined(ASIO_HAS_STD_ARRAY)
    memcpy(bytes.data(), data_.v6.sin6_addr.s6_addr, 16);
#else // defined(ASIO_HAS_STD_ARRAY)
    memcpy(bytes.elems, data_.v6.sin6_addr.s6_addr, 16);
#endif // defined(ASIO_HAS_STD_ARRAY)
    return zasio::ip::address_v6(bytes, data_.v6.sin6_scope_id);
  }
}

void endpoint::address(const zasio::ip::address& addr) ASIO_NOEXCEPT
{
  endpoint tmp_endpoint(addr, port());
  data_ = tmp_endpoint.data_;
}

bool operator==(const endpoint& e1, const endpoint& e2) ASIO_NOEXCEPT
{
  return e1.address() == e2.address() && e1.port() == e2.port();
}

bool operator<(const endpoint& e1, const endpoint& e2) ASIO_NOEXCEPT
{
  if (e1.address() < e2.address())
    return true;
  if (e1.address() != e2.address())
    return false;
  return e1.port() < e2.port();
}

#if !defined(ASIO_NO_IOSTREAM)
std::string endpoint::to_string() const
{
  std::ostringstream tmp_os;
  tmp_os.imbue(std::locale::classic());
  if (is_v4())
    tmp_os << address();
  else
    tmp_os << '[' << address() << ']';
  tmp_os << ':' << port();

  return tmp_os.str();
}
#endif // !defined(ASIO_NO_IOSTREAM)

} // namespace detail
} // namespace ip
} // namespace zasio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IP_DETAIL_IMPL_ENDPOINT_IPP
