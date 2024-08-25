// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDValueConversion.h"

#include "UsdWrappers/UsdStage.h"

#include "Templates/SharedPointer.h"
#include "Widgets/Views/SHeaderRow.h"

class FUsdObjectFieldsViewModel;

enum class EObjectFieldType : int32
{
	Metadata = 0,
	Attribute = 1,
	Relationship = 2,
	MAX = 3
};

namespace ObjectFieldColumnIds
{
	inline const FName TypeColumn = TEXT("FieldType");
	inline const FName NameColumn = TEXT("FieldName");
	inline const FName ValueColumn = TEXT("FieldValue");
};

class USDSTAGEEDITORVIEWMODELS_API FUsdObjectFieldViewModel : public TSharedFromThis<FUsdObjectFieldViewModel>
{
public:
	explicit FUsdObjectFieldViewModel(FUsdObjectFieldsViewModel* InOwner);

	// This member function is necessary because the no-RTTI slate module can't query USD for the available token options
	TArray<TSharedPtr<FString>> GetDropdownOptions() const;
	void SetAttributeValue(const UsdUtils::FConvertedVtValue& InValue);

public:
	EObjectFieldType Type;
	FString Label;
	UsdUtils::FConvertedVtValue Value;
	FString ValueRole;
	bool bReadOnly = false;

private:
	FUsdObjectFieldsViewModel* Owner;
};

class USDSTAGEEDITORVIEWMODELS_API FUsdObjectFieldsViewModel
{
public:
	template<typename T>
	void CreateField(
		EObjectFieldType Type,
		const FString& FieldName,
		const T& Value,
		UsdUtils::EUsdBasicDataTypes UsdType,
		const FString& ValueRole = FString(),
		bool bReadOnly = false
	);
	void CreateField(EObjectFieldType Type, const FString& FieldName, const UsdUtils::FConvertedVtValue& Value, bool bReadOnly = false);

	void SetFieldValue(const FString& FieldName, const UsdUtils::FConvertedVtValue& Value);

	void Refresh(const UE::FUsdStageWeak& UsdStage, const TCHAR* ObjectPath, float TimeCode);
	void Sort();

	UE::FUsdStageWeak GetUsdStage() const;
	FString GetObjectPath() const;

public:
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Ascending;
	FName CurrentSortColumn = ObjectFieldColumnIds::TypeColumn;
	TArray<TSharedPtr<FUsdObjectFieldViewModel>> Fields;

private:
	UE::FUsdStageWeak UsdStage;
	FString ObjectPath;
};
