// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Framework/PropertyViewer/PropertyPath.h"
#include "Framework/SlateDelegates.h"
#include "UObject/Field.h"
#include "UObject/Class.h"

class SWidget;

namespace UE::PropertyViewer
{
class INotifyHook;

namespace Private
{
class FPropertyValueFactoryImpl;
} // namespace


/** */
class ADVANCEDWIDGETS_API FPropertyValueFactory
{
public:
	struct FHandle
	{
	private:
		friend FPropertyValueFactory;
		int32 Id = 0;
	public:
		bool operator== (FHandle Other) const
		{
			return Id == Other.Id;
		}
		bool operator!= (FHandle Other) const
		{
			return Id != Other.Id;
		}
		bool IsValid() const
		{
			return Id != 0;
		}
	};

	struct FGenerateArgs
	{
		FPropertyPath Path;
		INotifyHook* NotifyHook = nullptr;
		bool bCanEditValue = false;
	};

	/** */
	DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FOnGenerate, FGenerateArgs);

public:
	FPropertyValueFactory();
	FPropertyValueFactory(const FPropertyValueFactory&) = delete;
	FPropertyValueFactory& operator=(const FPropertyValueFactory&) = delete;

	TSharedPtr<SWidget> Generate(FGenerateArgs Args) const;
	TSharedPtr<SWidget> GenerateDefault(FGenerateArgs Args) const;

	bool HasCustomPropertyValue(const FFieldClass* Property) const;
	bool HasCustomPropertyValue(const UStruct* Struct) const;

	FHandle Register(const FFieldClass* Field, FOnGenerate OnGenerate);
	FHandle Register(const UStruct* Struct, FOnGenerate OnGenerate);
	void Unregister(FHandle Handle);

private:
	static FHandle MakeHandle();
	TPimplPtr<Private::FPropertyValueFactoryImpl> Impl;
};

} //namespace
