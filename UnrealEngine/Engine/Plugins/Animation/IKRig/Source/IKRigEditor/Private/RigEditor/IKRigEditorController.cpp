// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigEditorController.h"

#include "RigEditor/IKRigController.h"
#include "RigEditor/SIKRigHierarchy.h"
#include "RigEditor/SIKRigSolverStack.h"
#include "RigEditor/IKRigAnimInstance.h"
#include "RigEditor/IKRigToolkit.h"
#include "RigEditor/SIKRigAssetBrowser.h"
#include "RigEditor/SIKRigOutputLog.h"
#include "Rig/Solvers/IKRig_FBIKSolver.h"
#include "Widgets/Input/SComboBox.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Dialog/SCustomDialog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigEditorController)

#if WITH_EDITOR

#include "HAL/PlatformApplicationMisc.h"

#endif

#define LOCTEXT_NAMESPACE "IKRigEditorController"

bool UIKRigBoneDetails::CurrentTransformRelative[3] = {true, true, true};
bool UIKRigBoneDetails::ReferenceTransformRelative[3] = {true, true, true};

TOptional<FTransform> UIKRigBoneDetails::GetTransform(EIKRigTransformType::Type TransformType) const
{
	if(!AnimInstancePtr.IsValid() || !AssetPtr.IsValid())
	{
		return TOptional<FTransform>();
	}
	
	FTransform LocalTransform = FTransform::Identity;
	FTransform GlobalTransform = FTransform::Identity;
	const bool* IsRelative = nullptr;

	const FIKRigSkeleton& Skeleton = AssetPtr->GetSkeleton();
	const int32 BoneIndex = Skeleton.GetBoneIndexFromName(SelectedBone);
	if(BoneIndex == INDEX_NONE)
	{
		return TOptional<FTransform>();
	}

	switch(TransformType)
	{
		case EIKRigTransformType::Current:
		{
			IsRelative = CurrentTransformRelative;
			
			USkeletalMeshComponent* SkeletalMeshComponent = AnimInstancePtr->GetSkelMeshComponent();
			const bool IsSkelMeshValid = SkeletalMeshComponent != nullptr &&
										SkeletalMeshComponent->GetSkeletalMeshAsset() != nullptr;
			if (IsSkelMeshValid)
			{
				GlobalTransform = SkeletalMeshComponent->GetBoneTransform(BoneIndex);
				const TArray<FTransform>& LocalTransforms = SkeletalMeshComponent->GetBoneSpaceTransforms();
				LocalTransform = LocalTransforms.IsValidIndex(BoneIndex) ? LocalTransforms[BoneIndex] : FTransform::Identity;
			}
			else
			{
				GlobalTransform = Skeleton.CurrentPoseGlobal[BoneIndex];
				LocalTransform = Skeleton.CurrentPoseLocal[BoneIndex];
			}
			break;
		}
		case EIKRigTransformType::Reference:
		{
			IsRelative = ReferenceTransformRelative;
			GlobalTransform = Skeleton.RefPoseGlobal[BoneIndex];
			LocalTransform = GlobalTransform;
			const int32 ParentBoneIndex = Skeleton.ParentIndices[BoneIndex];
			if(ParentBoneIndex != INDEX_NONE)
			{
				const FTransform ParentTransform = Skeleton.RefPoseGlobal[ParentBoneIndex];;
				LocalTransform = GlobalTransform.GetRelativeTransform(ParentTransform);
			}
			break;
		}
	}
	checkSlow(IsRelative);

	FTransform Transform = LocalTransform;
	if(!IsRelative[0]) Transform.SetLocation(GlobalTransform.GetLocation());
	if(!IsRelative[1]) Transform.SetRotation(GlobalTransform.GetRotation());
	if(!IsRelative[2]) Transform.SetScale3D(GlobalTransform.GetScale3D());
	return Transform;
}

bool UIKRigBoneDetails::IsComponentRelative(
	ESlateTransformComponent::Type Component,
	EIKRigTransformType::Type TransformType) const
{
	switch(TransformType)
	{
		case EIKRigTransformType::Current:
		{
			return CurrentTransformRelative[(int32)Component]; 
		}
		case EIKRigTransformType::Reference:
		{
			return ReferenceTransformRelative[(int32)Component]; 
		}
	}
	return true;
}

void UIKRigBoneDetails::OnComponentRelativeChanged(
	ESlateTransformComponent::Type Component,
	bool bIsRelative,
	EIKRigTransformType::Type TransformType)
{
	switch(TransformType)
	{
		case EIKRigTransformType::Current:
		{
			CurrentTransformRelative[(int32)Component] = bIsRelative;
			break; 
		}
		case EIKRigTransformType::Reference:
		{
			ReferenceTransformRelative[(int32)Component] = bIsRelative;
			break; 
		}
	}
}

#if WITH_EDITOR

void UIKRigBoneDetails::OnCopyToClipboard(ESlateTransformComponent::Type Component, EIKRigTransformType::Type TransformType) const
{
	TOptional<FTransform> Optional = GetTransform(TransformType);
	if(!Optional.IsSet())
	{
		return;
	}

	const FTransform Xfo = Optional.GetValue();
	
	FString Content;
	switch(Component)
	{
	case ESlateTransformComponent::Location:
		{
			GetContentFromData(Xfo.GetLocation(), Content);
			break;
		}
	case ESlateTransformComponent::Rotation:
		{
			GetContentFromData(Xfo.Rotator(), Content);
			break;
		}
	case ESlateTransformComponent::Scale:
		{
			GetContentFromData(Xfo.GetScale3D(), Content);
			break;
		}
	case ESlateTransformComponent::Max:
	default:
		{
			GetContentFromData(Xfo, Content);
			TBaseStructure<FTransform>::Get()->ExportText(Content, &Xfo, &Xfo, nullptr, PPF_None, nullptr);
			break;
		}
	}

	if(!Content.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Content);
	}
}

void UIKRigBoneDetails::OnPasteFromClipboard(ESlateTransformComponent::Type Component, EIKRigTransformType::Type TransformType)
{
	// paste is not supported yet.
}

#endif

void FRetargetChainAnalyzer::AssignBestGuessName(FBoneChain& Chain, const FIKRigSkeleton& IKRigSkeleton)
{
	// a map of common names used in bone hierarchies, used to derive a best guess name for a retarget chain
	static TMap<FString, TArray<FString>> ChainToBonesMap;
	ChainToBonesMap.Add(FString("Head"), {"head"});
	ChainToBonesMap.Add(FString("Neck"), {"neck"});
	ChainToBonesMap.Add(FString("Leg"), {"leg", "hip", "thigh", "calf", "knee", "foot", "ankle", "toe"});
	ChainToBonesMap.Add(FString("Arm"), {"arm", "clavicle", "shoulder", "elbow", "wrist", "hand"});
	ChainToBonesMap.Add(FString("Spine"), {"spine"});
	
	ChainToBonesMap.Add(FString("Jaw"),{"jaw"});
	ChainToBonesMap.Add(FString("Tail"),{"tail", "tentacle"});
	
	ChainToBonesMap.Add(FString("Thumb"), {"thumb"});
	ChainToBonesMap.Add(FString("Index"), {"index"});
	ChainToBonesMap.Add(FString("Middle"), {"middle"});
	ChainToBonesMap.Add(FString("Ring"), {"ring"});
	ChainToBonesMap.Add(FString("Pinky"), {"pinky"});

	ChainToBonesMap.Add(FString("Root"), {"root"});

	TArray<int32> BonesInChainIndices;
	const bool bIsChainValid = IKRigSkeleton.ValidateChainAndGetBones(Chain, BonesInChainIndices);
	if (!bIsChainValid)
	{
		Chain.ChainName = GetDefaultChainName();
		return;
	}

	// initialize map of chain names with initial scores of 0 for each chain
	TArray<FString> ChainNames;
	ChainToBonesMap.GetKeys(ChainNames);
	TMap<FString, float> ChainScores;
	for (const FString& ChainName : ChainNames)
	{
		ChainScores.Add(*ChainName, 0.f);
	}

	// run text filter on the predefined bone names and record score for each predefined chain name
	for (const int32 ChainBoneIndex : BonesInChainIndices)
	{
		const FName& ChainBoneName = IKRigSkeleton.GetBoneNameFromIndex(ChainBoneIndex);
		const FString ChainBoneNameStr = ChainBoneName.ToString().ToLower();
		for (const FString& ChainName : ChainNames)
		{
			for (const FString& BoneNameToTry : ChainToBonesMap[ChainName])
			{
				TextFilter->SetFilterText(FText::FromString(BoneNameToTry));
				if (TextFilter->TestTextFilter(FBasicStringFilterExpressionContext(ChainBoneNameStr)))
				{
					++ChainScores[ChainName];	
				}
			}
		}
	}

	// find the chain name with the highest score
	float HighestScore = 0.f;
	FString RecommendedChainName = GetDefaultChainName().ToString();
	for (const FString& ChainName : ChainNames)
	{
		if (ChainScores[ChainName] > HighestScore)
		{
			HighestScore = ChainScores[ChainName];
			RecommendedChainName = ChainName;
		}
	}

	// now determine "sidedness" of the chain, and add a suffix if its on the left or right side
	const EChainSide ChainSide = GetSideOfChain(BonesInChainIndices, IKRigSkeleton);
	switch (ChainSide)
	{
	case EChainSide::Left:
		RecommendedChainName = "Left" + RecommendedChainName;
		break;
	case EChainSide::Right:
		RecommendedChainName = "Right" + RecommendedChainName;
		break;
	default:
		break;
	}
	
	Chain.ChainName = FName(RecommendedChainName);
}

FName FRetargetChainAnalyzer::GetDefaultChainName()
{
	static FText NewChainText = LOCTEXT("NewRetargetChainLabel", "NewRetargetChain");
	return FName(*NewChainText.ToString());
}

EChainSide FRetargetChainAnalyzer::GetSideOfChain(const TArray<int32>& BoneIndices, const FIKRigSkeleton& IKRigSkeleton)
{
	// determine "sidedness" of the chain based on the location of the bones (left, right or center of YZ plane)
	float AverageXPositionOfChain = 0.f;
	for (const int32 BoneIndex : BoneIndices)
	{
		AverageXPositionOfChain += IKRigSkeleton.RefPoseGlobal[BoneIndex].GetLocation().X;
	}
	AverageXPositionOfChain /= static_cast<float>(BoneIndices.Num());

	constexpr float CenterThresholdDistance = 1.0f;
	if (FMath::Abs(AverageXPositionOfChain) < CenterThresholdDistance)
	{
		return EChainSide::Center;
	}

	return AverageXPositionOfChain > 0 ? EChainSide::Left :  EChainSide::Right;
}

void FIKRigEditorController::Initialize(TSharedPtr<FIKRigEditorToolkit> Toolkit, const UIKRigDefinition* IKRigAsset)
{
	EditorToolkit = Toolkit;
	AssetController = UIKRigController::GetController(IKRigAsset);
	BoneDetails = NewObject<UIKRigBoneDetails>();
	
	// register callback to be informed when rig asset is modified by editor
	if (!AssetController->OnIKRigNeedsInitialized().IsBoundToObject(this))
	{
		ReinitializeDelegateHandle = AssetController->OnIKRigNeedsInitialized().AddSP(this, &FIKRigEditorController::HandleIKRigNeedsInitialized);
	}
}

void FIKRigEditorController::Close() const
{
	AssetController->OnIKRigNeedsInitialized().Remove(ReinitializeDelegateHandle);
}

void FIKRigEditorController::PromptUserToAssignMesh()
{
	// do we already have a skeletal mesh assigned?
	if (AssetController->GetSkeletalMesh())
	{
		return;
	}

	// is there already an imported hierarchy of bones?
	if (!AssetController->GetIKRigSkeleton().BoneNames.IsEmpty())
	{
		return;
	}

	// no skeletal mesh imported yet... so let's prompt the user to pick one...
	
	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FAssetPickerConfig AssetPickerConfig;
	// must set the parent UObject so that the resulting list filters correctly in multi-project environments
	AssetPickerConfig.AdditionalReferencingAssets.Add(AssetController->GetAsset());
	// the asset picker will only show skeletal meshes
	AssetPickerConfig.Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	// the delegate that fires when an asset is selected
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		MeshPickerWindow->RequestDestroyWindow();
		
		if (const TObjectPtr<USkeletalMesh> SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			// import the skeleton data into the IK Rig
			AssetController->SetSkeletalMesh(SkeletalMesh.Get());
		}
		
	});
	// the default view mode should be a list view
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	MeshPickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateIKRigOptions", "Assign Skeletal Mesh to IK Rig"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];

	GEditor->EditorAddModalWindow(MeshPickerWindow.ToSharedRef());
	MeshPickerWindow.Reset();
}

UIKRigProcessor* FIKRigEditorController::GetIKRigProcessor() const
{
	if (AnimInstance)
	{
		return AnimInstance->GetCurrentlyRunningProcessor();
	}

	return nullptr;
}

const FIKRigSkeleton* FIKRigEditorController::GetCurrentIKRigSkeleton() const
{
	if (const UIKRigProcessor* Processor = GetIKRigProcessor())
	{
		if (Processor->IsInitialized())
		{
			return &Processor->GetSkeleton();	
		}
	}

	return nullptr;
}

void FIKRigEditorController::HandleIKRigNeedsInitialized(UIKRigDefinition* ModifiedIKRig) const
{
	if (ModifiedIKRig != AssetController->GetAsset())
	{
		return;
	}

	ClearOutputLog();

	// currently running processor needs reinit
	AnimInstance->SetProcessorNeedsInitialized();
	
	// in case the skeletal mesh was swapped out, we need to ensure the preview scene is up-to-date
	USkeletalMesh* NewMesh = AssetController->GetSkeletalMesh();
	SkelMeshComponent->SetSkeletalMesh(NewMesh);
	const TSharedRef<IPersonaPreviewScene> PreviewScene = EditorToolkit.Pin()->GetPersonaToolkit()->GetPreviewScene();
	if (PreviewScene->GetPreviewMesh() != NewMesh)
	{
		PreviewScene->SetPreviewMeshComponent(SkelMeshComponent);
		PreviewScene->SetPreviewMesh(NewMesh);
	}

	// re-initializes the anim instances running in the viewport
	if (AnimInstance)
	{
		SkelMeshComponent->PreviewInstance = AnimInstance;
		AnimInstance->InitializeAnimation();
		SkelMeshComponent->EnablePreview(true, nullptr);
	}


	// update the bone details so it can pull on the current data
	BoneDetails->AnimInstancePtr = AnimInstance;
	BoneDetails->AssetPtr = ModifiedIKRig;

	RefreshAllViews();
}

void FIKRigEditorController::Reset() const
{
	SkelMeshComponent->ShowReferencePose(true);
	AssetController->ResetGoalTransforms();
}

void FIKRigEditorController::RefreshAllViews() const
{
	if (SolverStackView.IsValid())
	{
		SolverStackView->RefreshStackView();
	}

	if (SkeletonView.IsValid())
	{
		SkeletonView->RefreshTreeView();
	}

	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}

	if (RetargetingView.IsValid())
	{
		RetargetingView->RefreshView();
	}

	// refresh the asset browser to ensure it shows compatible sequences
	if (AssetBrowserView.IsValid())
	{
		AssetBrowserView.Get()->RefreshView();
	}
}

void FIKRigEditorController::RefreshTreeView() const
{
	if (SkeletonView.IsValid())
	{
		SkeletonView->RefreshTreeView();
	}
}

void FIKRigEditorController::ClearOutputLog() const
{
	if (OutputLogView.IsValid())
	{
		OutputLogView.Get()->ClearLog();
		if (const UIKRigProcessor* Processor = GetIKRigProcessor())
		{
			Processor->Log.Clear();
		}
	}
}

void FIKRigEditorController::AddNewGoals(const TArray<FName>& GoalNames, const TArray<FName>& BoneNames)
{
	check(GoalNames.Num() == BoneNames.Num());

	// add a default solver if there isn't one already
	PromptToAddDefaultSolver();

	// create goals
	FName LastCreatedGoalName = NAME_None;
	for (int32 I=0; I<GoalNames.Num(); ++I)
	{
		const FName& GoalName = GoalNames[I];
		const FName& BoneName = BoneNames[I];

		// create a new goal
		const FName NewGoalName = AssetController->AddNewGoal(GoalName, BoneName);
		if (NewGoalName == NAME_None)
		{
			continue; // already exists
		}

		// ask user if they want to assign this goal to a chain (if there is one on this bone)
		PromptToAssignGoalToChain(NewGoalName);

		// connect the new goal to selected solvers
		TArray<TSharedPtr<FSolverStackElement>> SelectedSolvers;
		GetSelectedSolvers(SelectedSolvers);
		for (TSharedPtr<FSolverStackElement>& Solver : SelectedSolvers)
		{
			AssetController->ConnectGoalToSolver(NewGoalName, Solver.Get()->IndexInStack);
		}

		LastCreatedGoalName = GoalName;
	}
	
	// were any goals created?
	if (LastCreatedGoalName != NAME_None)
	{
		// show last created goal in details view
		ShowDetailsForGoal(LastCreatedGoalName);
		// update all views
		RefreshAllViews();
	}
}

void FIKRigEditorController::ClearSelection()
{
	if (SkeletonView.IsValid())
	{
		SkeletonView->TreeView->ClearSelection();	
	}
	
	ShowEmptyDetails();
}

void FIKRigEditorController::HandleGoalSelectedInViewport(const FName& GoalName, bool bReplace) const
{
	if (SkeletonView.IsValid())
	{
		SkeletonView->AddSelectedItemFromViewport(GoalName, IKRigTreeElementType::GOAL, bReplace);
		ShowDetailsForElements(SkeletonView->GetSelectedItems());
		return;
	}

	ShowDetailsForGoal(GoalName);
}

void FIKRigEditorController::HandleBoneSelectedInViewport(const FName& BoneName, bool bReplace) const
{
	if (SkeletonView.IsValid())
	{
		SkeletonView->AddSelectedItemFromViewport(BoneName, IKRigTreeElementType::BONE, bReplace);
		ShowDetailsForElements(SkeletonView->GetSelectedItems());
		return;
	}
	
	ShowDetailsForBone(BoneName);
}

void FIKRigEditorController::GetSelectedSolvers(TArray<TSharedPtr<FSolverStackElement>>& OutSelectedSolvers) const
{
	if (SolverStackView.IsValid())
	{
		OutSelectedSolvers.Reset();
		OutSelectedSolvers.Append(SolverStackView->ListView->GetSelectedItems());
	}
}

int32 FIKRigEditorController::GetSelectedSolverIndex()
{
	if (!SolverStackView.IsValid())
	{
		return INDEX_NONE;
	}
	
	TArray<TSharedPtr<FSolverStackElement>> SelectedSolvers = SolverStackView->ListView->GetSelectedItems();
	if (SelectedSolvers.IsEmpty())
	{
		return INDEX_NONE;
	}

	return SelectedSolvers[0]->IndexInStack;
}

void FIKRigEditorController::GetSelectedGoalNames(TArray<FName>& OutGoalNames) const
{
	if (!SkeletonView.IsValid())
	{
		return;
	}

	SkeletonView->GetSelectedGoalNames(OutGoalNames);
}

int32 FIKRigEditorController::GetNumSelectedGoals() const
{
	if (!SkeletonView.IsValid())
	{
		return 0;
	}

	return SkeletonView->GetNumSelectedGoals();
}

void FIKRigEditorController::GetSelectedBoneNames(TArray<FName>& OutBoneNames) const
{
	if (!SkeletonView.IsValid())
	{
		return;
	}

	SkeletonView->GetSelectedBoneNames(OutBoneNames);
}

void FIKRigEditorController::GetSelectedBones(TArray<TSharedPtr<FIKRigTreeElement>>& OutBoneItems) const
{
	if (!SkeletonView.IsValid())
	{
		return;
	}

	SkeletonView->GetSelectedBones(OutBoneItems);
}

bool FIKRigEditorController::IsGoalSelected(const FName& GoalName) const
{
	if (!SkeletonView.IsValid())
	{
		return false;
	}

	return SkeletonView->IsGoalSelected(GoalName);
}

TArray<FName> FIKRigEditorController::GetSelectedChains() const
{
	if (!RetargetingView.IsValid())
	{
		return TArray<FName>();
	}

	return RetargetingView->GetSelectedChains();
}

bool FIKRigEditorController::DoesSkeletonHaveSelectedItems() const
{
	if (!SkeletonView.IsValid())
	{
		return false;
	}
	return SkeletonView->HasSelectedItems();
}

void FIKRigEditorController::GetChainsSelectedInSkeletonView(TArray<FBoneChain>& InOutChains)
{
	if (!SkeletonView.IsValid())
	{
		return;
	}

	SkeletonView->GetSelectedBoneChains(InOutChains);
}

void FIKRigEditorController::CreateNewRetargetChains()
{
	// get selected chains from hierarchy view
	TArray<FBoneChain> SelectedBoneChains;
	if (SkeletonView.IsValid())
	{
		SkeletonView->GetSelectedBoneChains(SelectedBoneChains);
	}

	const FIKRigSkeleton& IKRigSkeleton = AssetController->GetIKRigSkeleton();
	
	if (!SelectedBoneChains.IsEmpty())
	{
		// create a chain for each selected chain in hierarchy
		for (FBoneChain& BoneChain : SelectedBoneChains)
		{
			ChainAnalyzer.AssignBestGuessName(BoneChain, IKRigSkeleton);
			PromptToAddNewRetargetChain(BoneChain);
		}
	}
	else
	{
		// create an empty chain
		FBoneChain Chain(FRetargetChainAnalyzer::GetDefaultChainName(), NAME_None, NAME_None, NAME_None);
		PromptToAddNewRetargetChain(Chain);
	}
	
	RefreshAllViews();
}

bool FIKRigEditorController::PromptToAddDefaultSolver() const
{
	if (AssetController->GetNumSolvers() > 0)
	{
		return true;
	}

	TArray<TSharedPtr<FIKRigSolverTypeAndName>> SolverTypes;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class->IsNative())
		{
			continue;
		}

		if (!ClassIt->IsChildOf(UIKRigSolver::StaticClass()))
		{
			continue;
		}

		if (Class == UIKRigSolver::StaticClass())
		{
			continue; // skip base class
		}

		const UIKRigSolver* SolverCDO = CastChecked<UIKRigSolver>(Class->ClassDefaultObject);
		TSharedPtr<FIKRigSolverTypeAndName> SolverType = MakeShared<FIKRigSolverTypeAndName>();
		SolverType->NiceName = SolverCDO->GetNiceName();
		SolverType->SolverType = TSubclassOf<UIKRigSolver>(Class);
		SolverTypes.Add(SolverType);
	}

	// select "full body IK" by default
	TSharedPtr<FIKRigSolverTypeAndName> SelectedSolver = SolverTypes[0];
	for (TSharedPtr<FIKRigSolverTypeAndName>& SolverType :SolverTypes)
	{
		if (SolverType->SolverType == UIKRigFBIKSolver::StaticClass())
		{
			SelectedSolver = SolverType;
			break;
		}
	}
	
	TSharedRef<SComboBox<TSharedPtr<FIKRigSolverTypeAndName>>> SolverOptionBox = SNew(SComboBox<TSharedPtr<FIKRigSolverTypeAndName>>)
	.OptionsSource(&SolverTypes)
	.OnGenerateWidget_Lambda([](TSharedPtr<FIKRigSolverTypeAndName> Item)
	{
		return SNew(STextBlock).Text(Item->NiceName);
	})
	.OnSelectionChanged_Lambda([&SelectedSolver](TSharedPtr<FIKRigSolverTypeAndName> Item, ESelectInfo::Type)
	{
		SelectedSolver = Item;
	})
	.Content()
	[
		SNew(STextBlock)
		.MinDesiredWidth(200)
		.Text_Lambda([&SelectedSolver]()
		{
			return SelectedSolver->NiceName;
		})
	];
	
	TSharedRef<SCustomDialog> AddSolverDialog = SNew(SCustomDialog)
		.Title(FText(LOCTEXT("EditorController_IKRigFirstSolver", "Add Default Solver")))
		.Content()
		[
			SolverOptionBox
		]
		.Buttons({
			SCustomDialog::FButton(LOCTEXT("AddSolver", "Add Solver")),
			SCustomDialog::FButton(LOCTEXT("Skip", "Skip"))
	});

	if (AddSolverDialog->ShowModal() != 0)
	{
		return false; // cancel button pressed, or window closed
	}

	if (SelectedSolver->SolverType != nullptr && SolverStackView.IsValid())
	{
		SolverStackView->AddNewSolver(SelectedSolver->SolverType);
	}

	return true;
}

void FIKRigEditorController::ShowDetailsForBone(const FName BoneName) const
{
	BoneDetails->SetBone(BoneName);
	DetailsView->SetObject(BoneDetails);
}

void FIKRigEditorController::ShowDetailsForBoneSettings(const FName& BoneName, int32 SolverIndex) const
{
	if (UObject* BoneSettings = AssetController->GetBoneSettings(BoneName, SolverIndex))
	{
		DetailsView->SetObject(BoneSettings);
	}
}

void FIKRigEditorController::ShowDetailsForGoal(const FName& GoalName) const
{
	DetailsView->SetObject(AssetController->GetGoal(GoalName));
}

void FIKRigEditorController::ShowDetailsForGoalSettings(const FName GoalName, const int32 SolverIndex) const
{
	// get solver that owns this effector
	if (const UIKRigSolver* SolverWithEffector = AssetController->GetSolverAtIndex(SolverIndex))
	{
		if (UObject* EffectorSettings = SolverWithEffector->GetGoalSettings(GoalName))
		{
			DetailsView->SetObject(EffectorSettings);
		}
	}
}

void FIKRigEditorController::ShowDetailsForSolver(const int32 SolverIndex) const
{
	DetailsView->SetObject(AssetController->GetSolverAtIndex(SolverIndex));
}

void FIKRigEditorController::ShowEmptyDetails() const
{
	DetailsView->SetObject(AssetController->GetAsset());
}

void FIKRigEditorController::ShowDetailsForElements(const TArray<TSharedPtr<FIKRigTreeElement>>& InItems) const
{
	if (!InItems.Num())
	{
		ShowEmptyDetails();
		return;
	}

	const TSharedPtr<FIKRigTreeElement>& LastItem = InItems.Last();

	// check is the items are all of the same type
	const bool bContainsSeveralTypes = InItems.ContainsByPredicate( [LastItem](const TSharedPtr<FIKRigTreeElement>& Item)
	{
		return Item->ElementType != LastItem->ElementType;
	});

	// if all elements are similar then treat them once
	if (!bContainsSeveralTypes)
	{
		TArray<TWeakObjectPtr<>> Objects;
		for (const TSharedPtr<FIKRigTreeElement>& Item: InItems)
		{
			TWeakObjectPtr<> Object = Item->GetObject();
			if (Object.IsValid())
			{
				Objects.Add(Object);
			}
		}
		DetailsView->SetObjects(Objects);
		return;
	}

	// fallback to the last selected element
	switch (LastItem->ElementType)
	{
	case IKRigTreeElementType::BONE:
		ShowDetailsForBone(LastItem->BoneName);
		break;
		
	case IKRigTreeElementType::GOAL:
		ShowDetailsForGoal(LastItem->GoalName);
		break;
		
	case IKRigTreeElementType::SOLVERGOAL:
		ShowDetailsForGoalSettings(LastItem->EffectorGoalName, LastItem->EffectorIndex);
		break;
		
	case IKRigTreeElementType::BONE_SETTINGS:
		ShowDetailsForBoneSettings(LastItem->BoneSettingBoneName, LastItem->BoneSettingsSolverIndex);
		break;
		
	default:
		ensure(false);
		break;
	}
}

void FIKRigEditorController::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent) const
{
	const bool bPreviewMeshChanged = PropertyChangedEvent.GetPropertyName() == UIKRigDefinition::GetPreviewMeshPropertyName();
	if (bPreviewMeshChanged)
	{
		USkeletalMesh* Mesh = AssetController->GetSkeletalMesh();
		AssetController->SetSkeletalMesh(Mesh);
	}
}

void FIKRigEditorController::SetDetailsView(const TSharedPtr<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FIKRigEditorController::OnFinishedChangingDetails);
	ShowEmptyDetails();
}

void FIKRigEditorController::PromptToAssignGoalToChain(const FName NewGoalName) const
{
	const UIKRigEffectorGoal* NewGoal = AssetController->GetGoal(NewGoalName);
	check(NewGoal);
	const TArray<FBoneChain>& AllRetargetChains = AssetController->GetRetargetChains();
	FName ChainToAddGoalTo = NAME_None;
	for (const FBoneChain& Chain : AllRetargetChains)
	{
		if (Chain.EndBone == NewGoal->BoneName)
		{
			ChainToAddGoalTo = Chain.ChainName;
		}
	}

	if (ChainToAddGoalTo == NAME_None)
	{
		return;
	}

	TSharedRef<SCustomDialog> AddGoalToChainDialog = SNew(SCustomDialog)
		.Title(FText(LOCTEXT("AssignGoalToNewChainTitle", "Assign Existing Goal to Retarget Chain")))
		.Content()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("AssignGoalToChainLabel", "Assign goal, {0} to retarget chain, {1}?"), FText::FromName(NewGoal->GoalName), FText::FromName(ChainToAddGoalTo)))
		]
		.Buttons({
			SCustomDialog::FButton(LOCTEXT("AssignGoal", "Assign Goal")),
			SCustomDialog::FButton(LOCTEXT("SkipGoal", "Skip Goal"))
	});

	if (AddGoalToChainDialog->ShowModal() != 0)
	{
		return; // cancel button pressed, or window closed
	}

	AssetController->SetRetargetChainGoal(ChainToAddGoalTo, NewGoal->GoalName);
}

FName FIKRigEditorController::PromptToAddNewRetargetChain(FBoneChain& BoneChain) const
{
	TArray<SCustomDialog::FButton> Buttons;
	Buttons.Add(SCustomDialog::FButton(LOCTEXT("AddChain", "Add Chain")));

	const bool bHasExistingGoal = BoneChain.IKGoalName != NAME_None;
	if (bHasExistingGoal)
	{
		Buttons.Add(SCustomDialog::FButton(LOCTEXT("AddChainUsingGoal", "Add Chain using Goal")));
	}
	else
	{
		Buttons.Add(SCustomDialog::FButton(LOCTEXT("AddChainAndGoal", "Add Chain and Goal")));
	}
	
	Buttons.Add(SCustomDialog::FButton(LOCTEXT("Cancel", "Cancel")));

	// ask user if they want to add a goal to the end of this chain
	const TSharedRef<SCustomDialog> AddNewRetargetChainDialog =
		SNew(SCustomDialog)
		.Title(FText(LOCTEXT("AddNewChainTitleLabel", "Add New Retarget Chain")))
		.HAlignContent(HAlign_Center)
		.Content()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("ChainNameLabel", "Chain Name"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
						.MinDesiredWidth(200.0f)
						.OnTextChanged_Lambda([&BoneChain](const FText& InText)
						{
							BoneChain.ChainName = FName(InText.ToString());
						})
						.Text(FText::FromName(BoneChain.ChainName))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("StartBoneLabel", "Start Bone"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromName(BoneChain.StartBone.BoneName))
						.IsReadOnly(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("EndBoneLabel", "End Bone"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromName(BoneChain.EndBone.BoneName))
						.IsReadOnly(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("GoalLabel", "Goal"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
						.Text(FText::FromName(BoneChain.IKGoalName))
						.IsReadOnly(true)
					]
				]
			]
			
		]
		.Buttons(Buttons);

	// show the dialog and handle user choice
	const int32 UserChoice = AddNewRetargetChainDialog->ShowModal();
	if (UserChoice == 2 || UserChoice < 0)
	{
		return NAME_None;  // cancel button pressed, or window closed
	}

	// add the retarget chain
	const FName NewChainName = AssetController->AddRetargetChain(BoneChain);

	// did user choose to assign a goal
	if (UserChoice == 1)
	{
		FName GoalName = NAME_None;
		if (bHasExistingGoal)
		{
			// use the existing goal
			GoalName = BoneChain.IKGoalName;
		}
		else
		{
			// add a default solver if there isn't one already
			PromptToAddDefaultSolver();
			// create new goal
			const FName NewGoalName = FName(FText::Format(LOCTEXT("GoalOnNewChainName", "{0}_Goal"), FText::FromName(BoneChain.ChainName)).ToString());
			GoalName = AssetController->AddNewGoal(NewGoalName, BoneChain.EndBone.BoneName);

			// connect the new goal to selected solvers
			TArray<TSharedPtr<FSolverStackElement>> SelectedSolvers;
			GetSelectedSolvers(SelectedSolvers);
			for (TSharedPtr<FSolverStackElement>& Solver : SelectedSolvers)
			{
				AssetController->ConnectGoalToSolver(GoalName, Solver.Get()->IndexInStack);
			}
		}
		
		// assign the existing goal to it
		AssetController->SetRetargetChainGoal(NewChainName, GoalName);
	}

	RefreshAllViews();
	
	return NewChainName;
}

void FIKRigEditorController::PlayAnimationAsset(UAnimationAsset* AssetToPlay)
{
	if (AssetToPlay && AnimInstance)
	{
		AnimInstance->SetAnimationAsset(AssetToPlay);
	}
}

EIKRigSelectionType FIKRigEditorController::GetLastSelectedType() const
{
	return LastSelectedType;
}

void FIKRigEditorController::SetLastSelectedType(EIKRigSelectionType SelectionType)
{
	LastSelectedType = SelectionType;
}

TObjectPtr<UIKRigBoneDetails> FIKRigEditorController::CreateBoneDetails(const TSharedPtr<FIKRigTreeElement const>& InBoneItem) const
{
	// ensure that the element is related to a bone
	if (InBoneItem->ElementType != IKRigTreeElementType::BONE)
	{
		return nullptr;
	}
	
	// create and store a new one
	UIKRigBoneDetails* NewBoneDetails = NewObject<UIKRigBoneDetails>(AssetController->GetAsset(), FName(InBoneItem->BoneName), RF_Standalone | RF_Transient );
	NewBoneDetails->SelectedBone = InBoneItem->BoneName;
	NewBoneDetails->AnimInstancePtr = AnimInstance;
	NewBoneDetails->AssetPtr = AssetController->GetAsset();
	
	return NewBoneDetails;
}

#undef LOCTEXT_NAMESPACE

