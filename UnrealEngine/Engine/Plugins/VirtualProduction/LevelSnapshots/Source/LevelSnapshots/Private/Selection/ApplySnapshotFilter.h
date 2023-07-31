// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selection/PropertySelectionMap.h"

class AActor;
class UActorComponent;
class ULevelSnapshot;
class ULevelSnapshotFilter;

namespace UE::LevelSnapshots::Private
{
	class FApplySnapshotFilter
	{
	public:

		static FApplySnapshotFilter Make(ULevelSnapshot* Snapshot, AActor* DeserializedSnapshotActor, AActor* WorldActor, const ULevelSnapshotFilter* Filter);
		
		FApplySnapshotFilter& AllowUnchangedProperties(bool bNewValue)
		{
			bAllowUnchangedProperties = bNewValue;
			return *this;
		}
		FApplySnapshotFilter& AllowNonEditableProperties(bool bNewValue)
		{
			bAllowNonEditableProperties = bNewValue;
			return *this;
		}
		
		void ApplyFilterToFindSelectedProperties(FPropertySelectionMap& MapToAddTo);

	private:

		enum class EFilterObjectPropertiesResult
		{
			HasCustomSubobjects,
			HasOnlyNormalProperties
		};
		
		struct FPropertyContainerContext
		{
			FPropertySelectionMap& MapToAddTo;
			FPropertySelection& SelectionToAddTo;
		
			UStruct* ContainerClass;
			void* SnapshotContainer;
			void* WorldContainer;
			UObject* AnalysedSnapshotObject;
			UObject* AnalysedWorldObject;
				
			/* Information passed to blueprints. Property name is appended to this.
			 * Example: [FooComponent] [BarStructPropertyName]...
			 */
			TArray<FString> AuthoredPathInformation;

			/* Keeps track of the structs leading to this container */
			FLevelSnapshotPropertyChain PropertyChain;
			/* Class that PropertyChain begins from. */
			UClass* RootClass;

			FPropertyContainerContext(FPropertySelectionMap& MapToAddTo, FPropertySelection& SelectionToAddTo, UStruct* ContainerClass, void* SnapshotContainer, void* WorldContainer, UObject* AnalysedSnapshotObject, UObject* AnalysedWorldObject, const TArray<FString>& AuthoredPathInformation, const FLevelSnapshotPropertyChain& PropertyChain, UClass* RootClass);
		};

		enum class EPropertySearchResult
		{
			FoundProperties,
			NoPropertiesFound
		};
		
		FApplySnapshotFilter(ULevelSnapshot* Snapshot, AActor* DeserializedSnapshotActor, AActor* WorldActor, const ULevelSnapshotFilter* Filter);
		bool EnsureParametersAreValid() const;
		
		void AnalyseComponentProperties(FPropertySelectionMap& MapToAddTo);

		void FilterActorPair(FPropertySelectionMap& MapToAddTo);
		EPropertySearchResult FilterSubobjectPair(FPropertySelectionMap& MapToAddTo, UObject* SnapshotSubobject, UObject* WorldSubobject);
		EPropertySearchResult FilterStructPair(FPropertyContainerContext& Parent, FStructProperty* StructProperty);
		EFilterObjectPropertiesResult FindAndFilterCustomSubobjectPairs(FPropertySelectionMap& MapToAddTo, UObject* SnapshotOwner, UObject* WorldOwner);

		void AnalyseRootProperties(FPropertyContainerContext& ContainerContext, UObject* SnapshotObject, UObject* WorldObject);
		void ExtendAnalysedProperties(FPropertyContainerContext& ContainerContext, UObject* SnapshotObject, UObject* WorldObject);
		TOptional<EPropertySearchResult> HandlePossibleStructProperties(FPropertyContainerContext& ContainerContext, FProperty* PropertyToHandle);
		EPropertySearchResult AnalyseStructProperties(FPropertyContainerContext& ContainerContext);

		
		TOptional<EPropertySearchResult> HandlePossibleSubobjectProperties(FPropertyContainerContext& ContainerContext, FProperty* PropertyToHandle);
		
		TOptional<EPropertySearchResult> AnalysePossibleArraySubobjectProperties(FPropertyContainerContext& ContainerContext, FArrayProperty* PropertyToHandle);
		TOptional<EPropertySearchResult> AnalysePossibleSetSubobjectProperties(FPropertyContainerContext& ContainerContext, FSetProperty* PropertyToHandle);
		TOptional<EPropertySearchResult> AnalysePossibleMapSubobjectProperties(FPropertyContainerContext& ContainerContext, FMapProperty* PropertyToHandle);
		
		template<typename TCollectionData>
		TOptional<EPropertySearchResult> AnalysePossibleSubobjectsInCollection(FPropertyContainerContext& ContainerContext, FObjectPropertyBase* ObjectProperty, TCollectionData& Detail);
		
		TOptional<EPropertySearchResult> AnalysePossibleSubobjectProperties(FPropertyContainerContext& ContainerContext, FObjectPropertyBase* PropertyToHandle, void* SnapshotValuePtr, void* WorldValuePtr);

		

		EPropertySearchResult AnalyseProperty(FPropertyContainerContext& ContainerContext, FProperty* PropertyInCommon, bool bSkipEqualityTest = false);
		EPropertySearchResult TrackChangedProperties(FPropertyContainerContext& ContainerContext, FProperty* PropertyInCommon);

		
		ULevelSnapshot* Snapshot;
		AActor* DeserializedSnapshotActor;
		AActor* WorldActor;
		const ULevelSnapshotFilter* Filter;
		/** Every time an object is analysed, it is added here, so we do not analyse more than once. */
		TMap<UObject*, EPropertySearchResult> AnalysedSnapshotObjects;

		/* Do we allow properties that do not show up in the details panel? */
		bool bAllowNonEditableProperties = false;

		/* Do we allow adding properties that did not change to the selection map? */
		bool bAllowUnchangedProperties = false;
	};
}

