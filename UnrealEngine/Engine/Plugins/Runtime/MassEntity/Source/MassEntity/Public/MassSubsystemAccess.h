// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassRequirements.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#else
#include "Engine/Engine.h"
#endif // WITH_EDITOR


struct MASSENTITY_API FMassSubsystemAccess
{
	explicit FMassSubsystemAccess(UWorld* InWorld = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Statically-typed subsystems

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem()
	{
		const uint32 SystemIndex = FMassExternalSubsystemBitSet::GetTypeIndex<T>();
		if (ensure(MutableSubsystemsBitSet.IsBitSet(SystemIndex)))
		{
			return GetSubsystemInternal<T>(SystemIndex);
		}

		return nullptr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked()
	{
		T* InstancePtr = GetMutableSubsystem<T>();
		check(InstancePtr);
		return *InstancePtr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem()
	{
		const uint32 SystemIndex = FMassExternalSubsystemBitSet::GetTypeIndex<T>();
		if (ensure(ConstSubsystemsBitSet.IsBitSet(SystemIndex) || MutableSubsystemsBitSet.IsBitSet(SystemIndex)))
		{
			return GetSubsystemInternal<T>(SystemIndex);
		}
		return nullptr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked()
	{
		const T* InstancePtr = GetSubsystem<T>();
		check(InstancePtr);
		return *InstancePtr;
	}

	//////////////////////////////////////////////////////////////////////////
	// UClass-provided subsystems

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		const uint32 SystemIndex = FMassExternalSubsystemBitSet::GetTypeIndex(**SubsystemClass);
		if (ensure(MutableSubsystemsBitSet.IsBitSet(SystemIndex)))
		{
			return GetSubsystemInternal<T>(SystemIndex, SubsystemClass);
		}

		return nullptr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		T* InstancePtr = GetMutableSubsystem<T>(SubsystemClass);
		check(InstancePtr);
		return *InstancePtr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		const uint32 SystemIndex = FMassExternalSubsystemBitSet::GetTypeIndex(**SubsystemClass);
		if (ensure(ConstSubsystemsBitSet.IsBitSet(SystemIndex) || MutableSubsystemsBitSet.IsBitSet(SystemIndex)))
		{
			return GetSubsystemInternal<T>(SystemIndex, SubsystemClass);
		}
		return nullptr;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		const T* InstancePtr = GetSubsystem<T>(SubsystemClass);
		check(InstancePtr);
		return *InstancePtr;
	}

	//////////////////////////////////////////////////////////////////////////
	// remaining API

	bool CacheSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements);
	void SetSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements);

	void GetSubsystemRequirementBits(FMassExternalSubsystemBitSet& OutConstSubsystemsBitSet, FMassExternalSubsystemBitSet& OutMutableSubsystemsBitSet)
	{
		OutConstSubsystemsBitSet = ConstSubsystemsBitSet;
		OutMutableSubsystemsBitSet = MutableSubsystemsBitSet;
	}

	void SetSubsystemRequirementBits(const FMassExternalSubsystemBitSet& InConstSubsystemsBitSet, const FMassExternalSubsystemBitSet& InMutableSubsystemsBitSet)
	{
		ConstSubsystemsBitSet = InConstSubsystemsBitSet;
		MutableSubsystemsBitSet = InMutableSubsystemsBitSet;
	}

	template<typename T>
	static constexpr bool DoesRequireWorld()
	{
		constexpr bool bIsWorldSubsystem = TIsDerivedFrom<T, UWorldSubsystem>::IsDerived;
		constexpr bool bIsGameInstanceSubsystem = TIsDerivedFrom<T, UGameInstanceSubsystem>::IsDerived;
		constexpr bool bIsLocalPlayerSubsystem = TIsDerivedFrom<T, ULocalPlayerSubsystem>::IsDerived;

		return (bIsWorldSubsystem || bIsGameInstanceSubsystem || bIsLocalPlayerSubsystem);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	static T* FetchSubsystemInstance(UWorld* World)
	{
		check(World);
		if constexpr (TIsDerivedFrom<T, UWorldSubsystem>::IsDerived)
		{
			return UWorld::GetSubsystem<T>(World);
		}
		else if constexpr (TIsDerivedFrom<T, UGameInstanceSubsystem>::IsDerived)
		{
			return UGameInstance::GetSubsystem<T>(World->GetGameInstance());
		}
		else if constexpr (TIsDerivedFrom<T, ULocalPlayerSubsystem>::IsDerived)
		{
			// note that this default implementation will work only for the first player in a local-coop game
			// to customize this behavior specialize the FetchSubsystemInstance template function for the type you need. 
			return ULocalPlayer::GetSubsystem<T>(World->GetFirstLocalPlayerFromController());
		}
		else
		{
			checkf(false, TEXT("FMassSubsystemAccess::FetchSubsystemInstance: Unhandled world-related USubsystem class %s"), *T::StaticClass()->GetName());
		}
	}
	
	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	static T* FetchSubsystemInstance()
	{
		if constexpr (TIsDerivedFrom<T, UEngineSubsystem>::IsDerived)
		{
			return GEngine->GetEngineSubsystem<T>();
		}
#if WITH_EDITOR
		else if constexpr (TIsDerivedFrom<T, UEditorSubsystem>::IsDerived)
		{
			return GEditor->GetEditorSubsystem<T>();
		}
#endif // WITH_EDITOR
		else
		{
			checkf(false, TEXT("FMassSubsystemAccess::FetchSubsystemInstance: Unhandled world-less USubsystem class %s"), *T::StaticClass()->GetName());
		}
	}

	static USubsystem* FetchSubsystemInstance(UWorld* World, TSubclassOf<USubsystem> SubsystemClass);

protected:
	template<typename T>
	T* GetSubsystemInternal(const uint32 SystemIndex)
	{
		if (UNLIKELY(Subsystems.IsValidIndex(SystemIndex) == false))
		{
			Subsystems.AddZeroed(Subsystems.Num() - SystemIndex + 1);
		}

		T* SystemInstance = (T*)Subsystems[SystemIndex];
		if (SystemInstance == nullptr)
		{
			if constexpr (DoesRequireWorld<T>())
			{
				SystemInstance = FetchSubsystemInstance<std::remove_const_t<T>>(World.Get());
			}
			else
			{
				SystemInstance = FetchSubsystemInstance<std::remove_const_t<T>>();
			}
			Subsystems[SystemIndex] = SystemInstance;
		}
		return SystemInstance;
	}

	template<typename T>
	T* GetSubsystemInternal(const uint32 SystemIndex, const TSubclassOf<USubsystem> SubsystemClass)
	{
		if (UNLIKELY(Subsystems.IsValidIndex(SystemIndex) == false))
		{
			Subsystems.AddZeroed(Subsystems.Num() - SystemIndex + 1);
		}

		USubsystem* SystemInstance = (T*)Subsystems[SystemIndex];
		if (SystemInstance == nullptr)
		{
			SystemInstance = FetchSubsystemInstance(World.Get(), SubsystemClass);
			Subsystems[SystemIndex] = SystemInstance;
		}
		return Cast<T>(SystemInstance);
	}

	bool CacheSubsystem(const uint32 SystemIndex);

	FMassExternalSubsystemBitSet ConstSubsystemsBitSet;
	FMassExternalSubsystemBitSet MutableSubsystemsBitSet;
	TArray<USubsystem*> Subsystems;
	TWeakObjectPtr<UWorld> World;
};
