//
// ssl/error.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_SSL_ERROR_HPP
#define ASIO_SSL_ERROR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/error_code.hpp"
#include "asio/ssl/detail/openssl_types.hpp"

#include "asio/detail/push_options.hpp"

namespace zasio {
namespace error {

enum ssl_errors
{
  // Error numbers are those produced by openssl.
};

extern ASIO_DECL
const zasio::error_category& get_ssl_category();

static const zasio::error_category&
  ssl_category ASIO_UNUSED_VARIABLE
  = zasio::error::get_ssl_category();

} // namespace error
namespace ssl {
namespace error {

enum stream_errors
{
#if defined(GENERATING_DOCUMENTATION)
  /// The underlying stream closed before the ssl stream gracefully shut down.
  stream_truncated,

  /// The underlying SSL library returned a system error without providing
  /// further information.
  unspecified_system_error,

  /// The underlying SSL library generated an unexpected result from a function
  /// call.
  unexpected_result
#else // defined(GENERATING_DOCUMENTATION)
# if (OPENSSL_VERSION_NUMBER < 0x10100000L) \
    && !defined(OPENSSL_IS_BORINGSSL) \
    && !defined(ASIO_USE_WOLFSSL)
  stream_truncated = ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ),
# else
  stream_truncated = 1,
# endif
  unspecified_system_error = 2,
  unexpected_result = 3
#endif // defined(GENERATING_DOCUMENTATION)
};

extern ASIO_DECL
const zasio::error_category& get_stream_category();

static const zasio::error_category&
  stream_category ASIO_UNUSED_VARIABLE
  = zasio::ssl::error::get_stream_category();

} // namespace error
} // namespace ssl
} // namespace zasio

#if defined(ASIO_HAS_STD_SYSTEM_ERROR)
namespace std {

template<> struct is_error_code_enum<zasio::error::ssl_errors>
{
  static const bool value = true;
};

template<> struct is_error_code_enum<zasio::ssl::error::stream_errors>
{
  static const bool value = true;
};

} // namespace std
#endif // defined(ASIO_HAS_STD_SYSTEM_ERROR)

namespace zasio {
namespace error {

inline zasio::error_code make_error_code(ssl_errors e)
{
  return zasio::error_code(
      static_cast<int>(e), get_ssl_category());
}

} // namespace error
namespace ssl {
namespace error {

inline zasio::error_code make_error_code(stream_errors e)
{
  return zasio::error_code(
      static_cast<int>(e), get_stream_category());
}

} // namespace error
} // namespace ssl
} // namespace zasio

#include "asio/detail/pop_options.hpp"

#if defined(ASIO_HEADER_ONLY)
# include "asio/ssl/impl/error.ipp"
#endif // defined(ASIO_HEADER_ONLY)

#endif // ASIO_SSL_ERROR_HPP
