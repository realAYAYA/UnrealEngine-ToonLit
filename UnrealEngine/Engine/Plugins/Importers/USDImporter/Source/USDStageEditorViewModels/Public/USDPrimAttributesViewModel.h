// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDValueConversion.h"

#include "UsdWrappers/UsdStage.h"

#include "Templates/SharedPointer.h"

class FUsdPrimAttributesViewModel;

class USDSTAGEEDITORVIEWMODELS_API FUsdPrimAttributeViewModel : public TSharedFromThis< FUsdPrimAttributeViewModel >
{
public:
	explicit FUsdPrimAttributeViewModel( FUsdPrimAttributesViewModel* InOwner );

	// This member function is necessary because the no-RTTI slate module can't query USD for the available token options
	TArray< TSharedPtr< FString > > GetDropdownOptions() const;
	void SetAttributeValue( const UsdUtils::FConvertedVtValue& InValue );

public:
	FString Label;
	UsdUtils::FConvertedVtValue Value;
	FString ValueRole;
	bool bReadOnly = false;

private:
	FUsdPrimAttributesViewModel* Owner;
};


class USDSTAGEEDITORVIEWMODELS_API FUsdPrimAttributesViewModel
{
public:
	template<typename T>
	void CreatePrimAttribute( const FString& AttributeName, const T& Value, UsdUtils::EUsdBasicDataTypes UsdType, const FString& ValueRole = FString(), bool bReadOnly = false );
	void CreatePrimAttribute( const FString& AttributeName, const UsdUtils::FConvertedVtValue& Value, bool bReadOnly = false );

	void SetPrimAttribute( const FString& AttributeName, const UsdUtils::FConvertedVtValue& Value );

	void Refresh( const UE::FUsdStageWeak& UsdStage, const TCHAR* PrimPath, float TimeCode );

public:
	TArray< TSharedPtr< FUsdPrimAttributeViewModel > > PrimAttributes;

private:
	UE::FUsdStageWeak UsdStage;
	FString PrimPath;
};
