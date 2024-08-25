// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "EntitySystem/MovieScenePartialProperties.inl"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "EntitySystem/MovieSceneBlenderSystem.h"
#include "EntitySystem/MovieSceneInitialValueCache.h"
#include "EntitySystem/MovieScenePropertySystemTypes.inl"
#include "EntitySystem/MovieSceneOperationalTypeConversions.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationExtension.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedPropertyStorage.h"


namespace UE
{
namespace MovieScene
{


template<typename PropertyTraits, typename MetaDatatype, typename MetaDataIndices, typename CompositeIndices, typename ...CompositeTypes>
struct TPropertyComponentHandlerImpl;

template<typename PropertyTraits, typename ...CompositeTypes>
struct TPropertyComponentHandler
	: TPropertyComponentHandlerImpl<PropertyTraits, typename PropertyTraits::MetaDataType, TMakeIntegerSequence<int, PropertyTraits::MetaDataType::Num>, TMakeIntegerSequence<int, sizeof...(CompositeTypes)>, CompositeTypes...>
{
};

template<typename, typename, typename>
struct TInitialValueProcessorImpl;

template<typename PropertyTraits, typename ...MetaDataTypes, int ...MetaDataIndices>
struct TInitialValueProcessorImpl<PropertyTraits, TPropertyMetaData<MetaDataTypes...>, TIntegerSequence<int, MetaDataIndices...>> : IInitialValueProcessor
{
	using StorageType = typename PropertyTraits::StorageType;

	TSortedMap<FInterrogationChannel, StorageType> ValuesByChannel;

	FBuiltInComponentTypes* BuiltInComponents;
	IInterrogationExtension* Interrogation;
	const FPropertyDefinition* PropertyDefinition;
	FCustomAccessorView CustomAccessors;

	FEntityAllocationWriteContext WriteContext;
	TPropertyValueStorage<PropertyTraits>* CacheStorage;

	TInitialValueProcessorImpl()
		: WriteContext(FEntityAllocationWriteContext::NewAllocation())
	{
		BuiltInComponents = FBuiltInComponentTypes::Get();

		Interrogation = nullptr;
		CacheStorage = nullptr;
	}

	virtual void Initialize(UMovieSceneEntitySystemLinker* Linker, const FPropertyDefinition* Definition, FInitialValueCache* InitialValueCache) override
	{
		PropertyDefinition = Definition;
		Interrogation = Linker->FindExtension<IInterrogationExtension>();
		WriteContext  = FEntityAllocationWriteContext(Linker->EntityManager);

		check(PropertyDefinition->MetaDataTypes.Num() == PropertyTraits::MetaDataType::Num);

		if (PropertyDefinition->CustomPropertyRegistration)
		{
			CustomAccessors = PropertyDefinition->CustomPropertyRegistration->GetAccessors();
		}

		if (InitialValueCache)
		{
			CacheStorage = InitialValueCache->GetStorage<PropertyTraits>(Definition->InitialValueType);
		}
	}

	virtual void Process(const FEntityAllocation* Allocation, const FComponentMask& AllocationType) override
	{
		if (Interrogation && AllocationType.Contains(BuiltInComponents->Interrogation.OutputKey))
		{
			VisitInterrogationAllocation(Allocation);
		}
		else if (CacheStorage)
		{
			VisitAllocationCached(Allocation);
		}
		else
		{
			VisitAllocation(Allocation);
		}
	}

	virtual void Finalize() override
	{
		ValuesByChannel.Empty();
		Interrogation = nullptr;
		CacheStorage = nullptr;
		CustomAccessors = FCustomAccessorView();
	}

	void VisitAllocation(const FEntityAllocation* Allocation)
	{
		const int32 Num = Allocation->Num();

		TComponentWriter<StorageType> InitialValues = Allocation->WriteComponents(PropertyDefinition->InitialValueType.ReinterpretCast<StorageType>(), WriteContext);
		TComponentReader<UObject*>    BoundObjects  = Allocation->ReadComponents(BuiltInComponents->BoundObject);

		TTuple< TComponentReader<MetaDataTypes>... > MetaData(
			Allocation->ReadComponents(PropertyDefinition->GetMetaDataComponent<MetaDataTypes>(MetaDataIndices))...
		);

		if (TOptionalComponentReader<FCustomPropertyIndex> CustomIndices = Allocation->TryReadComponents(BuiltInComponents->CustomPropertyIndex))
		{
			const FCustomPropertyIndex* RawIndices = CustomIndices.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., CustomAccessors[RawIndices[Index].Value], InitialValues[Index]);
			}
		}

		else if (TOptionalComponentReader<uint16> FastOffsets = Allocation->TryReadComponents(BuiltInComponents->FastPropertyOffset))
		{
			const uint16* RawOffsets = FastOffsets.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., RawOffsets[Index], InitialValues[Index]);
			}
		}

		else if (TOptionalComponentReader<TSharedPtr<FTrackInstancePropertyBindings>> SlowProperties = Allocation->TryReadComponents(BuiltInComponents->SlowProperty))
		{
			const TSharedPtr<FTrackInstancePropertyBindings>* RawProperties = SlowProperties.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., RawProperties[Index].Get(), InitialValues[Index]);
			}
		}
	}

	void VisitAllocationCached(const FEntityAllocation* Allocation)
	{
		const int32 Num = Allocation->Num();

		TComponentWriter<FInitialValueIndex> InitialValueIndices = Allocation->WriteComponents(BuiltInComponents->InitialValueIndex, WriteContext);
		TComponentWriter<StorageType>        InitialValues       = Allocation->WriteComponents(PropertyDefinition->InitialValueType.ReinterpretCast<StorageType>(), WriteContext);
		TComponentReader<UObject*>           BoundObjects        = Allocation->ReadComponents(BuiltInComponents->BoundObject);

		TTuple< TComponentReader<MetaDataTypes>... > MetaData(
			Allocation->ReadComponents(PropertyDefinition->GetMetaDataComponent<MetaDataTypes>(MetaDataIndices))...
		);

		if (TOptionalComponentReader<FCustomPropertyIndex> CustomIndices = Allocation->TryReadComponents(BuiltInComponents->CustomPropertyIndex))
		{
			const FCustomPropertyIndex* RawIndices = CustomIndices.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				TOptional<FInitialValueIndex> ExistingIndex = CacheStorage->FindPropertyIndex(BoundObjects[Index], RawIndices[Index]);
				if (ExistingIndex)
				{
					InitialValues[Index] = CacheStorage->GetCachedValue(ExistingIndex.GetValue());
				}
				else
				{
					StorageType Value{};
					PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., CustomAccessors[RawIndices[Index].Value], Value);

					InitialValues[Index] = Value;
					InitialValueIndices[Index] = CacheStorage->AddInitialValue(BoundObjects[Index], Value, RawIndices[Index]);
				}
			}
		}

		else if (TOptionalComponentReader<uint16> FastOffsets = Allocation->TryReadComponents(BuiltInComponents->FastPropertyOffset))
		{
			const uint16* RawOffsets = FastOffsets.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				TOptional<FInitialValueIndex> ExistingIndex = CacheStorage->FindPropertyIndex(BoundObjects[Index], FastOffsets[Index]);
				if (ExistingIndex)
				{
					InitialValues[Index] = CacheStorage->GetCachedValue(ExistingIndex.GetValue());
				}
				else
				{
					StorageType Value{};
					PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., RawOffsets[Index], Value);

					InitialValues[Index] = Value;
					InitialValueIndices[Index] = CacheStorage->AddInitialValue(BoundObjects[Index], Value, RawOffsets[Index]);
				}
			}
		}

		else if (TOptionalComponentReader<TSharedPtr<FTrackInstancePropertyBindings>> SlowProperties = Allocation->TryReadComponents(BuiltInComponents->SlowProperty))
		{
			const TSharedPtr<FTrackInstancePropertyBindings>* RawProperties = SlowProperties.AsPtr();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				TOptional<FInitialValueIndex> ExistingIndex = CacheStorage->FindPropertyIndex(BoundObjects[Index], *RawProperties[Index]->GetPropertyPath());
				if (ExistingIndex)
				{
					InitialValues[Index] = CacheStorage->GetCachedValue(ExistingIndex.GetValue());
				}
				else
				{
					StorageType Value{};
					PropertyTraits::GetObjectPropertyValue(BoundObjects[Index], MetaData.template Get<MetaDataIndices>()[Index]..., RawProperties[Index].Get(), Value);

					InitialValues[Index] = Value;
					InitialValueIndices[Index] = CacheStorage->AddInitialValue(BoundObjects[Index], Value, RawProperties[Index].Get());
				}
			}
		}
	}

	void VisitInterrogationAllocation(const FEntityAllocation* Allocation)
	{
		const int32 Num = Allocation->Num();

		TComponentWriter<StorageType>       InitialValues = Allocation->WriteComponents(PropertyDefinition->InitialValueType.ReinterpretCast<StorageType>(), WriteContext);
		TComponentReader<FInterrogationKey> OutputKeys    = Allocation->ReadComponents(BuiltInComponents->Interrogation.OutputKey);

		TTuple< TComponentReader<MetaDataTypes>... > MetaData(
			Allocation->ReadComponents(PropertyDefinition->GetMetaDataComponent<MetaDataTypes>(MetaDataIndices))...
		);

		const FSparseInterrogationChannelInfo& SparseChannelInfo = Interrogation->GetSparseChannelInfo();

		for (int32 Index = 0; Index < Num; ++Index)
		{
			FInterrogationChannel Channel = OutputKeys[Index].Channel;

			// Did we already cache this value?
			if (const StorageType* CachedValue = ValuesByChannel.Find(Channel))
			{
				InitialValues[Index] = *CachedValue;
				continue;
			}

			const FInterrogationChannelInfo* ChannelInfo = SparseChannelInfo.Find(Channel);
			UObject* Object = ChannelInfo ? ChannelInfo->WeakObject.Get() : nullptr;
			if (!ChannelInfo || !Object || ChannelInfo->PropertyBinding.PropertyName.IsNone())
			{
				continue;
			}

			TOptional< FResolvedFastProperty > Property = FPropertyRegistry::ResolveFastProperty(Object, ChannelInfo->PropertyBinding, CustomAccessors);

			// Retrieve a cached value if possible
			if (CacheStorage)
			{
				const StorageType* CachedValue = nullptr;
				if (!Property.IsSet())
				{
					CachedValue = CacheStorage->FindCachedValue(Object, ChannelInfo->PropertyBinding.PropertyPath);
				}
				else if (const FCustomPropertyIndex* CustomIndex = Property->TryGet<FCustomPropertyIndex>())
				{
					CachedValue = CacheStorage->FindCachedValue(Object, *CustomIndex);
				}
				else
				{
					CachedValue = CacheStorage->FindCachedValue(Object, Property->Get<uint16>());
				}
				if (CachedValue)
				{
					InitialValues[Index] = *CachedValue;
					ValuesByChannel.Add(Channel, *CachedValue);
					continue;
				}
			}

			// No cached value available, must retrieve it now
			TOptional<StorageType> CurrentValue;

			if (!Property.IsSet())
			{
				PropertyTraits::GetObjectPropertyValue(Object, MetaData.template Get<MetaDataIndices>()[Index]..., ChannelInfo->PropertyBinding.PropertyPath, CurrentValue.Emplace());
			}
			else if (const FCustomPropertyIndex* Custom = Property->TryGet<FCustomPropertyIndex>())
			{
				PropertyTraits::GetObjectPropertyValue(Object, MetaData.template Get<MetaDataIndices>()[Index]..., CustomAccessors[Custom->Value], CurrentValue.Emplace());
			}
			else
			{
				const uint16 FastPtrOffset = Property->Get<uint16>();
				PropertyTraits::GetObjectPropertyValue(Object, MetaData.template Get<MetaDataIndices>()[Index]..., FastPtrOffset, CurrentValue.Emplace());
			}

			InitialValues[Index] = CurrentValue.GetValue();
			ValuesByChannel.Add(Channel, CurrentValue.GetValue());
		};
	}
};

template<typename PropertyTraits>
struct TInitialValueProcessor : TInitialValueProcessorImpl<PropertyTraits, typename PropertyTraits::MetaDataType, TMakeIntegerSequence<int, PropertyTraits::MetaDataType::Num>>
{};

template<typename T, typename U = decltype(T::bIsComposite)>
constexpr bool IsCompositePropertyTraits(T*)
{
	return T::bIsComposite;
}
constexpr bool IsCompositePropertyTraits(...)
{
	return true;
}

template<typename PropertyTraits, typename ...MetaDataTypes, int ...MetaDataIndices, typename ...CompositeTypes, int ...CompositeIndices>
struct TPropertyComponentHandlerImpl<PropertyTraits, TPropertyMetaData<MetaDataTypes...>, TIntegerSequence<int, MetaDataIndices...>, TIntegerSequence<int, CompositeIndices...>, CompositeTypes...> : IPropertyComponentHandler
{
	static constexpr bool bIsComposite = IsCompositePropertyTraits((PropertyTraits*)nullptr);

	using StorageType        = typename PropertyTraits::StorageType;
	using CompleteSetterTask = std::conditional_t<bIsComposite, TSetCompositePropertyValues<PropertyTraits, CompositeTypes...>, TSetPropertyValues<PropertyTraits>>;

	using PreAnimatedStorageType = TPreAnimatedPropertyStorage<PropertyTraits>;

	TAutoRegisterPreAnimatedStorageID<PreAnimatedStorageType> StorageID;

	TPropertyComponentHandlerImpl()
	{
	}

	virtual TSharedPtr<IPreAnimatedStorage> GetPreAnimatedStateStorage(const FPropertyDefinition& Definition, FPreAnimatedStateExtension* Container) override
	{
		TSharedPtr<PreAnimatedStorageType> Existing = Container->FindStorage(StorageID);
		if (!Existing)
		{
			Existing = MakeShared<PreAnimatedStorageType>(Definition);
			Existing->Initialize(StorageID, Container);
			Container->AddStorage(StorageID, Existing);
		}

		return Existing;
	}

	virtual void ScheduleSetterTasks(const FPropertyDefinition& Definition, TArrayView<const FPropertyCompositeDefinition> Composites, const FPropertyStats& Stats, IEntitySystemScheduler* TaskScheduler, UMovieSceneEntitySystemLinker* Linker)
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.ReadOneOf(BuiltInComponents->CustomPropertyIndex, BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty)
		.ReadAllOf(Definition.GetMetaDataComponent<MetaDataTypes>(MetaDataIndices)...)
		.ReadAllOf(Composites[CompositeIndices].ComponentTypeID.ReinterpretCast<CompositeTypes>()...)
		.FilterAll({ Definition.PropertyType })
		.SetStat(Definition.StatID)
		.SetDesiredThread(Linker->EntityManager.GetGatherThread())
		.template Fork_PerAllocation<CompleteSetterTask>(&Linker->EntityManager, TaskScheduler, Definition.CustomPropertyRegistration);

		if constexpr (bIsComposite)
		{
			if (Stats.NumPartialProperties > 0)
			{
				using PartialSetterTask  = TSetPartialPropertyValues<PropertyTraits, CompositeTypes...>;

				FComponentMask CompletePropertyMask;
				for (const FPropertyCompositeDefinition& Composite : Composites)
				{
					CompletePropertyMask.Set(Composite.ComponentTypeID);
				}

				FEntityTaskBuilder()
				.Read(BuiltInComponents->BoundObject)
				.ReadOneOf(BuiltInComponents->CustomPropertyIndex, BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty)
				.ReadAllOf(Definition.GetMetaDataComponent<MetaDataTypes>(MetaDataIndices)...)
				.ReadAnyOf(Composites[CompositeIndices].ComponentTypeID.ReinterpretCast<CompositeTypes>()...)
				.FilterAny({ CompletePropertyMask })
				.FilterAll({ Definition.PropertyType })
				.FilterOut(CompletePropertyMask)
				.SetStat(Definition.StatID)
				.SetDesiredThread(Linker->EntityManager.GetGatherThread())
				.template Fork_PerAllocation<PartialSetterTask>(&Linker->EntityManager, TaskScheduler, Definition.CustomPropertyRegistration, Composites);
			}
		}
	}

	virtual void DispatchSetterTasks(const FPropertyDefinition& Definition, TArrayView<const FPropertyCompositeDefinition> Composites, const FPropertyStats& Stats, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker)
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.ReadOneOf(BuiltInComponents->CustomPropertyIndex, BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty)
		.ReadAllOf(Definition.GetMetaDataComponent<MetaDataTypes>(MetaDataIndices)...)
		.ReadAllOf(Composites[CompositeIndices].ComponentTypeID.ReinterpretCast<CompositeTypes>()...)
		.FilterAll({ Definition.PropertyType })
		.SetStat(Definition.StatID)
		.SetDesiredThread(Linker->EntityManager.GetGatherThread())
		.template Dispatch_PerAllocation<CompleteSetterTask>(&Linker->EntityManager, InPrerequisites, &Subsequents, Definition.CustomPropertyRegistration);

		if constexpr (bIsComposite)
		{
			if (Stats.NumPartialProperties > 0)
			{
				using PartialSetterTask  = TSetPartialPropertyValues<PropertyTraits, CompositeTypes...>;

				FComponentMask CompletePropertyMask;
				for (const FPropertyCompositeDefinition& Composite : Composites)
				{
					CompletePropertyMask.Set(Composite.ComponentTypeID);
				}

				FEntityTaskBuilder()
				.Read(BuiltInComponents->BoundObject)
				.ReadOneOf(BuiltInComponents->CustomPropertyIndex, BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty)
				.ReadAllOf(Definition.GetMetaDataComponent<MetaDataTypes>(MetaDataIndices)...)
				.ReadAnyOf(Composites[CompositeIndices].ComponentTypeID.ReinterpretCast<CompositeTypes>()...)
				.FilterAny({ CompletePropertyMask })
				.FilterAll({ Definition.PropertyType })
				.FilterOut(CompletePropertyMask)
				.SetStat(Definition.StatID)
				.SetDesiredThread(Linker->EntityManager.GetGatherThread())
				.template Dispatch_PerAllocation<PartialSetterTask>(&Linker->EntityManager, InPrerequisites, &Subsequents, Definition.CustomPropertyRegistration, Composites);
			}
		}
	}

	virtual IInitialValueProcessor* GetInitialValueProcessor() override
	{
		static TInitialValueProcessor<PropertyTraits> Processor;
		return &Processor;
	}

	virtual void RecomposeBlendOperational(const FPropertyDefinition& PropertyDefinition, TArrayView<const FPropertyCompositeDefinition> Composites, const FValueDecompositionParams& InParams, UMovieSceneBlenderSystem* Blender, FConstPropertyComponentView InCurrentValue, FPropertyComponentArrayView OutResult) override
	{
		RecomposeBlendImpl(PropertyDefinition, Composites, InParams, Blender, InCurrentValue.ReinterpretCast<StorageType>(), OutResult.ReinterpretCast<StorageType>());
	}

	void RecomposeBlendImpl(const FPropertyDefinition& PropertyDefinition, TArrayView<const FPropertyCompositeDefinition> Composites, const FValueDecompositionParams& InParams, UMovieSceneBlenderSystem* Blender, const StorageType& InCurrentValue, TArrayView<StorageType> OutResults)
	{
		check(OutResults.Num() == InParams.Query.Entities.Num());

		IMovieSceneValueDecomposer* ValueDecomposer = Cast<IMovieSceneValueDecomposer>(Blender);
		if (!ValueDecomposer)
		{
			return;
		}

		FEntityManager& EntityManager = Blender->GetLinker()->EntityManager;
		EntityManager.LockDown();

		constexpr int32 NumComposites = sizeof...(CompositeTypes);
		check(Composites.Num() == NumComposites);

		FAlignedDecomposedValue AlignedOutputs[NumComposites];

		FValueDecompositionParams LocalParams = InParams;

		FGraphEventArray Tasks;
		for (int32 Index = 0; Index < NumComposites; ++Index)
		{
			if ((PropertyDefinition.DoubleCompositeMask & (1 << Index)) == 0)
			{
				continue;
			}

			LocalParams.ResultComponentType = Composites[Index].ComponentTypeID;
			FGraphEventRef Task = ValueDecomposer->DispatchDecomposeTask(LocalParams, &AlignedOutputs[Index]);
			if (Task)
			{
				Tasks.Add(Task);
			}
		}

		if (Tasks.Num() != 0)
		{
			FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GameThread);
		}

		// Get the initial value in case we have a value without a full-weighted absolute channel.
		TOptionalComponentReader<StorageType> InitialValueComponent;
		if (InParams.PropertyEntityID)
		{
			TComponentTypeID<StorageType> InitialValueType = PropertyDefinition.InitialValueType.ReinterpretCast<StorageType>();
			InitialValueComponent = EntityManager.ReadComponent(InParams.PropertyEntityID, InitialValueType);
		}

		for (int32 Index = 0; Index < LocalParams.Query.Entities.Num(); ++Index)
		{
			FMovieSceneEntityID EntityID = LocalParams.Query.Entities[Index];

			uint8* Result = reinterpret_cast<uint8*>(&OutResults[Index]);

			for (int32 CompositeIndex = 0; CompositeIndex < NumComposites; ++CompositeIndex)
			{
				if ((PropertyDefinition.DoubleCompositeMask & ( 1 << CompositeIndex)) != 0)
				{
					const double* InitialValueComposite = nullptr;
					FAlignedDecomposedValue& AlignedOutput = AlignedOutputs[CompositeIndex];
					if (InitialValueComponent)
					{
						const StorageType* InitialValuePtr = InitialValueComponent.AsPtr();
						InitialValueComposite = reinterpret_cast<const double*>(reinterpret_cast<const uint8*>(InitialValuePtr) + Composites[CompositeIndex].CompositeOffset);
					}

					const double NewComposite = *reinterpret_cast<const double*>(reinterpret_cast<const uint8*>(&InCurrentValue) + Composites[CompositeIndex].CompositeOffset);

					double* RecomposedComposite = reinterpret_cast<double*>(Result + Composites[CompositeIndex].CompositeOffset);
					*RecomposedComposite = AlignedOutput.Value.Recompose(EntityID, NewComposite, InitialValueComposite);
				}
			}
		}

		EntityManager.ReleaseLockDown();
	}

	virtual void RecomposeBlendChannel(const FPropertyDefinition& PropertyDefinition, TArrayView<const FPropertyCompositeDefinition> Composites, int32 CompositeIndex, const FValueDecompositionParams& InParams, UMovieSceneBlenderSystem* Blender, double InCurrentValue, TArrayView<double> OutResults) override
	{
		check(OutResults.Num() == InParams.Query.Entities.Num());

		constexpr int32 NumComposites = sizeof...(CompositeTypes);
		check(Composites.Num() == NumComposites);
		const FPropertyCompositeDefinition& Composite = Composites[CompositeIndex];

		IMovieSceneValueDecomposer* ValueDecomposer = Cast<IMovieSceneValueDecomposer>(Blender);
		if (!ValueDecomposer)
		{
			return;
		}

		FEntityManager& EntityManager = Blender->GetLinker()->EntityManager;
		EntityManager.LockDown();

		FAlignedDecomposedValue AlignedOutput;

		FValueDecompositionParams LocalParams = InParams;

		LocalParams.ResultComponentType = Composite.ComponentTypeID;
		FGraphEventRef Task = ValueDecomposer->DispatchDecomposeTask(LocalParams, &AlignedOutput);
		if (Task)
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task, ENamedThreads::GameThread);
		}

		// Get the initial value in case we have a value without a full-weighted absolute channel.
		TOptionalComponentReader<StorageType> InitialValueComponent;
		if (InParams.PropertyEntityID)
		{
			TComponentTypeID<StorageType> InitialValueType = PropertyDefinition.InitialValueType.ReinterpretCast<StorageType>();
			InitialValueComponent = EntityManager.ReadComponent(InParams.PropertyEntityID, InitialValueType);
		}

		for (int32 Index = 0; Index < LocalParams.Query.Entities.Num(); ++Index)
		{
			FMovieSceneEntityID EntityID = LocalParams.Query.Entities[Index];

			if ((PropertyDefinition.DoubleCompositeMask & (1 << CompositeIndex)) != 0)
			{
				const double* InitialValueComposite = nullptr;
				if (InitialValueComponent)
				{
					const StorageType* InitialValuePtr = InitialValueComponent.AsPtr();
					InitialValueComposite = reinterpret_cast<const double*>(reinterpret_cast<const uint8*>(InitialValuePtr) + Composite.CompositeOffset);
				}

				const double RecomposedComposite = AlignedOutput.Value.Recompose(EntityID, InCurrentValue, InitialValueComposite);
				OutResults[Index] = RecomposedComposite;
			}
		}

		EntityManager.ReleaseLockDown();
	}

	virtual void RebuildOperational(const FPropertyDefinition& PropertyDefinition, TArrayView<const FPropertyCompositeDefinition> Composites, const TArrayView<FMovieSceneEntityID>& EntityIDs, UMovieSceneEntitySystemLinker* Linker, FPropertyComponentArrayView OutResult) override
	{
		TArrayView<StorageType> TypedResults = OutResult.ReinterpretCast<StorageType>();

		constexpr int32 NumComposites = sizeof...(CompositeTypes);
		check(Composites.Num() == NumComposites);

		check(TypedResults.Num() == EntityIDs.Num());

		FEntityManager& EntityManager = Linker->EntityManager;

		for (int32 Index = 0; Index < EntityIDs.Num(); ++Index)
		{
			FMovieSceneEntityID EntityID = EntityIDs[Index];
			if (!EntityID)
			{
				continue;
			}

			FEntityDataLocation Location = EntityManager.GetEntity(EntityIDs[Index]).Data;

			PatchCompositeValue(Composites, &TypedResults[Index],
				Location.Allocation->TryReadComponents(Composites[CompositeIndices].ComponentTypeID.ReinterpretCast<CompositeTypes>()).ComponentAtIndex(Location.ComponentOffset)...
			);
		}
	}
};




template<typename PropertyTraits>
struct TPropertyDefinitionBuilder
{
	TPropertyDefinitionBuilder<PropertyTraits>& AddSoleChannel(TComponentTypeID<typename PropertyTraits::StorageType> InComponent)
	{
		checkf(Definition == &Registry->GetProperties().Last(), TEXT("Cannot re-define a property type after another has been added."));
		checkf(Definition->CompositeSize == 0, TEXT("Property already has a composite."));

		FPropertyCompositeDefinition NewChannel = { InComponent, 0 };
		Registry->CompositeDefinitions.Add(NewChannel);

		Definition->CompositeSize = 1;

		static_assert(!std::is_same_v<typename PropertyTraits::StorageType, float>, "Please use double-precision composites");

		if constexpr (std::is_same_v<typename PropertyTraits::StorageType, double>)
		{
			Definition->DoubleCompositeMask = 1;
		}

		return *this;
	}

	template<int InlineSize>
	TPropertyDefinitionBuilder<PropertyTraits>& SetCustomAccessors(TCustomPropertyRegistration<PropertyTraits, InlineSize>* InCustomAccessors)
	{
		Definition->CustomPropertyRegistration = InCustomAccessors;
		return *this;
	}

	TPropertyDefinitionBuilder<PropertyTraits>& SetStat(TStatId InStatID)
	{
		Definition->StatID = InStatID;
		return *this;
	}

	template<typename BlenderSystemType>
	TPropertyDefinitionBuilder<PropertyTraits>& SetBlenderSystem()
	{
		Definition->BlenderSystemClass = BlenderSystemType::StaticClass();
		return *this;
	}

	TPropertyDefinitionBuilder<PropertyTraits>& SetBlenderSystem(UClass* BlenderSystemClass)
	{
		Definition->BlenderSystemClass = BlenderSystemClass;
		return *this;
	}

	void Commit()
	{
		Definition->Handler = TPropertyComponentHandler<PropertyTraits, typename PropertyTraits::StorageType>();
	}

	template<typename HandlerType>
	void Commit(HandlerType&& InHandler)
	{
		Definition->Handler = Forward<HandlerType>(InHandler);
	}

protected:

	friend FPropertyRegistry;

	TPropertyDefinitionBuilder(FPropertyDefinition* InDefinition, FPropertyRegistry* InRegistry)
		: Definition(InDefinition), Registry(InRegistry)
	{}

	FPropertyDefinition* Definition;
	FPropertyRegistry* Registry;
};


template<typename PropertyTraits, typename... Composites>
struct TCompositePropertyDefinitionBuilder
{
	using StorageType = typename PropertyTraits::StorageType;

	static_assert(sizeof...(Composites) <= 32, "More than 32 composites is not supported");

	TCompositePropertyDefinitionBuilder(FPropertyDefinition* InDefinition, FPropertyRegistry* InRegistry)
		: Definition(InDefinition), Registry(InRegistry)
	{}

	template<typename T>
	TCompositePropertyDefinitionBuilder<PropertyTraits, Composites..., T> AddComposite(TComponentTypeID<T> InComponent, T StorageType::*DataPtr)
	{
		checkf(Definition == &Registry->GetProperties().Last(), TEXT("Cannot re-define a property type after another has been added."));

		const PTRINT CompositeOffset = (PTRINT)&(((StorageType*)0)->*DataPtr);

		FPropertyCompositeDefinition NewChannel = { InComponent, static_cast<uint16>(CompositeOffset) };
		Registry->CompositeDefinitions.Add(NewChannel);

		static_assert(!std::is_same_v<T, float>, "Please use double-precision composites");

		if constexpr (std::is_same_v<T, double>)
		{
			Definition->DoubleCompositeMask |= 1 << Definition->CompositeSize;
		}

		++Definition->CompositeSize;
		return TCompositePropertyDefinitionBuilder<PropertyTraits, Composites..., T>(Definition, Registry);
	}

	TCompositePropertyDefinitionBuilder<PropertyTraits, Composites..., double> AddComposite(TComponentTypeID<double> InComponent, double StorageType::*DataPtr)
	{
		checkf(Definition == &Registry->GetProperties().Last(), TEXT("Cannot re-define a property type after another has been added."));

		const PTRINT CompositeOffset = (PTRINT)&(((StorageType*)0)->*DataPtr);

		FPropertyCompositeDefinition NewChannel = { InComponent, static_cast<uint16>(CompositeOffset) };
		Registry->CompositeDefinitions.Add(NewChannel);

		Definition->DoubleCompositeMask |= 1 << Definition->CompositeSize;

		++Definition->CompositeSize;
		return TCompositePropertyDefinitionBuilder<PropertyTraits, Composites..., double>(Definition, Registry);
	}

	template<int InlineSize>
	TCompositePropertyDefinitionBuilder<PropertyTraits, Composites...>& SetCustomAccessors(TCustomPropertyRegistration<PropertyTraits, InlineSize>* InCustomAccessors)
	{
		Definition->CustomPropertyRegistration = InCustomAccessors;
		return *this;
	}

	template<typename BlenderSystemType>
	TCompositePropertyDefinitionBuilder<PropertyTraits, Composites...>& SetBlenderSystem()
	{
		Definition->BlenderSystemClass = BlenderSystemType::StaticClass();
		return *this;
	}

	TCompositePropertyDefinitionBuilder<PropertyTraits, Composites...>& SetBlenderSystem(UClass* BlenderSystemClass)
	{
		Definition->BlenderSystemClass = BlenderSystemClass;
		return *this;
	}

	void Commit()
	{
		Definition->Handler = TPropertyComponentHandler<PropertyTraits, Composites...>();
	}
	
	template<typename HandlerType>
	void Commit(HandlerType&& InHandler)
	{
		Definition->Handler = Forward<HandlerType>(InHandler);
	}

private:

	FPropertyDefinition* Definition;
	FPropertyRegistry* Registry;
};


struct FPropertyRecomposerPropertyInfo
{
	static constexpr uint16 INVALID_BLEND_CHANNEL = uint16(-1);

	uint16 BlendChannel = INVALID_BLEND_CHANNEL;
	UMovieSceneBlenderSystem* BlenderSystem = nullptr;
	FMovieSceneEntityID PropertyEntityID;

	static FPropertyRecomposerPropertyInfo Invalid()
	{ 
		return FPropertyRecomposerPropertyInfo { INVALID_BLEND_CHANNEL, nullptr, FMovieSceneEntityID::Invalid() };
	}
};

DECLARE_DELEGATE_RetVal_TwoParams(FPropertyRecomposerPropertyInfo, FOnGetPropertyRecomposerPropertyInfo, FMovieSceneEntityID, UObject*);

struct FPropertyRecomposerImpl
{
	template<typename PropertyTraits>
	TRecompositionResult<typename PropertyTraits::StorageType> RecomposeBlendOperational(const TPropertyComponents<PropertyTraits>& InComponents, const FDecompositionQuery& InQuery, const typename PropertyTraits::StorageType& InCurrentValue);

	FOnGetPropertyRecomposerPropertyInfo OnGetPropertyInfo;
};

template<typename PropertyTraits>
TRecompositionResult<typename PropertyTraits::StorageType> FPropertyRecomposerImpl::RecomposeBlendOperational(const TPropertyComponents<PropertyTraits>& Components, const FDecompositionQuery& InQuery, const typename PropertyTraits::StorageType& InCurrentValue)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FPropertyDefinition& PropertyDefinition = BuiltInComponents->PropertyRegistry.GetDefinition(Components.CompositeID);

	TRecompositionResult<typename PropertyTraits::StorageType> Result(InCurrentValue, InQuery.Entities.Num());

	if (InQuery.Entities.Num() == 0)
	{
		return Result;
	}

	const FPropertyRecomposerPropertyInfo Property = OnGetPropertyInfo.Execute(InQuery.Entities[0], InQuery.Object);

	if (Property.BlendChannel == FPropertyRecomposerPropertyInfo::INVALID_BLEND_CHANNEL)
	{
		return Result;
	}

	UMovieSceneBlenderSystem* Blender = Property.BlenderSystem;
	if (!Blender)
	{
		return Result;
	}

	FValueDecompositionParams Params;
	Params.Query = InQuery;
	Params.PropertyEntityID = Property.PropertyEntityID;
	Params.DecomposeBlendChannel = Property.BlendChannel;
	Params.PropertyTag = PropertyDefinition.PropertyType;

	TArrayView<const FPropertyCompositeDefinition> Composites = BuiltInComponents->PropertyRegistry.GetComposites(PropertyDefinition);

	PropertyDefinition.Handler->RecomposeBlendOperational(PropertyDefinition, Composites, Params, Blender, InCurrentValue, Result.Values);

	return Result;
}


} // namespace MovieScene
} // namespace UE


