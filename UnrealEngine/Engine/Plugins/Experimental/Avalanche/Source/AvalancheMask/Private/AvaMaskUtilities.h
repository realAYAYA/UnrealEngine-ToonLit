// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GeometryMaskTypes.h"
#include "MaterialTypes.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTypeTraits.h"

class UMaterialFunctionInterface;
class UDynamicMaterialInstance;
class UMaterialInstance;

namespace UE::AvaMask::Internal
{
	/** Note that this assumes the parameters have the same name for all materials, which they should if using the provided material function. */
	static FMaterialParameterInfo TextureParameterInfo = { TEXT("Mask_Texture") };
	static FMaterialParameterInfo BaseOpacityParameterInfo = { TEXT("Mask_BaseOpacity") };
	static FMaterialParameterInfo ChannelParameterInfo = { TEXT("Mask_Channel") };
	static FMaterialParameterInfo InvertParameterInfo = { TEXT("Mask_Invert") };
	static FMaterialParameterInfo PaddingParameterInfo = { TEXT("Mask_Padding") };
	static FMaterialParameterInfo FeatherParameterInfo = { TEXT("Mask_Feather") };
	
	static TMap<FLinearColor, EGeometryMaskColorChannel> VectorToMaskChannelEnum =
	{
		{ {1, 0, 0, 0}, EGeometryMaskColorChannel::Red },
		{ {0, 1, 0, 0}, EGeometryMaskColorChannel::Green },
		{ {0, 0, 1, 0}, EGeometryMaskColorChannel::Blue },
		{ {0, 0, 0, 1}, EGeometryMaskColorChannel::Alpha }
	};
	
	static TMap<EGeometryMaskColorChannel, FLinearColor> MaskChannelEnumToVector =
	{
		{ EGeometryMaskColorChannel::Red, {1, 0, 0, 0} },
		{ EGeometryMaskColorChannel::Green, {0, 1, 0, 0} },
		{ EGeometryMaskColorChannel::Blue, {0, 0, 1, 0} },
		{ EGeometryMaskColorChannel::Alpha, {0, 0, 0, 1} }
	};

	static FName HandleTag = TEXT("Modifier.Mask2D");

	/** Utility function, will set "OtherActor" if the components owner differs from the provided one. */
	FSoftComponentReference MakeComponentReference(const AActor* InOwner, const UActorComponent* InComponent);

	/** Creates a unique (deterministic) key based on the provided parameters. */
	uint32 MakeMaterialInstanceKey(const UMaterialInterface* InMaterial, const FName InMaskChannelName, EBlendMode InBlendMode, const int32 InSeed = 128);

	UActorComponent* FindOrAddComponent(const TSubclassOf<UActorComponent>& InComponentClass, AActor* InActor);

	template<
		typename InComponentClass
		UE_REQUIRES(std::is_base_of_v<UActorComponent, InComponentClass>)>
	InComponentClass* FindOrAddComponent(AActor* InActor)
	{
		return Cast<InComponentClass>(FindOrAddComponent(InComponentClass::StaticClass(), InActor));
	}

	void RemoveComponentByInterface(const TSubclassOf<UInterface>& InInterfaceType, const AActor* InActor);

	template<
		typename InInterfaceType
		UE_REQUIRES(std::is_base_of_v<UInterface, InInterfaceType>)>
	void RemoveComponentByInterface(AActor* InActor)
	{
		RemoveComponentByInterface(InInterfaceType::StaticClass(), InActor);
	}

	FString GetGeneratedMaterialPath();

	FString GetBlendModeString(const EBlendMode InBlendMode);

	/** Returns whichever blend mode satisfies the requirement AND user specified blend mode */
	EBlendMode GetTargetBlendMode(const EBlendMode InFromMaterial, const EBlendMode InRequired);

	template <typename ElementType, typename KeyFuncs, typename AllocatorType, typename LookupType, typename LambdaType>
	ElementType& FindOrAddByLambda(TSet<ElementType, KeyFuncs, AllocatorType>& Set, const LookupType& Key, LambdaType Lambda)
	{
		uint32 KeyHash = KeyFuncs::GetKeyHash(Key);
		if (ElementType* Found = Set.FindByHash(Key))
		{
			return *Found;
		}

		ElementType NewElement = Invoke(Lambda, Key);
		check(KeyFuncs::GetKeyHash(NewElement) == KeyHash);
		FSetElementId NewElementId = Set.AddByHash(KeyHash, MoveTemp(NewElement));
		return Set[NewElementId];
	}

	template <typename KeyType, typename ValueType, typename AllocatorType, typename KeyFuncs, typename InitKeyType, typename LambdaType>
	ValueType& FindOrAddByLambda(TMap<KeyType, ValueType, AllocatorType, KeyFuncs>& Map, InitKeyType&& Key, LambdaType Lambda)
	{
		uint32 KeyHash = KeyFuncs::GetKeyHash(Key);
		if (ValueType* Found = Map.FindByHash(KeyHash, Key))
		{
			return *Found;
		}

		ValueType NewElement = Invoke(Lambda);
		return Map.AddByHash(KeyHash, Forward<InitKeyType>(Key), MoveTemp(NewElement));
	}

	UMaterialInterface* GetNonTransientParentMaterial(const UMaterialInstance* InMaterialInstance);
}
