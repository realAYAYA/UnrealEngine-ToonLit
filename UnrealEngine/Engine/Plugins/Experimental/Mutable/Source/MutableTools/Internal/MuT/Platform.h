// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>
#include <unordered_map>


namespace mu
{

	//! STL-like containers using this allocator
	template< typename T >
	using vector = std::vector<T>;

	template< typename T >
	using basic_string = std::basic_string<T, std::char_traits<T>>;

	using string = std::basic_string<char, std::char_traits<char> >;

	template< typename K, typename T >
	using map = std::map< K, T, std::less<K> >;

	template< typename T >
	using set = std::set< T, std::less<T> >;

	template< typename T >
	using multiset = std::multiset< T, std::less<T> >;

	template< typename K, typename T >
	using pair = std::pair<K, T>;

}

