// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "Roles/LiveLinkAnimationRole.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LiveLinkComponent)

// Sets default values for this component's properties
ULiveLinkComponent::ULiveLinkComponent()
	: bIsDirty(false)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;
	bTickInEditor = true;
}

void ULiveLinkComponent::OnRegister()
{
	bIsDirty = true;
	Super::OnRegister();
}


// Called every frame
void ULiveLinkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// If we have been recently registered then ensure all Skeletal Mesh Components on the actor run in editor
	if (bIsDirty)
	{
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		GetOwner()->GetComponents(SkeletalMeshComponents);
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			SkeletalMeshComponent->SetUpdateAnimationInEditor(true);
		}
		bIsDirty = false;
	}
	
	if (OnLiveLinkUpdated.IsBound())
	{
		FEditorScriptExecutionGuard ScriptGuard;

		OnLiveLinkUpdated.Broadcast(DeltaTime);
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool ULiveLinkComponent::HasLiveLinkClient()
{
	if (LiveLinkClient == nullptr)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		}
	}

	return (LiveLinkClient != nullptr);
}

void ULiveLinkComponent::GetAvailableSubjectNames(TArray<FName>& SubjectNames)
{
	if (HasLiveLinkClient())
	{
		TArray<FLiveLinkSubjectKey> SubjectKeys = LiveLinkClient->GetSubjects(false, true);
		SubjectNames.Reset(SubjectKeys.Num());
		for (const FLiveLinkSubjectKey& SubjectKey : SubjectKeys)
		{
			SubjectNames.Add(SubjectKey.SubjectName);
		}
	}
	else
	{
		SubjectNames.Reset();
	}
}

void ULiveLinkComponent::GetSubjectData(const FName SubjectName, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle)
{
	bSuccess = false;
	if (HasLiveLinkClient())
	{
		FLiveLinkSubjectFrameData FrameData;
		if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkAnimationRole::StaticClass(), FrameData))
		{
			const FLiveLinkSkeletonStaticData* SkeletonData = FrameData.StaticData.Cast<FLiveLinkSkeletonStaticData>();
			const FLiveLinkAnimationFrameData* AnimationFrameData = FrameData.FrameData.Cast<FLiveLinkAnimationFrameData>();
			FLiveLinkBlueprintDataStruct BlueprintDataWrapper(FSubjectFrameHandle::StaticStruct(), &SubjectFrameHandle);

			if (SkeletonData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Static data was invalid."), ELogVerbosity::Error);
				return;
			}

			if (AnimationFrameData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Frame data was invalid."), ELogVerbosity::Error);
				return;
			}

			bSuccess = ULiveLinkAnimationRole::StaticClass()->GetDefaultObject<ULiveLinkAnimationRole>()->InitializeBlueprintData(FrameData, BlueprintDataWrapper);
		}

	}
}

void ULiveLinkComponent::GetSubjectDataAtWorldTime(const FName SubjectName, const float WorldTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GetSubjectDataAtTime(SubjectName, (double)WorldTime, bSuccess, SubjectFrameHandle);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void ULiveLinkComponent::GetSubjectDataAtTime(const FName SubjectName, const double WorldTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle)
{
	bSuccess = false;
	if (HasLiveLinkClient())
	{
		FLiveLinkSubjectFrameData FrameData;
		if (LiveLinkClient->EvaluateFrameAtWorldTime_AnyThread(SubjectName, WorldTime, ULiveLinkAnimationRole::StaticClass(), FrameData))
		{
			const FLiveLinkSkeletonStaticData* SkeletonData = FrameData.StaticData.Cast<FLiveLinkSkeletonStaticData>();
			const FLiveLinkAnimationFrameData* AnimationFrameData = FrameData.FrameData.Cast<FLiveLinkAnimationFrameData>();
			FLiveLinkBlueprintDataStruct BlueprintDataWrapper(FSubjectFrameHandle::StaticStruct(), &SubjectFrameHandle);

			if (SkeletonData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Static data was invalid."), ELogVerbosity::Error);
				return;
			}

			if (AnimationFrameData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Frame data was invalid."), ELogVerbosity::Error);
				return;
			}

			bSuccess = ULiveLinkAnimationRole::StaticClass()->GetDefaultObject<ULiveLinkAnimationRole>()->InitializeBlueprintData(FrameData, BlueprintDataWrapper);
		}
	}
}

void ULiveLinkComponent::GetSubjectDataAtSceneTime(const FName SubjectName, const FTimecode& SceneTime, bool& bSuccess, FSubjectFrameHandle& SubjectFrameHandle)
{
	bSuccess = false;
	if (HasLiveLinkClient())
	{
		FLiveLinkSubjectFrameData FrameData;
		if (LiveLinkClient->EvaluateFrameAtSceneTime_AnyThread(SubjectName, FQualifiedFrameTime(SceneTime, FApp::GetTimecodeFrameRate()), ULiveLinkAnimationRole::StaticClass(), FrameData))
		{
			const FLiveLinkSkeletonStaticData* SkeletonData = FrameData.StaticData.Cast<FLiveLinkSkeletonStaticData>();
			const FLiveLinkAnimationFrameData* AnimationFrameData = FrameData.FrameData.Cast<FLiveLinkAnimationFrameData>();
			FLiveLinkBlueprintDataStruct BlueprintDataWrapper(FSubjectFrameHandle::StaticStruct(), &SubjectFrameHandle);

			if (SkeletonData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Static data was invalid."), ELogVerbosity::Error);
				return;
			}

			if (AnimationFrameData == nullptr)
			{
				FFrame::KismetExecutionMessage(TEXT("Could not get subject data. Frame data was invalid."), ELogVerbosity::Error);
				return;
			}
			
			bSuccess = ULiveLinkAnimationRole::StaticClass()->GetDefaultObject<ULiveLinkAnimationRole>()->InitializeBlueprintData(FrameData, BlueprintDataWrapper);
		}
	}
}

