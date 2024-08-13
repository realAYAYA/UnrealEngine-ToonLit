#pragma once

#include <string>

void ZASIO_API AsioExceptionHandle(const std::string& Message);

namespace zasio {
	namespace detail {
		template <typename ExceptionType>
		void throw_exception(const ExceptionType& Exception)
		{
			std::string Message = Exception.what();
			AsioExceptionHandle(Message);
		}
	} // namespace detail
} // namespace asio

