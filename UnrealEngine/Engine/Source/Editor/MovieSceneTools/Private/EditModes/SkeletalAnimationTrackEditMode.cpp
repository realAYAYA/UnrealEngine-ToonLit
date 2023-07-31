// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalAnimationTrackEditMode.h"
#include "Toolkits/ToolkitManager.h"
#include "HitProxies.h"
#include "ISequencer.h"
#include "MovieScene.h"
#include "EditorViewportClient.h"
#include "EditorModeManager.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"

#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"

#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "SkeletalDebugRendering.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"

#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneSkeletalAnimationRootHitProxy.h"
#include "Systems/MovieSceneComponentTransformSystem.h"


FName FSkeletalAnimationTrackEditMode::ModeName("EditMode.SkeletalAnimationTrackEditMode");

#define LOCTEXT_NAMESPACE "SkeletalAnimationTrackEditMode"

//// Edit Mode Tool used by the Edit Mode

class FSkeletalAnimationTrackEditModeTool : public FModeTool
{
public:
	FSkeletalAnimationTrackEditModeTool(FSkeletalAnimationTrackEditMode* InMode) : EdMode(InMode) {}
	virtual ~FSkeletalAnimationTrackEditModeTool() {};

	virtual FString GetName() const override { return TEXT("Sequencer Edit"); }

	/**
	 * @return		true if the key was handled by this editor mode tool.
	 */
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

private:
	FSkeletalAnimationTrackEditMode* EdMode;
};

bool FSkeletalAnimationTrackEditModeTool::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Key == EKeys::LeftMouseButton)
	{
		if (Event == IE_Pressed)
		{
			int32 HitX = ViewportClient->Viewport->GetMouseX();
			int32 HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy* HitResult = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

			if (HitResult)
			{
				if (HitResult->IsA(HMovieSceneSkeletalAnimationRootHitProxy::StaticGetType()))
				{
					HMovieSceneSkeletalAnimationRootHitProxy* Proxy = (HMovieSceneSkeletalAnimationRootHitProxy*)HitResult;
					EdMode->OnKeySelected(ViewportClient->Viewport, Proxy);
				}
			}
		}
	}

	return FModeTool::InputKey(ViewportClient, Viewport, Key, Event);
}


///// EDIT MODE
FSkeletalAnimationTrackEditMode::FSkeletalAnimationTrackEditMode()
	: bIsTransacting(false)
	, bManipulatorMadeChange(false)
	, RootTransform(FTransform::Identity)
{
	FSkeletalAnimationTrackEditModeTool* EdModeTool = new FSkeletalAnimationTrackEditModeTool(this);

	Tools.Add(EdModeTool);
	SetCurrentTool(EdModeTool);
}

FSkeletalAnimationTrackEditMode::~FSkeletalAnimationTrackEditMode()
{
}

bool FSkeletalAnimationTrackEditMode::UsesToolkits() const
{
	return false;
}

void FSkeletalAnimationTrackEditMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();


}

void FSkeletalAnimationTrackEditMode::Exit()
{
	if (bIsTransacting)
	{
		if (GEditor)
		{
			GEditor->EndTransaction();
		}
		bIsTransacting = false;
	}
	FEdMode::Exit();
}

//todo remove me in UE5
void FSkeletalAnimationTrackEditMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);
}

static void DrawBones(const TArray<FBoneIndexType>& RequiredBones, const FReferenceSkeleton& RefSkeleton, const TArray<FTransform>& WorldTransforms,
	FSkeletalMeshLODRenderData* LODData,
	FPrimitiveDrawInterface* PDI, const TArray<FLinearColor>& BoneColours, float BoundRadius, float LineThickness/*=0.f*/)
{

	// we may not want to axis to be too big, so clamp at 1 % of bound
	const float MaxDrawRadius = BoundRadius * 0.01f;
	// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
	for (int32 Index = 0; Index < RequiredBones.Num(); ++Index)
	{
		const int32 BoneIndex = RequiredBones[Index];
		{

			//Check whether the bone is vertex weighted
			if (LODData)
			{
				int32 LODIndex = LODData->ActiveBoneIndices.Find(BoneIndex); //same as meshbone?? todo
				if (LODIndex == INDEX_NONE)
				{
					continue;
				}
			}

			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			FVector Start, End;
			FLinearColor LineColor = BoneColours[BoneIndex];

			if (ParentIndex > 0 && BoneIndex != 0)
			{
				Start = WorldTransforms[ParentIndex].GetLocation();
				End = WorldTransforms[BoneIndex].GetLocation();
			}
			else
			{
				//unlike other bone drawings we don't draw back to origin, too messy.
				continue;
			}

			const float BoneLength = (End - Start).Size();
			// clamp by bound, we don't want too long or big
			const float Radius = FMath::Clamp(BoneLength * 0.05f, 0.1f, MaxDrawRadius);
			SkeletalDebugRendering::DrawWireBone(PDI, Start, End, LineColor, SDPG_Foreground, Radius);
		}
	}
}

static void DrawBonesFromCompactPose(const FCompactPose& Pose, USkeletalMeshComponent* MeshComponent, FPrimitiveDrawInterface* PDI,
	const FLinearColor& DrawColour, FCompactPoseBoneIndex RootBoneIndex, FVector& LocationOfRootBone)
{
	LocationOfRootBone = FVector(TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	if (Pose.GetNumBones() > 0 && MeshComponent != nullptr)
	{

		FSkeletalMeshLODRenderData* LODData = nullptr;
		if (MeshComponent->GetSkeletalMeshAsset() && MeshComponent->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData.Num())
		{
			//Get current LOD
			const int32 LODIndex = FMath::Clamp(MeshComponent->GetPredictedLODLevel(), 0, MeshComponent->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData.Num() - 1);
			LODData = &(MeshComponent->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[LODIndex]);
		}

		TArray<FTransform> WorldTransforms;
		WorldTransforms.AddUninitialized(Pose.GetBoneContainer().GetNumBones());

		TArray<FLinearColor> BoneColours;
		BoneColours.AddUninitialized(Pose.GetBoneContainer().GetNumBones());

		// we could cache parent bones as we calculate, but right now I'm not worried about perf issue of this
		for (FCompactPoseBoneIndex BoneIndex : Pose.ForEachBoneIndex())
		{
			FMeshPoseBoneIndex MeshBoneIndex = Pose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex);

			int32 ParentIndex = Pose.GetBoneContainer().GetParentBoneIndex(MeshBoneIndex.GetInt());

			if (ParentIndex == INDEX_NONE)
			{
				WorldTransforms[MeshBoneIndex.GetInt()] = Pose[BoneIndex] * MeshComponent->GetComponentTransform();
			}
			else
			{
				WorldTransforms[MeshBoneIndex.GetInt()] = Pose[BoneIndex] * WorldTransforms[ParentIndex];
			}
			if (RootBoneIndex == BoneIndex)
			{
				LocationOfRootBone.X = WorldTransforms[MeshBoneIndex.GetInt()].GetLocation().X;
				LocationOfRootBone.Y = WorldTransforms[MeshBoneIndex.GetInt()].GetLocation().Y;
			}
			if (LocationOfRootBone.Z > WorldTransforms[MeshBoneIndex.GetInt()].GetLocation().Z)
			{
				LocationOfRootBone.Z = WorldTransforms[MeshBoneIndex.GetInt()].GetLocation().Z;
			}
			BoneColours[MeshBoneIndex.GetInt()] = DrawColour;
		}

		if (MeshComponent && MeshComponent->GetSkeletalMeshAsset())
		{
			DrawBones(Pose.GetBoneContainer().GetBoneIndicesArray(), MeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton(), WorldTransforms,
				LODData, PDI, BoneColours, MeshComponent->Bounds.SphereRadius, 1.0f);
		}

	}
}
void FSkeletalAnimationTrackEditMode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdMode::AddReferencedObjects(Collector);
}


static void RenderTrail(UObject* BoundObject, UMovieSceneSkeletalAnimationTrack* SkelAnimTrack, UE::MovieScene::FSystemInterrogator& Interrogator,  UMovieScene3DTransformTrack* TransformTrack,
	const TSharedPtr<ISequencer>& Sequencer, FPrimitiveDrawInterface* PDI)
{
	const FLinearColor RootMotionColor(0.75, 0.75, 0.75, 1.0f);
	TArray<const UObject*> Parents;
	MovieSceneToolHelpers::GetParents(Parents, BoundObject);

	TOptional<FVector> OldKeyPos_G;

	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

	FFrameTime NewKeyTime = SkelAnimTrack->RootMotionParams.StartFrame;
	Interrogator.Reset();
	Interrogator.ImportTrack(TransformTrack, UE::MovieScene::FInterrogationChannel::Default());

	for (const FTransform& Transform : SkelAnimTrack->RootMotionParams.RootTransforms)
	{
		Interrogator.AddInterrogation(NewKeyTime);
		NewKeyTime += SkelAnimTrack->RootMotionParams.FrameTick;
	}

	Interrogator.Update();

	TArray<UE::MovieScene::FIntermediate3DTransform> Transforms;
	Transforms.SetNum(SkelAnimTrack->RootMotionParams.RootTransforms.Num());
	Interrogator.QueryLocalSpaceTransforms(UE::MovieScene::FInterrogationChannel::Default(), Transforms);

	int32 Index = 0;

	for (const FTransform& Transform : SkelAnimTrack->RootMotionParams.RootTransforms)
	{
		FVector NewKeyPos = Transform.GetLocation();
		FTransform NewPosRefTM = FTransform::Identity;// todo need to handle parent transforms. @andrew.rodhamMovieSceneToolHelpers::GetRefFrameFromParents(Sequencer, Parents, NewKeyTime);

		FRotator NewTransformRot = Transforms[Index].GetRotation();
		FVector NewTransformPos = Transforms[Index].GetTranslation();
		++Index;
		FTransform TransformTrackTM(NewTransformRot, NewTransformPos);
		NewPosRefTM = NewPosRefTM * TransformTrackTM;

		FVector NewKeyPos_G = NewPosRefTM.TransformPosition(NewKeyPos);
		if (OldKeyPos_G.IsSet())
		{
			PDI->DrawLine(OldKeyPos_G.GetValue(), NewKeyPos_G, RootMotionColor, SDPG_Foreground);
		}
		OldKeyPos_G = NewKeyPos_G;
	}
}

void FSkeletalAnimationTrackEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}
	// Gather a map of object bindings to their implict selection state
	TMap<const FMovieSceneBinding*, bool> ObjectBindingNodesSelectionMap;

	const FSequencerSelection& Selection = Sequencer->GetSelection();
	for (const FMovieSceneBinding& Binding : Sequence->GetMovieScene()->GetBindings())
	{

		TArrayView<TWeakObjectPtr<UObject>> BoundObjects = Sequencer->FindObjectsInCurrentSequence(Binding.GetObjectGuid());
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if(!Track->IsEvalDisabled())
			{ 
				if (UMovieSceneSkeletalAnimationTrack* SkelAnimTrack = Cast<UMovieSceneSkeletalAnimationTrack>(Track))
				{
					for (TWeakObjectPtr<> WeakBinding : BoundObjects)
					{
						UObject* BoundObject = WeakBinding.Get();
						if (BoundObject)
						{
							//show root motions
							if (SkelAnimTrack->bShowRootMotionTrail)
							{
								//Get Transform Track if so then exit
								//TODO a bunch, what if no transform track, what if multiple what if external transform?
								for (UMovieSceneTrack* TrackTrack : Binding.GetTracks())
								{
									UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(TrackTrack);
									if (!TransformTrack)
									{
										continue;
									}
									RenderTrail(BoundObject, SkelAnimTrack, InterrogationLinker, TransformTrack, Sequencer, PDI);

								}
							}
							//show skeletons
							USkeletalMeshComponent* SkelMeshComp = MovieSceneToolHelpers::AcquireSkeletalMeshFromObject(BoundObject);
							if (SkelMeshComp && SkelMeshComp->GetAnimInstance())
							{
								const FLinearColor Colors[5] = { FLinearColor(1.0f,0.0f,1.0f,1.0f), FLinearColor(0.0f,1.0f,0.0f,1.0f), FLinearColor(0.0f, 0.0f, 1.0f, 0.0f),
									FLinearColor(0.5f, 0.5f, 0.0f, 1.0f), FLinearColor(0.0f, 0.5f, 0.5f, 1.0f) };

								int32 SectionIndex = 0;
								FQualifiedFrameTime CurrentTime = Sequencer->GetLocalTime();

								for (UMovieSceneSection* Section : Track->GetAllSections())
								{
									UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(Section);
									if (AnimSection && AnimSection->bShowSkeleton && AnimSection->IsActive())
									{
										UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSection->Params.Animation);
										if (AnimSequence)
										{
											int32 Index = AnimSection->SetBoneIndexForRootMotionCalculations(SkelAnimTrack->bBlendFirstChildOfRoot);
											FMemMark Mark(FMemStack::Get());	
											FCompactPose OutPose;
											OutPose.ResetToRefPose(SkelMeshComp->GetAnimInstance()->GetRequiredBones());

											FBlendedCurve OutCurve;
											OutCurve.InitFrom(SkelMeshComp->GetAnimInstance()->GetRequiredBones());
											UE::Anim::FStackAttributeContainer TempAttributes;
											FAnimationPoseData OutAnimationPoseData(OutPose, OutCurve, TempAttributes);

											const float Seconds = static_cast<float>(AnimSection->MapTimeToAnimation(CurrentTime.Time, CurrentTime.Rate));
											FAnimExtractContext ExtractionContext(Seconds, false);

											AnimSequence->GetAnimationPose(OutAnimationPoseData, ExtractionContext);

											FFrameTime TimeToSample = CurrentTime.Time;
											if (TimeToSample < AnimSection->GetRange().GetLowerBoundValue())
											{
												TimeToSample = AnimSection->GetRange().GetLowerBoundValue();
											}
											else if (TimeToSample > AnimSection->GetRange().GetUpperBoundValue())
											{
												TimeToSample = AnimSection->GetRange().GetUpperBoundValue();
											}
											FTransform RootMotionAtStart;
											float Weight;
											bool bAdditive;
											AnimSection->GetRootMotionTransform(TimeToSample, MovieScene->GetTickResolution(), OutAnimationPoseData,bAdditive, RootMotionAtStart, Weight);
											RootMotionAtStart = RootMotionAtStart * AnimSection->TempOffsetTransform;
											OutPose[FCompactPoseBoneIndex(Index)] = RootMotionAtStart;

											FVector RootLocation;
											DrawBonesFromCompactPose(OutPose, SkelMeshComp, PDI, Colors[SectionIndex % 5], FCompactPoseBoneIndex(Index), RootLocation);
											if (IsRootSelected(AnimSection))
											{
												RootTransform = RootMotionAtStart;
												RootTransform.SetLocation(RootLocation);
											}
											if (PDI != nullptr)
											{
												if (PDI->IsHitTesting())
												{
													PDI->SetHitProxy(new HMovieSceneSkeletalAnimationRootHitProxy(AnimSection, SkelMeshComp));
												}
												PDI->DrawPoint(RootLocation, Colors[SectionIndex % 5], 10.f, SDPG_Foreground);
												if (PDI->IsHitTesting())
												{
													PDI->SetHitProxy(nullptr);
												}

											}
											++SectionIndex;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

bool FSkeletalAnimationTrackEditMode::IsRootSelected(UMovieSceneSkeletalAnimationSection* AnimSection)
{
	for (FSelectedRootData& RootData : SelectedRootData)
	{
		if (RootData.SelectedSection.Get() == AnimSection)
		{
			return true;
		}
	}
	return false;
}

bool FSkeletalAnimationTrackEditMode::InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent)
{
	return FEdMode::InputKey(InViewportClient, InViewport, InKey, InEvent);
}

bool FSkeletalAnimationTrackEditMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (bIsTransacting)
	{
		if (bManipulatorMadeChange)
		{
			bManipulatorMadeChange = false;
			GEditor->EndTransaction();

		}
		bIsTransacting = false;
		return true;
	}

	bManipulatorMadeChange = false;

	return false;
}

bool FSkeletalAnimationTrackEditMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (!bIsTransacting)
	{
		bIsTransacting = (IsSomethingSelected());

		bManipulatorMadeChange = false;

		return bIsTransacting;
	}
	return false;
}

bool FSkeletalAnimationTrackEditMode::UsesTransformWidget() const
{
	return IsSomethingSelected();
}

bool FSkeletalAnimationTrackEditMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	return IsSomethingSelected();
}

FVector FSkeletalAnimationTrackEditMode::GetWidgetLocation() const
{
	if (IsSomethingSelected())
	{
		return RootTransform.GetLocation();
	}
	return FEdMode::GetWidgetLocation();
}


bool FSkeletalAnimationTrackEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	SelectedRootData.Empty();
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		if (HitProxy && HitProxy->IsA(HMovieSceneSkeletalAnimationRootHitProxy::StaticGetType()))
		{
			HMovieSceneSkeletalAnimationRootHitProxy* Proxy = (HMovieSceneSkeletalAnimationRootHitProxy*)HitProxy;

			if (Sequencer.IsValid())
			{
				UMovieSceneSkeletalAnimationSection* Section = Proxy->AnimSection.Get();
				USkeletalMeshComponent* Comp = Proxy->SkelMeshComp.Get();
				if (Section && Comp)
				{
					FSelectedRootData RootData(Section, Comp);
					SelectedRootData.Add(RootData);
				}
			}
		}
	}
	return false;
}

bool FSkeletalAnimationTrackEditMode::IsSomethingSelected() const
{
	return	SelectedRootData.Num() > 0;
}

void FSkeletalAnimationTrackEditMode::SelectNone()
{
	SelectedRootData.Empty();
	FEdMode::SelectNone();
}

void FSkeletalAnimationTrackEditMode::OnKeySelected(FViewport* Viewport, HMovieSceneSkeletalAnimationRootHitProxy* Proxy)
{
	if (!Proxy)
	{
		SelectedRootData.Empty();
		return;
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		UMovieSceneSkeletalAnimationSection* Section = Proxy->AnimSection.Get();
		USkeletalMeshComponent* Comp = Proxy->SkelMeshComp.Get();
		SelectedRootData.Empty();

		if (Section && Comp)
		{
			FSelectedRootData RootData(Section, Comp);
			SelectedRootData.Add(RootData);
		}
	}
}

bool FSkeletalAnimationTrackEditMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	FVector Drag = InDrag;
	FRotator Rot = InRot;

	const bool bCtrlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bShiftDown = InViewport->KeyState(EKeys::LeftShift) || InViewport->KeyState(EKeys::RightShift);
	const bool bAltDown = InViewport->KeyState(EKeys::LeftAlt) || InViewport->KeyState(EKeys::RightAlt);
	const bool bMouseButtonDown = InViewport->KeyState(EKeys::LeftMouseButton);

	const UE::Widget::EWidgetMode WidgetMode = InViewportClient->GetWidgetMode();
	const EAxisList::Type CurrentAxis = InViewportClient->GetCurrentWidgetAxis();

	if (bIsTransacting && bMouseButtonDown && !bCtrlDown && !bShiftDown && !bAltDown && CurrentAxis != EAxisList::None)
	{
		const bool bDoRotation = !Rot.IsZero() && (WidgetMode == UE::Widget::WM_Rotate || WidgetMode == UE::Widget::WM_TranslateRotateZ);
		const bool bDoTranslation = !Drag.IsZero() && (WidgetMode == UE::Widget::WM_Translate || WidgetMode == UE::Widget::WM_TranslateRotateZ);

		if (IsSomethingSelected())
		{
			for (FSelectedRootData Data : SelectedRootData)
			{
				if (bManipulatorMadeChange == false)
				{
					bManipulatorMadeChange = true;
					GEditor->BeginTransaction(LOCTEXT("MoveRootControlTransaction", "Move Root Control"));
				}
				if (Data.SelectedSection.IsValid() && Data.SelectedMeshComp.IsValid())
				{
					FTransform CompTransform = Data.SelectedMeshComp->GetComponentTransform();
					if (bDoRotation)
					{
						UMovieSceneSkeletalAnimationTrack* Track = Data.SelectedSection.Get()->GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
						Track->Modify();
						Data.SelectedSection.Get()->Modify();
						Data.SelectedSection.Get()->StartRotationOffset = (Rot)+Data.SelectedSection.Get()->StartRotationOffset;
						Data.SelectedSection.Get()->GetRootMotionParams()->bRootMotionsDirty = true;
						TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
						if (Sequencer.IsValid())
						{
							Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
						}
					}
					if (bDoTranslation)
					{
						UMovieSceneSkeletalAnimationTrack* Track = Data.SelectedSection.Get()->GetTypedOuter<UMovieSceneSkeletalAnimationTrack>();
						Track->Modify();
						Data.SelectedSection.Get()->Modify();
						Drag = CompTransform.GetRotation().Inverse().RotateVector(Drag);
						Data.SelectedSection.Get()->StartLocationOffset = (Drag)+Data.SelectedSection.Get()->StartLocationOffset;
						Data.SelectedSection.Get()->GetRootMotionParams()->bRootMotionsDirty = true;
						TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
						if (Sequencer.IsValid())
						{
							Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
						}
					}
				}
			}
			return true;
		}
	}
	return false;
}

bool FSkeletalAnimationTrackEditMode::ShouldDrawWidget() const
{
	if (IsSomethingSelected())
	{
		return true;
	}

	return FEdMode::ShouldDrawWidget();
}

bool FSkeletalAnimationTrackEditMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	return true;
}




#undef LOCTEXT_NAMESPACE
