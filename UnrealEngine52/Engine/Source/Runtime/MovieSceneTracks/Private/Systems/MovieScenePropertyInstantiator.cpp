// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieScenePropertyInstantiator.h"
#include "Algo/AllOf.h"
#include "EntitySystem/MovieSceneEntityBuilder.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneBlenderSystem.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"

#include "Algo/AllOf.h"
#include "Algo/IndexOf.h"
#include "ProfilingDebugging/CountersTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieScenePropertyInstantiator)

DECLARE_CYCLE_STAT(TEXT("DiscoverInvalidatedProperties"), MovieSceneEval_DiscoverInvalidatedProperties, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("ProcessInvalidatedProperties"), MovieSceneEval_ProcessInvalidatedProperties, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("InitializePropertyMetaData"), MovieSceneEval_InitializePropertyMetaData, STATGROUP_MovieSceneECS);

UMovieScenePropertyInstantiatorSystem::UMovieScenePropertyInstantiatorSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	BuiltInComponents = FBuiltInComponentTypes::Get();

	RecomposerImpl.OnGetPropertyInfo = FOnGetPropertyRecomposerPropertyInfo::CreateUObject(
				this, &UMovieScenePropertyInstantiatorSystem::FindPropertyFromSource);

	SystemCategories = FSystemInterrogator::GetExcludedFromInterrogationCategory();
	RelevantComponent = BuiltInComponents->PropertyBinding;
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObject);
		DefineComponentProducer(GetClass(), BuiltInComponents->BlendChannelInput);
		DefineComponentProducer(GetClass(), BuiltInComponents->SymbolicTags.CreatesEntities);
	}
}

UE::MovieScene::FPropertyStats UMovieScenePropertyInstantiatorSystem::GetStatsForProperty(UE::MovieScene::FCompositePropertyTypeID PropertyID) const
{
	const int32 Index = PropertyID.AsIndex();
	if (PropertyStats.IsValidIndex(Index))
	{
		return PropertyStats[Index];
	}

	return UE::MovieScene::FPropertyStats();
}

void UMovieScenePropertyInstantiatorSystem::OnLink()
{
	Linker->Events.CleanTaggedGarbage.AddUObject(this, &UMovieScenePropertyInstantiatorSystem::CleanTaggedGarbage);

	CleanFastPathMask.Reset();
	CleanFastPathMask.SetAll({ BuiltInComponents->FastPropertyOffset, BuiltInComponents->SlowProperty, BuiltInComponents->CustomPropertyIndex });
	CleanFastPathMask.CombineWithBitwiseOR(Linker->EntityManager.GetComponents()->GetMigrationMask(), EBitwiseOperatorFlags::MaxSize);
}

void UMovieScenePropertyInstantiatorSystem::OnUnlink()
{
	using namespace UE::MovieScene;

	Linker->Events.CleanTaggedGarbage.RemoveAll(this);

	const bool bAllPropertiesClean = (
				ResolvedProperties.Num() == 0 &&
				Contributors.Num() == 0 &&
				NewContributors.Num() == 0 &&
				EntityToProperty.Num() == 0 &&
				ObjectPropertyToResolvedIndex.Num() == 0);
	if (!ensure(bAllPropertiesClean))
	{
		ResolvedProperties.Reset();
		Contributors.Reset();
		NewContributors.Reset();
		EntityToProperty.Reset();
		ObjectPropertyToResolvedIndex.Reset();
		PropertyStats.Reset();
	}

	const bool bAllPropertiesGone = Algo::AllOf(
			PropertyStats, [](const FPropertyStats& Item) 
			{
				return Item.NumProperties == 0 && Item.NumPartialProperties == 0;
			});
	if (!ensure(bAllPropertiesGone))
	{
		PropertyStats.Reset();
	}

	const bool bAllTasksDone = (
			InitializePropertyMetaDataTasks.Num() == 0 &&
			SaveGlobalStateTasks.Num() == 0);
	if (!ensure(bAllTasksDone))
	{
		InitializePropertyMetaDataTasks.Reset();
		SaveGlobalStateTasks.Reset();
	}
}

void UMovieScenePropertyInstantiatorSystem::CleanTaggedGarbage(UMovieSceneEntitySystemLinker*)
{
	using namespace UE::MovieScene;

	TBitArray<> InvalidatedProperties;
	DiscoverInvalidatedProperties(InvalidatedProperties);

	if (InvalidatedProperties.Num() != 0)
	{
		ProcessInvalidatedProperties(InvalidatedProperties);
	}
}

void UMovieScenePropertyInstantiatorSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	TBitArray<> InvalidatedProperties;
	DiscoverInvalidatedProperties(InvalidatedProperties);

	if (InvalidatedProperties.Num() != 0)
	{
		UpgradeFloatToDoubleProperties(InvalidatedProperties);
		ProcessInvalidatedProperties(InvalidatedProperties);
	}

	if (InitializePropertyMetaDataTasks.Find(true) != INDEX_NONE)
	{
		InitializePropertyMetaData(InPrerequisites, Subsequents);
	}

	ObjectPropertyToResolvedIndex.Compact();
	EntityToProperty.Compact();
}

void UMovieScenePropertyInstantiatorSystem::DiscoverInvalidatedProperties(TBitArray<>& OutInvalidatedProperties)
{
	using namespace UE::MovieScene;

	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_DiscoverInvalidatedProperties);

	TBitArray<> InvalidatedProperties;

	TArrayView<const FPropertyDefinition> Properties = this->BuiltInComponents->PropertyRegistry.GetProperties();

	PropertyStats.SetNum(Properties.Num());

	auto VisitNewProperties = [this, Properties, &OutInvalidatedProperties](const FEntityAllocation* Allocation, const FMovieSceneEntityID* EntityIDs, UObject* const * ObjectPtrs, const FMovieScenePropertyBinding* PropertyPtrs)
	{
		const int32 PropertyDefinitionIndex = Algo::IndexOfByPredicate(Properties, [=](const FPropertyDefinition& InDefinition){ return Allocation->HasComponent(InDefinition.PropertyType); });
		if (PropertyDefinitionIndex == INDEX_NONE)
		{
			return;
		}

		const FPropertyDefinition& PropertyDefinition = Properties[PropertyDefinitionIndex];

		FCustomAccessorView CustomAccessors = PropertyDefinition.CustomPropertyRegistration ? PropertyDefinition.CustomPropertyRegistration->GetAccessors() : FCustomAccessorView();

		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const int32 PropertyIndex = this->ResolveProperty(CustomAccessors, ObjectPtrs[Index], PropertyPtrs[Index], PropertyDefinitionIndex);
			
			// If the property did not resolve, we still add it to the LUT
			// So that the ensure inside VisitExpiredEntities only fires
			// for genuine link/unlink disparities
			this->EntityToProperty.Add(EntityIDs[Index], PropertyIndex);

			if (PropertyIndex != INDEX_NONE)
			{
				this->Contributors.Add(PropertyIndex, EntityIDs[Index]);
				this->NewContributors.Add(PropertyIndex, EntityIDs[Index]);

				OutInvalidatedProperties.PadToNum(PropertyIndex + 1, false);
				OutInvalidatedProperties[PropertyIndex] = true;
			}
		}
	};

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->BoundObject)
	.Read(BuiltInComponents->PropertyBinding)
	.FilterNone({ BuiltInComponents->BlendChannelOutput })
	.FilterAll({ BuiltInComponents->Tags.NeedsLink })
	.Iterate_PerAllocation(&Linker->EntityManager, VisitNewProperties);


	auto VisitExpiredEntities = [this, &OutInvalidatedProperties](FMovieSceneEntityID EntityID)
	{
		const int32* PropertyIndexPtr = this->EntityToProperty.Find(EntityID);
		if (ensureMsgf(PropertyIndexPtr, TEXT("Could not find entity to clean up from linker entity ID - this indicates VisitNewProperties never got called for this entity, or a garbage collection has somehow destroyed the entity without flushing the ecs.")))
		{
			const int32 PropertyIndex = *PropertyIndexPtr;
			if (PropertyIndex != INDEX_NONE)
			{
				OutInvalidatedProperties.PadToNum(PropertyIndex + 1, false);
				OutInvalidatedProperties[PropertyIndex] = true;

				this->Contributors.Remove(PropertyIndex, EntityID);
				
				// The output entity of a property could be going away if that property is using the
				// fast path, which means its sole contributor is that output entity. It can go away when
				// that track or section gets reimported.
				FObjectPropertyInfo& PropertyInfo = this->ResolvedProperties[PropertyIndex];
				if (PropertyInfo.PropertyEntityID == EntityID)
				{
					PropertyInfo.PropertyEntityID = FMovieSceneEntityID::Invalid();
				}
			}

			// Always remove the entity ID from the LUT
			this->EntityToProperty.Remove(EntityID);
		}
	};

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.FilterNone({ BuiltInComponents->BlendChannelOutput })
	.FilterAll({ BuiltInComponents->BoundObject, BuiltInComponents->PropertyBinding, BuiltInComponents->Tags.NeedsUnlink })
	.Iterate_PerEntity(&Linker->EntityManager, VisitExpiredEntities);
}

void UMovieScenePropertyInstantiatorSystem::UpgradeFloatToDoubleProperties(const TBitArray<>& InvalidatedProperties)
{
	using namespace UE::MovieScene;

	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ProcessInvalidatedProperties);

	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();
	TArrayView<const FPropertyDefinition> Properties = BuiltInComponents->PropertyRegistry.GetProperties();

	for (TConstSetBitIterator<> It(InvalidatedProperties); It; ++It)
	{
		const int32 PropertyIndex = It.GetIndex();
		if (!ResolvedProperties.IsValidIndex(PropertyIndex))
		{
			continue;
		}

		FObjectPropertyInfo& PropertyInfo = ResolvedProperties[PropertyIndex];
		const bool bHadConversionInfo = PropertyInfo.ConvertedFromPropertyDefinitionIndex.IsSet();

		// The first time we encounter a specific property, we need to figure out if it needs type conversion or not.
		if (!bHadConversionInfo)
		{
			// Don't do anything if the property isn't of the kind of type we need to care about. Right now, we only
			// support dealing with float->double and FVectorXf->FVectorXd.
			const FPropertyDefinition& PropertyDefinition = Properties[PropertyInfo.PropertyDefinitionIndex];
			if (PropertyDefinition.PropertyType != TrackComponents->Float.PropertyTag &&
				PropertyDefinition.PropertyType != TrackComponents->FloatVector.PropertyTag)
			{
				PropertyInfo.ConvertedFromPropertyDefinitionIndex = INDEX_NONE;
				continue;
			}

			FProperty* BoundProperty = FTrackInstancePropertyBindings::FindProperty(PropertyInfo.BoundObject, PropertyInfo.PropertyBinding.PropertyPath.ToString());
			if (!BoundProperty)
			{
				continue;
			}

			// Patch the resolved property info to point to the double-precision property definition.
			PropertyInfo.ConvertedFromPropertyDefinitionIndex = INDEX_NONE;
			if (PropertyDefinition.PropertyType == TrackComponents->Float.PropertyTag)
			{
				const bool bIsDouble = BoundProperty->IsA<FDoubleProperty>();
				if (bIsDouble)
				{
					const int32 DoublePropertyDefinitionIndex = Properties.IndexOfByPredicate(
						[TrackComponents](const FPropertyDefinition& Item) { return Item.PropertyType == TrackComponents->Double.PropertyTag; });
					ensure(DoublePropertyDefinitionIndex != INDEX_NONE);
					PropertyInfo.ConvertedFromPropertyDefinitionIndex = PropertyInfo.PropertyDefinitionIndex;
					PropertyInfo.PropertyDefinitionIndex = DoublePropertyDefinitionIndex;
				}
			}
			else if (PropertyDefinition.PropertyType == TrackComponents->FloatVector.PropertyTag)
			{
				const UScriptStruct* BoundStructProperty = CastField<FStructProperty>(BoundProperty)->Struct;
				const bool bIsDouble = (
					(BoundStructProperty == TBaseStructure<FVector2D>::Get()
						) ||
					(
						BoundStructProperty == TBaseStructure<FVector>::Get() ||
						BoundStructProperty == TVariantStructure<FVector3d>::Get()
						) ||
					(
						BoundStructProperty == TBaseStructure<FVector4>::Get() ||
						BoundStructProperty == TVariantStructure<FVector4d>::Get()
						));
				if (bIsDouble)
				{
					const int32 DoublePropertyDefinitionIndex = Properties.IndexOfByPredicate(
						[TrackComponents](const FPropertyDefinition& Item) { return Item.PropertyType == TrackComponents->DoubleVector.PropertyTag; });
					ensure(DoublePropertyDefinitionIndex != INDEX_NONE);
					PropertyInfo.ConvertedFromPropertyDefinitionIndex = PropertyInfo.PropertyDefinitionIndex;
					PropertyInfo.PropertyDefinitionIndex = DoublePropertyDefinitionIndex;
				}
			}
			else
			{
				check(false);
			}
		}

		const int32 OldPropertyDefinitionIndex = PropertyInfo.ConvertedFromPropertyDefinitionIndex.Get(INDEX_NONE);
		if (OldPropertyDefinitionIndex == INDEX_NONE)
		{
			continue;
		}

		// Now we need to patch the contributors so that they have double-precision components.
		// We only need to do it for the *new* contributors discovered this frame.
		FComponentTypeID OldPropertyTag, NewPropertyTag;
		const FPropertyDefinition& PropertyDefinition = Properties[PropertyInfo.PropertyDefinitionIndex];
		if (PropertyDefinition.PropertyType == TrackComponents->Double.PropertyTag)
		{
			OldPropertyTag = TrackComponents->Float.PropertyTag;
			NewPropertyTag = TrackComponents->Double.PropertyTag;
		}
		else if (PropertyDefinition.PropertyType == TrackComponents->DoubleVector.PropertyTag)
		{
			OldPropertyTag = TrackComponents->FloatVector.PropertyTag;
			NewPropertyTag = TrackComponents->DoubleVector.PropertyTag;
		}

		if (ensure(OldPropertyTag && NewPropertyTag))
		{
			// Swap out the property tag
			for (auto ContribIt = NewContributors.CreateKeyIterator(PropertyIndex); ContribIt; ++ContribIt)
			{
				const FMovieSceneEntityID CurID(ContribIt.Value());
				FComponentMask EntityType = Linker->EntityManager.GetEntityType(CurID);
				EntityType.Remove(OldPropertyTag);
				EntityType.Set(NewPropertyTag);
				Linker->EntityManager.ChangeEntityType(CurID, EntityType);
			}

			// Update contributor info and stats. This is only done the first time this property is encountered.
			if (!bHadConversionInfo)
			{
				--PropertyStats[OldPropertyDefinitionIndex].NumProperties;
				++PropertyStats[PropertyInfo.PropertyDefinitionIndex].NumProperties;
			}
		}
	}
}

void UMovieScenePropertyInstantiatorSystem::ProcessInvalidatedProperties(const TBitArray<>& InvalidatedProperties)
{
	using namespace UE::MovieScene;

	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ProcessInvalidatedProperties);

	TBitArray<> StaleProperties;

	TArrayView<const FPropertyDefinition> Properties = BuiltInComponents->PropertyRegistry.GetProperties();

	FPropertyParameters Params;

	// This is all random access at this point :(
	for (TConstSetBitIterator<> It(InvalidatedProperties); It; ++It)
	{
		const int32 PropertyIndex = It.GetIndex();
		if (!ResolvedProperties.IsValidIndex(PropertyIndex))
		{
			continue;
		}

		// Update our view of how this property is animated
		Params.PropertyInfo = &ResolvedProperties[PropertyIndex];
		Params.PropertyDefinition = &Properties[Params.PropertyInfo->PropertyDefinitionIndex];
		Params.PropertyInfoIndex = PropertyIndex;

		UpdatePropertyInfo(Params);

		// Does it have anything at all contributing to it anymore?
		if (!Contributors.Contains(PropertyIndex))
		{
			StaleProperties.PadToNum(PropertyIndex + 1, false);
			StaleProperties[PropertyIndex] = true;
		}
		// Does it support fast path?
		else if (PropertySupportsFastPath(Params))
		{
			InitializeFastPath(Params);
		}
		// Else use the (slightly more) expensive blend path
		else
		{
			InitializeBlendPath(Params);
		}
	}

	// Restore and destroy stale properties
	if (StaleProperties.Find(true) != INDEX_NONE)
	{
		for (TConstSetBitIterator<> It(StaleProperties); It; ++It)
		{
			const int32 PropertyIndex = It.GetIndex();
			FObjectPropertyInfo* PropertyInfo = &ResolvedProperties[PropertyIndex];

			if (PropertyInfo->BlendChannel != INVALID_BLEND_CHANNEL)
			{
				if (UMovieSceneBlenderSystem* Blender = PropertyInfo->Blender.Get())
				{
					const FMovieSceneBlendChannelID BlendChannelID(Blender->GetBlenderSystemID(), PropertyInfo->BlendChannel);
					Blender->ReleaseBlendChannel(BlendChannelID);
				}
				Linker->EntityManager.AddComponents(PropertyInfo->PropertyEntityID, BuiltInComponents->FinishedMask);
			}

			if (PropertyInfo->EmptyChannels.Find(true) != INDEX_NONE)
			{
				--PropertyStats[PropertyInfo->PropertyDefinitionIndex].NumPartialProperties;
			}

			--PropertyStats[PropertyInfo->PropertyDefinitionIndex].NumProperties;
			ResolvedProperties.RemoveAt(PropertyIndex);

			// PropertyInfo is now garbage
		}

		// @todo: If perf is a real issue with this look, we could call ObjectPropertyToResolvedIndex.Remove(MakeTuple(PropertyInfo->BoundObject, PropertyInfo->PropertyBinding.PropertyPath));
		// In the loop above, but it is possible that BoundObject no longer relates to a valid object at that point
		for (auto It = ObjectPropertyToResolvedIndex.CreateIterator(); It; ++It)
		{
			if (!ResolvedProperties.IsAllocated(It.Value()))
			{
				It.RemoveCurrent();
			}
		}
	}

	NewContributors.Empty();
}

void UMovieScenePropertyInstantiatorSystem::UpdatePropertyInfo(const FPropertyParameters& Params)
{
	using namespace UE::MovieScene;

	TArrayView<const FPropertyCompositeDefinition> Composites = BuiltInComponents->PropertyRegistry.GetComposites(*Params.PropertyDefinition);

	FChannelMask EmptyChannels(true, Params.PropertyDefinition->CompositeSize);

	bool bWantsRestoreState = false;
	int32 NumContributors = 0;

	for (auto ContributorIt = Contributors.CreateConstKeyIterator(Params.PropertyInfoIndex); ContributorIt; ++ContributorIt)
	{
		++NumContributors;

		if (!bWantsRestoreState && Linker->EntityManager.HasComponent(ContributorIt.Value(), BuiltInComponents->Tags.RestoreState))
		{
			bWantsRestoreState = true;
		}

		for (int32 CompositeIndex = 0; CompositeIndex < Params.PropertyDefinition->CompositeSize; ++CompositeIndex)
		{
			if (EmptyChannels[CompositeIndex] == false)
			{
				continue;
			}

			FComponentTypeID ThisChannel = Composites[CompositeIndex].ComponentTypeID;
			if (ThisChannel && Linker->EntityManager.HasComponent(ContributorIt.Value(), ThisChannel))
			{
				EmptyChannels[CompositeIndex] = false;
			}
		}
	}

	// Reset the restore state status of the property if we still have contributors
	// We do not do this if there are no contributors to ensure that stale properties are restored correctly
	if (NumContributors > 0)
	{
		const bool bWasPartial = Params.PropertyInfo->EmptyChannels.Find(true) != INDEX_NONE;
		const bool bIsPartial  = EmptyChannels.Find(true) != INDEX_NONE;

		if (bWasPartial != bIsPartial)
		{
			const int32 StatIndex = Params.PropertyInfo->PropertyDefinitionIndex;
			PropertyStats[StatIndex].NumPartialProperties += bIsPartial ? 1 : -1;
		}

		Params.PropertyInfo->EmptyChannels = EmptyChannels;
		Params.PropertyInfo->bWantsRestoreState = bWantsRestoreState;
	}
}

bool UMovieScenePropertyInstantiatorSystem::PropertySupportsFastPath(const FPropertyParameters& Params) const
{
	using namespace UE::MovieScene;

	// Properties that are already blended, or are currently animated must use the blend path
	if (ResolvedProperties[Params.PropertyInfoIndex].BlendChannel != INVALID_BLEND_CHANNEL || Params.PropertyInfo->PropertyEntityID.IsValid())
	{
		return false;
	}

	int32 NumContributors = 0;
	for (auto It = Contributors.CreateConstKeyIterator(Params.PropertyInfoIndex); It; ++It)
	{
		++NumContributors;
		if (NumContributors > 1)
		{
			return false;
		}

		FComponentMask Type = Linker->EntityManager.GetEntityType(It.Value());
		if (Type.Contains(BuiltInComponents->Tags.RelativeBlend) || 
				Type.Contains(BuiltInComponents->Tags.AdditiveBlend) || 
				Type.Contains(BuiltInComponents->Tags.AdditiveFromBaseBlend) || 
				Type.Contains(BuiltInComponents->WeightAndEasingResult))
		{
			return false;
		}
	}

	return true;
}

void UMovieScenePropertyInstantiatorSystem::InitializeFastPath(const FPropertyParameters& Params)
{
	using namespace UE::MovieScene;

	FMovieSceneEntityID SoleContributor = Contributors.FindChecked(Params.PropertyInfoIndex);

	// Have we ever seen this property before?
	if (SoleContributor == Params.PropertyInfo->PropertyEntityID)
	{
		return;
	}

	Params.PropertyInfo->PropertyEntityID = SoleContributor;

	check(Params.PropertyInfo->BlendChannel == INVALID_BLEND_CHANNEL);
	switch (Params.PropertyInfo->Property.GetIndex())
	{
	case 0:
		Linker->EntityManager.AddComponent(SoleContributor, BuiltInComponents->FastPropertyOffset,  Params.PropertyInfo->Property.template Get<uint16>());
		break;
	case 1:
		Linker->EntityManager.AddComponent(SoleContributor, BuiltInComponents->CustomPropertyIndex, Params.PropertyInfo->Property.template Get<FCustomPropertyIndex>());
		break;
	case 2:
		Linker->EntityManager.AddComponent(SoleContributor, BuiltInComponents->SlowProperty,        Params.PropertyInfo->Property.template Get<FSlowPropertyPtr>());
		break;
	}

	if (Linker->EntityManager.HasComponent(SoleContributor, BuiltInComponents->Tags.AlwaysCacheInitialValue))
	{
		Linker->EntityManager.AddComponent(SoleContributor, Params.PropertyDefinition->InitialValueType);
	}

	if (Params.PropertyDefinition->MetaDataTypes.Num() > 0)
	{
		InitializePropertyMetaDataTasks.PadToNum(Params.PropertyInfo->PropertyDefinitionIndex+1, false);
		InitializePropertyMetaDataTasks[Params.PropertyInfo->PropertyDefinitionIndex] = true;

		FComponentMask NewMask;
		for (FComponentTypeID Component : Params.PropertyDefinition->MetaDataTypes)
		{
			NewMask.Set(Component);
		}
		Linker->EntityManager.AddComponents(SoleContributor, NewMask);
	}
}

UMovieScenePropertyInstantiatorSystem::FSetupBlenderSystemResult UMovieScenePropertyInstantiatorSystem::SetupBlenderSystem(const FPropertyParameters& Params)
{
	using namespace UE::MovieScene;

	FSetupBlenderSystemResult Result;

	UClass* NewBlenderClass = nullptr;
	int32 BlenderClassPriority = TNumericLimits<int32>::Lowest();

	// Iterate all the contributors to locate the correct blender system by-priority
	for (auto ContributorIt = Contributors.CreateConstKeyIterator(Params.PropertyInfoIndex); ContributorIt; ++ContributorIt)
	{
		FMovieSceneEntityID Contributor = ContributorIt.Value();
		TOptionalComponentReader<TSubclassOf<UMovieSceneBlenderSystem>> BlenderTypeComponent = Linker->EntityManager.ReadComponent(Contributor, BuiltInComponents->BlenderType);
		if (!BlenderTypeComponent)
		{
			continue;
		}

		UClass* ProspectiveBlenderClass = BlenderTypeComponent->Get();
		if (NewBlenderClass == ProspectiveBlenderClass)
		{
			// If it's already the same, don't waste time getting the CDO or anything like that
			continue;
		}

		const UMovieSceneBlenderSystem* CDO = GetDefault<UMovieSceneBlenderSystem>(ProspectiveBlenderClass);

		if (!NewBlenderClass || CDO->GetSelectionPriority() > BlenderClassPriority)
		{
			NewBlenderClass = ProspectiveBlenderClass;
			BlenderClassPriority = CDO->GetSelectionPriority();
		}
		else
		{
#if DO_CHECK
			ensureMsgf(CDO->GetSelectionPriority() != BlenderClassPriority,
				TEXT("Encountered 2 different blender classes being used with the same priority - this is undefined behavior. Please check the system classes to ensure they have different priorities (%s and %s)."),
				*ProspectiveBlenderClass->GetName(), *NewBlenderClass->GetName());
#endif
		}
	}

	if (!NewBlenderClass)
	{
		NewBlenderClass = Params.PropertyDefinition->BlenderSystemClass;
	}

	if (!ensureMsgf(NewBlenderClass, TEXT("No default blender class specified on property, and no custom blender specified on entities. Falling back to double blender.")))
	{
		NewBlenderClass = UMovieScenePiecewiseDoubleBlenderSystem::StaticClass();
	}

	FComponentTypeID BlenderTypeTag = GetDefault<UMovieSceneBlenderSystem>(NewBlenderClass)->GetBlenderTypeTag();
	ensureMsgf(BlenderTypeTag, TEXT("Encountered a blender system (%s) with an invalid type tag."), *NewBlenderClass->GetName());

	FBlenderSystemInfo NewBlenderInfo{ NewBlenderClass, BlenderTypeTag };
	FBlenderSystemInfo OldBlenderInfo;

	UMovieSceneBlenderSystem* ExistingBlender = Params.PropertyInfo->Blender.Get();
	if (ExistingBlender)
	{
		UClass* OldBlenderClass = ExistingBlender->GetClass();
		if (OldBlenderClass != NewBlenderClass)
		{
			OldBlenderInfo = FBlenderSystemInfo{ OldBlenderClass, ExistingBlender->GetBlenderTypeTag() };
		}
		else
		{
			// It's the same - keep the same info
			OldBlenderInfo = NewBlenderInfo;
		}
	}

	return FSetupBlenderSystemResult{NewBlenderInfo, OldBlenderInfo};
}

void UMovieScenePropertyInstantiatorSystem::InitializeBlendPath(const FPropertyParameters& Params)
{
	using namespace UE::MovieScene;

	TArrayView<const FPropertyCompositeDefinition> Composites = BuiltInComponents->PropertyRegistry.GetComposites(*Params.PropertyDefinition);

	FSetupBlenderSystemResult SetupResult = SetupBlenderSystem(Params);

	// -----------------------------------------------------------------------------------------------------
	// Situation 1: New or modified contributors (inputs) but we're already set up for blending using the same system
	if (SetupResult.CurrentInfo.BlenderSystemClass == SetupResult.PreviousInfo.BlenderSystemClass)
	{
		UMovieSceneBlenderSystem* Blender = Params.PropertyInfo->Blender.Get();
		check(Blender);

		// Ensure the output entity still matches the correct set of channels of all inputs
		FComponentMask NewEntityType;
		Params.MakeOutputComponentType(Linker->EntityManager, Composites, NewEntityType);
		Linker->EntityManager.ChangeEntityType(Params.PropertyInfo->PropertyEntityID, NewEntityType);

		const FMovieSceneBlendChannelID BlendChannel(Blender->GetBlenderSystemID(), Params.PropertyInfo->BlendChannel);

		// Change new contributors to include the blend input components
		for (auto ContributorIt = NewContributors.CreateConstKeyIterator(Params.PropertyInfoIndex); ContributorIt; ++ContributorIt)
		{
			FEntityBuilder()
			.Add(BuiltInComponents->BlendChannelInput, BlendChannel)
			.AddTag(SetupResult.CurrentInfo.BlenderTypeTag)
			.MutateExisting(&Linker->EntityManager, ContributorIt.Value());
		}

		// Nothing more to do
		return;
	}

	// -----------------------------------------------------------------------------------------------------
	// Situation 2: Never used blending before, or the blender type has changed
	UMovieSceneBlenderSystem* OldBlender = Params.PropertyInfo->Blender.Get();
	UMovieSceneBlenderSystem* NewBlender = CastChecked<UMovieSceneBlenderSystem>(Linker->LinkSystem(SetupResult.CurrentInfo.BlenderSystemClass));

	const FMovieSceneBlendChannelID NewBlendChannel = NewBlender->AllocateBlendChannel();

	Params.PropertyInfo->Blender = NewBlender;
	Params.PropertyInfo->BlendChannel = NewBlendChannel.ChannelID;

	FTypelessMutation InputMutation;

	// -----------------------------------------------------------------------------------------------------
	// Situation 2.1: We're already set up for blending, but with a different blender system
	if (OldBlender)
	{
		const FMovieSceneBlendChannelID OldBlendChannel(OldBlender->GetBlenderSystemID(), Params.PropertyInfo->BlendChannel);

		OldBlender->ReleaseBlendChannel(OldBlendChannel);

		// Change the output entity by adding the new blend channel and tag, while simultaneously
		// updating the channels and restore state flags etc added by MakeOutputComponentType
		{
			FTypelessMutation OutputMutation;
			OutputMutation.RemoveAll();

			Params.MakeOutputComponentType(Linker->EntityManager, Composites, OutputMutation.AddMask);

			// Remove the old blender type tag before add the new one
			OutputMutation.AddMask.Remove({ SetupResult.PreviousInfo.BlenderTypeTag });

			FEntityBuilder()
			.Add(BuiltInComponents->BlendChannelOutput, NewBlendChannel)
			.AddTag(SetupResult.CurrentInfo.BlenderTypeTag)
			.MutateExisting(&Linker->EntityManager, Params.PropertyInfo->PropertyEntityID, OutputMutation);
		}

		// Ensure that the old blend tag is removed from inputs
		InputMutation.Remove({ SetupResult.PreviousInfo.BlenderTypeTag });
	}

	// -----------------------------------------------------------------------------------------------------
	// Situation 2.2: Never encountered blending before - need to create a new output entity to receive the blend result
	else
	{
		FComponentMask NewMask;
		NewMask.Set(Params.PropertyDefinition->InitialValueType);

		if (Params.PropertyDefinition->MetaDataTypes.Num() > 0)
		{
			InitializePropertyMetaDataTasks.PadToNum(Params.PropertyInfo->PropertyDefinitionIndex+1, false);
			InitializePropertyMetaDataTasks[Params.PropertyInfo->PropertyDefinitionIndex] = true;

			for (FComponentTypeID Component : Params.PropertyDefinition->MetaDataTypes)
			{
				NewMask.Set(Component);
			}
		}

		for (int32 Index = 0; Index < Composites.Num(); ++Index)
		{
			if (Params.PropertyInfo->EmptyChannels[Index] == false)
			{
				NewMask.Set(Composites[Index].ComponentTypeID);
			}
		}
		NewMask.Set(Params.PropertyDefinition->PropertyType);

		FMovieSceneEntityID NewOutputEntityID;

		auto NewOutputEntity = FEntityBuilder()
		.Add(BuiltInComponents->BlendChannelOutput,      NewBlendChannel)
		.Add(BuiltInComponents->PropertyBinding,         Params.PropertyInfo->PropertyBinding)
		.Add(BuiltInComponents->BoundObject,             Params.PropertyInfo->BoundObject)
		.Add(BuiltInComponents->BlendChannelOutput,      NewBlendChannel)
		.AddTagConditional(BuiltInComponents->Tags.MigratedFromFastPath, Params.PropertyInfo->PropertyEntityID.IsValid())
		.AddTagConditional(BuiltInComponents->Tags.RestoreState, Params.PropertyInfo->bWantsRestoreState)
		.AddTag(SetupResult.CurrentInfo.BlenderTypeTag)
		.AddTag(BuiltInComponents->Tags.NeedsLink)
		.AddMutualComponents();

		switch (Params.PropertyInfo->Property.GetIndex())
		{
		// Never seen this property before
		case 0:
			NewOutputEntityID = NewOutputEntity
			.Add(BuiltInComponents->FastPropertyOffset, Params.PropertyInfo->Property.template Get<uint16>())
			.CreateEntity(&Linker->EntityManager, NewMask);
			break;

		case 1:
			NewOutputEntityID = NewOutputEntity
			.Add(BuiltInComponents->CustomPropertyIndex, Params.PropertyInfo->Property.template Get<FCustomPropertyIndex>())
			.CreateEntity(&Linker->EntityManager, NewMask);
			break;

		case 2:
			NewOutputEntityID = NewOutputEntity
			.Add(BuiltInComponents->SlowProperty, Params.PropertyInfo->Property.template Get<FSlowPropertyPtr>())
			.CreateEntity(&Linker->EntityManager, NewMask);
			break;
		}

		// Params.PropertyInfo->PropertyEntityID will be set if the entity was previously using the fast path
		if (Params.PropertyInfo->PropertyEntityID)
		{
			// Move any copiable/migratable components over from the existing fast-path entity
			Linker->EntityManager.CopyComponents(Params.PropertyInfo->PropertyEntityID, NewOutputEntityID, Linker->EntityManager.GetComponents()->GetCopyAndMigrationMask());

			// Clean the previously-fast-path entity
			InputMutation.RemoveMask = CleanFastPathMask;
		}

		// The property entity ID is now the blend output entity
		Params.PropertyInfo->PropertyEntityID = NewOutputEntityID;
	}

	// Change *all* contributors (not just new ones because the old ones will have the old blender's channel and tag on them)
	// to include the new blend channel input, and remove the old blender type. No need to remove the clean fast path mask because
	// that will have already happened as part of the 'completely new blending' branch below
	for (auto ContributorIt = Contributors.CreateConstKeyIterator(Params.PropertyInfoIndex); ContributorIt; ++ContributorIt)
	{
		const FMovieSceneEntityID Contributor = ContributorIt.Value();

		FEntityBuilder()
		.Add(BuiltInComponents->BlendChannelInput, NewBlendChannel)
		.AddTag(SetupResult.CurrentInfo.BlenderTypeTag)
		.MutateExisting(&Linker->EntityManager, Contributor, InputMutation);
	}
}

int32 UMovieScenePropertyInstantiatorSystem::ResolveProperty(UE::MovieScene::FCustomAccessorView CustomAccessors, UObject* Object, const FMovieScenePropertyBinding& PropertyBinding, int32 PropertyDefinitionIndex)
{
	using namespace UE::MovieScene;

	TTuple<UObject*, FName> Key = MakeTuple(Object, PropertyBinding.PropertyPath);
	if (const int32* ExistingPropertyIndex = ObjectPropertyToResolvedIndex.Find(Key))
	{
		return *ExistingPropertyIndex;
	}

	TOptional<FResolvedProperty> ResolvedProperty = FPropertyRegistry::ResolveProperty(Object, PropertyBinding, CustomAccessors);
	if (!ResolvedProperty.IsSet())
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Unable to resolve property '%s' from '%s' instance '%s'"), *PropertyBinding.PropertyPath.ToString(), *Object->GetClass()->GetName(), *Object->GetName());
		return INDEX_NONE;
	}

	FObjectPropertyInfo NewInfo(MoveTemp(ResolvedProperty.GetValue()));

	NewInfo.BoundObject = Object;
	NewInfo.PropertyBinding = PropertyBinding;
	NewInfo.PropertyDefinitionIndex = PropertyDefinitionIndex;

	const int32 NewPropertyIndex = ResolvedProperties.Add(NewInfo);

	ObjectPropertyToResolvedIndex.Add(Key, NewPropertyIndex);

	++PropertyStats[PropertyDefinitionIndex].NumProperties;

	return NewPropertyIndex;
}

UE::MovieScene::FPropertyRecomposerPropertyInfo UMovieScenePropertyInstantiatorSystem::FindPropertyFromSource(FMovieSceneEntityID EntityID, UObject* Object) const
{
	using namespace UE::MovieScene;

	TOptionalComponentReader<FMovieScenePropertyBinding> PropertyBinding = Linker->EntityManager.ReadComponent(EntityID, BuiltInComponents->PropertyBinding);
	if (!PropertyBinding)
	{
		return FPropertyRecomposerPropertyInfo::Invalid();
	}

	TTuple<UObject*, FName> Key = MakeTuple(Object, PropertyBinding->PropertyPath);
	if (const int32* PropertyIndex = ObjectPropertyToResolvedIndex.Find(Key))
	{
		const FObjectPropertyInfo& PropertyInfo = ResolvedProperties[*PropertyIndex];
		return FPropertyRecomposerPropertyInfo { PropertyInfo.BlendChannel, PropertyInfo.Blender.Get(), PropertyInfo.PropertyEntityID };
	}

	return FPropertyRecomposerPropertyInfo::Invalid();
}

void UMovieScenePropertyInstantiatorSystem::InitializePropertyMetaData(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_InitializePropertyMetaData);

	for (TConstSetBitIterator<> TypesToCache(InitializePropertyMetaDataTasks); TypesToCache; ++TypesToCache)
	{
		FCompositePropertyTypeID PropertyID = FCompositePropertyTypeID::FromIndex(TypesToCache.GetIndex());

		const FPropertyDefinition& Definition = BuiltInComponents->PropertyRegistry.GetDefinition(PropertyID);
		Definition.Handler->DispatchInitializePropertyMetaDataTasks(Definition, InPrerequisites, Subsequents, Linker);
	}

	InitializePropertyMetaDataTasks.Empty();
}

void UMovieScenePropertyInstantiatorSystem::FPropertyParameters::MakeOutputComponentType(
	const UE::MovieScene::FEntityManager& EntityManager,
	TArrayView<const UE::MovieScene::FPropertyCompositeDefinition> Composites,
	UE::MovieScene::FComponentMask& OutComponentType) const
{
	using namespace UE::MovieScene;

	// Get the existing type
	OutComponentType = EntityManager.GetEntityType(PropertyInfo->PropertyEntityID);

	// Ensure the property has only the exact combination of channels that constitute its animation
	for (int32 Index = 0; Index < Composites.Num(); ++Index)
	{
		FComponentTypeID Composite = Composites[Index].ComponentTypeID;
		OutComponentType[Composite] = (PropertyInfo->EmptyChannels[Index] != true);
	}
	OutComponentType.Set(PropertyDefinition->PropertyType);

	// Set the restore state tag appropriately
	if (PropertyInfo->bWantsRestoreState)
	{
		OutComponentType.Set(FBuiltInComponentTypes::Get()->Tags.RestoreState);
	}
	else
	{
		OutComponentType.Remove(FBuiltInComponentTypes::Get()->Tags.RestoreState);
	}
}
