// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Containers/ArrayView.h"
#include "Delegates/Delegate.h"
#include "IAnimBlueprintCompilerHandler.h"

enum class EPropertyAccessBatchType : uint8;
class IAnimBlueprintGeneratedClassCompiledData;
class IAnimBlueprintCompilationBracketContext;

// Delegate called when the library is compiled (whether successfully or not)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPostLibraryCompiled, IAnimBlueprintCompilationBracketContext& /*InCompilationContext*/, IAnimBlueprintGeneratedClassCompiledData& /*OutCompiledData*/)

class UE_DEPRECATED(5.0, "FPropertyAccessCompilerHandler is no longer used. Use UAnimBlueprintExtension_PropertyAccess instead") FPropertyAccessCompilerHandler;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class FPropertyAccessCompilerHandler : public IAnimBlueprintCompilerHandler
{
public:
	// Add a copy to the property access library we are compiling
	// @return an integer handle to the pending copy. This can be resolved to a true copy index by calling MapCopyIndex
	virtual int32 AddCopy(TArrayView<FString> InSourcePath, TArrayView<FString> InDestPath, EPropertyAccessBatchType InBatchType, UObject* InObject = nullptr) = 0;

	// Delegate called when the library is compiled (whether successfully or not)
	virtual FSimpleMulticastDelegate& OnPreLibraryCompiled() = 0;

	// Delegate called when the library is compiled (whether successfully or not)
	virtual FOnPostLibraryCompiled& OnPostLibraryCompiled() = 0;

	// Maps the initial integer copy handle to a true handle, post compilation
	virtual int32 MapCopyIndex(int32 InIndex) const = 0;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS