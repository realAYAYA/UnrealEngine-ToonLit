// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreSharedPCH.h"

#include "HAL/PlatformAffinity.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Math/RandomStream.h"
#include "Math/TransformCalculus.h"
#include "Math/TransformCalculus2D.h"
#include "Misc/Attribute.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "Serialization/MemoryReader.h"

// From CoreUObject:
#include "Serialization/ArchiveUObject.h"
#include "UObject/Class.h"
#include "UObject/CoreNative.h"
#include "UObject/GarbageCollection.h"
#include "UObject/GCObject.h"
#include "UObject/GeneratedCppIncludes.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectMarks.h"
#include "UObject/WeakObjectPtr.h"
