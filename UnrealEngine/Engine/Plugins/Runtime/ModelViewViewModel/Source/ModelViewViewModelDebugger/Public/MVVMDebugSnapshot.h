// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/ObjectMacros.h"
#include "MVVMDebugItemId.h"

struct FMVVMViewDebugEntry;
struct FMVVMViewClassDebugEntry;
struct FMVVMViewModelDebugEntry;
class UMVVMViewClass;

namespace UE::MVVM
{

class MODELVIEWVIEWMODELDEBUGGER_API FDebugSnapshot
{
private:
	TArray<TSharedPtr<FMVVMViewDebugEntry>> Views;
	TArray<TSharedPtr<FMVVMViewClassDebugEntry>> ViewClasses;
	TArray<TSharedPtr<FMVVMViewModelDebugEntry>> ViewModels;

public:
	TArrayView<TSharedPtr<FMVVMViewDebugEntry>> GetViews()
	{
		return Views;
	}
	TArrayView<TSharedPtr<FMVVMViewModelDebugEntry>> GetViewModels()
	{
		return ViewModels;
	}

	TSharedPtr<FMVVMViewDebugEntry> FindView(FGuid Id) const;
	TSharedPtr<FMVVMViewModelDebugEntry> FindViewModel(FGuid Id) const;

	static TSharedPtr<FDebugSnapshot> CreateSnapshot();

private:
	TSharedRef<FMVVMViewClassDebugEntry> FindOrAddViewClassEntry(const UMVVMViewClass* ViewClass);
};

};