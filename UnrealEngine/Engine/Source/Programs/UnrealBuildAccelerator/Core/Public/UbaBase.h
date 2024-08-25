// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace uba
{
	using u8 = unsigned char;
	using u16 = unsigned short;
	using u32 = unsigned int;
	using u64 = unsigned long long;
	using s64 = long long;

	struct Guid
	{
		u32 data1 = 0; u16 data2 = 0; u16 data3 = 0; u8 data4[8] = { 0 };
		bool operator==(const Guid& o) const { return *(u64*)&data1 == *(u64*)&o.data1 && *(u64*)data4 == *(u64*)o.data4; }
	};

	template<class T> const T& Min(const T& a, const T& b) { return (b < a) ? b : a; }
	template<class T> const T& Max(const T& a, const T& b) { return (b > a) ? b : a; }

	#if PLATFORM_WINDOWS
	inline constexpr bool IsWindows = true;
	using tchar = wchar_t;
	#define TC(x) L##x
	#define PERCENT_HS L"%hs"
	#else
	inline constexpr bool IsWindows = false;
	using tchar = char;
	#define TC(x) x
	#define PERCENT_HS "%s"
	#endif
}

#if !defined(UBA_API)
	#if PLATFORM_WINDOWS
		#define UBA_API __declspec(dllimport)
	#else
		#define UBA_API
	#endif
#endif
