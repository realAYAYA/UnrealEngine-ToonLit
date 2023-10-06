// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerEditorModel.h"
#include "IDetailsView.h"
#include "MLDeformerModule.h"
#include "MLDeformerModel.h"
#include "MLDeformerEditorStyle.h"
#include "MLDeformerEditorModule.h"
#include "MLDeformerModelRegistry.h"
#include "MLDeformerEditorToolkit.h"
#include "MLDeformerEditorActor.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerSampler.h"
#include "MLDeformerModelInstance.h"
#include "AnimationEditorPreviewActor.h"
#include "AnimationEditorViewportClient.h"
#include "AnimPreviewInstance.h"
#include "Animation/MeshDeformer.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "IPersonaPreviewScene.h"
#include "IPersonaToolkit.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EditorViewportClient.h"
#include "Components/TextRenderComponent.h"
#include "Materials/Material.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheTrack.h"
#include "BoneContainer.h"
#include "Misc/ScopedSlowTask.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Engine/SkinnedAssetCommon.h"
#include "SMLDeformerInputWidget.h"

#define LOCTEXT_NAMESPACE "MLDeformerEditorModel"

namespace UE::MLDeformer
{
	FMLDeformerEditorModel::~FMLDeformerEditorModel()
	{
		if (InputObjectModifiedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectModified.Remove(InputObjectModifiedHandle);
		}

		if (InputObjectPropertyChangedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(InputObjectPropertyChangedHandle);
		}

		if (PostEditPropertyDelegateHandle.IsValid())
		{
			Model->OnPostEditChangeProperty().Remove(PostEditPropertyDelegateHandle);
		}

		ClearWorld();

		FMLDeformerEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FMLDeformerEditorModule>("MLDeformerFrameworkEditor");
		EditorModule.GetModelRegistry().RemoveEditorModelInstance(this);
	}

	void FMLDeformerEditorModel::Init(const InitSettings& Settings)
	{
		check(Settings.Editor);
		check(Settings.Model);

		Editor = Settings.Editor;
		Model = Settings.Model;

		EditorInputInfo = Model->CreateInputInfo();
		check(EditorInputInfo);

		Sampler = CreateSampler();
		check(Sampler);
		Sampler->Init(this);

		ViewRange = TRange<double>(0.0, 100.0);
		WorkingRange = TRange<double>(0.0, 100.0);
		PlaybackRange = TRange<double>(0.0, 100.0);

		// Start listening to property changes in the ML Deformer model.
		PostEditPropertyDelegateHandle = Model->OnPostEditChangeProperty().AddRaw(this, &FMLDeformerEditorModel::OnPostEditChangeProperty);

		// Start listening for object modified and object property changed events.
		// We use this to trigger reinits of the UI and components, when one of our input assets got modified (by property changes, or reimports, etc).
		// This listens to changes in ALL objects in the engine.
		InputObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FMLDeformerEditorModel::OnObjectModified);
		InputObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda
		(
			[this](UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
			{
				if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
				{
					OnObjectModified(Object);
				}
			}
		);
	}

	void FMLDeformerEditorModel::OnObjectModified(UObject* Object)
	{
		if (Model->GetSkeletalMesh() == Object || Model->GetInputInfo()->GetSkeletalMesh() == Object)
		{
			UE_LOG(LogMLDeformer, Display, TEXT("Detected a modification in skeletal mesh %s, reinitializing inputs."), *Object->GetName());
			bNeedsAssetReinit = true;
		}

		if (Model->GetAnimSequence() == Object)
		{
			UE_LOG(LogMLDeformer, Display, TEXT("Detected a modification in training anim sequence %s, reinitializing inputs."), *Object->GetName());
			bNeedsAssetReinit = true;
		}

		if (Model->GetVizSettings()->GetTestAnimSequence() == Object)
		{
			UE_LOG(LogMLDeformer, Display, TEXT("Detected a modification in test anim sequence %s, reinitializing inputs."), *Object->GetName());
			bNeedsAssetReinit = true;
		}
	}

	void FMLDeformerEditorModel::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(EditorInputInfo);
	}

	void FMLDeformerEditorModel::UpdateEditorInputInfo()
	{
		InitInputInfo(EditorInputInfo);
	}

	UWorld* FMLDeformerEditorModel::GetWorld() const
	{
		check(Editor);
		return Editor->GetPersonaToolkit()->GetPreviewScene()->GetWorld();
	}

	FMLDeformerSampler* FMLDeformerEditorModel::CreateSampler() const
	{
		return new FMLDeformerSampler();
	}

	void FMLDeformerEditorModel::CreateTrainingLinearSkinnedActor(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
	{
		UWorld* World = InPersonaPreviewScene->GetWorld();

		// Spawn the linear skinned actor.
		FActorSpawnParameters BaseSpawnParams;
		BaseSpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), "Train Base Actor");
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), BaseSpawnParams);
		Actor->SetFlags(RF_Transient);

		// Create the preview skeletal mesh component.
		const FLinearColor BaseWireColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.BaseMesh.WireframeColor");
		UDebugSkelMeshComponent* SkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
		SkelMeshComponent->SetWireframeMeshOverlayColor(BaseWireColor);
		SkelMeshComponent->SetVisibility(false);
		SkelMeshComponent->MarkRenderStateDirty();

		// Set the skeletal mesh on the component.
		// NOTE: This must be done AFTER setting the AnimInstance so that the correct root anim node is loaded.
		USkeletalMesh* Mesh = Model->GetSkeletalMesh();
		SkelMeshComponent->SetSkeletalMesh(Mesh);

		// Update the persona scene.
		InPersonaPreviewScene->SetActor(Actor);
		InPersonaPreviewScene->SetPreviewMeshComponent(SkelMeshComponent);
		InPersonaPreviewScene->AddComponent(SkelMeshComponent, FTransform::Identity);
		InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);
		InPersonaPreviewScene->SetPreviewMesh(Mesh);
		if (SkelMeshComponent->GetAnimInstance())
		{
			SkelMeshComponent->GetAnimInstance()->GetRequiredBones().SetUseRAWData(true);
		}

		// Register the editor actor.
		const FLinearColor LabelColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.BaseMesh.LabelColor");
		FMLDeformerEditorActor::FConstructSettings Settings;
		Settings.Actor = Actor;
		Settings.TypeID = ActorID_Train_Base;
		Settings.LabelColor = LabelColor;
		Settings.LabelText = LOCTEXT("TrainBaseActorLabelText", "Training Base");
		Settings.bIsTrainingActor = true;
		FMLDeformerEditorActor* EditorActor = CreateEditorActor(Settings);
		EditorActor->SetSkeletalMeshComponent(SkelMeshComponent);
		EditorActor->SetMeshOffsetFactor(0.0f);
		EditorActors.Add(EditorActor);
	}

	void FMLDeformerEditorModel::CreateTestLinearSkinnedActor(UWorld* World)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), "Test Linear Skinned Actor");
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		Actor->SetFlags(RF_Transient);

		const FLinearColor BaseWireColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.BaseMesh.WireframeColor");
		UDebugSkelMeshComponent* SkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
		SkelMeshComponent->SetWireframeMeshOverlayColor(BaseWireColor);
		SkelMeshComponent->SetSkeletalMesh(Model->GetSkeletalMesh());
		Actor->SetRootComponent(SkelMeshComponent);
		SkelMeshComponent->RegisterComponent();
		SkelMeshComponent->SetVisibility(false);
		SkelMeshComponent->MarkRenderStateDirty();

		// Register the editor actor.
		const FLinearColor LabelColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.BaseMesh.LabelColor");
		FMLDeformerEditorActor::FConstructSettings Settings;
		Settings.Actor = Actor;
		Settings.TypeID = ActorID_Test_Base;
		Settings.LabelColor = LabelColor;
		Settings.LabelText = LOCTEXT("TestBaseActorLabelText", "Linear Skinned");
		Settings.bIsTrainingActor = false;
		FMLDeformerEditorActor* EditorActor = CreateEditorActor(Settings);
		EditorActor->SetSkeletalMeshComponent(SkelMeshComponent);
		EditorActor->SetMeshOffsetFactor(0.0f);
		EditorActors.Add(EditorActor);
	}

	void FMLDeformerEditorModel::CreateTestMLDeformedActor(UWorld* World)
	{
		// Create the ML deformed actor.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), "Test ML Deformed");
		AActor* Actor = World->SpawnActor<AActor>(SpawnParams);
		Actor->SetFlags(RF_Transient);

		// Create the skeletal mesh component.
		const FLinearColor MLDeformedWireColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.MLDeformedMesh.WireframeColor");
		UDebugSkelMeshComponent* SkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
		SkelMeshComponent->SetSkeletalMesh(Model->GetSkeletalMesh());
		Actor->SetRootComponent(SkelMeshComponent);
		SkelMeshComponent->RegisterComponent();
		SkelMeshComponent->SetWireframeMeshOverlayColor(MLDeformedWireColor);
		SkelMeshComponent->SetVisibility(false);
		SkelMeshComponent->MarkRenderStateDirty();

		// Create the ML Deformer component.
		UMLDeformerAsset* DeformerAsset = Model->GetDeformerAsset();
		UMLDeformerComponent* MLDeformerComponent = NewObject<UMLDeformerComponent>(Actor);
		MLDeformerComponent->SetSuppressMeshDeformerLogWarnings(true);
		MLDeformerComponent->SetDeformerAsset(DeformerAsset);
		MLDeformerComponent->RegisterComponent();
		MLDeformerComponent->SetupComponent(DeformerAsset, SkelMeshComponent);

		// Create the editor actor.
		const FLinearColor LabelColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.MLDeformedMesh.LabelColor");
		FMLDeformerEditorActor::FConstructSettings Settings;
		Settings.Actor = Actor;
		Settings.TypeID = ActorID_Test_MLDeformed;
		Settings.LabelColor = LabelColor;
		Settings.LabelText = LOCTEXT("TestMLDeformedActorLabelText", "ML Deformed");
		Settings.bIsTrainingActor = false;
		FMLDeformerEditorActor* EditorActor = static_cast<FMLDeformerEditorActor*>(CreateEditorActor(Settings));
		EditorActor->SetSkeletalMeshComponent(SkelMeshComponent);
		EditorActor->SetMLDeformerComponent(MLDeformerComponent);
		EditorActor->SetMeshOffsetFactor(1.0f);
		EditorActors.Add(EditorActor);
	}

	void FMLDeformerEditorModel::CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
	{
		UWorld* World = InPersonaPreviewScene->GetWorld();
		CreateTrainingLinearSkinnedActor(InPersonaPreviewScene);
		CreateTestLinearSkinnedActor(World);
		CreateTestMLDeformedActor(World);
		CreateTrainingGroundTruthActor(World);
		CreateTestGroundTruthActor(World);

		// Set the default mesh translation offsets for our ground truth actors.
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor && EditorActor->IsGroundTruthActor())
			{			
				// The mesh offset factor basically just offsets the actor position by a given factor.
				// The amount the actor is moved from the origin is: (MeshSpacing * MeshOffsetFactor).
				// In test mode we have 3 actors (Linear, ML Deformed, Ground Truth), so it's mesh offset factor will be 2.0 for the ground truth.
				// It is 2.0 because the ground truth actor in testing mode is all the way on the right, next to the ML Deformed model.
				// In training mode we have only the Linear Skinned actor and the ground truth, so there the spacing factor is 1.0.
				// TLDR: Basically 1.0 means its the first actor next to the linear skinned actor while 2.0 means its the second character, etc.
				EditorActor->SetMeshOffsetFactor(EditorActor->IsTestActor() ? 2.0f : 1.0f);
			}
		}
	}

	void FMLDeformerEditorModel::ClearWorld()
	{
		// First remove actors from the world and add them to the destroy list.
		TArray<AActor*> ActorsToDestroy;
		ActorsToDestroy.Reserve(EditorActors.Num());
		TSharedRef<IPersonaPreviewScene> PreviewScene = Editor->GetPersonaToolkit()->GetPreviewScene();
		UWorld* World = PreviewScene->GetWorld();
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor)
			{				
				World->RemoveActor(EditorActor->GetActor(), true);
				ActorsToDestroy.Add(EditorActor->GetActor());
			}
		}

		// Now delete the editor actors, which removes components.
		DeleteEditorActors();

		for (AActor* Actor : ActorsToDestroy)
		{
			Actor->Destroy();
		}
	}

	void FMLDeformerEditorModel::ClearPersonaPreviewScene()
	{
		TSharedRef<IPersonaPreviewScene> PreviewScene = Editor->GetPersonaToolkit()->GetPreviewScene();
		UWorld* World = PreviewScene->GetWorld();

		PreviewScene->DeselectAll();
		PreviewScene->SetPreviewAnimationAsset(nullptr);
		PreviewScene->SetPreviewMeshComponent(nullptr);
		PreviewScene->SetActor(nullptr);
	}

	void FMLDeformerEditorModel::ClearWorldAndPersonaPreviewScene()
	{
		ClearWorld();
		ClearPersonaPreviewScene();
	}

	FMLDeformerEditorActor* FMLDeformerEditorModel::CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const
	{ 
		return new FMLDeformerEditorActor(Settings);
	}

	void FMLDeformerEditorModel::DeleteEditorActors()
	{
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			delete EditorActor;
		}
		EditorActors.Empty();
	}

	FMLDeformerEditorActor* FMLDeformerEditorModel::FindEditorActor(int32 ActorTypeID) const
	{
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor && EditorActor->GetTypeID() == ActorTypeID)
			{
				return EditorActor;
			}
		}

		return nullptr;
	}

	void FMLDeformerEditorModel::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
	{		
		if (bNeedsAssetReinit)
		{
			TriggerInputAssetChanged();
			Model->InitVertexMap();
			Model->InitGPUData();
			bNeedsAssetReinit = false;
		}

		// Force the training sequence to use Step interpolation.
		// We do this in the Tick, as this is reset to false in the engine sometimes.
		UAnimSequence* TrainingAnimSequence = Model->GetAnimSequence();
		if (TrainingAnimSequence)
		{
			TrainingAnimSequence->Interpolation = EAnimInterpolationType::Step;
		}

		// Use raw data for everything.
		for (FMLDeformerEditorActor* Actor : EditorActors)
		{
			if (Actor && Actor->GetSkeletalMeshComponent())
			{
				USkeletalMeshComponent* SkelMeshComp = Actor->GetSkeletalMeshComponent();			
				if (SkelMeshComp->GetAnimInstance())
				{
					SkelMeshComp->GetAnimInstance()->GetRequiredBones().SetUseRAWData(true);
				}
			}
		}

		// Do the same for the test anim sequence.
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		{
			UAnimSequence* TestAnimSequence = VizSettings->GetTestAnimSequence();
			if (TestAnimSequence)
			{			
				// Enable step interpolation when doing showing a heatmap vs ground truth.
				if (VizSettings->HasTestGroundTruth() && VizSettings->GetShowHeatMap() && VizSettings->GetHeatMapMode() == EMLDeformerHeatMapMode::GroundTruth)
				{
					TestAnimSequence->Interpolation = EAnimInterpolationType::Step;
				}
				else
				{
					TestAnimSequence->Interpolation = EAnimInterpolationType::Linear;
				}
			}
		}

		UpdateActorTransforms();
		UpdateLabels();
		CheckTrainingDataFrameChanged();

		// Update the ML Deformer component of the ML Deformed test actor.
		FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID_Test_MLDeformed);
		if (EditorActor)
		{
			UMLDeformerComponent* DeformerComponent = EditorActor->GetMLDeformerComponent();
			if (DeformerComponent)
			{		
				DeformerComponent->SetWeight(VizSettings->GetWeight());
				DeformerComponent->SetQualityLevel(VizSettings->GetQualityLevel());
			}
		}
	}

	void FMLDeformerEditorModel::UpdateLabels()
	{
		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		const bool bDrawTrainingActors = (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData);
		const bool bDrawTestActors = (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData);

		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor == nullptr)
			{
				continue;
			}

			UTextRenderComponent* LabelComponent = EditorActor->GetLabelComponent();
			if (LabelComponent == nullptr)
			{
				continue;
			}

			if (VizSettings->GetDrawLabels())
			{
				const AActor* Actor = EditorActor->GetActor();
				const FVector ActorLocation = Actor->GetActorLocation();
				const FVector AlignmentOffset = EditorActor->IsGroundTruthActor() ? Model->GetAlignmentTransform().GetTranslation() : FVector::ZeroVector;

				LabelComponent->SetRelativeLocation(ActorLocation + FVector(0.0f, 0.0f, VizSettings->GetLabelHeight()) - AlignmentOffset);
				LabelComponent->SetRelativeRotation(FQuat(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(90.0f)));
				LabelComponent->SetRelativeScale3D(FVector(VizSettings->GetLabelScale() * 0.3f));

				// Update visibility.
				const bool bLabelIsVisible = (bDrawTrainingActors && EditorActor->IsTrainingActor()) || (bDrawTestActors && EditorActor->IsTestActor());
				LabelComponent->SetVisibility(bLabelIsVisible);

				// Handle test ground truth, disable its label when no ground truth asset was selected.
				if (EditorActor->GetTypeID() == ActorID_Test_GroundTruth && !VizSettings->HasTestGroundTruth())
				{
					LabelComponent->SetVisibility(false);
				}
			}
			else
			{
				LabelComponent->SetVisibility(false);
			}
		}
	}

	void FMLDeformerEditorModel::UpdateActorTransforms()
	{
		const FVector MeshSpacingVector = Model->GetVizSettings()->GetMeshSpacingOffsetVector();
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor)
			{
				FTransform Transform = EditorActor->IsGroundTruthActor() ? Model->GetAlignmentTransform() : FTransform::Identity;
				Transform.AddToTranslation(MeshSpacingVector * EditorActor->GetMeshOffsetFactor());
				EditorActor->GetActor()->SetActorTransform(Transform);
			}
		}
	}

	void FMLDeformerEditorModel::UpdateActorVisibility()
	{
		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		const bool bShowTrainingData = (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData);
		const bool bShowTestData = (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData);
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor)
			{
				bool bIsVisible = (EditorActor->IsTestActor() && bShowTestData) || (EditorActor->IsTrainingActor() && bShowTrainingData);
				if (EditorActor->GetTypeID() == ActorID_Test_Base)
				{
					bIsVisible &= VizSettings->GetDrawLinearSkinnedActor();
				}
				else if (EditorActor->GetTypeID() == ActorID_Test_MLDeformed)
				{
					bIsVisible &= VizSettings->GetDrawMLDeformedActor();
				}
				else if (EditorActor->GetTypeID() == ActorID_Test_GroundTruth)
				{
					bIsVisible &= VizSettings->GetDrawGroundTruthActor();
				}
				EditorActor->SetVisibility(bIsVisible);
			}
		}
	}

	void FMLDeformerEditorModel::ChangeSkeletalMeshOnComponent(USkeletalMeshComponent* SkelMeshComponent, USkeletalMesh* Mesh)
	{
		if (Mesh != SkelMeshComponent->GetSkeletalMeshAsset())
		{
			// Inform the skeletal mesh component about changes to the mesh.
			SkelMeshComponent->SetSkeletalMesh(Mesh);
			FPropertyChangedEvent SkelMeshChangedEvent(USkeletalMeshComponent::StaticClass()->FindPropertyByName(TEXT("SkeletalMeshAsset")));
			SkelMeshComponent->PostEditChangeProperty(SkelMeshChangedEvent);

			if (Mesh)
			{
				// Force all materials in the Skeletal Mesh Component to be the ones coming from the Mesh to avoid issues with material slots not updating.
				const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
				for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
				{
					SkelMeshComponent->SetMaterial(MaterialIndex, Materials[MaterialIndex].MaterialInterface);
				}
				SkelMeshComponent->MarkRenderStateDirty();
			}
		}
	}

	void FMLDeformerEditorModel::OnInputAssetsChanged()
	{
		// Force the training sequence to use Step interpolation and sample raw animation data.
		UAnimSequence* TrainingAnimSequence = Model->GetAnimSequence();
		if (TrainingAnimSequence)
		{
			TrainingAnimSequence->Interpolation = EAnimInterpolationType::Step;
		}

		// Update the training base actor.
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		UAnimSequence* TestAnimSequence = VizSettings->GetTestAnimSequence();
		const float TestAnimSpeed = VizSettings->GetAnimPlaySpeed();

		const FMLDeformerEditorActor* TrainBaseActor = FindEditorActor(ActorID_Train_Base);
		UDebugSkelMeshComponent* SkeletalMeshComponent = TrainBaseActor ? TrainBaseActor->GetSkeletalMeshComponent() : nullptr;
		if (SkeletalMeshComponent)
		{
			ChangeSkeletalMeshOnComponent(SkeletalMeshComponent, Model->GetSkeletalMesh());
			if (GetEditor()->GetPersonaToolkitPointer())
			{
				GetEditor()->GetPersonaToolkit()->GetPreviewScene()->SetPreviewMesh(Model->GetSkeletalMesh());
			}
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			const float CurrentPlayTime = SkeletalMeshComponent->GetPosition();
			SkeletalMeshComponent->SetAnimation(TrainingAnimSequence);
			SkeletalMeshComponent->SetPosition(CurrentPlayTime);
			SkeletalMeshComponent->SetPlayRate(TestAnimSpeed);
			SkeletalMeshComponent->Play(true);
			if (SkeletalMeshComponent->GetAnimInstance())
			{
				SkeletalMeshComponent->GetAnimInstance()->GetRequiredBones().SetUseRAWData(true);
			}
		}

		// Update the test base model.
		const FMLDeformerEditorActor* TestBaseActor = FindEditorActor(ActorID_Test_Base);
		SkeletalMeshComponent = TestBaseActor ? TestBaseActor->GetSkeletalMeshComponent() : nullptr;
		if (SkeletalMeshComponent)
		{
			ChangeSkeletalMeshOnComponent(SkeletalMeshComponent, Model->GetSkeletalMesh());
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			const float CurrentPlayTime = SkeletalMeshComponent->GetPosition();
			SkeletalMeshComponent->SetAnimation(TestAnimSequence);
			SkeletalMeshComponent->SetPosition(CurrentPlayTime);
			SkeletalMeshComponent->SetPlayRate(TestAnimSpeed);
			SkeletalMeshComponent->Play(true);
		}

		// Update the test ML Deformed skeletal mesh component.
		const FMLDeformerEditorActor* TestMLActor = FindEditorActor(ActorID_Test_MLDeformed);
		SkeletalMeshComponent = TestMLActor ? TestMLActor->GetSkeletalMeshComponent() : nullptr;
		if (SkeletalMeshComponent)
		{
			ChangeSkeletalMeshOnComponent(SkeletalMeshComponent, Model->GetSkeletalMesh());
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			const float CurrentPlayTime = SkeletalMeshComponent->GetPosition();
			SkeletalMeshComponent->SetAnimation(TestAnimSequence);
			SkeletalMeshComponent->SetPosition(CurrentPlayTime);
			SkeletalMeshComponent->SetPlayRate(TestAnimSpeed);
			SkeletalMeshComponent->Play(true);
		}
	}

	void FMLDeformerEditorModel::OnPostInputAssetChanged()
	{
		CurrentTrainingFrame = -1;
		Editor->UpdateTimeSliderRange();
		Model->UpdateCachedNumVertices();
		UpdateDeformerGraph();
		RefreshMLDeformerComponents();
		UpdateIsReadyForTrainingState();

		const int32 TrainingFrame = Model->GetVizSettings()->GetTrainingFrameNumber();
		SetTrainingFrame(TrainingFrame);

		const int32 TestFrame = Model->GetVizSettings()->GetTestingFrameNumber();
		SetTestFrame(TestFrame);

		UpdateEditorInputInfo();
		CheckTrainingDataFrameChanged();
	}

	void FMLDeformerEditorModel::OnTimeSliderScrubPositionChanged(double NewScrubTime, bool bIsScrubbing)
	{
		float PlayOffset = static_cast<float>(NewScrubTime);

		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			const int32 TargetFrame = GetTrainingFrameAtTime(NewScrubTime);
			for (FMLDeformerEditorActor* EditorActor : EditorActors)
			{
				if (EditorActor && EditorActor->IsTrainingActor())
				{
					PlayOffset = GetTrainingTimeAtFrame(TargetFrame);
					EditorActor->SetPlayPosition(PlayOffset);
				}
			}
			VizSettings->SetTrainingFrameNumber(TargetFrame);
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			const int32 TargetFrame = GetTestFrameAtTime(NewScrubTime);
			for (FMLDeformerEditorActor* EditorActor : EditorActors)
			{
				if (EditorActor && EditorActor->IsTestActor())
				{
					if (Model->GetVizSettings()->HasTestGroundTruth())
					{
						PlayOffset = GetTestTimeAtFrame(TargetFrame);
					}
					EditorActor->SetPlayPosition(PlayOffset);
				}
			}
			VizSettings->SetTestingFrameNumber(TargetFrame);
		}
	}

	float FMLDeformerEditorModel::CorrectedFrameTime(int32 FrameNumber, float FrameTimeForFrameNumber, FFrameRate FrameRate)
	{
		const FFrameTime PlayOffsetCurrentTime(FrameRate.AsFrameTime(FrameTimeForFrameNumber));

		if (PlayOffsetCurrentTime.FloorToFrame() != FrameNumber)
		{
			// correct the float for error
			const double CorrectedPlayOffset = FrameRate.AsInterval() * (double)(FrameNumber);
			float FloatPlayOffset = CorrectedPlayOffset;
			FloatPlayOffset = nextafterf(FloatPlayOffset, FloatPlayOffset + 1.0f);
			return FloatPlayOffset;
		}
		return FrameTimeForFrameNumber;
	}

	double FMLDeformerEditorModel::GetTrainingTimeAtFrame(int32 FrameNumber) const
	{
		UAnimSequence* AnimSequence = Model->GetAnimSequence();
		if (AnimSequence)
		{
			const FFrameRate FrameRate = AnimSequence->GetSamplingFrameRate();
			const float UncorrectedTime = AnimSequence->GetTimeAtFrame(FrameNumber);
			// due to floating point errors, return a double that when converted to a float cleanly maps to the value
			return CorrectedFrameTime(FrameNumber, UncorrectedTime, FrameRate);
		}
		return 0.0; 
	}

	int32 FMLDeformerEditorModel::GetTrainingFrameAtTime(double TimeInSeconds) const
	{
		return Model->GetAnimSequence() ? Model->GetAnimSequence()->GetFrameAtTime(TimeInSeconds) : 0;
	}

	double FMLDeformerEditorModel::GetTestTimeAtFrame(int32 FrameNumber) const
	{
		return Model->GetVizSettings()->GetTestAnimSequence() ? Model->GetVizSettings()->GetTestAnimSequence()->GetTimeAtFrame(FrameNumber) : 0.0;
	}

	int32 FMLDeformerEditorModel::GetTestFrameAtTime(double TimeInSeconds) const
	{
		return Model->GetVizSettings()->GetTestAnimSequence() ? Model->GetVizSettings()->GetTestAnimSequence()->GetFrameAtTime(TimeInSeconds) : 0;
	}

	void FMLDeformerEditorModel::SetTrainingFrame(int32 FrameNumber)
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		VizSettings->SetTrainingFrameNumber(FrameNumber);
		ClampCurrentTrainingFrameIndex();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			OnTimeSliderScrubPositionChanged(GetTrainingTimeAtFrame(FrameNumber), false);
		}
	}

	void FMLDeformerEditorModel::SetTestFrame(int32 FrameNumber)
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		VizSettings->SetTestingFrameNumber(FrameNumber);
		ClampCurrentTestFrameIndex();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			OnTimeSliderScrubPositionChanged(GetTestTimeAtFrame(FrameNumber), false);
		}
	}

	void FMLDeformerEditorModel::OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		// When we change one of these properties below, restart animations etc.
		if (Property->GetFName() == UMLDeformerModel::GetSkeletalMeshPropertyName())
		{
			TriggerInputAssetChanged();
			SetResamplingInputOutputsNeeded(true);
			Model->InitVertexMap();
			Model->InitGPUData();
			UpdateDeformerGraph();
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetTestAnimSequencePropertyName())
		{
			TriggerInputAssetChanged(true);
		}
		else if (Property->GetFName() == UMLDeformerModel::GetAnimSequencePropertyName())
		{
			TriggerInputAssetChanged();
			SetResamplingInputOutputsNeeded(true);
		}
		if (Property->GetFName() == UMLDeformerModel::GetAlignmentTransformPropertyName() ||
		    Property->GetFName() == UMLDeformerModel::GetDeltaCutoffLengthPropertyName())
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
				SetResamplingInputOutputsNeeded(true);
				SampleDeltas();
			}
		}
		else if (Property->GetFName() == UMLDeformerModel::GetMaxTrainingFramesPropertyName())
		{
			TriggerInputAssetChanged();
			SetResamplingInputOutputsNeeded(true);
		}
		else
		if (Property->GetFName() == UMLDeformerModel::GetBoneIncludeListPropertyName() ||
			Property->GetFName() == UMLDeformerModel::GetCurveIncludeListPropertyName())
		{
			SetResamplingInputOutputsNeeded(true);
			UpdateIsReadyForTrainingState();
			RefreshInputWidget();
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetAnimPlaySpeedPropertyName())
		{
			UpdateTestAnimPlaySpeed();
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetTrainingFrameNumberPropertyName())
		{
			ClampCurrentTrainingFrameIndex();
			const int32 CurrentFrameNumber = Model->GetVizSettings()->GetTrainingFrameNumber();
			OnTimeSliderScrubPositionChanged(GetTrainingTimeAtFrame(CurrentFrameNumber), false);
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetTestingFrameNumberPropertyName())
		{
			ClampCurrentTestFrameIndex();
			const int32 CurrentFrameNumber = Model->GetVizSettings()->GetTestingFrameNumber();
			OnTimeSliderScrubPositionChanged(GetTestTimeAtFrame(CurrentFrameNumber), false);
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetShowHeatMapPropertyName())
		{
			SetHeatMapMaterialEnabled(Model->GetVizSettings()->GetShowHeatMap());
			UpdateDeformerGraph();
		}
		else
		if (Property->GetFName() == UMLDeformerVizSettings::GetDrawLinearSkinnedActorPropertyName() ||
			Property->GetFName() == UMLDeformerVizSettings::GetDrawMLDeformedActorPropertyName() ||
			Property->GetFName() == UMLDeformerVizSettings::GetDrawGroundTruthActorPropertyName())
		{
			UpdateActorVisibility();
		}
		else if (Property->GetFName() == UMLDeformerVizSettings::GetDrawVertexDeltasPropertyName())
		{
			SampleDeltas();
		} 
		else if (Property->GetFName() == UMLDeformerVizSettings::GetDeformerGraphPropertyName())
		{
			UpdateDeformerGraph();
			GetEditor()->GetVizSettingsDetailsView()->ForceRefresh();
		};
	}

	FMLDeformerEditorActor* FMLDeformerEditorModel::GetVisualizationModeBaseActor() const
	{
		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		const bool bTestMode = VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData;
		return bTestMode ? FindEditorActor(ActorID_Test_Base) : FindEditorActor(ActorID_Train_Base);
	}

	void FMLDeformerEditorModel::OnPlayPressed()
	{
		// The play button shouldn't do anything in training mode.
		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		const FMLDeformerEditorActor* BaseActor = GetVisualizationModeBaseActor();
		const bool bMustPause = (BaseActor && BaseActor->IsPlaying());
		const bool bTestMode = (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData);

		// Update the current frame numbers.
		if (bMustPause)
		{
			if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
			{
				SetTrainingFrame(GetTrainingFrameAtTime(CalcTimelinePosition()));
			}
			else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
			{
				SetTestFrame(GetTestFrameAtTime(CalcTimelinePosition()));
			}
		}

		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor && ((EditorActor->IsTestActor() && bTestMode) || (EditorActor->IsTrainingActor() && !bTestMode)))
			{
				EditorActor->SetPlaySpeed(VizSettings->GetAnimPlaySpeed());
				EditorActor->Pause(bMustPause);
			}
		}
	}

	bool FMLDeformerEditorModel::IsPlayingAnim() const
	{
		const FMLDeformerEditorActor* BaseActor = GetVisualizationModeBaseActor();
		if (BaseActor)
		{
			UDebugSkelMeshComponent* SkeletalMeshComponent = BaseActor->GetSkeletalMeshComponent();
			if (SkeletalMeshComponent)
			{
				return !SkeletalMeshComponent->bPauseAnims;
			}
		}
		return false;
	}

	bool FMLDeformerEditorModel::IsPlayingForward() const
	{
		FMLDeformerEditorActor* BaseActor = GetVisualizationModeBaseActor();
		if (BaseActor)
		{
			return BaseActor->GetPlaySpeed() > 0.0f;
		}
		return false;
	}


	double FMLDeformerEditorModel::CalcTrainingTimelinePosition() const
	{
		FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID_Train_GroundTruth);
		if (EditorActor && EditorActor->HasVisualMesh())
		{
			return EditorActor->GetPlayPosition();
		}

		EditorActor = FindEditorActor(ActorID_Train_Base);
		if (EditorActor && EditorActor->HasVisualMesh())
		{
			return EditorActor->GetPlayPosition();
		}

		return 0.0;
	}

	double FMLDeformerEditorModel::CalcTestTimelinePosition() const
	{
		FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID_Test_GroundTruth);
		if (EditorActor && EditorActor->HasVisualMesh())
		{
			return EditorActor->GetPlayPosition();
		}

		EditorActor = FindEditorActor(ActorID_Test_Base);
		if (EditorActor && EditorActor->HasVisualMesh())
		{
			return EditorActor->GetPlayPosition();
		}

		return 0.0;
	}

	void FMLDeformerEditorModel::UpdateTestAnimPlaySpeed()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		const float Speed = VizSettings->GetAnimPlaySpeed();
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor && EditorActor->IsTestActor()) // Only do test actors, no training actors.
			{
				EditorActor->SetPlaySpeed(Speed);
			}
		}
	}

	void FMLDeformerEditorModel::ClampCurrentTrainingFrameIndex()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (GetNumTrainingFrames() > 0)
		{
			const int32 ClampedNumber = FMath::Min(VizSettings->GetTrainingFrameNumber(), GetNumTrainingFrames() - 1);
			VizSettings->SetTrainingFrameNumber(ClampedNumber);
		}
		else
		{
			VizSettings->SetTrainingFrameNumber(0);
		}
	}

	void FMLDeformerEditorModel::ClampCurrentTestFrameIndex()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (GetNumTestFrames() > 0)
		{
			const int32 ClampedNumber = FMath::Min(VizSettings->GetTestingFrameNumber(), GetNumTestFrames() - 1);
			VizSettings->SetTestingFrameNumber(ClampedNumber);
		}
		else
		{
			VizSettings->SetTestingFrameNumber(0);
		}
	}

	int32 FMLDeformerEditorModel::GetNumTestFrames() const
	{
		const UAnimSequence* Sequence = Model->GetVizSettings()->GetTestAnimSequence();
		if (Sequence)
		{
			return Sequence->GetNumberOfSampledKeys();
		}
		return 0;
	}

	int32 FMLDeformerEditorModel::GetNumFramesForTraining() const 
	{ 
		return FMath::Min(GetNumTrainingFrames(), Model->GetTrainingFrameLimit());
	}

	FText FMLDeformerEditorModel::GetBaseAssetChangedErrorText() const
	{
		FText Result;
		if (Model->GetSkeletalMesh() && Model->GetInputInfo())
		{
			if (Model->GetNumBaseMeshVerts() != Model->GetInputInfo()->GetNumBaseMeshVertices() &&
				Model->GetNumBaseMeshVerts() > 0 && Model->GetInputInfo()->GetNumBaseMeshVertices() > 0)
			{
				Result = FText::Format(LOCTEXT("BaseMeshMismatch", "Number of vertices in base mesh has changed from {0} to {1} vertices since this ML Deformer Asset was saved! {2}"),
					Model->GetInputInfo()->GetNumBaseMeshVertices(),
					Model->GetNumBaseMeshVerts(),
					IsTrained() ? LOCTEXT("BaseMeshMismatchNN", "Model needs to be retrained!") : FText());
			}
		}
		return Result;
	}

	FText FMLDeformerEditorModel::GetVertexMapChangedErrorText() const
	{
		FText Result;
		if (Model->GetSkeletalMesh())
		{
			bool bVertexMapMatch = true;
			const FSkeletalMeshModel* ImportedModel = Model->GetSkeletalMesh()->GetImportedModel();
			if (ImportedModel)
			{
				const TArray<int32>& MeshVertexMap = ImportedModel->LODModels[0].MeshToImportVertexMap;
				const TArray<int32>& ModelVertexMap = Model->GetVertexMap();
				if (MeshVertexMap.Num() == ModelVertexMap.Num())
				{
					for (int32 Index = 0; Index < ModelVertexMap.Num(); ++Index)
					{
						if (MeshVertexMap[Index] != ModelVertexMap[Index])
						{
							bVertexMapMatch = false;
							break;
						}
					}
					
					if (!bVertexMapMatch)
					{
						Result = FText(LOCTEXT("VertexMapMismatch", "The vertex order of your Skeletal Mesh changed."));
					}
				}
			}
		}
		return Result;
	}

	FText FMLDeformerEditorModel::GetSkeletalMeshNeedsReimportErrorText() const
	{
		FText Result;

		if (Model->GetSkeletalMesh())
		{
			FSkeletalMeshModel* ImportedModel = Model->GetSkeletalMesh()->GetImportedModel();
			check(ImportedModel);

			const TArray<FSkelMeshImportedMeshInfo>& SkelMeshInfos = ImportedModel->LODModels[0].ImportedMeshInfos;
			if (SkelMeshInfos.IsEmpty())
			{
				Result = LOCTEXT("SkelMeshNeedsReimport", "Skeletal Mesh asset needs to be reimported.");
			}
		}

		return Result;
	}

	FText FMLDeformerEditorModel::GetInputsErrorText() const
	{
		if (Model->GetSkeletalMesh() && GetEditorInputInfo()->IsEmpty())
		{
			FString ErrorString;
			ErrorString += FText(LOCTEXT("InputsNeededErrorText", "The training process needs inputs.\n")).ToString();

			ErrorString.RemoveFromEnd("\n");
			return FText::FromString(ErrorString);
		}

		return FText();
	}

	FText FMLDeformerEditorModel::GetIncompatibleSkeletonErrorText(USkeletalMesh* InSkelMesh, UAnimSequence* InAnimSeq) const
	{
		FText Result;
		if (InSkelMesh && InAnimSeq)
		{
			if (!InSkelMesh->GetSkeleton()->IsCompatibleForEditor(InAnimSeq->GetSkeleton()))
			{
				Result = LOCTEXT("SkeletonMismatch", "The base skeletal mesh and anim sequence use incompatible skeletons. The animation might not play correctly.");
			}
		}
		return Result;
	}

	FText FMLDeformerEditorModel::GetTargetAssetChangedErrorText() const
	{
		FText Result;

		UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
		if (Model->HasTrainingGroundTruth() && InputInfo)
		{
			if (Model->GetNumTargetMeshVerts() != InputInfo->GetNumTargetMeshVertices() &&
				Model->GetNumTargetMeshVerts() > 0 && InputInfo->GetNumTargetMeshVertices() > 0)
			{
				Result = FText::Format(LOCTEXT("TargetMeshMismatch", "Number of vertices in target mesh has changed from {0} to {1} vertices since this ML Deformer Asset was saved! {2}"),
					InputInfo->GetNumTargetMeshVertices(),
					Model->GetNumTargetMeshVerts(),
					IsTrained() ? LOCTEXT("BaseMeshMismatchModel", "Model needs to be retrained!") : FText());
			}
		}

		return Result;
	}

	void FMLDeformerEditorModel::AddAnimatedBonesToBonesIncludeList()
	{
		if (!Model->GetAnimSequence())
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Cannot initialize bone list as no Anim Sequence has been picked."));
			UpdateEditorInputInfo();
			return;
		}

		const IAnimationDataModel* DataModel = Model->GetAnimSequence()->GetDataModel();
		if (!DataModel)
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Anim sequence has no data model."));
			UpdateEditorInputInfo();
			return;
		}

		if (!Model->GetSkeletalMesh())
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Skeletal Mesh has not been set."));
			UpdateEditorInputInfo();
			return;
		}

		USkeleton* Skeleton = Model->GetSkeletalMesh()->GetSkeleton();
		if (!Skeleton)
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Skeletal Mesh has no skeleton."));
			UpdateEditorInputInfo();
			return;
		}

		// Iterate over all bones that are both in the skeleton and the animation.
		TArray<FName> AnimatedBoneList;
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		const int32 NumBones = RefSkeleton.GetNum();

		TArray<FName> TrackNames;
		DataModel->GetBoneTrackNames(TrackNames);
		TArray<FTransform> BoneTransforms;		
		for (int32 Index = 0; Index < NumBones; ++Index)
		{
			const FName BoneName = RefSkeleton.GetBoneName(Index);
			if (!TrackNames.Contains(BoneName))
			{
				continue;
			}

			// Check if there is actually animation data.
			BoneTransforms.Reset();
			DataModel->GetBoneTrackTransforms(BoneName, BoneTransforms);
			bool bIsAnimated = false;
			if (!BoneTransforms.IsEmpty())
			{
				const FQuat FirstQuat = BoneTransforms[0].GetRotation();
				for (const FTransform& KeyValue : BoneTransforms)
				{
					if (!KeyValue.GetRotation().Equals(FirstQuat))
					{
						bIsAnimated = true;
						break;
					}
				}

				if (!bIsAnimated)
				{
					UE_LOG(LogMLDeformer, Display, TEXT("Bone '%s' has keyframes but isn't animated."), *BoneName.ToString());
				}
			}

			if (bIsAnimated)
			{
				AnimatedBoneList.Add(BoneName);
			}
		}

		// Init the bone include list using the animated bones.
		TArray<FBoneReference>& BoneList = Model->GetBoneIncludeList();
		for (const FName BoneName : AnimatedBoneList)
		{
			BoneList.AddUnique(FBoneReference(BoneName));
		}

		UpdateEditorInputInfo();
	}

	void FMLDeformerEditorModel::AddAnimatedCurvesToCurvesIncludeList()
	{
		if (!Model->GetAnimSequence())
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Cannot initialize curve list as no Anim Sequence has been picked."));
			UpdateEditorInputInfo();
			return;
		}

		const IAnimationDataModel* DataModel = Model->GetAnimSequence()->GetDataModel();
		if (!DataModel)
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Anim sequence has no data model."));
			UpdateEditorInputInfo();
			return;
		}

		if (!Model->GetSkeletalMesh())
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Skeletal Mesh has not been set."));
			UpdateEditorInputInfo();
			return;
		}

		USkeleton* Skeleton = Model->GetSkeletalMesh()->GetSkeleton();
		if (!Skeleton)
		{
			UE_LOG(LogMLDeformer, Warning, TEXT("Skeletal Mesh has no skeleton."));
			UpdateEditorInputInfo();
			return;
		}

		TArray<FName> AnimatedCurveList;
		TArray<FName> SkeletonCurveNames;
		Skeleton->GetCurveMetaDataNames(SkeletonCurveNames);
		for (const FName SkeletonCurveName : SkeletonCurveNames)
		{
			const TArray<FFloatCurve>& AnimCurves = DataModel->GetFloatCurves();
			for (const FFloatCurve& AnimCurve : AnimCurves)
			{
				if (AnimCurve.GetName() == SkeletonCurveName)
				{
					TArray<float> TimeValues;
					TArray<float> KeyValues;
					AnimCurve.GetKeys(TimeValues, KeyValues);
					if (KeyValues.Num() > 0)
					{
						const float FirstKeyValue = KeyValues[0];
						for (float CurKeyValue : KeyValues)
						{
							if (CurKeyValue != FirstKeyValue)
							{
								AnimatedCurveList.Add(SkeletonCurveName);
								break;
							}
						}
					}
					break;
				}
			}
		}

		TArray<FMLDeformerCurveReference>& CurveList = Model->GetCurveIncludeList();
		for (const FName CurveName : AnimatedCurveList)
		{
			CurveList.AddUnique(FMLDeformerCurveReference(CurveName));
		}
		UpdateEditorInputInfo();
	}

	void FMLDeformerEditorModel::InitInputInfo(UMLDeformerInputInfo* InputInfo)
	{
		InputInfo->Reset();

		TArray<FName>& BoneNames = InputInfo->GetBoneNames();
		TArray<FName>& CurveNames = InputInfo->GetCurveNames();

		USkeletalMesh* SkeletalMesh = Model->GetSkeletalMesh();
		InputInfo->SetSkeletalMesh(SkeletalMesh);

		BoneNames.Reset();
		CurveNames.Reset();

		InputInfo->SetNumBaseVertices(Model->GetNumBaseMeshVerts());
		InputInfo->SetNumTargetVertices(Model->GetNumTargetMeshVerts());

		const USkeleton* Skeleton = Model->GetSkeletalMesh() ? Model->GetSkeletalMesh()->GetSkeleton() : nullptr;

		// Handle bones.
		if (SkeletalMesh)
		{
			// Include all the bones when no list was provided.
			const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
			for (const FBoneReference& BoneReference : Model->GetBoneIncludeList())
			{
				if (BoneReference.BoneName.IsValid())
				{
					const FName BoneName = BoneReference.BoneName;
					if (RefSkeleton.FindBoneIndex(BoneName) == INDEX_NONE)
					{
						UE_LOG(LogMLDeformer, Warning, TEXT("Bone '%s' in the bones include list doesn't exist, ignoring it."), *BoneName.ToString());
						continue;
					}

					BoneNames.Add(BoneName);
				}
			}

			// Anim curves.
			if (Skeleton)
			{
				for (const FMLDeformerCurveReference& CurveReference : Model->GetCurveIncludeList())
				{
					if (CurveReference.CurveName.IsValid())
					{
						CurveNames.Add(CurveReference.CurveName);
					}
				}
			}
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		InputInfo->UpdateNameStrings();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	void FMLDeformerEditorModel::InitBoneIncludeListToAnimatedBonesOnly()
	{
		TArray<FBoneReference>& BoneList = Model->GetBoneIncludeList();
		BoneList.Empty();
		AddAnimatedBonesToBonesIncludeList();
	}

	void FMLDeformerEditorModel::InitCurveIncludeListToAnimatedCurvesOnly()
	{
		TArray<FMLDeformerCurveReference>& CurveList = Model->GetCurveIncludeList();
		CurveList.Empty();
		AddAnimatedCurvesToCurvesIncludeList();
	}

	void FMLDeformerEditorModel::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
	{
		// Make sure that before we render anything, that our sampler is ready.
		if (!Sampler->IsInitialized())
		{
			Sampler->Init(this); // This can still fail.
			Sampler->SetVertexDeltaSpace(EVertexDeltaSpace::PostSkinning);
			if (Sampler->IsInitialized()) // If we actually managed to initialize this frame.
			{
				SampleDeltas(); // Update the deltas.
			}
		}

		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			const TArray<float>& VertexDeltas = Sampler->GetVertexDeltas();
			const TArray<FVector3f>& LinearSkinnedPositions = Sampler->GetSkinnedVertexPositions();
			bool bDrawDeltas = (VizSettings->GetDrawVertexDeltas() && (VertexDeltas.Num() / 3) == LinearSkinnedPositions.Num());

			// Disable drawing deltas when we're playing the training anim sequence, as that can get too slow.
			const FMLDeformerEditorActor* BaseActor = FindEditorActor(ActorID_Train_Base);
			if (BaseActor)
			{
				bDrawDeltas &= !BaseActor->IsPlaying();
			}

			// Draw the deltas for the current frame.
			if (bDrawDeltas)
			{
				const FLinearColor DeltasColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.Deltas.Color");
				const FLinearColor DebugVectorsColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.DebugVectors.Color");
				const FLinearColor DebugVectorsColor2 = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.DebugVectors.Color2");
				const uint8 DepthGroup = VizSettings->GetXRayDeltas() ? 100 : 0;
				for (int32 Index = 0; Index < LinearSkinnedPositions.Num(); ++Index)
				{
					const int32 ArrayIndex = 3 * Index;
					const FVector Delta(
						VertexDeltas[ArrayIndex], 
						VertexDeltas[ArrayIndex + 1], 
						VertexDeltas[ArrayIndex + 2]);
					const FVector VertexPos = (FVector)LinearSkinnedPositions[Index];
					PDI->DrawLine(VertexPos, VertexPos + Delta, DeltasColor, DepthGroup);
				}
			}
		}
	}

	void FMLDeformerEditorModel::SampleDeltas()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		ClampCurrentTrainingFrameIndex();

		// If we have no Persona toolkit yet, then it is not yet safe to init the sampler.
		if (Editor->GetPersonaToolkitPointer() != nullptr && !Sampler->IsInitialized())
		{
			Sampler->Init(this);
		}

		if (Sampler->IsInitialized())
		{
			Sampler->SetVertexDeltaSpace(EVertexDeltaSpace::PostSkinning);
			Sampler->Sample(VizSettings->GetTrainingFrameNumber());
		}
	}

	void FMLDeformerEditorModel::CheckTrainingDataFrameChanged()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		ClampCurrentTrainingFrameIndex();
		if (CurrentTrainingFrame != VizSettings->GetTrainingFrameNumber())
		{
			OnTrainingDataFrameChanged();
		}
	}

	void FMLDeformerEditorModel::OnTrainingDataFrameChanged()
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();

		// If the current frame number changed, re-sample the deltas if needed.
		if (CurrentTrainingFrame != VizSettings->GetTrainingFrameNumber())
		{
			CurrentTrainingFrame = VizSettings->GetTrainingFrameNumber();
			if (VizSettings->GetDrawVertexDeltas() && VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
			{
				SampleDeltas();
			}
		}
	}

	void FMLDeformerEditorModel::RefreshMLDeformerComponents()
	{
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor && EditorActor->GetMLDeformerComponent())
			{
				USkeletalMeshComponent* SkelMeshComponent = EditorActor ? EditorActor->GetSkeletalMeshComponent() : nullptr;
				UMLDeformerAsset* DeformerAsset = GetModel()->GetDeformerAsset();
				EditorActor->GetMLDeformerComponent()->SetupComponent(DeformerAsset, SkelMeshComponent);
				UMLDeformerModelInstance* ModelInstance = EditorActor->GetMLDeformerComponent()->GetModelInstance();
				if (ModelInstance)
				{
					ModelInstance->UpdateCompatibilityStatus();
				}
			}
		}
	}

	void FMLDeformerEditorModel::CreateHeatMapMaterial()
	{
		const FString HeatMapMaterialPath = GetHeatMapMaterialPath();
		UObject* MaterialObject = StaticLoadObject(UMaterial::StaticClass(), nullptr, *HeatMapMaterialPath);
		HeatMapMaterial = Cast<UMaterial>(MaterialObject);
	}

	void FMLDeformerEditorModel::CreateHeatMapDeformerGraph()
	{
		const FString HeatMapDeformerPath = GetHeatMapDeformerGraphPath();
		UObject* DeformerObject = StaticLoadObject(UMeshDeformer::StaticClass(), nullptr, *HeatMapDeformerPath);
		HeatMapDeformerGraph = Cast<UMeshDeformer>(DeformerObject);
	}

	void FMLDeformerEditorModel::CreateHeatMapAssets()
	{
		CreateHeatMapMaterial();
		CreateHeatMapDeformerGraph();
	}

	void FMLDeformerEditorModel::SetHeatMapMaterialEnabled(bool bEnabled)
	{
		FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID_Test_MLDeformed);
		if (EditorActor == nullptr)
		{
			return;
		}

		USkeletalMeshComponent* Component = EditorActor->GetSkeletalMeshComponent();
		if (Component)
		{
			if (bEnabled)
			{
				for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
				{
					Component->SetMaterial(Index, HeatMapMaterial);
				}
			}
			else
			{
				Component->EmptyOverrideMaterials();
			}
		}

		UpdateDeformerGraph();
	}

	UMeshDeformer* FMLDeformerEditorModel::LoadDefaultDeformerGraph() const
	{
		const FString GraphAssetPath = Model->GetDefaultDeformerGraphAssetPath();
		if (!GraphAssetPath.IsEmpty())
		{
			UObject* Object = StaticLoadObject(UMeshDeformer::StaticClass(), nullptr, *GraphAssetPath);
			UMeshDeformer* DeformerGraph = Cast<UMeshDeformer>(Object);
			if (DeformerGraph == nullptr)
			{
				UE_LOG(LogMLDeformer, Warning, TEXT("Failed to load default Mesh Deformer from: %s"), *GraphAssetPath);
			}
			else
			{
				UE_LOG(LogMLDeformer, Verbose, TEXT("Loaded default Mesh Deformer from: %s"), *GraphAssetPath);
			}
			return DeformerGraph;
		}

		return nullptr;
	}

	void FMLDeformerEditorModel::SetDefaultDeformerGraphIfNeeded()
	{
		// Initialize the asset on the default plugin deformer graph.
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (VizSettings && VizSettings->GetDeformerGraph() == nullptr)
		{
			UMeshDeformer* DefaultGraph = LoadDefaultDeformerGraph();
			VizSettings->SetDeformerGraph(DefaultGraph);
		}
	}

	FText FMLDeformerEditorModel::GetOverlayText() const
	{
		const FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID_Test_MLDeformed);
		const UMLDeformerComponent* DeformerComponent = EditorActor ? EditorActor->GetMLDeformerComponent() : nullptr;
		if (DeformerComponent)
		{
			const UMLDeformerModelInstance* ModelInstance = DeformerComponent->GetModelInstance();
			if (ModelInstance &&
				ModelInstance->GetSkeletalMeshComponent() && 
				ModelInstance->GetSkeletalMeshComponent()->GetSkeletalMeshAsset() &&
				!ModelInstance->IsCompatible() )
			{
				return FText::FromString( ModelInstance->GetCompatibilityErrorText() );
			}
		}
	 
		return FText::GetEmpty();
	}

	void FMLDeformerEditorModel::UpdateDeformerGraph()
	{	
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			if (EditorActor == nullptr || EditorActor->GetMLDeformerComponent() == nullptr)
			{
				continue;
			}

			USkeletalMeshComponent* SkelMeshComponent = EditorActor->GetSkeletalMeshComponent();
			if (SkelMeshComponent == nullptr)
			{
				continue;
			}

			if (SkelMeshComponent)
			{
				const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
				UMeshDeformer* MeshDeformer = IsTrained() ? VizSettings->GetDeformerGraph() : nullptr;	
				const bool bUseHeatMapDeformer = VizSettings->GetShowHeatMap();
				SkelMeshComponent->SetMeshDeformer(bUseHeatMapDeformer ? HeatMapDeformerGraph.Get() : MeshDeformer);
			}
		}
	}

	void FMLDeformerEditorModel::OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted)
	{
		Sampler->SetVertexDeltaSpace(EVertexDeltaSpace::PostSkinning);
		SampleDeltas();
		Model->InitGPUData();
		UpdateMemoryUsage();
	}

	FMLDeformerEditorActor* FMLDeformerEditorModel::GetTimelineEditorActor() const
	{
		const UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			return FindEditorActor(ActorID_Train_GroundTruth);
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			return FindEditorActor(ActorID_Test_GroundTruth);
		}
		return nullptr;
	}

	bool FMLDeformerEditorModel::IsEditorReadyForTrainingBasicChecks()
	{
		// Make sure we have picked required assets.
		if (!Model->HasTrainingGroundTruth() ||
			Model->GetAnimSequence() == nullptr ||
			Model->GetSkeletalMesh() == nullptr ||
			GetNumTrainingFrames() == 0)
		{
			UpdateEditorInputInfo();
			return false;
		}

		// Make sure we have inputs.
		UpdateEditorInputInfo();
		if (GetEditorInputInfo()->IsEmpty())
		{
			return false;
		}

		return true;
	}

	void FMLDeformerEditorModel::UpdateMemoryUsage()
	{
		Model->InvalidateMemUsage();
	}

	void FMLDeformerEditorModel::TriggerInputAssetChanged(bool bRefreshVizSettings)
	{
		OnInputAssetsChanged();
		OnPostInputAssetChanged();
		UpdateMemoryUsage();

		if (Editor->GetModelDetailsView())
		{
			Editor->GetModelDetailsView()->ForceRefresh();
		}

		if (bRefreshVizSettings)
		{
			if (Editor->GetVizSettingsDetailsView())
			{
				Editor->GetVizSettingsDetailsView()->ForceRefresh();
			}
		}
	}

	void FMLDeformerEditorModel::ZeroDeltasByThreshold(TArray<FVector3f>& Deltas, float Threshold)
	{
		for (int32 VertexIndex = 0; VertexIndex < Deltas.Num(); ++VertexIndex)
		{
			if (Deltas[VertexIndex].Length() <= Threshold)
			{
				Deltas[VertexIndex] = FVector3f::ZeroVector;
			}
		}
	}

	void FMLDeformerEditorModel::CalcMeshNormals(TArrayView<const FVector3f> VertexPositions, TArrayView<const uint32> IndexArray, TArrayView<const int32> VertexMap, TArray<FVector3f>& OutNormals) const
	{
		const int32 NumVertices = VertexPositions.Num();
		OutNormals.Reset();
		OutNormals.SetNumZeroed(NumVertices);

		checkf(IndexArray.Num() % 3 == 0, TEXT("Expecting a triangle mesh!"));
		const int32 NumTriangles = IndexArray.Num() / 3;
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			const int32 ImportedIndices[3]	{ VertexMap[IndexArray[TriangleIndex * 3]], VertexMap[IndexArray[TriangleIndex * 3 + 1]], VertexMap[IndexArray[TriangleIndex * 3 + 2]] };
			const FVector3f Positions[3]	{ VertexPositions[ImportedIndices[0]], VertexPositions[ImportedIndices[1]], VertexPositions[ImportedIndices[2]] };

			const FVector3f EdgeA = (Positions[1] - Positions[0]).GetSafeNormal();
			const FVector3f EdgeB = (Positions[2] - Positions[0]).GetSafeNormal();
			if (EdgeA.SquaredLength() > 0.00001f && EdgeB.SquaredLength() > 0.00001f)
			{	
				const FVector3f FaceNormal = EdgeB.Cross(EdgeA);
				OutNormals[ImportedIndices[0]] += FaceNormal;
				OutNormals[ImportedIndices[1]] += FaceNormal;
				OutNormals[ImportedIndices[2]] += FaceNormal;
			}
		}

		// Renormalize.
		for (int32 Index = 0; Index < NumVertices; ++Index)
		{
			OutNormals[Index] = OutNormals[Index].GetSafeNormal();
		}
	}

	void FMLDeformerEditorModel::GenerateNormalsForMorphTarget(int32 LOD, USkeletalMesh* SkelMesh, int32 MorphTargetIndex, TArrayView<const FVector3f> Deltas, TArrayView<const FVector3f> BaseVertexPositions, TArrayView<FVector3f> BaseNormals, TArray<FVector3f>& OutDeltaNormals)
	{
		const FSkeletalMeshModel* ImportedModel = SkelMesh->GetImportedModel();
		if (ImportedModel == nullptr || !ImportedModel->LODModels.IsValidIndex(LOD))
		{
			OutDeltaNormals.Reset();
			OutDeltaNormals.SetNumZeroed(Model->GetNumBaseMeshVerts());
			return;
		}
		const TArrayView<const uint32> IndexArray = ImportedModel->LODModels[LOD].IndexBuffer;
		const TArrayView<const int32> VertexMap = ImportedModel->LODModels[LOD].MeshToImportVertexMap;

		// Build the array of displaced vertex positions.
		TArray<FVector3f> MorphedVertexPositions;
		const int32 NumBaseMeshVerts = Model->GetNumBaseMeshVerts();
		MorphedVertexPositions.SetNumUninitialized(NumBaseMeshVerts);
		for (int32 VertexIndex = 0; VertexIndex < NumBaseMeshVerts; ++VertexIndex)
		{
			const int32 DeltaIndex = (MorphTargetIndex * NumBaseMeshVerts) + VertexIndex;
			MorphedVertexPositions[VertexIndex] = BaseVertexPositions[VertexIndex] + Deltas[DeltaIndex];
		}

		// Calculate the normals of that displaced mesh.
		TArray<FVector3f> MorphedNormals;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CalcMeshNormals(MorphedVertexPositions, IndexArray, VertexMap, MorphedNormals);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Calculate and output the difference between the morphed normal and base normal.
		OutDeltaNormals.Reset();
		OutDeltaNormals.SetNumUninitialized(NumBaseMeshVerts);
		for (int32 VertexIndex = 0; VertexIndex < NumBaseMeshVerts; ++VertexIndex)
		{
			OutDeltaNormals[VertexIndex] = MorphedNormals[VertexIndex] - BaseNormals[VertexIndex];
			if (OutDeltaNormals[VertexIndex].SquaredLength() < 0.00001f)
			{
				OutDeltaNormals[VertexIndex] = FVector3f::ZeroVector;
			}
		}
	}

	void FMLDeformerEditorModel::CreateEngineMorphTargets(
		TArray<UMorphTarget*>& OutMorphTargets, 
		const TArray<FVector3f>& Deltas, 
		const FString& NamePrefix, 
		int32 LOD, 
		float DeltaThreshold, 
		bool bIncludeNormals, 
		EMLDeformerMaskChannel MaskChannel,
		bool bInvertMaskChannel)
	{
		OutMorphTargets.Reset();
		if (Deltas.IsEmpty())
		{
			return;
		}

		// Used to sort vertices.
		struct FMLDeformerCompareMorphTargetDeltas
		{
			FORCEINLINE bool operator()(const FMorphTargetDelta& A, const FMorphTargetDelta& B) const
			{
				return A.SourceIdx < B.SourceIdx;
			}
		};

		const int32 NumBaseMeshVerts = Model->GetNumBaseMeshVerts();
		check(Deltas.Num() % NumBaseMeshVerts == 0);
		const int32 NumMorphTargets = Deltas.Num() / NumBaseMeshVerts;
		check((Deltas.Num() / NumMorphTargets) == NumBaseMeshVerts);
		check(!Model->GetVertexMap().IsEmpty());

		FScopedSlowTask Task(NumMorphTargets, LOCTEXT("CreateEngineMorphTargetProgress", "Creating morph targets"));
		Task.MakeDialog(false);	

		USkeletalMesh* SkelMesh = Model->GetSkeletalMesh();
		FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
		check(RenderData);
		check(!RenderData->LODRenderData.IsEmpty());
		const int32 NumRenderVertices = RenderData->LODRenderData[LOD].GetNumVertices();

		// Calculate the normals for the base mesh.
		const FSkeletalMeshModel* ImportedModel = SkelMesh->GetImportedModel();
		check(ImportedModel);
		check(ImportedModel->LODModels.IsValidIndex(LOD));
		const TArrayView<const uint32> IndexArray = ImportedModel->LODModels[LOD].IndexBuffer;
		const TArrayView<const int32> VertexMap = ImportedModel->LODModels[LOD].MeshToImportVertexMap;
		const TArrayView<const FVector3f> BaseVertexPositions = Sampler->GetUnskinnedVertexPositions();

		TArray<FVector3f> BaseNormals;
		if (bIncludeNormals)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			CalcMeshNormals(BaseVertexPositions, IndexArray, VertexMap, BaseNormals);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		const FColorVertexBuffer& ColorBuffer = RenderData->LODRenderData[LOD].StaticVertexBuffers.ColorVertexBuffer;

		// Initialize an engine morph target for each model morph target.
		UE_LOG(LogMLDeformer, Display, TEXT("Initializing %d engine morph targets of %d vertices each"), NumMorphTargets, Deltas.Num() / NumMorphTargets);
		TArray<FVector3f> DeltaNormals;
		for (int32 MorphTargetIndex = 0; MorphTargetIndex < NumMorphTargets; ++MorphTargetIndex)
		{
			if (bIncludeNormals)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				GenerateNormalsForMorphTarget(LOD, SkelMesh, MorphTargetIndex, Deltas, BaseVertexPositions, BaseNormals, DeltaNormals);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}

			const FName MorphName = *FString::Printf(TEXT("%s%.3d"), *NamePrefix, MorphTargetIndex);
			UMorphTarget* MorphTarget = NewObject<UMorphTarget>(SkelMesh, MorphName);
			MorphTarget->BaseSkelMesh = SkelMesh;
			OutMorphTargets.Add(MorphTarget);

			// Create a new LOD model for this morph.
			TArray<FMorphTargetLODModel>& MorphLODs = MorphTarget->GetMorphLODModels();
			MorphLODs.AddDefaulted();
			FMorphTargetLODModel& MorphLODModel = MorphLODs.Last();

			// Initialize the morph target LOD level.
			MorphLODModel.Reset();
			MorphLODModel.bGeneratedByEngine = true;
			MorphLODModel.NumBaseMeshVerts = NumRenderVertices;

			// Init deltas for this morph target.
			MorphLODModel.Vertices.Reserve(NumRenderVertices);
			for (int32 VertexIndex = 0; VertexIndex < NumRenderVertices; ++VertexIndex)
			{
				const int32 ImportedVertexNumber = VertexMap[VertexIndex];
				if (ImportedVertexNumber != INDEX_NONE)
				{
					float VertexWeight = 1.0f;
					if (ColorBuffer.GetNumVertices() != 0 && MaskChannel != EMLDeformerMaskChannel::Disabled)
					{
						const FLinearColor& VertexColor = ColorBuffer.VertexColor(VertexIndex);
						switch (MaskChannel)
						{
							case EMLDeformerMaskChannel::VertexColorRed:	{ VertexWeight = VertexColor.R; break; }
							case EMLDeformerMaskChannel::VertexColorGreen:	{ VertexWeight = VertexColor.G; break; }
							case EMLDeformerMaskChannel::VertexColorBlue:	{ VertexWeight = VertexColor.B; break; }
							case EMLDeformerMaskChannel::VertexColorAlpha:	{ VertexWeight = VertexColor.A; break; }
							default: 
								checkf(false, TEXT("Unexpected mask channel value."));
								break;
						};

						if (bInvertMaskChannel)
						{
							VertexWeight = FMath::Clamp<float>(1.0f - VertexWeight, 0.0f, 1.0f);
						}
					}

					// Update the sections.
					int32 SectionIndex = INDEX_NONE;
					int32 SectionVertexIndex = INDEX_NONE;
					ImportedModel->LODModels[LOD].GetSectionFromVertexIndex(VertexIndex, SectionIndex, SectionVertexIndex);

					const FVector3f Delta = Deltas[ImportedVertexNumber + MorphTargetIndex * NumBaseMeshVerts] * VertexWeight;
					if (Delta.Length() > DeltaThreshold)
					{
						MorphLODModel.Vertices.AddDefaulted();
						FMorphTargetDelta& MorphTargetDelta = MorphLODModel.Vertices.Last();
						MorphTargetDelta.PositionDelta = Delta;
						MorphTargetDelta.SourceIdx = VertexIndex;
						MorphTargetDelta.TangentZDelta = bIncludeNormals ? DeltaNormals[ImportedVertexNumber] * VertexWeight : FVector3f::ZeroVector;

						MorphLODModel.SectionIndices.AddUnique(SectionIndex);
					}
				}
			}

			MorphLODModel.Vertices.Shrink();
			MorphLODModel.Vertices.Sort(FMLDeformerCompareMorphTargetDeltas());
			MorphLODModel.NumVertices = MorphLODModel.Vertices.Num();
			Task.EnterProgressFrame();
		}
	}

	void FMLDeformerEditorModel::CompressEngineMorphTargets(FMorphTargetVertexInfoBuffers& OutMorphBuffers, const TArray<UMorphTarget*>& MorphTargets, int32 LOD, float MorphErrorTolerance)
	{
		FScopedSlowTask Task(2, LOCTEXT("CompressEngineMorphTargetProgress", "Compressing morph targets"));
		Task.MakeDialog(false);	

		USkeletalMesh* SkelMesh = Model->GetSkeletalMesh();
		FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
		check(RenderData);
		check(!RenderData->LODRenderData.IsEmpty());
		const int32 NumRenderVertices = RenderData->LODRenderData[LOD].GetNumVertices();

		// Release any existing morph buffer data.
		if (OutMorphBuffers.IsRHIIntialized() && OutMorphBuffers.IsInitialized())
		{
			ReleaseResourceAndFlush(&OutMorphBuffers);
		}

		// Don't empty the array of morph target data when we init the RHI buffers, as we need them to serialize later on.
		OutMorphBuffers = FMorphTargetVertexInfoBuffers();
		OutMorphBuffers.SetEmptyMorphCPUDataOnInitRHI(false);

		// Initialize the compressed morph target buffers.
		OutMorphBuffers.InitMorphResources
		(
			GMaxRHIShaderPlatform,
			RenderData->LODRenderData[LOD].RenderSections,
			MorphTargets,
			NumRenderVertices,
			LOD,
			MorphErrorTolerance
		);

		Task.EnterProgressFrame();

		// Reinit the render resources.
		if (OutMorphBuffers.IsMorphCPUDataValid() && OutMorphBuffers.GetNumMorphs() > 0 && OutMorphBuffers.GetNumBatches() > 0)
		{
			BeginInitResource(&OutMorphBuffers);
		}

		// Update the editor actor skel mesh components for all the ones that also have an ML Deformer on it.
		for (FMLDeformerEditorActor* EditorActor : EditorActors)
		{
			const UMLDeformerComponent* MLDeformerComponent = EditorActor->GetMLDeformerComponent();
			USkeletalMeshComponent* SkelMeshComponent = EditorActor->GetSkeletalMeshComponent();
			if (SkelMeshComponent && MLDeformerComponent)
			{
				SkelMeshComponent->RefreshExternalMorphTargetWeights();
			}
		}

		UpdateMemoryUsage();
		Task.EnterProgressFrame();
	}

	void FMLDeformerEditorModel::DrawMorphTarget(FPrimitiveDrawInterface* PDI, const TArray<FVector3f>& MorphDeltas, float DeltaThreshold, int32 MorphTargetIndex, const FVector& DrawOffset)
	{
		UMLDeformerVizSettings* VizSettings = Model->GetVizSettings();
		if (!MorphDeltas.IsEmpty())
		{
			const int32 NumVerts = Model->GetNumBaseMeshVerts();
			check(MorphDeltas.Num() % NumVerts == 0);

			const TArray<FVector3f>& UnskinnedPositions = Sampler->GetUnskinnedVertexPositions();
			check(NumVerts == UnskinnedPositions.Num());

			// Draw all deltas.			
			const int32 NumMorphTargets = MorphDeltas.Num() / NumVerts;
			const int32 FinalMorphTargetIndex = FMath::Clamp<int32>(MorphTargetIndex, 0, NumMorphTargets - 1);

			const FLinearColor IncludedColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.Morphs.IncludedVertexColor");
			const FLinearColor ExcludedColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.Morphs.ExcludedVertexColor");
			for (int32 VertexIndex = 0; VertexIndex < NumVerts; ++VertexIndex)
			{
				const FVector StartPoint = FVector(UnskinnedPositions[VertexIndex]) + DrawOffset;
				const int32 DeltaArrayOffset = NumVerts * FinalMorphTargetIndex + VertexIndex;
				const FVector Delta(MorphDeltas[DeltaArrayOffset]);

				if (Delta.Length() >= DeltaThreshold)
				{
					PDI->DrawPoint(StartPoint, IncludedColor, 1.0f, 0);
					PDI->DrawLine(StartPoint, StartPoint + Delta, IncludedColor, 0);
				}
				else
				{
					PDI->DrawPoint(StartPoint + Delta, ExcludedColor, 0.75f, 0);
				}
			}
		}
	}

	int32 FMLDeformerEditorModel::GetNumCurvesOnSkeletalMesh(USkeletalMesh* SkelMesh) const
	{
		if (SkelMesh)
		{
			const USkeleton* Skeleton = SkelMesh->GetSkeleton();
			if (Skeleton)
			{
				return Skeleton->GetNumCurveMetaData();
			}
		}

		return 0;
	}

	FString FMLDeformerEditorModel::GetHeatMapMaterialPath() const
	{
		return FString(TEXT("/MLDeformerFramework/Materials/MLDeformerHeatMapMat.MLDeformerHeatMapMat"));
	}

	FString FMLDeformerEditorModel::GetHeatMapDeformerGraphPath() const
	{
		return FString(TEXT("/MLDeformerFramework/Deformers/DG_MLDeformerModel_HeatMap.DG_MLDeformerModel_HeatMap"));
	}

	void FMLDeformerEditorModel::RefreshInputWidget()
	{
		if (InputWidget.IsValid())
		{
			InputWidget->Refresh();
		}
	}

	TSharedPtr<SMLDeformerInputWidget> FMLDeformerEditorModel::CreateInputWidget()
	{
		return SNew(SMLDeformerInputWidget)
			.EditorModel(this);
	}

	const UAnimSequence* FMLDeformerEditorModel::GetAnimSequence() const
	{
		if (GetModel())
		{
			const UMLDeformerVizSettings* VizSettings = GetModel()->GetVizSettings();
			if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
			{
				return GetModel()->GetAnimSequence();
			}
			else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
			{
				return GetModel()->GetVizSettings()->GetTestAnimSequence();
			}
		}
		return nullptr;
	}

	void FMLDeformerEditorModel::UpdateRanges()
	{
		ViewRange = TRange<double>(0.0, 100.0);
		WorkingRange = TRange<double>(0.0, 100.0);
		PlaybackRange = TRange<double>(0.0, 100.0);
		const UAnimSequence* AnimSeq = GetAnimSequence();
		const double Duration = AnimSeq ? AnimSeq->GetPlayLength() : 0.0;
		ViewRange = TRange<double>(0.0, Duration);
		WorkingRange = ViewRange;
		PlaybackRange = WorkingRange;
		ClampCurrentTrainingFrameIndex();
	}

	void FMLDeformerEditorModel::HandleModelChanged()
	{
		UpdateRanges();
	}

	void FMLDeformerEditorModel::HandleVizModeChanged(EMLDeformerVizMode Mode)
	{
		UpdateRanges();

		// Update the preview scene so that the debug rendering draws the skeleton for the correct actor, depending on whether we are in training or testing mode.
		// On default we render the bones etc for the ML Deformed character.
		// Additionally trigger the input assets changed event, as that updates the animation play offsets etc. This is needed when we switch from say training back to testing mode.
		UpdatePreviewScene();
		TriggerInputAssetChanged();
	}

	void FMLDeformerEditorModel::UpdatePreviewScene()
	{
		FMLDeformerEditorActor* EditorActor = nullptr;
		UAnimationAsset* AnimAsset = nullptr;
		const EMLDeformerVizMode Mode = Model->GetVizSettings()->GetVisualizationMode();
		if (Mode == EMLDeformerVizMode::TrainingData)
		{
			EditorActor = FindEditorActor(ActorID_Train_Base);
			AnimAsset = Model->GetAnimSequence();
		}
		else if (Mode == EMLDeformerVizMode::TestData)
		{
			EditorActor = FindEditorActor(ActorID_Test_MLDeformed);
			AnimAsset = Model->GetVizSettings()->GetTestAnimSequence();
		}

		IPersonaToolkit* ToolkitPtr = GetEditor()->GetPersonaToolkitPointer();
		if (EditorActor && ToolkitPtr)
		{
			IPersonaPreviewScene& Scene = ToolkitPtr->GetPreviewScene().Get();
			Scene.SetActor(EditorActor->GetActor());
			Scene.SetSelectedActor(EditorActor->GetActor());
			UDebugSkelMeshComponent* SkelMeshComponent = EditorActor->GetSkeletalMeshComponent();
			if (SkelMeshComponent)
			{
				Scene.SetPreviewMeshComponent(SkelMeshComponent);
				Scene.SetPreviewMesh(SkelMeshComponent->GetSkeletalMeshAsset());
			}
			Scene.SetPreviewAnimationAsset(AnimAsset);
		}
	}

	/** Get the current view range */
	TRange<double> FMLDeformerEditorModel::GetViewRange() const
	{
		return ViewRange;
	}

	void FMLDeformerEditorModel::SetViewRange(TRange<double> InRange)
	{
		ViewRange = InRange;

		if (WorkingRange.HasLowerBound() && WorkingRange.HasUpperBound())
		{
			WorkingRange = TRange<double>::Hull(WorkingRange, ViewRange);
		}
		else
		{
			WorkingRange = ViewRange;
		}
	}

	TRange<double> FMLDeformerEditorModel::GetWorkingRange() const
	{
		return WorkingRange;
	}

	TRange<FFrameNumber> FMLDeformerEditorModel::GetPlaybackRange() const
	{
		const int32 Resolution = GetTickResolution();
		return TRange<FFrameNumber>(FFrameNumber(FMath::RoundToInt32(PlaybackRange.GetLowerBoundValue() * (double)Resolution)), FFrameNumber(FMath::RoundToInt32(PlaybackRange.GetUpperBoundValue() * (double)Resolution)));

	}

	int32 FMLDeformerEditorModel::GetTicksPerFrame() const
	{
		// Default to millisecond resolution
		const int32 TimelineScrubSnapValue = 1000;
		return TimelineScrubSnapValue;
	}

	FFrameNumber FMLDeformerEditorModel::GetTickResScrubPosition() const
	{
		const double Resolution = GetTickResolution();
		const UMLDeformerVizSettings* VizSettings = GetModel()->GetVizSettings();
		double TrainingFrameNumber = 0.0;
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			TrainingFrameNumber = GetTrainingFrameAtTime(CalcTimelinePosition());
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			TrainingFrameNumber = GetTestFrameAtTime(CalcTimelinePosition());
		}
		FFrameNumber FrameNumber = (int32)(TrainingFrameNumber * Resolution / (double)GetFrameRate());
		return FrameNumber;
	}

	float FMLDeformerEditorModel::GetScrubTime() const
	{
		return (float)ScrubPosition.AsDecimal() / (float)GetFrameRate();
	}

	void FMLDeformerEditorModel::SetScrubPosition(FFrameTime NewScrubPostion)
	{
		const double FrameNumber = NewScrubPostion.AsDecimal() * (double)GetFrameRate() / (double)GetTickResolution();
		ScrubPosition.FrameNumber = (int32)FrameNumber;

		const UMLDeformerVizSettings* VizSettings = GetModel()->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			SetTrainingFrame(ScrubPosition.FrameNumber.Value);
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			SetTestFrame(ScrubPosition.FrameNumber.Value);
		}
	}

	void FMLDeformerEditorModel::SetScrubPosition(FFrameNumber NewScrubPostion)
	{
		const double Resolution = GetTickResolution();
		const double ClampedValue = FMath::Clamp((double)NewScrubPostion.Value, PlaybackRange.GetLowerBoundValue() * Resolution, PlaybackRange.GetUpperBoundValue() * Resolution);
		const int32 FrameValue = ClampedValue * (double)GetFrameRate() / Resolution;

		const UMLDeformerVizSettings* VizSettings = GetModel()->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			SetTrainingFrame(FrameValue);
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			SetTestFrame(FrameValue);
		}
	}

	void FMLDeformerEditorModel::SetDisplayFrames(bool bInDisplayFrames)
	{
		bDisplayFrames = bInDisplayFrames;
	}

	bool FMLDeformerEditorModel::IsDisplayingFrames() const
	{
		return bDisplayFrames;
	}

	void FMLDeformerEditorModel::HandleViewRangeChanged(TRange<double> InRange)
	{
		SetViewRange(InRange);
	}

	void FMLDeformerEditorModel::HandleWorkingRangeChanged(TRange<double> InRange)
	{
		WorkingRange = InRange;
	}

	double FMLDeformerEditorModel::CalcTimelinePosition() const
	{
		const UMLDeformerVizSettings* VizSettings = GetModel()->GetVizSettings();
		if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TrainingData)
		{
			return CalcTrainingTimelinePosition();
		}
		else if (VizSettings->GetVisualizationMode() == EMLDeformerVizMode::TestData)
		{
			return CalcTestTimelinePosition();
		}
		return 0.0;
	}

	double FMLDeformerEditorModel::GetFrameRate() const
	{
		const UAnimSequence* AnimSequence = GetAnimSequence();
		if (AnimSequence)
		{
			return AnimSequence->GetSamplingFrameRate().AsDecimal();
		}
		else
		{
			return 30.0;
		}
	}

	int32 FMLDeformerEditorModel::GetTickResolution() const
	{
		return FMath::RoundToInt32((double)GetTicksPerFrame() * GetFrameRate());
	}

	void FMLDeformerEditorModel::OnPostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		OnPropertyChanged(PropertyChangedEvent);
	}

	FString FMLDeformerEditorModel::GetTrainedNetworkOnnxFile() const
	{
		const FString ClassName = Model->GetClass()->GetFName().ToString();

		// This returns something like:
		// "<intermediatedir>/YourModel/YourModel.onnx"
		return FString::Printf(TEXT("%s%s/%s.onnx"), *FPaths::ProjectIntermediateDir(), *ClassName, *ClassName);
	}

	ETrainingResult FMLDeformerEditorModel::Train()
	{ 
		UE_LOG(LogMLDeformer, Warning, TEXT("Please override the FMLDeformerEditorModel::Train() method inside your derived editor model class."));
		return ETrainingResult::Success;
	}

	int32 FMLDeformerEditorModel::GetNumTrainingFrames() const
	{ 
		UE_LOG(LogMLDeformer, Warning, TEXT("Please override the FMLDeformerEditorModel::GetNumTrainingFrames() method inside your derived editor model class."));
		return 0;
	}

	UMLDeformerComponent* FMLDeformerEditorModel::FindMLDeformerComponent(int32 ActorID) const
	{
		const FMLDeformerEditorActor* EditorActor = FindEditorActor(ActorID);
		if (EditorActor)
		{
			return EditorActor->GetMLDeformerComponent();
		}
		return nullptr;
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
