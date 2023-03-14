//===----------------------------- coroutine -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#ifndef WITH_CPP_COROUTINES
#define WITH_CPP_COROUTINES 1
#endif

#if WITH_CPP_COROUTINES

#if __has_include(<coroutine>)
#include <coroutine>
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#define USE_EXPERIMENTAL_COROUTINE_SUPPORT
#elif PLATFORM_COMPILER_CLANG
#define USE_EXPERIMENTAL_COROUTINE_SUPPORT
#include "CoreTypes.h"
namespace std
{
	namespace experimental
	{
		template< class... >
		using Void_t = void;

		typedef decltype(nullptr) Nullptr_t;

		template <class Tp, class = void>
		struct coroutine_traits_sfinae 
		{
		};

		template <class Tp>
		struct coroutine_traits_sfinae<Tp, Void_t<typename Tp::promise_type>>
		{
			using promise_type = typename Tp::promise_type;
		};

		template <typename Ret, typename... Args>
		struct coroutine_traits : public coroutine_traits_sfinae<Ret>
		{
		};

		template <typename Promise = void>
		class coroutine_handle;

		template <>
		class coroutine_handle<void> 
		{
		public:
			inline constexpr coroutine_handle() noexcept : handle(nullptr) 
			{
			}

			inline constexpr coroutine_handle(Nullptr_t) noexcept : handle(nullptr) 
			{
			}

			inline coroutine_handle& operator=(Nullptr_t) noexcept 
			{
				handle = nullptr;
				return *this;
			}

			inline constexpr void* address() const noexcept 
			{ 
				return handle; 
			}

			inline constexpr explicit operator bool() const noexcept 
			{ 
				return handle; 
			}

			inline void operator()() 
			{ 
				resume(); 
			}

			inline void resume() 
			{
				checkfSlow(is_suspended(), TEXT("resume() can only be called on suspended coroutines"));
				checkfSlow(!done(), TEXT("resume() has undefined behavior when the coroutine is done"));
				__builtin_coro_resume(handle);
			}

			inline void destroy() 
			{
				checkfSlow(is_suspended(), TEXT("destroy() can only be called on suspended coroutines"));
				__builtin_coro_destroy(handle);
			}

			inline bool done() const 
			{
				checkfSlow(is_suspended(), TEXT("done() can only be called on suspended coroutines"));
				return __builtin_coro_done(handle);
			}

		public:
			inline static coroutine_handle from_address(void* addr) noexcept 
			{
				coroutine_handle tmp;
				tmp.handle = addr;
				return tmp;
			}

			inline static coroutine_handle from_address(Nullptr_t) noexcept 
			{
				return coroutine_handle(nullptr);
			}

			template <class Tp, bool CallIsValid = false>
			static coroutine_handle from_address(Tp*) 
			{
				static_assert(CallIsValid, "coroutine_handle<void>::from_address cannot be called with non-void pointers");
			}

		private:
			bool is_suspended() const noexcept  
			{
				return handle != nullptr;
			}

			template <class PromiseT> friend class coroutine_handle;
			void* handle;
		};

		inline bool operator==(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return x.address() == y.address();
		}
		inline bool operator!=(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return !(x == y);
		}
		inline bool operator<(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return std::less<void*>()(x.address(), y.address());
		}
		inline bool operator>(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return y < x;
		}
		inline bool operator<=(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return !(x > y);
		}
		inline bool operator>=(coroutine_handle<> x, coroutine_handle<> y) noexcept 
		{
			return !(x < y);
		}

		template <typename Promise>
		class coroutine_handle : public coroutine_handle<> 
		{
			using Base = coroutine_handle<>;

		public:
			using coroutine_handle<>::coroutine_handle;

			inline coroutine_handle& operator=(Nullptr_t) noexcept 
			{
				Base::operator=(nullptr);
				return *this;
			}

			inline Promise& promise() const 
			{
				return *static_cast<Promise*>(__builtin_coro_promise(this->handle, alignof(Promise), false));
			}

		public:
			inline static coroutine_handle from_address(void* addr) noexcept 
			{
				coroutine_handle tmp;
				tmp.handle = addr;
				return tmp;
			}

			inline static coroutine_handle from_address(Nullptr_t) noexcept 
			{
				return coroutine_handle(nullptr);
			}

			template <class Tp, bool CallIsValid = false>
			static coroutine_handle from_address(Tp*) 
			{
				static_assert(CallIsValid, "coroutine_handle<promise_type>::from_address cannot be called with non-void pointers");
			}

			template <bool CallIsValid = false>
			static coroutine_handle from_address(Promise*) 
			{
				static_assert(CallIsValid, "coroutine_handle<promise_type>::from_address cannot be used with pointers to the coroutine's promise type; use 'from_promise' instead");
			}

			inline static coroutine_handle from_promise(Promise& promise) noexcept 
			{
				typedef typename std::remove_cv<Promise>::type RawPromise;
				coroutine_handle tmp;
				tmp.handle = __builtin_coro_promise(std::addressof(const_cast<RawPromise&>(promise)), alignof(Promise), true);
				return tmp;
			}
		};

		struct suspend_never 
		{
			  inline bool await_ready() const noexcept 
			  { 
				  return true;
			  }

			  inline void await_suspend(coroutine_handle<>) const noexcept 
			  {
			  }

			  inline void await_resume() const noexcept 
			  {
			  }
		};

		struct suspend_always 
		{
			  inline bool await_ready() const noexcept 
			  { 
				  return false; 
			  }

			  inline void await_suspend(coroutine_handle<>) const noexcept 
			  {
			  }

			  inline void await_resume() const noexcept 
			  {
			  }
		};

	}

	template <class Tp>
	struct hash<experimental::coroutine_handle<Tp>> 
	{
		using arg_type = experimental::coroutine_handle<Tp>;
		inline size_t operator()(arg_type const& v) const noexcept
		{
			return hash<void*>()(v.address());
		}
	};
}
#else
#error  C++ compiler with coroutine support required.
#endif

#ifndef USE_EXPERIMENTAL_COROUTINE_SUPPORT
template<typename HandleType>
using coroutine_handle = std::coroutine_handle<HandleType>;
using suspend_always = std::suspend_always;
using suspend_never = std::suspend_never;
#else
template<typename HandleType>
using coroutine_handle = std::experimental::coroutine_handle<HandleType>;
using suspend_always = std::experimental::suspend_always;
using suspend_never = std::experimental::suspend_never;
#endif
using coroutine_handle_base = coroutine_handle<void>;

#endif