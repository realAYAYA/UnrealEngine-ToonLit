// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Templates/Casts.h"
#include "UObject/Interface.h"
#include "Engine/AssetUserData.h"
#include "Interface_AssetUserData.generated.h"

/** Interface for assets/objects that can own UserData **/
UINTERFACE(MinimalApi, meta=(CannotImplementInterfaceInBlueprint))
class UInterface_AssetUserData : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IInterface_AssetUserData
{
	GENERATED_IINTERFACE_BODY()

	virtual void AddAssetUserData(UAssetUserData* InUserData) {}
	
	/**
	* Returns an instance of the provided AssetUserData class if it's contained in the target asset.
	*
	* @param	InUserDataClass		UAssetUserData sub class to get
	*
	* @return	The instance of the UAssetUserData class contained, or null if it doesn't exist
	*/
	UFUNCTION(BlueprintCallable, Category = AssetUserData)
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
	{
		return nullptr;
	}

	/**
	* Checks whether or not an instance of the provided AssetUserData class is contained.
	*
	* @param	InUserDataClass		UAssetUserData sub class to check for
	*
	* @return	Whether or not an instance of InUserDataClass was found
	*/
	UFUNCTION(BlueprintCallable, Category=AssetUserData)
	virtual bool HasAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
	{		
		return GetAssetUserDataOfClass(InUserDataClass) != nullptr;
	}
	
/**
	* Creates and adds an instance of the provided AssetUserData class to the target asset.
	*
	* @param	InUserDataClass		UAssetUserData sub class to create
	*
	* @return	Whether or not an instance of InUserDataClass was succesfully added
	*/
	UFUNCTION(BlueprintCallable, Category=AssetUserData)
	virtual bool AddAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
	{
		check(InUserDataClass.Get() != nullptr);
		AddAssetUserData(NewObject<UAssetUserData>(_getUObject(), InUserDataClass.Get()));
		return GetAssetUserDataOfClass(InUserDataClass.Get()) != nullptr;
	}

	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const
	{
		return nullptr;
	}

	template<typename T>
	T* GetAssetUserData()
	{
		return Cast<T>( GetAssetUserDataOfClass(T::StaticClass()) );
	}

	template<typename T>
	T* GetAssetUserDataChecked()
	{
		return CastChecked<T>(GetAssetUserDataOfClass(T::StaticClass()));
	}

	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) {}

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
