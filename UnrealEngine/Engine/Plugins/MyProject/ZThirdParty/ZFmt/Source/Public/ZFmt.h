#pragma once

#include <string>

#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/xchar.h"

#include "HAL/Platform.h"

#if PLATFORM_TCHAR_IS_CHAR16
typedef std::u16string ZU16String;
typedef fmt::basic_string_view<char16_t> ZU16StringView;
typedef fmt::basic_memory_buffer<char16_t> ZU16MemoryBuffer;
typedef fmt::basic_format_parse_context<char16_t> ZU16FormatParseContext;
#else
typedef std::wstring ZU16String;
typedef fmt::basic_string_view<wchar_t> ZU16StringView;
typedef fmt::basic_memory_buffer<wchar_t> ZU16MemoryBuffer;
typedef fmt::basic_format_parse_context<wchar_t> ZU16FormatParseContext;
#endif
