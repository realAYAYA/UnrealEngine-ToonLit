// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMaskLog.h"
#include "IAvaObjectHandle.h"
#include "Misc/TVariant.h"
#include "StructView.h"
#include "Subsystems/EngineSubsystem.h"

#include "AvaObjectHandleSubsystem.generated.h"

class UAvaObjectHandleSubsystem;

/** Responsible for providing Handlers for a given UObject. */
UCLASS()
class UAvaObjectHandleSubsystem
	: public UEngineSubsystem
{
	GENERATED_BODY()

public:
	// ~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// ~End USubsystem

	TSharedPtr<IAvaObjectHandle> MakeHandle(const UStruct* InStruct, const TVariant<UObject*, FStructView>& InInstance, FName InTag = NAME_None);

	template <typename HandleType, typename ObjectType = UObject
		UE_REQUIRES(!std::is_const_v<HandleType> && std::is_base_of_v<IAvaObjectHandle, HandleType>
				&&	!std::is_const_v<ObjectType> && TModels_V<CStaticClassProvider, std::decay_t<ObjectType>>)>
    TSharedPtr<HandleType> MakeHandle(ObjectType* InInstance, FName InTag = NAME_None)
    {
		if (!InInstance)
		{
			UE_LOG(LogAvaMask, Warning, TEXT("Invalid or null object provided to MakeHandle"));
			return nullptr;
		}

		TVariant<UObject*, FStructView> Variant(TInPlaceType<UObject*>(), InInstance);
		
    	return StaticCastSharedPtr<HandleType>(MakeHandle(InInstance->GetClass(), MoveTemp(Variant), InTag));
    }

	template <typename HandleType, typename StructType
		UE_REQUIRES(!std::is_const_v<HandleType> && std::is_base_of_v<IAvaObjectHandle, HandleType>
				&&	!std::is_const_v<StructType> && TModels_V<CStaticStructProvider, std::decay_t<StructType>>)>
	TSharedPtr<HandleType> MakeHandle(const FStructView& InInstance, FName InTag = NAME_None)
	{
		if (!InInstance.IsValid())
		{
			UE_LOG(LogAvaMask, Warning, TEXT("Invalid or null StructView provided to MakeHandle"));
			return nullptr;
		}

		if (!ensure(InInstance.GetPtr<StructType>()))
		{
			UE_LOG(LogAvaMask, Warning, TEXT("StructView does not point to the expected struct type \'%s\'"), *StructType::StaticStruct()->GetName());
			return nullptr;			
		}
		
		TVariant<UObject*, FStructView> Variant(TInPlaceType<FStructView>(), InInstance);
		
		return StaticCastSharedPtr<HandleType>(MakeHandle(InInstance.GetScriptStruct(), MoveTemp(Variant), InTag));
	}
 
private:
	void FindObjectHandleFactories();

	FString GetInstanceName(const UStruct* InStruct, const TVariant<UObject*, FStructView>& InInstance);

private:
	using FIsSupportedFunction = TFunction<bool(const UStruct*, const TVariant<UObject*, FStructView>&, FName)>;
	using FMakeHandleFunction = TFunction<TSharedPtr<IAvaObjectHandle>(const TVariant<UObject*, FStructView>&)>;

	TArray<TPair<FIsSupportedFunction, FMakeHandleFunction>> ObjectHandleFactories;
};
