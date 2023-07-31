// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/**
 * 
 */
class FMVVMEditorStyle : public FSlateStyleSet
{

public:
	static void CreateInstance();
	static void DestroyInstance();

	static FMVVMEditorStyle& Get()
	{
		check(Instance);
		return *Instance;
	}

private:
	FMVVMEditorStyle();
	FMVVMEditorStyle(const FMVVMEditorStyle&) = delete;
	FMVVMEditorStyle& operator=(const FMVVMEditorStyle&) = delete;
	~FMVVMEditorStyle();
	struct FCustomDeleter
	{
		void operator()(FMVVMEditorStyle* Ptr) const 
		{
			delete Ptr;
		}
	};
	static TUniquePtr<FMVVMEditorStyle, FCustomDeleter> Instance;
};
