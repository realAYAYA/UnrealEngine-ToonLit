// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaStringBuffer.h"
#include "UbaPlatform.h"

namespace uba
{
	bool StartsWith(const tchar* data, const tchar* str, bool ignoreCase)
	{
		if (ignoreCase)
		{
			for (const tchar* ia = data, *ib = str;; ++ia, ++ib)
			{
				if (!*ib)
					return true;
				if (!*ia || ToLower(*ia) != ToLower(*ib))
					return false;
			}
		}
		else
		{
			for (const tchar* ia = data, *ib = str;; ++ia, ++ib)
			{
				if (!*ib)
					return true;
				if (!*ia || *ia != *ib)
					return false;
			}
		}
	}

	bool EndsWith(const tchar* str, u64 strLen, const tchar* value, bool ignoreCase)
	{
		u64 valueLen = TStrlen(value);
		if (strLen < valueLen)
			return false;
		if (!ignoreCase)
			return memcmp(str + strLen - valueLen, value, valueLen * sizeof(tchar)) == 0;
		for (const tchar* ia = str + strLen - valueLen, *ib = value; *ia; ++ia, ++ib)
			if (ToLower(*ia) != ToLower(*ib))
				return false;
		return true;
	}

	bool Contains(const tchar* str, const tchar* sub, bool ignoreCase, const tchar** pos)
	{
		if (!ignoreCase)
			return TStrstr(str, sub) != 0;
		for (const tchar* a = str; *a; ++a)
		{
			bool contains = true;
			for (const tchar* b = sub, *a2 = a; contains && *b; ++b, ++a2)
				contains = ToLower(*a2) == ToLower(*b);
			if (!contains)
				continue;
			if (pos)
				*pos = a;
			return true;
		}
		if (pos)
			*pos = nullptr;
		return false;
	}

	bool Equals(const tchar* str1, const tchar* str2, bool ignoreCase)
	{
		if (ignoreCase)
		{
			for (const tchar* ia = str1, *ib = str2;; ++ia, ++ib)
			{
				if (ToLower(*ia) != ToLower(*ib))
					return false;
				if (!*ia || !*ib)
					return *ia == *ib;
			}
		}
		else
			return TStrcmp(str1, str2) == 0;
	}

	bool Equals(const tchar* str1, const tchar* str2, u64 count, bool ignoreCase)
	{
		if (!count)
			return true;

		if (ignoreCase)
		{
			for (const tchar* ia = str1, *ib = str2;; ++ia, ++ib)
			{
				if (ToLower(*ia) != ToLower(*ib))
					return false;
				if (!--count)
					return true;
				if (!*ia || !*ib)
					return false;
			}
		}
		else
		{
			for (const tchar* ia = str1, *ib = str2;; ++ia, ++ib)
			{
				if (*ia != *ib)
					return false;
				if (!--count)
					return true;
				if (!*ia || !*ib)
					return false;
			}
		}
	}

	void Replace(tchar* str, tchar from, tchar to)
	{
		if (from == to)
			return;

		while (*str)
		{
			if (*str == from)
				*str = to;
			++str;
		}
	}

	void FixPathSeparators(tchar* str)
	{
		Replace(str, NonPathSeparator, PathSeparator);
	}

	StringBufferBase& StringBufferBase::Append(const tchar* str)
	{
		return Append(str, u32(TStrlen(str)));
	}

	StringBufferBase& StringBufferBase::Append(const tchar* str, u64 charCount)
	{
		UBA_ASSERTF(count + charCount < capacity, TC("Trying to append %u character string to buffer which is %u long and has %u capacity left"), charCount, count, capacity - count);
		memcpy(data + count, str, charCount * sizeof(tchar));
		count += u32(charCount);
		data[count] = 0;
		return *this;
	}

	StringBufferBase& StringBufferBase::Appendf(const tchar* format, ...)
	{
		if (*format)
		{
			va_list arg;
			va_start(arg, format);
			Append(format, arg);
			va_end(arg);
		}
		return *this;
	}

	StringBufferBase& StringBufferBase::AppendDir(const StringBufferBase& str)
	{
		if (const tchar* last = str.Last(PathSeparator))
			return Append(str.data, u64(last - str.data));
		return *this;
	}

	StringBufferBase& StringBufferBase::AppendDir(const tchar* dir)
	{
		if (const tchar* last = TStrrchr(dir, PathSeparator))
			return Append(dir, u64(last - dir));
		return *this;
	}


	StringBufferBase& StringBufferBase::AppendFileName(const tchar* str)
	{
		const tchar* last = TStrrchr(str, PathSeparator);
		if (!last)
			last = TStrrchr(str, '/');
		else if (const tchar* last2 = TStrrchr(str, '/'))
			last = last2;
		if (last)
			return Append(last + 1);
		return Append(str);
	}

	StringBufferBase& StringBufferBase::AppendHex(u64 v)
	{
		tchar buf[256];
		ValueToString(buf, sizeof_array(buf), v);
		return Append(buf);
	}

	StringBufferBase& StringBufferBase::AppendValue(u64 v)
	{
		tchar buf[256];
		TSprintf_s(buf, sizeof_array(buf), TC("%llu"), v);
		return Append(buf);
	}

	StringBufferBase& ReplaceEnd(StringBufferBase& sb, const tchar* str)
	{
		auto len = TStrlen(str);
		if (sb.count > sb.capacity - len - 1)
			sb.count = sb.capacity - len - 1;
		return sb.Append(str, len);
	}

	StringBufferBase& StringBufferBase::Append(const tchar* format, va_list& args)
	{
		#if PLATFORM_WINDOWS
		int len = _vscwprintf(format, args);
		va_list& args2 = args;
		#else
		va_list args2;
		va_copy(args2, args);
		auto g = MakeGuard([&]() { va_end(args2); });
		int len = vsnprintf(0, 0, format, args);
		#endif

		if (len < 0)
			return ReplaceEnd(*this, TC("PRINTF ERROR!"));

		if (len >= int(capacity - count) - 1)
			return ReplaceEnd(*this, TC("BUFFEROVERFLOW!"));

		int res = Tvsprintf_s(data + count, capacity - count, format, args2);

		if (res > 0)
			count += u32(res);
		else
			return ReplaceEnd(*this, TC("SPRINTF_ERROR!"));

		return *this;
	}

	#if PLATFORM_WINDOWS
	StringBufferBase& StringBufferBase::Append(const char* str)
	{
		u32 capacityEnd = capacity - 1;
		for (const char* i = str; *i; ++i)
			if (count < capacityEnd)
				data[count++] = *i;
		data[count] = 0;
		return *this;
	}
	#endif

	StringBufferBase& StringBufferBase::Resize(u64 newSize)
	{
		UBA_ASSERT(newSize < capacity);
		data[newSize] = 0;
		count = u32(newSize);
		return *this;
	}

	StringBufferBase& StringBufferBase::Clear()
	{
		data[0] = 0;
		count = 0;
		return *this;
	}

	bool StringBufferBase::Contains(tchar c) const
	{
		return TStrchr(data, c) != nullptr;
	}

	const tchar* StringBufferBase::First(tchar c, u64 offset) const
	{
		return TStrchr(data + offset, c);
	}

	const tchar* StringBufferBase::Last(tchar c, u64 offset) const
	{
		return TStrrchr(data + offset, c);
	}

	const tchar* StringBufferBase::GetFileName() const
	{
		if (const tchar* lps = TStrrchr(data, PathSeparator))
			return lps + 1;
		return data;
	}

	StringBufferBase& StringBufferBase::EnsureEndsWithSlash()
	{
		UBA_ASSERT(count);
		if (data[count - 1] == PathSeparator)
			return *this;
		UBA_ASSERT(count < capacity - 1);
		data[count++] = PathSeparator;
		data[count] = 0;
		return *this;
	}

	StringBufferBase& StringBufferBase::FixPathSeparators()
	{
		uba::FixPathSeparators(data);
		return *this;
	}

	StringBufferBase& StringBufferBase::MakeLower()
	{
		for (tchar* it = data; *it; ++it)
			*it = ToLower(*it);
		return *this;
	}

	bool StringBufferBase::Parse(u64& out)
	{
		if (!count)
			return false;

		#if PLATFORM_WINDOWS
		out = wcstoull(data, nullptr, 10);
		#else
		out = strtoull(data, nullptr, 10);
		#endif
		return out != 0 || Equals(TC("0"));
	}

	bool StringBufferBase::Parse(u32& out)
	{
		if (!count)
			return false;
		#if PLATFORM_WINDOWS
		out = wcstoul(data, nullptr, 10);
		#else
		out = strtoul(data, nullptr, 10);
		#endif
		return out != 0 || Equals(TC("0"));
	}

	bool StringBufferBase::Parse(u16& out)
	{
		u32 temp;
		if (!Parse(temp))
			return false;
		if (temp > 65535)
			return false;
		out = u16(temp);
		return true;
	}

	bool StringBufferBase::Parse(float& out)
	{
		if (!count)
			return false;
		#if PLATFORM_WINDOWS
		out = wcstof(data, nullptr);
		#else
		out = strtof(data, nullptr);
		#endif
		return out != 0 || Equals(TC("0"));
	}
}
