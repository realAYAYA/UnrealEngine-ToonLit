// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/AnsiString.h"
#include "Containers/StringIncludes.cpp.inl"

#define UE_STRING_CLASS             FAnsiString
#define UE_STRING_CHARTYPE          ANSICHAR
#define UE_STRING_CHARTYPE_IS_TCHAR 0
	#include "Containers/String.cpp.inl"
#undef UE_STRING_CHARTYPE_IS_TCHAR
#undef UE_STRING_CHARTYPE
#undef UE_STRING_CLASS
