// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaObjectHandleSubsystem.h"

#include "AvaHandleUtilities.h"
#include "AvaMaskActorMaterialCollectionHandle.h"
#include "AvaMaskAvaShapeMaterialCollectionHandle.h"
#include "AvaMaskLog.h"
#include "AvaMaskText3DActorMaterialCollectionHandle.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Handling/AvaMaskDesignedMaterialHandle.h"
#include "Handling/AvaMaskMaterialInstanceHandle.h"
#include "Handling/AvaMaskParametricMaterialHandle.h"
#include "IAvaMaskMaterialCollectionHandle.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"

void UAvaObjectHandleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FindObjectHandleFactories();
}

TSharedPtr<IAvaObjectHandle> UAvaObjectHandleSubsystem::MakeHandle(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
	, FName InTag)
{
	for (const TPair<FIsSupportedFunction, FMakeHandleFunction>& SupportedFactoryPair : ObjectHandleFactories)
	{
		if (SupportedFactoryPair.Key(InStruct, InInstance, InTag))
		{
			TSharedPtr<IAvaObjectHandle> Handle = SupportedFactoryPair.Value(InInstance);
			if (!UE::Ava::Internal::IsHandleValid(Handle))
			{
				const FString ObjectName = GetInstanceName(InStruct, InInstance);
				if (InTag.IsNone())
				{
					UE_LOG(LogAvaMask, Warning, TEXT("Object Handle for '%s' was created but invalid."), *ObjectName)
				}
				else
				{
					UE_LOG(LogAvaMask, Warning, TEXT("Object Handle for '%s' (with tag '%s') was created but invalid."), *ObjectName, *InTag.ToString())
				}
			}
			return Handle;
		}
	}

	UE_LOG(LogAvaMask, Display, TEXT("No ObjectHandle found for '%s'"), *InStruct->GetName());
	
	return nullptr;
}

// @note: registration order matters!
void UAvaObjectHandleSubsystem::FindObjectHandleFactories()
{
	// Material Collection Handles
	{
		ObjectHandleFactories.Add({
			FAvaMaskAvaTextActorMaterialCollectionHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskAvaTextActorMaterialCollectionHandle>(Cast<AActor>(InObject.Get<UObject*>()));
			}});
	
		ObjectHandleFactories.Add({
			FAvaMaskAvaShapeMaterialCollectionHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskAvaShapeMaterialCollectionHandle>(Cast<AActor>(InObject.Get<UObject*>()));
			}});
	
		ObjectHandleFactories.Add({
			FAvaMaskText3DActorMaterialCollectionHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskText3DActorMaterialCollectionHandle>(Cast<AActor>(InObject.Get<UObject*>()));
			}});
	
		ObjectHandleFactories.Add({
			FAvaMaskActorMaterialCollectionHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InObject) -> TSharedPtr<IAvaMaskMaterialCollectionHandle>
			{
				return MakeShared<FAvaMaskActorMaterialCollectionHandle>(Cast<AActor>(InObject.Get<UObject*>()));
			}});
	}

	// Material Handles
	{
		ObjectHandleFactories.Add({
			FAvaMaskDesignedMaterialHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskDesignedMaterialHandle>(Cast<UDynamicMaterialInstance>(InMaterial.Get<UObject*>()));
			}});

		ObjectHandleFactories.Add({
			FAvaMaskParametricMaterialHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskParametricMaterialHandle>(InMaterial.Get<FStructView>());
			}});
	
		ObjectHandleFactories.Add({
			FAvaMaskMaterialInstanceHandle::IsSupported
			, [](const TVariant<UObject*, FStructView>& InMaterial) -> TSharedPtr<IAvaMaskMaterialHandle>
			{
				return MakeShared<FAvaMaskMaterialInstanceHandle>(Cast<UMaterialInterface>(InMaterial.Get<UObject*>()));
			}});
	}
}

FString UAvaObjectHandleSubsystem::GetInstanceName(const UStruct* InStruct, const TVariant<UObject*, FStructView>& InInstance)
{
	struct FNameGetter
	{
		FString operator()(const UObject* InValue) const
		{
			if (::IsValid(InValue))
			{
				return InValue->GetName();
			}

			return TEXT("");
		}
		
		FString operator()(const FStructView& InValue) const
		{
			return InValue.GetScriptStruct()->GetName();
		}
	};

	FNameGetter NameGetter;

	if (FString Name = Visit(NameGetter, InInstance);
		!Name.IsEmpty())
	{
		return Name;
	}

	if (const UStruct* Struct = InStruct)
	{
		return Struct->GetName();
	}

	return TEXT("");
}
