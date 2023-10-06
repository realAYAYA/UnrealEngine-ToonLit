// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"

#define UE_API CORE_API

class FCbFieldViewIterator;
class FText;

namespace UE { class FLogTemplate; }

namespace UE
{

UE_API FLogTemplate* CreateLogTemplate(const TCHAR* Format);
UE_API FLogTemplate* CreateLogTemplate(const TCHAR* TextNamespace, const TCHAR* TextKey, const TCHAR* Format);
UE_API void DestroyLogTemplate(FLogTemplate* Template);

UE_API void FormatLogTo(FUtf8StringBuilderBase& Out, const FLogTemplate* Template, const FCbFieldViewIterator& Fields);
UE_API void FormatLogTo(FWideStringBuilderBase& Out, const FLogTemplate* Template, const FCbFieldViewIterator& Fields);
UE_API FText FormatLogToText(const FLogTemplate* Template, const FCbFieldViewIterator& Fields);

} // UE

#undef UE_API
