// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextFromMetadataCommandlet.generated.h"

/**
 *	UGatherTextFromMetaDataCommandlet: Localization commandlet that collects all text to be localized from generated metadata.
 */
UCLASS()
class UGatherTextFromMetaDataCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	//~ Begin UGatherTextCommandletBase  Interface
	virtual bool ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const override;
	//~ End UGatherTextCommandletBase  Interface

	struct FGatherParameters
	{
		TArray<FString> InputKeys;
		TArray<FString> OutputNamespaces;
		TArray<FString> OutputKeys;
	};

private:
	void GatherTextFromUObjects(const TArray<FString>& IncludePaths, const TArray<FString>& ExcludePaths, const FGatherParameters& Arguments);

	void GatherTextFromField(UField* Field, const FGatherParameters& Arguments, const FName InPlatformName);
	void GatherTextFromField(FField* Field, const FGatherParameters& Arguments, const FName InPlatformName);
	
	template <typename FieldType>
	bool ShouldGatherFromField(const FieldType* Field, const bool bIsEditorOnly);

	template <typename FieldType>
	void GatherTextFromFieldImpl(FieldType* Field, const FGatherParameters& Arguments, const FName InPlatformName);

	template <typename FieldType>
	void EnsureFieldDisplayNameImpl(FieldType* Field, const bool bIsBool);

private:
	bool ShouldGatherFromEditorOnlyData;

	struct FFieldClassFilter
	{
		explicit FFieldClassFilter(const FFieldClass* InFieldClass)
			: FieldClass(InFieldClass)
			, ObjectClass(nullptr)
		{
		}

		explicit FFieldClassFilter(const UClass* InObjectClass)
			: FieldClass(nullptr)
			, ObjectClass(InObjectClass)
		{
		}

		FString GetName() const
		{
			if (FieldClass)
			{
				return FieldClass->GetName();
			}
			if (ObjectClass)
			{
				return ObjectClass->GetName();
			}
			return FString();
		}

		bool TestClass(const FFieldClass* InFieldClass) const
		{
			return FieldClass && InFieldClass->IsChildOf(FieldClass);
		}

		bool TestClass(const UClass* InObjectClass) const
		{
			return ObjectClass && InObjectClass->IsChildOf(ObjectClass);
		}

	private:
		const FFieldClass* FieldClass;
		const UClass* ObjectClass;
	};

	/** Array of field types (eg, FProperty, UFunction, UScriptStruct, etc) that should be included or excluded in the current gather */
	TArray<FFieldClassFilter> FieldTypesToInclude;
	TArray<FFieldClassFilter> FieldTypesToExclude;

	/** Array of field owner types (eg, UMyClass, FMyStruct, etc) that should have fields within them included or excluded in the current gather */
	TArray<const UStruct*> FieldOwnerTypesToInclude;
	TArray<const UStruct*> FieldOwnerTypesToExclude;
};
