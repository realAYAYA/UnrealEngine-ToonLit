// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AttributeTypes.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Misc/DelayedAutoRegister.h"
#include "Animation/AnimationSettings.h"
#include "Engine/UserDefinedStruct.h"

namespace UE
{
	namespace Anim
	{		
		TArray<TWeakObjectPtr<const UScriptStruct>> AttributeTypes::RegisteredTypes;
		TArray<TUniquePtr<IAttributeBlendOperator>> AttributeTypes::Operators;
		TArray<TWeakObjectPtr<const UScriptStruct>> AttributeTypes::InterpolatableTypes;
		std::atomic<bool> AttributeTypes::bInitialized = false;		
		AttributeTypes::FOnAttributeTypesChanged AttributeTypes::OnAttributeTypesChangedDelegate;
		
		void AttributeTypes::LazyInitialize()
		{
			bool bWasUninitialized = false;
			if (bInitialized.compare_exchange_strong(bWasUninitialized, true))
			{
				Initialize();
			}
		}

		void AttributeTypes::Initialize()
		{
			RegisterType<FFloatAnimationAttribute>();
			RegisterType<FIntegerAnimationAttribute>();
			RegisterType<FStringAnimationAttribute>();
			RegisterType<FTransformAnimationAttribute>();
			RegisterType<FVectorAnimationAttribute>();
			RegisterType<FQuaternionAnimationAttribute>();

			for (const TSoftObjectPtr<UUserDefinedStruct>& UserDefinedStruct : UAnimationSettings::Get()->UserDefinedStructAttributes)
			{
				UE::Anim::AttributeTypes::RegisterNonBlendableType(UserDefinedStruct.LoadSynchronous());
			}
		}
		
		static FDelayedAutoRegisterHelper DelayedAttributeTypesInitializationHelper(EDelayedRegisterRunPhase::ObjectSystemReady, []()
		{
			UE::Anim::AttributeTypes::LazyInitialize();
		});

		bool AttributeTypes::RegisterNonBlendableType(const UScriptStruct* InScriptStruct)
		{
			if (!InScriptStruct)
			{
				return false;
			}
				
			if (IsTypeRegistered(InScriptStruct))
			{
				return false;
			}
				
			AttributeTypes::RegisteredTypes.Add(InScriptStruct);
			AttributeTypes::Operators.Add(MakeUnique<FNonBlendableAttributeBlendOperator>(InScriptStruct));

			OnAttributeTypesChangedDelegate.Broadcast(InScriptStruct, true);
				
			return true;
		}

		void AttributeTypes::UnregisterType(const UScriptStruct* InScriptStruct)
		{
			if (UObjectInitialized())
			{
				const int32 Index = AttributeTypes::RegisteredTypes.IndexOfByKey(InScriptStruct);
				if (Index != INDEX_NONE)
				{
					AttributeTypes::RegisteredTypes.RemoveAtSwap(Index);
					AttributeTypes::Operators.RemoveAtSwap(Index);
				}
			}
				
			OnAttributeTypesChangedDelegate.Broadcast(InScriptStruct, false);
		}
	}
}

