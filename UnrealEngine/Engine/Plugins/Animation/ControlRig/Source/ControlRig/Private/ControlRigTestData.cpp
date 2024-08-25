// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigTestData.h"
#include "ControlRig.h"
#include "ControlRigObjectVersion.h"
#include "HAL/PlatformTime.h"
#if WITH_EDITOR
#include "AssetToolsModule.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigTestData)

bool FControlRigTestDataFrame::Store(UControlRig* InControlRig, bool bInitial)
{
	if(InControlRig == nullptr)
	{
		return false;
	}

	AbsoluteTime = InControlRig->GetAbsoluteTime();
	DeltaTime = InControlRig->GetDeltaTime();
	Pose = InControlRig->GetHierarchy()->GetPose(bInitial);
	Variables.Reset();

	const TArray<FRigVMExternalVariable> ExternalVariables = InControlRig->GetExternalVariables(); 
	for(const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		FControlRigTestDataVariable VariableData;
		VariableData.Name = ExternalVariable.Name;
		VariableData.CPPType = ExternalVariable.TypeName;

		if(ExternalVariable.Property && ExternalVariable.Memory)
		{
			ExternalVariable.Property->ExportText_Direct(
				VariableData.Value,
				ExternalVariable.Memory,
				ExternalVariable.Memory,
				nullptr,
				PPF_None,
				nullptr
			);
		}

		Variables.Add(VariableData);
	}

	return true;
}

bool FControlRigTestDataFrame::Restore(UControlRig* InControlRig, bool bInitial) const
{
	if(InControlRig == nullptr)
	{
		return false;
	}

	URigHierarchy* Hierarchy = InControlRig->GetHierarchy();

	// check if the pose can be applied
	for(const FRigPoseElement& PoseElement : Pose.Elements)
	{
		const FRigElementKey& Key = PoseElement.Index.GetKey(); 
		if(!Hierarchy->Contains(Key))
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig does not contain hierarchy element '%s'. Please re-create the test data asset."), *Key.ToString());
			return false;
		}
	}

	Hierarchy->SetPose(Pose, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal);

	return RestoreVariables(InControlRig);
}

bool FControlRigTestDataFrame::RestoreVariables(UControlRig* InControlRig) const
{
	class FControlRigTestDataFrame_ErrorPipe : public FOutputDevice
	{
	public:

		TArray<FString> Errors;

		FControlRigTestDataFrame_ErrorPipe()
			: FOutputDevice()
		{
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			Errors.Add(FString::Printf(TEXT("Error convert to string: %s"), V));
		}
	};

	const TArray<FRigVMExternalVariable> ExternalVariables = InControlRig->GetExternalVariables();

	if(ExternalVariables.Num() != Variables.Num())
	{
		UE_LOG(LogControlRig, Error, TEXT("Variable data does not match the Rig. Please re-create the test data asset."));
		return false;
	}
	
	for(const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		if(ExternalVariable.Memory == nullptr || ExternalVariable.Property == nullptr)
		{
			UE_LOG(LogControlRig, Error, TEXT("Variable '%s' is not valid."), *ExternalVariable.Name.ToString());
			return false;
		}
		
		const FControlRigTestDataVariable* VariableData = Variables.FindByPredicate(
			[ExternalVariable](const FControlRigTestDataVariable& InVariable) -> bool
			{
				return InVariable.Name == ExternalVariable.Name &&
					InVariable.CPPType == ExternalVariable.TypeName;
			}
		);

		if(VariableData)
		{
			FControlRigTestDataFrame_ErrorPipe ErrorPipe;
			ExternalVariable.Property->ImportText_Direct(
				*VariableData->Value,
				ExternalVariable.Memory,
				nullptr,
				PPF_None,
				&ErrorPipe
			);

			if(!ErrorPipe.Errors.IsEmpty())
			{
				for(const FString& ImportError : ErrorPipe.Errors)
				{
					UE_LOG(LogControlRig, Error, TEXT("Import Error for Variable '%s': %s"), *ExternalVariable.Name.ToString(), *ImportError);
				}
				return false;
			}
		}
		else
		{
			UE_LOG(LogControlRig, Error, TEXT("Variable data for '%s' is not part of the test file. Please re-create the test data asset."), *ExternalVariable.Name.ToString());
			return false;
		}
	}

	return true;
}

void UControlRigTestData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);	
	UObject::Serialize(Ar);
	LastFrameIndex = INDEX_NONE;

	// If pose is older than RigPoseWithParentKey, set the active parent of all poses to invalid key
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RigPoseWithParentKey)
	{
		for (FRigPoseElement& Element : Initial.Pose)
		{
			Element.ActiveParent = FRigElementKey();
		}
		for (FControlRigTestDataFrame& Frame : InputFrames)
		{
			for (FRigPoseElement& Element : Frame.Pose)
			{
				Element.ActiveParent = FRigElementKey();
			}
		}
		for (FControlRigTestDataFrame& Frame : OutputFrames)
		{
			for (FRigPoseElement& Element : Frame.Pose)
			{
				Element.ActiveParent = FRigElementKey();
			}
		}
	}
}

UControlRigTestData* UControlRigTestData::CreateNewAsset(FString InDesiredPackagePath, FString InBlueprintPathName)
{
#if WITH_EDITOR
	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString UniquePackageName;
	FString UniqueAssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(InDesiredPackagePath, TEXT(""), UniquePackageName, UniqueAssetName);

	if (UniquePackageName.EndsWith(UniqueAssetName))
	{
		UniquePackageName = UniquePackageName.LeftChop(UniqueAssetName.Len() + 1);
	}

	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(*UniqueAssetName, *UniquePackageName, UControlRigTestData::StaticClass(), nullptr);
	if(NewAsset)
	{
		// make sure the package is never cooked.
		UPackage* Package = NewAsset->GetOutermost();
		Package->SetPackageFlags(Package->GetPackageFlags() | PKG_EditorOnly);
			
		if(UControlRigTestData* TestData = Cast<UControlRigTestData>(NewAsset))
		{
			TestData->ControlRigObjectPath = InBlueprintPathName;
			return TestData;
		}
	}
#endif
	return nullptr;
}

FVector2D UControlRigTestData::GetTimeRange(bool bInput) const
{
	const TArray<FControlRigTestDataFrame>& Frames = bInput ? InputFrames : OutputFrames;
	if(Frames.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	return FVector2D(Frames[0].AbsoluteTime, Frames.Last().AbsoluteTime);
}

int32 UControlRigTestData::GetFrameIndexForTime(double InSeconds, bool bInput) const
{
	const TArray<FControlRigTestDataFrame>& Frames = bInput ? InputFrames : OutputFrames;

	if(LastFrameIndex == INDEX_NONE)
	{
		LastFrameIndex = 0;
	}

	while(Frames.IsValidIndex(LastFrameIndex) && Frames[LastFrameIndex].AbsoluteTime < InSeconds)
	{
		LastFrameIndex++;
	}

	while(Frames.IsValidIndex(LastFrameIndex) && Frames[LastFrameIndex].AbsoluteTime > InSeconds)
	{
		LastFrameIndex--;
	}

	LastFrameIndex = FMath::Clamp(LastFrameIndex, 0, Frames.Num() - 1);

	return LastFrameIndex;
}

bool UControlRigTestData::Record(UControlRig* InControlRig, double InRecordingDuration)
{
	if(InControlRig == nullptr)
	{
		return false;
	}

	ReleaseReplay();
	ClearDelegates(InControlRig);

	DesiredRecordingDuration = InRecordingDuration;
	TimeAtStartOfRecording = FPlatformTime::Seconds();

	// if this is the first frame
	if(InputFrames.IsEmpty())
	{
		InControlRig->RequestInit();
		PreConstructionHandle = InControlRig->OnPreConstruction_AnyThread().AddLambda(
			[this](UControlRig* InControlRig, const FName& InEventName)
			{
				Initial.Store(InControlRig, true);
			}
		);
	}

	PreForwardHandle = InControlRig->OnPreForwardsSolve_AnyThread().AddLambda(
		[this](UControlRig* InControlRig, const FName& InEventName)
		{
			FControlRigTestDataFrame Frame;
			Frame.Store(InControlRig);

			// reapply the variable data. we are doing this to make sure that
			// the results in the rig are the same during a recording and replay.
			Frame.RestoreVariables(InControlRig);
			
			InputFrames.Add(Frame);
		}
	);

	PostForwardHandle = InControlRig->OnPostForwardsSolve_AnyThread().AddLambda(
		[this](UControlRig* InControlRig, const FName& InEventName)
		{
			FControlRigTestDataFrame Frame;
			Frame.Store(InControlRig);
			OutputFrames.Add(Frame);
			LastFrameIndex = INDEX_NONE;
			(void)MarkPackageDirty();

			const double TimeNow = FPlatformTime::Seconds();
			const double TimeDelta = TimeNow - TimeAtStartOfRecording;
			if(DesiredRecordingDuration <= TimeDelta)
			{
				DesiredRecordingDuration = 0.0;

				// Once clear delegates is called, we no longer have access to this pointer
				ClearDelegates(InControlRig);
			}
		}
	);

	return true;
}

bool UControlRigTestData::SetupReplay(UControlRig* InControlRig, bool bGroundTruth)
{
	ReleaseReplay();
	ClearDelegates(InControlRig);

	if(InControlRig == nullptr)
	{
		return false;
	}
	
	bIsApplyingOutputs = bGroundTruth;

	if(InputFrames.IsEmpty() || OutputFrames.IsEmpty())
	{
		return false;
	}

	// reset the control rig's absolute time
	InControlRig->SetAbsoluteAndDeltaTime(InputFrames[0].AbsoluteTime, InputFrames[0].DeltaTime);
	
	InControlRig->RequestInit();
	PreConstructionHandle = InControlRig->OnPreConstruction_AnyThread().AddLambda(
		[this](UControlRig* InControlRig, const FName& InEventName)
		{
			Initial.Restore(InControlRig, true);
		}
	);

	PreForwardHandle = InControlRig->OnPreForwardsSolve_AnyThread().AddLambda(
		[this](UControlRig* InControlRig, const FName& InEventName)
		{
			// loop the animation data
			if(InControlRig->GetAbsoluteTime() < GetTimeRange().X - SMALL_NUMBER ||
				InControlRig->GetAbsoluteTime() > GetTimeRange().Y + SMALL_NUMBER)
			{
				InControlRig->SetAbsoluteAndDeltaTime(GetTimeRange().X, InControlRig->GetDeltaTime());
			}
			
			const int32 FrameIndex = GetFrameIndexForTime(InControlRig->GetAbsoluteTime(), true);
			const FControlRigTestDataFrame& Frame = InputFrames[FrameIndex];
			Frame.Restore(InControlRig, false);

			if(Frame.DeltaTime > SMALL_NUMBER)
			{
				InControlRig->SetDeltaTime(Frame.DeltaTime);
			}
		}
	);

	PostForwardHandle = InControlRig->OnPostForwardsSolve_AnyThread().AddLambda(
		[this](UControlRig* InControlRig, const FName& InEventName)
		{
			const FRigPose CurrentPose = InControlRig->GetHierarchy()->GetPose();

			const FControlRigTestDataFrame& Frame = OutputFrames[LastFrameIndex];
			if(bIsApplyingOutputs)
			{
				Frame.Restore(InControlRig, false);
			}

			const FRigPose& ExpectedPose = Frame.Pose;

			// draw differences of the pose result of the rig onto the screen
			FRigVMDrawInterface& DrawInterface = InControlRig->GetDrawInterface();
			for(const FRigPoseElement& ExpectedPoseElement : ExpectedPose)
			{
				const int32 CurrentPoseIndex = CurrentPose.GetIndex(ExpectedPoseElement.Index.GetKey());
				if(CurrentPoseIndex != INDEX_NONE)
				{
					const FRigPoseElement& CurrentPoseElement = CurrentPose[CurrentPoseIndex];

					if(!CurrentPoseElement.LocalTransform.Equals(ExpectedPoseElement.LocalTransform, 0.001f))
					{
						DrawInterface.DrawAxes(
							FTransform::Identity,
							bIsApplyingOutputs ? CurrentPoseElement.GlobalTransform : ExpectedPoseElement.GlobalTransform,
							bIsApplyingOutputs ? FLinearColor::Red : FLinearColor::Green,
							15.0f,
							1.0f
						);
					}
				}
			}
		}
	);

	ReplayControlRig = InControlRig;
	return true;
}

void UControlRigTestData::ReleaseReplay()
{
	if(UControlRig* ControlRig = ReplayControlRig.Get())
	{
		ClearDelegates(ControlRig);
		ReplayControlRig.Reset();
	}
}

EControlRigTestDataPlaybackMode UControlRigTestData::GetPlaybackMode() const
{
	if(IsReplaying())
	{
		if(bIsApplyingOutputs)
		{
			return EControlRigTestDataPlaybackMode::GroundTruth;
		}
		return EControlRigTestDataPlaybackMode::ReplayInputs;
	}
	return EControlRigTestDataPlaybackMode::Live;
}

bool UControlRigTestData::IsReplaying() const
{
	return ReplayControlRig.IsValid();
}

void UControlRigTestData::ClearDelegates(UControlRig* InControlRig)
{
	if(InControlRig)
	{
		if(PreConstructionHandle.IsValid())
		{
			InControlRig->OnPreConstruction_AnyThread().Remove(PreConstructionHandle);
			PreConstructionHandle.Reset();
		}
		if(PreForwardHandle.IsValid())
		{
			InControlRig->OnPreForwardsSolve_AnyThread().Remove(PreForwardHandle);
			PreForwardHandle.Reset();
		}
		if(PostForwardHandle.IsValid())
		{
			InControlRig->OnPostForwardsSolve_AnyThread().Remove(PostForwardHandle);
			PostForwardHandle.Reset();
		}
	}
}
