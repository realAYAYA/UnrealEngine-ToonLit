// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceBindingReference.h"
#include "Engine/Level.h"
#include "LevelSequenceLegacyObjectReference.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealEngine.h"
#include "MovieSceneFwd.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LevelStreamingDynamic.h"
#include "WorldPartition/IWorldPartitionObjectResolver.h"
#include "IMovieSceneBoundObjectProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceBindingReference)

namespace UE::MovieScene
{

UObject* FindBoundObjectProxy(UObject* BoundObject)
{
	if (!BoundObject)
	{
		return nullptr;
	}

	IMovieSceneBoundObjectProxy* RawInterface = Cast<IMovieSceneBoundObjectProxy>(BoundObject);
	if (RawInterface)
	{
		return RawInterface->NativeGetBoundObjectForSequencer(BoundObject);
	}
	else if (BoundObject->GetClass()->ImplementsInterface(UMovieSceneBoundObjectProxy::StaticClass()))
	{
		return IMovieSceneBoundObjectProxy::Execute_BP_GetBoundObjectForSequencer(BoundObject, BoundObject);
	}
	return BoundObject;
}

} // namespace UE::MovieScene

FLevelSequenceBindingReference::FLevelSequenceBindingReference(UObject* InObject, UObject* InContext)
{
	check(InContext && InObject);

	if (!InContext->IsA<UWorld>() && InObject->IsIn(InContext))
	{
		ObjectPath = InObject->GetPathName(InContext);
	}
	else
	{
		FString FullPath = InObject->GetPathName();

#if WITH_EDITORONLY_DATA
		UPackage* ObjectPackage = InObject->GetOutermost();

		if (ensure(ObjectPackage))
		{
			// If this is being set from PIE we need to remove the pie prefix and point to the editor object
			if (ObjectPackage->GetPIEInstanceID() != INDEX_NONE)
			{
				FString PIEPrefix = FString::Printf(PLAYWORLD_PACKAGE_PREFIX TEXT("_%d_"), ObjectPackage->GetPIEInstanceID());
				FullPath.ReplaceInline(*PIEPrefix, TEXT(""));
			}
		}
#endif
		
		ExternalObjectPath = FSoftObjectPath(FullPath);
	}
}

UObject* FLevelSequenceBindingReference::Resolve(UObject* InContext, const FResolveBindingParams& InResolveBindingParams) const
{
	// tidy up todo: StreamedLevelAsset path and WorldPartitionResolveData code paths could probably share most their code
	//				 InContext could be reverted back to always be the UWorld in both paths and instead StreamedLevelAsset code path could use the StreamingWorld to resolve

	if (InContext && InContext->IsA<AActor>())
	{
		if (ExternalObjectPath.IsNull())
		{
			if (UE::IsSavingPackage(nullptr) || IsGarbageCollecting())
			{
				return nullptr;
			}

			return FindObject<UObject>(InContext, *ObjectPath, false);
		}
	}
	else if (InContext && InContext->IsA<ULevel>() && InResolveBindingParams.StreamedLevelAssetPath.IsValid() && ExternalObjectPath.GetAssetPath() == InResolveBindingParams.StreamedLevelAssetPath)
	{
		if (UE::IsSavingPackage(nullptr) || IsGarbageCollecting())
		{
			return nullptr;
		}

		UObject* ResolvedObject = nullptr;
		// ExternalObjectPath.GetSubPathString() specifies the path from the package (so includes PersistentLevel.) so we must do a ResolveSubObject from its outer
		UObject* ContextOuter = InContext->GetOuter();
		check(ContextOuter);
		ContextOuter->ResolveSubobject(*ExternalObjectPath.GetSubPathString(), ResolvedObject, /*bLoadIfExists*/false);
		return ResolvedObject;
	} 
	else if (UObject* ResolvedObject = nullptr; ExternalObjectPath.IsValid() && InResolveBindingParams.WorldPartitionResolveData && InResolveBindingParams.WorldPartitionResolveData->ResolveObject(InResolveBindingParams.StreamingWorld, ExternalObjectPath, ResolvedObject))
	{
		return ResolvedObject;
	}
	else
	{
		FSoftObjectPath TempPath = ExternalObjectPath;

		// Soft Object Paths don't follow asset redirectors when attempting to call ResolveObject or TryLoad.
		// We want to follow the asset redirector so that maps that have been renamed (from Untitled to their first asset name)
		// properly resolve. This fixes Possessable bindings losing their references the first time you save a map.
		TempPath.PreSavePath();

	#if WITH_EDITORONLY_DATA
		// Sequencer is explicit about providing a resolution context for its bindings. We never want to resolve to objects
		// with a different PIE instance ID, even if the current callstack is being executed inside a different GPlayInEditorID
		// scope. Since ResolveObject will always call FixupForPIE in editor based on GPlayInEditorID, we always override the current
		// GPlayInEditorID to be the current PIE instance of the provided context.
		const int32 ContextPlayInEditorID = InContext ? InContext->GetOutermost()->GetPIEInstanceID() : INDEX_NONE;
		FTemporaryPlayInEditorIDOverride PIEGuard(ContextPlayInEditorID);
	#endif

		return TempPath.ResolveObject();
	}

	return nullptr;
}

bool FLevelSequenceBindingReference::operator==(const FLevelSequenceBindingReference& Other) const
{
	return ExternalObjectPath == Other.ExternalObjectPath && ObjectPath == Other.ObjectPath;
}

void FLevelSequenceBindingReference::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() && !PackageName_DEPRECATED.IsEmpty())
	{
		// This was saved as two strings, combine into one soft object path so it handles PIE and redirectors properly
		FString FullPath = PackageName_DEPRECATED + TEXT(".") + ObjectPath;

		ExternalObjectPath.SetPath(FullPath);
		ObjectPath.Reset();
		PackageName_DEPRECATED.Reset();
	}
}

UObject* ResolveByPath(UObject* InContext, const FString& InObjectPath)
{
	if (UE::IsSavingPackage(nullptr) || IsGarbageCollecting())
	{
		return nullptr;
	}

	if (!InObjectPath.IsEmpty())
	{
		if (UObject* FoundObject = FindObject<UObject>(InContext, *InObjectPath, false))
		{
			return FoundObject;
		}

#if WITH_EDITOR
		UWorld* WorldContext = InContext->GetWorld();
		if (WorldContext && WorldContext->IsPlayInEditor())
		{
			FString PackageRoot, PackagePath, PackageName;
			if (FPackageName::SplitLongPackageName(InObjectPath, PackageRoot, PackagePath, PackageName))
			{
				int32 ObjectDelimiterIdx = INDEX_NONE;
				PackageName.FindChar('.', ObjectDelimiterIdx);
				const FString SubLevelObjPath = PackageName.Mid(ObjectDelimiterIdx + 1);

				for (ULevel* Level : WorldContext->GetLevels())
				{
					UPackage* Pkg = Level->GetOutermost();
					if (UObject* FoundObject = FindObject<UObject>(Pkg, *SubLevelObjPath, false))
 					{
 						return FoundObject;
					}
				}
			}
		}
#endif

		if (UObject* FoundObject = FindFirstObject<UObject>(*InObjectPath, EFindFirstObjectOptions::NativeFirst))
		{
			return FoundObject;
		}
	}

	return nullptr;
}

UObject* FLevelSequenceLegacyObjectReference::Resolve(UObject* InContext) const
{
	if (ObjectId.IsValid() && InContext != nullptr)
	{
		int32 PIEInstanceID = InContext->GetOutermost()->GetPIEInstanceID();
		FUniqueObjectGuid FixedUpId = PIEInstanceID == -1 ? ObjectId : ObjectId.FixupForPIE(PIEInstanceID);

		if (PIEInstanceID != -1 && FixedUpId == ObjectId)
		{
			UObject* FoundObject = ResolveByPath(InContext, ObjectPath);
			if (FoundObject)
			{
				return FoundObject;
			}

			UE_LOG(LogMovieScene, Warning, TEXT("Attempted to resolve object (%s) with a PIE instance that has not been fixed up yet. This is probably due to a streamed level not being available yet."), *ObjectPath);
			return nullptr;
		}
		FLazyObjectPtr LazyPtr;
		LazyPtr = FixedUpId;

		if (UObject* FoundObject = LazyPtr.Get())
		{
			return FoundObject;
		}
	}

	return ResolveByPath(InContext, ObjectPath);
}

bool FLevelSequenceObjectReferenceMap::Serialize(FArchive& Ar)
{
	int32 Num = Map.Num();
	Ar << Num;

	if (Ar.IsLoading())
	{
		while(Num-- > 0)
		{
			FGuid Key;
			Ar << Key;

			FLevelSequenceLegacyObjectReference Value;
			Ar << Value;

			Map.Add(Key, Value);
		}
	}
	else if (Ar.IsSaving() || Ar.IsCountingMemory() || Ar.IsObjectReferenceCollector())
	{
		for (auto& Pair : Map)
		{
			Ar << Pair.Key;
			Ar << Pair.Value;
		}
	}
	return true;
}

bool FLevelSequenceBindingReferences::HasBinding(const FGuid& ObjectId) const
{
	return BindingIdToReferences.Contains(ObjectId) || AnimSequenceInstances.Contains(ObjectId) || PostProcessInstances.Contains(ObjectId);
}

void FLevelSequenceBindingReferences::AddBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext)
{
	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(InObject))
	{
		if (AnimInstance->GetOwningComponent()->GetAnimInstance() == InObject)
		{
			AnimSequenceInstances.Add(ObjectId);
		}
		else if (AnimInstance->GetOwningComponent()->GetPostProcessInstance() == InObject)
		{
			PostProcessInstances.Add(ObjectId);
		}
		else
		{
			UE_LOG(LogMovieScene, Warning, TEXT("Attempted to add a binding for %s which is not an anim instance or post process instance"), *GetNameSafe(InObject));
		}
	}
	else
	{
		BindingIdToReferences.FindOrAdd(ObjectId).References.Emplace(InObject, InContext);
	}
}

void FLevelSequenceBindingReferences::RemoveBinding(const FGuid& ObjectId)
{
	BindingIdToReferences.Remove(ObjectId);
	AnimSequenceInstances.Remove(ObjectId);
}

void FLevelSequenceBindingReferences::RemoveObjects(const FGuid& ObjectId, const TArray<UObject*>& InObjects, UObject* InContext)
{
	FLevelSequenceBindingReferenceArray* ReferenceArray = BindingIdToReferences.Find(ObjectId);
	if (!ReferenceArray)
	{
		return;
	}

	for (int32 ReferenceIndex = 0; ReferenceIndex < ReferenceArray->References.Num(); )
	{
		UObject* ResolvedObject = ReferenceArray->References[ReferenceIndex].Resolve(InContext, FLevelSequenceBindingReference::FResolveBindingParams());
		ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);

		if (InObjects.Contains(ResolvedObject))
		{
			ReferenceArray->References.RemoveAt(ReferenceIndex);
		}
		else
		{
			++ReferenceIndex;
		}
	}
}

void FLevelSequenceBindingReferences::RemoveInvalidObjects(const FGuid& ObjectId, UObject* InContext)
{
	FLevelSequenceBindingReferenceArray* ReferenceArray = BindingIdToReferences.Find(ObjectId);
	if (!ReferenceArray)
	{
		return;
	}

	for (int32 ReferenceIndex = 0; ReferenceIndex < ReferenceArray->References.Num(); )
	{
		UObject* ResolvedObject = ReferenceArray->References[ReferenceIndex].Resolve(InContext, FLevelSequenceBindingReference::FResolveBindingParams());
		ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);

		if (!IsValid(ResolvedObject))
		{
			ReferenceArray->References.RemoveAt(ReferenceIndex);
		}
		else
		{
			++ReferenceIndex;
		}
	}
}

void FLevelSequenceBindingReferences::ResolveBinding(const FGuid& ObjectId, UObject* InContext, const FLevelSequenceBindingReference::FResolveBindingParams& InResolveBindingParams, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	if (const FLevelSequenceBindingReferenceArray* ReferenceArray = BindingIdToReferences.Find(ObjectId))
	{
		for (const FLevelSequenceBindingReference& Reference : ReferenceArray->References)
		{
			UObject* ResolvedObject = Reference.Resolve(InContext, InResolveBindingParams);
			ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);
			if (ResolvedObject && ResolvedObject->GetWorld())
			{
				OutObjects.Add(ResolvedObject);
			}
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InContext))
	{
		// If the object ID exists in the AnimSequenceInstances set, then this binding relates to an anim instance on a skeletal mesh component
		if (SkeletalMeshComponent)
		{
			if (AnimSequenceInstances.Contains(ObjectId) && SkeletalMeshComponent->GetAnimInstance())
			{
				OutObjects.Add(SkeletalMeshComponent->GetAnimInstance());
			}
			else if (PostProcessInstances.Contains(ObjectId) && SkeletalMeshComponent->GetPostProcessInstance())
			{
				OutObjects.Add(SkeletalMeshComponent->GetPostProcessInstance());
			}
		}
	}
}

FGuid FLevelSequenceBindingReferences::FindBindingFromObject(UObject* InObject, UObject* InContext) const
{
	FLevelSequenceBindingReference Predicate(InObject, InContext);

	for (const TPair<FGuid, FLevelSequenceBindingReferenceArray>& Pair : BindingIdToReferences)
	{
		if (Pair.Value.References.Contains(Predicate))
		{
			return Pair.Key;
		}
	}

	return FGuid();
}

void FLevelSequenceBindingReferences::RemoveInvalidBindings(const TSet<FGuid>& ValidBindingIDs)
{
	for (auto It = BindingIdToReferences.CreateIterator(); It; ++It)
	{
		if (!ValidBindingIDs.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
}


UObject* FLevelSequenceObjectReferenceMap::ResolveBinding(const FGuid& ObjectId, UObject* InContext) const
{
	const FLevelSequenceLegacyObjectReference* Reference = Map.Find(ObjectId);
	UObject* ResolvedObject = Reference ? Reference->Resolve(InContext) : nullptr;
	ResolvedObject = UE::MovieScene::FindBoundObjectProxy(ResolvedObject);

	if (ResolvedObject != nullptr)
	{
		// if the resolved object does not have a valid world (e.g. world is being torn down), dont resolve
		return ResolvedObject->GetWorld() != nullptr ? ResolvedObject : nullptr;
	}
	return nullptr;
}

