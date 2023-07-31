// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigEditorController.h"

#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Dialog/SCustomDialog.h"
#include "Dialogs/Dialogs.h"

#include "RigEditor/IKRigController.h"
#include "RigEditor/SIKRigHierarchy.h"
#include "RigEditor/SIKRigSolverStack.h"
#include "RigEditor/IKRigAnimInstance.h"
#include "RigEditor/IKRigToolkit.h"
#include "RigEditor/SIKRigAssetBrowser.h"
#include "RigEditor/SIKRigOutputLog.h"
#include "Solvers/IKRig_PBIKSolver.h"

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

	const int32 BoneIndex = AssetPtr->Skeleton.GetBoneIndexFromName(SelectedBone);
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
				GlobalTransform = AssetPtr->Skeleton.CurrentPoseGlobal[BoneIndex];
				LocalTransform = AssetPtr->Skeleton.CurrentPoseLocal[BoneIndex];
			}
			break;
		}
		case EIKRigTransformType::Reference:
		{
			IsRelative = ReferenceTransformRelative;
			GlobalTransform = AssetPtr->Skeleton.RefPoseGlobal[BoneIndex];
			LocalTransform = GlobalTransform;
			const int32 ParentBoneIndex = AssetPtr->Skeleton.ParentIndices[BoneIndex];
			if(ParentBoneIndex != INDEX_NONE)
			{
				const FTransform ParentTransform = AssetPtr->Skeleton.RefPoseGlobal[ParentBoneIndex];;
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

void FIKRigEditorController::Initialize(TSharedPtr<FIKRigEditorToolkit> Toolkit, UIKRigDefinition* IKRigAsset)
{
	EditorToolkit = Toolkit;
	AssetController = UIKRigController::GetIKRigController(IKRigAsset);
	BoneDetails = NewObject<UIKRigBoneDetails>();
	
	// register callback to be informed when rig asset is modified by editor
	if (!AssetController->OnIKRigNeedsInitialized().IsBoundToObject(this))
	{
		ReinitializeDelegateHandle = AssetController->OnIKRigNeedsInitialized().AddSP(
			this, &FIKRigEditorController::OnIKRigNeedsInitialized);

		// Initialize editor's instances at first initialization
		InitializeSolvers();
	}
}

void FIKRigEditorController::Close() const
{
	AssetController->OnIKRigNeedsInitialized().Remove(ReinitializeDelegateHandle);
}

UIKRigProcessor* FIKRigEditorController::GetIKRigProcessor() const
{
	if (AnimInstance.IsValid())
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

void FIKRigEditorController::OnIKRigNeedsInitialized(UIKRigDefinition* ModifiedIKRig)
{
	if (ModifiedIKRig != AssetController->GetAsset())
	{
		return;
	}

	ClearOutputLog();
	
	AnimInstance->SetProcessorNeedsInitialized();

	// Initialize editor's instances on request
	InitializeSolvers();

	// update the bone details so it can pull on the current data
	BoneDetails->AnimInstancePtr = AnimInstance;
	BoneDetails->AssetPtr = ModifiedIKRig;
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
		UIKRigEffectorGoal* NewGoal = AssetController->AddNewGoal(GoalName, BoneName);
		if (!NewGoal)
		{
			continue; // already exists
		}

		// ask user if they want to assign this goal to a chain (if there is one on this bone)
		PromptToAssignGoalToChain(NewGoal);

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

void FIKRigEditorController::GetSelectedSolvers(TArray<TSharedPtr<FSolverStackElement>>& OutSelectedSolvers)
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
		const FBoneChain Chain(FRetargetChainAnalyzer::GetDefaultChainName(), NAME_None, NAME_None);
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
		if (SolverType->SolverType == UIKRigPBIKSolver::StaticClass())
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
	if (UObject* BoneSettings = AssetController->GetSettingsForBone(BoneName, SolverIndex))
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
	if (const UIKRigSolver* SolverWithEffector = AssetController->GetSolver(SolverIndex))
	{
		if (UObject* EffectorSettings = SolverWithEffector->GetGoalSettings(GoalName))
		{
			DetailsView->SetObject(EffectorSettings);
		}
	}
}

void FIKRigEditorController::ShowDetailsForSolver(const int32 SolverIndex) const
{
	DetailsView->SetObject(AssetController->GetSolver(SolverIndex));
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

void FIKRigEditorController::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const bool bPreviewChanged = PropertyChangedEvent.GetPropertyName() == UIKRigDefinition::GetPreviewMeshPropertyName();
	
	if (bPreviewChanged)
	{
		// set the source and target skeletal meshes on the component
		// NOTE: this must be done AFTER setting the AnimInstance so that the correct root anim node is loaded
		USkeletalMesh* NewMesh = AssetController->GetAsset()->GetPreviewMesh();
		if (NewMesh)
		{
			// apply the mesh to the preview scene
			TSharedRef<IPersonaPreviewScene> PreviewScene = EditorToolkit.Pin()->GetPersonaToolkit()->GetPreviewScene();
			if (PreviewScene->GetPreviewMesh() != NewMesh)
			{
				PreviewScene->SetPreviewMeshComponent(SkelMeshComponent);
				PreviewScene->SetPreviewMesh(NewMesh);
			}

			ClearOutputLog();
			AssetController->SetSkeletalMesh(NewMesh);
			AnimInstance->SetProcessorNeedsInitialized();
			AnimInstance->InitializeAnimation();
			AssetController->BroadcastNeedsReinitialized();
			AssetController->ResetGoalTransforms();
			RefreshAllViews();
		}
	}
}

void FIKRigEditorController::SetDetailsView(const TSharedPtr<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FIKRigEditorController::OnFinishedChangingDetails);
	ShowEmptyDetails();
}

void FIKRigEditorController::PromptToAssignGoalToChain(UIKRigEffectorGoal* NewGoal) const
{
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

FName FIKRigEditorController::PromptToAddNewRetargetChain(const FBoneChain& BoneChain) const
{
	const TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(FIKRigRetargetChainSettings::StaticStruct(), (uint8*)&BoneChain));
	TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
	KismetInspector->ShowSingleStruct(StructToDisplay);

	FName NewChainName = NAME_None;
	SGenericDialogWidget::FArguments DialogArguments;
	DialogArguments.OnOkPressed_Lambda([&BoneChain, &NewChainName, this] ()
	{
		// prompt to add a goal to the chain (skipped if already has a goal)
		UIKRigEffectorGoal* NewGoal = PromptToAddGoalToNewChain(BoneChain);

		// add the retarget chain
		NewChainName = AssetController->AddRetargetChain(BoneChain);

		// ask user if they want to assign this goal to a chain (if there is one on this bone)
		if (NewGoal)
		{
			PromptToAssignGoalToChain(NewGoal);
		}
		
		RefreshAllViews();
	});

	SGenericDialogWidget::OpenDialog(
		LOCTEXT("SIKRigRetargetChains", "Add New Retarget Chain"),
		KismetInspector,
		DialogArguments,
		true);

	return NewChainName;
}

UIKRigEffectorGoal* FIKRigEditorController::PromptToAddGoalToNewChain(const FBoneChain& BoneChain) const
{
	// check if there is already a goal on the end bone of this chain, if there is, then we're done
	UIKRigEffectorGoal* ExistingGoal = AssetController->GetGoalForBone(BoneChain.EndBone.BoneName);
	if (ExistingGoal)
	{
		return ExistingGoal;
	}

	// check if there is a corresponding bone, if there is none, then no need to create a goal
	const FIKRigSkeleton& IKRigSkeleton = AssetController->GetIKRigSkeleton();
	if (IKRigSkeleton.GetBoneIndexFromName(BoneChain.EndBone.BoneName) == INDEX_NONE)
	{
		return nullptr;
	}

	// ask user if they want to add a goal to the end of this chain
	const TSharedRef<SCustomDialog> AddGoalToChainDialog = SNew(SCustomDialog)
		.Title(FText(LOCTEXT("AddGoalToNewChainTitleLabel", "Add Goal to New Chain")))
		.Content()
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("AddGoalToNewChainLabel", "Add goal to end bone, {0} of new chain, {1}?"),
				FText::FromName(BoneChain.EndBone.BoneName), 
				FText::FromName(BoneChain.ChainName)))
		]
		.Buttons({
			SCustomDialog::FButton(LOCTEXT("AddGoal", "Add Goal")),
			SCustomDialog::FButton(LOCTEXT("NoGoal", "No Goal"))
	});

	if (AddGoalToChainDialog->ShowModal() != 0)
	{
		return nullptr; // cancel button pressed, or window closed
	}

	// add a default solver if there isn't one already
	PromptToAddDefaultSolver();

	// add the goal
	const FName NewGoalName = FName(FText::Format(LOCTEXT("GoalOnNewChainName", "{0}_Goal"), FText::FromName(BoneChain.ChainName)).ToString());
	return AssetController->AddNewGoal(NewGoalName, BoneChain.EndBone.BoneName);
}

void FIKRigEditorController::PlayAnimationAsset(UAnimationAsset* AssetToPlay)
{
	if (AssetToPlay && AnimInstance.IsValid())
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

void FIKRigEditorController::InitializeSolvers() const
{
	if (AssetController)
	{
		const FIKRigSkeleton& IKRigSkeleton = AssetController->GetIKRigSkeleton();
		const TArray<UIKRigSolver*>& Solvers = AssetController->GetSolverArray(); 
		for (UIKRigSolver* Solver: Solvers)
		{
			Solver->Initialize(IKRigSkeleton);
		}
	}
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

