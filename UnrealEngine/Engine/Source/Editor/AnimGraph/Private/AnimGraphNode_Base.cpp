// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimGraphNode_Base.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimInstance.h"
#include "AnimationGraphSchema.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "AnimBlueprintNodeOptionalPinManager.h"
#include "IAnimNodeEditMode.h"
#include "AnimNodeEditModes.h"
#include "AnimationGraph.h"
#include "EditorModeManager.h"

#include "AnimationEditorUtils.h"
#include "UObject/UnrealType.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AnimBlueprintCompiler.h"
#include "AnimBlueprintExtension_Base.h"
#include "AnimBlueprintExtension_Attributes.h"
#include "AnimBlueprintExtension_PropertyAccess.h"
#include "AnimBlueprintExtension_Tag.h"
#include "IAnimBlueprintCompilationContext.h"
#include "AnimBlueprintCompilationContext.h"
#include "AnimBlueprintExtension.h"
#include "FindInBlueprintManager.h"
#include "IPropertyAccessBlueprintBinding.h"
#include "IPropertyAccessEditor.h"
#include "ScopedTransaction.h"
#include "Algo/Accumulate.h"
#include "Fonts/FontMeasure.h"
#include "UObject/ReleaseObjectVersion.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Fonts/FontMeasure.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "UAnimGraphNode_Base"

/////////////////////////////////////////////////////
// UAnimGraphNode_Base

UAnimGraphNode_Base::UAnimGraphNode_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_Base::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	TUniquePtr<IAnimBlueprintCompilationContext> CompilationContext = IAnimBlueprintCompilationContext::Get(CompilerContext);
	UAnimBlueprintExtension_Base* Extension = UAnimBlueprintExtension_Base::GetExtension<UAnimBlueprintExtension_Base>(GetAnimBlueprint());
	Extension->CreateEvaluationHandlerForNode(*CompilationContext.Get(), this);
}

void UAnimGraphNode_Base::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin))
	{
		FOptionalPinManager::CacheShownPins(ShowPinForProperties, OldShownPins);
	}
}

void UAnimGraphNode_Base::PostEditUndo()
{
	Super::PostEditUndo();

	if(HasValidBlueprint())
	{
		// Node may have been removed or added by undo/redo so make sure extensions are refreshed
		GetAnimBlueprint()->RequestRefreshExtensions();
	}
}

void UAnimGraphNode_Base::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin) && MemberPropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_Base, ShowPinForProperties))
	{
		FOptionalPinManager::EvaluateOldShownPins(ShowPinForProperties, OldShownPins, this);
		GetSchema()->ReconstructNode(*this);
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_Base, InitialUpdateFunction))
	{
		GetFNode()->InitialUpdateFunction.SetFromFunction(InitialUpdateFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
		GetAnimBlueprint()->RequestRefreshExtensions();
		GetSchema()->ReconstructNode(*this);
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_Base, BecomeRelevantFunction))
	{
		GetFNode()->BecomeRelevantFunction.SetFromFunction(BecomeRelevantFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
		GetAnimBlueprint()->RequestRefreshExtensions();
		GetSchema()->ReconstructNode(*this);
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_Base, UpdateFunction))
	{
		GetFNode()->UpdateFunction.SetFromFunction(UpdateFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
		GetSchema()->ReconstructNode(*this);
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_Base, Tag))
	{
		GetAnimBlueprint()->RequestRefreshExtensions();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	PropertyChangeEvent.Broadcast(PropertyChangedEvent);
}

void UAnimGraphNode_Base::SetPinVisibility(bool bInVisible, int32 InOptionalPinIndex)
{
	if(ShowPinForProperties[InOptionalPinIndex].bShowPin != bInVisible)
	{
		FOptionalPinManager::CacheShownPins(ShowPinForProperties, OldShownPins);

		ShowPinForProperties[InOptionalPinIndex].bShowPin = bInVisible;
		
		FOptionalPinManager::EvaluateOldShownPins(ShowPinForProperties, OldShownPins, this);
		ReconstructNode();

		PinVisibilityChangedEvent.Broadcast(bInVisible, InOptionalPinIndex);
	}
}

void UAnimGraphNode_Base::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::AnimationGraphNodeBindingsDisplayedAsPins)
		{
			// Push any bindings to optional pins
			bool bPushedBinding = false;
			for(const TPair<FName, FAnimGraphNodePropertyBinding>& BindingPair : PropertyBindings)
			{
				for(FOptionalPinFromProperty& OptionalPin : ShowPinForProperties)
				{
					if(OptionalPin.bCanToggleVisibility && !OptionalPin.bShowPin && OptionalPin.PropertyName == BindingPair.Key)
					{
						OptionalPin.bShowPin = true;
						bPushedBinding = true;
					}
				}
			}

			if(bPushedBinding)
			{
				FOptionalPinManager::EvaluateOldShownPins(ShowPinForProperties, OldShownPins, this);
			}
		}
	}
}

void UAnimGraphNode_Base::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	// This makes sure that all anim BP extensions are registered that this node needs
	UAnimBlueprintExtension::RequestExtensionsForNode(this);
}

void UAnimGraphNode_Base::PostPasteNode()
{
	Super::PostPasteNode();

	// This makes sure that all anim BP extensions are registered that this node needs
	UAnimBlueprintExtension::RequestExtensionsForNode(this);
}

void UAnimGraphNode_Base::DestroyNode()
{
	// This node may have been the last using its extension, so refresh
	GetAnimBlueprint()->RequestRefreshExtensions();
	
	// Cleanup the pose watch if one exists on this node
	AnimationEditorUtils::RemovePoseWatchFromNode(this, GetAnimBlueprint());

	Super::DestroyNode();
}

void UAnimGraphNode_Base::CreateOutputPins()
{
	if (!IsSinkNode())
	{
		CreatePin(EGPD_Output, UAnimationGraphSchema::PC_Struct, FPoseLink::StaticStruct(), TEXT("Pose"));
	}
}

void UAnimGraphNode_Base::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	// Validate any bone references we have
	for(const TPair<FStructProperty*, const void*>& PropertyValuePair : TPropertyValueRange<FStructProperty>(GetClass(), this))
	{
		if(PropertyValuePair.Key->Struct == FBoneReference::StaticStruct())
		{
			const FBoneReference& BoneReference = *(const FBoneReference*)PropertyValuePair.Value;

			// Temporary fix where skeleton is not fully loaded during AnimBP compilation and thus virtual bone name check is invalid UE-39499 (NEED FIX) 
			if (ForSkeleton && !ForSkeleton->HasAnyFlags(RF_NeedPostLoad))
			{
				if (BoneReference.BoneName != NAME_None)
				{
					if (ForSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneReference.BoneName) == INDEX_NONE)
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("BoneName"), FText::FromName(BoneReference.BoneName));

						MessageLog.Warning(*FText::Format(LOCTEXT("NoBoneFoundToModify", "@@ - Bone {BoneName} not found in Skeleton"), Args).ToString(), this);
					}
				}
			}
		}
	}

	auto ValidateFunctionRef = [this, &MessageLog](const FMemberReference& InRef, const FText& InFunctionName)
	{
		if(InRef.GetMemberName() != NAME_None)
		{
			UFunction* Function = InRef.ResolveMember<UFunction>(GetAnimBlueprint()->SkeletonGeneratedClass);
			if(Function == nullptr)
			{
				MessageLog.Error(*FText::Format(LOCTEXT("CouldNotResolveFunctionErrorFormat", "Could not resolve {0} function @@"), InFunctionName).ToString(), this);
			}
			else
			{
				if(!FBlueprintEditorUtils::HasFunctionBlueprintThreadSafeMetaData(Function))
				{
					MessageLog.Error(*FText::Format(LOCTEXT("FunctionThreadSafetyErrorFormat", "{0} function is not thread safe @@"), InFunctionName).ToString(), this);
				}
			}
		}
	};
	
	bool bBaseClassIsExperimental = false;
	bool bBaseClassIsEarlyAccess = false;
	FString MostDerivedDevelopmentClassName;
	FObjectEditorUtils::GetClassDevelopmentStatus(GetClass(), bBaseClassIsExperimental, bBaseClassIsEarlyAccess, MostDerivedDevelopmentClassName);
	if (bBaseClassIsExperimental)
	{
		MessageLog.Note(*(LOCTEXT("ExperimentalNode", "@@ - Node is experimental")).ToString(), this);
	}
	if (bBaseClassIsEarlyAccess)
	{
		MessageLog.Note(*(LOCTEXT("EarlyAccessNode", "@@ - Node is in early access")).ToString(), this);
	}

	ValidateFunctionRef(InitialUpdateFunction, LOCTEXT("InitialUpdateFunctionName", "Initial Update"));
	ValidateFunctionRef(BecomeRelevantFunction, LOCTEXT("BecomeRelevantFunctionName", "Become Relevant"));
	ValidateFunctionRef(UpdateFunction, LOCTEXT("UpdateFunctionName", "Update"));
}

void UAnimGraphNode_Base::CopyTermDefaultsToDefaultObject(IAnimBlueprintCopyTermDefaultsContext& InCompilationContext, IAnimBlueprintNodeCopyTermDefaultsContext& InPerNodeContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	InPerNodeContext.GetTargetProperty()->CopyCompleteValue(InPerNodeContext.GetDestinationPtr(), InPerNodeContext.GetSourcePtr());

	OnCopyTermDefaultsToDefaultObject(InCompilationContext, InPerNodeContext, OutCompiledData);
}

void UAnimGraphNode_Base::OverrideAssets(IAnimBlueprintNodeOverrideAssetsContext& InContext) const
{
	if(InContext.GetAssets().Num() > 0)
	{
		if(UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(InContext.GetAssets()[0]))
		{
			// Call the legacy implementation
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			InContext.GetAnimNode<FAnimNode_Base>().OverrideAsset(AnimationAsset);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
	OnOverrideAssets(InContext);
}

void UAnimGraphNode_Base::InternalPinCreation(TArray<UEdGraphPin*>* OldPins)
{
	// preload required assets first before creating pins
	PreloadRequiredAssets();

	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
	if (const FStructProperty* NodeStruct = GetFNodeProperty())
	{
		// Display any currently visible optional pins
		{
			UObject* NodeDefaults = GetArchetype();
			FAnimBlueprintNodeOptionalPinManager OptionalPinManager(this, OldPins);
			OptionalPinManager.AllocateDefaultPins(NodeStruct->Struct, NodeStruct->ContainerPtrToValuePtr<uint8>(this), NodeDefaults ? NodeStruct->ContainerPtrToValuePtr<uint8>(NodeDefaults) : nullptr);
		}

		// Create the output pin, if needed
		CreateOutputPins();
	}

	if(HasValidBlueprint())
	{
		// Update any binding's display text
		IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
		for(TPair<FName, FAnimGraphNodePropertyBinding>& BindingPair : PropertyBindings)
		{
			BindingPair.Value.PathAsText = PropertyAccessEditor.MakeTextPath(BindingPair.Value.PropertyPath, GetBlueprint()->SkeletonGeneratedClass);
		}
	}
}

void UAnimGraphNode_Base::AllocateDefaultPins()
{
	InternalPinCreation(nullptr);

	CreateCustomPins(nullptr);
}

void UAnimGraphNode_Base::RecalculateBindingType(FAnimGraphNodePropertyBinding& InBinding)
{
	if(FProperty* BindingProperty = GetPinProperty(InBinding.PropertyName))
	{
		// Use the inner for array properties
		if(BindingProperty->IsA<FArrayProperty>() && InBinding.ArrayIndex != INDEX_NONE)
		{
			BindingProperty = CastFieldChecked<FArrayProperty>(BindingProperty)->Inner;
		}
		
		UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();

		IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
		const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
		
		FProperty* LeafProperty = nullptr;
		int32 ArrayIndex = INDEX_NONE;
		FPropertyAccessResolveResult Result = PropertyAccessEditor.ResolvePropertyAccess(AnimBlueprint->SkeletonGeneratedClass, InBinding.PropertyPath, LeafProperty, ArrayIndex);
		if(Result.Result == EPropertyAccessResolveResult::Succeeded)
		{
			if(LeafProperty)
			{
				Schema->ConvertPropertyToPinType(LeafProperty, InBinding.PinType);
				
				if(PropertyAccessEditor.GetPropertyCompatibility(LeafProperty, BindingProperty) == EPropertyAccessCompatibility::Promotable)
				{
					InBinding.bIsPromotion = true;
					Schema->ConvertPropertyToPinType(LeafProperty, InBinding.PromotedPinType);
				}
				else
				{
					InBinding.bIsPromotion = false;
					InBinding.PromotedPinType = InBinding.PinType;
				}
			}
		}
	}
}

void UAnimGraphNode_Base::ReconstructNode()
{
	if(HasValidBlueprint())
	{
		// Refresh bindings
		for(auto& BindingPair : PropertyBindings)
		{
			RecalculateBindingType(BindingPair.Value);
		}
	}
	
	Super::ReconstructNode();
}

void UAnimGraphNode_Base::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	InternalPinCreation(&OldPins);

	CreateCustomPins(&OldPins);

	RestoreSplitPins(OldPins);
}

bool UAnimGraphNode_Base::CanJumpToDefinition() const
{
	return GetJumpTargetForDoubleClick() != nullptr;
}

void UAnimGraphNode_Base::JumpToDefinition() const
{
	if (UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(HyperlinkTarget);
	}
}

FLinearColor UAnimGraphNode_Base::GetNodeTitleColor() const
{
	return FLinearColor::Black;
}

UScriptStruct* UAnimGraphNode_Base::GetFNodeType() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<FProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp->Struct;
			}
		}
	}

	return NULL;
}

FStructProperty* UAnimGraphNode_Base::GetFNodeProperty() const
{
	UScriptStruct* BaseFStruct = FAnimNode_Base::StaticStruct();

	for (TFieldIterator<FProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
		{
			if (StructProp->Struct->IsChildOf(BaseFStruct))
			{
				return StructProp;
			}
		}
	}

	return NULL;
}

FAnimNode_Base* UAnimGraphNode_Base::GetFNode()
{
	if(FStructProperty* Property = GetFNodeProperty())
	{
		return Property->ContainerPtrToValuePtr<FAnimNode_Base>(this);
	}

	return nullptr;
}

FString UAnimGraphNode_Base::GetNodeCategory() const
{
	return TEXT("Misc.");
}

void UAnimGraphNode_Base::GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const
{
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Type" ), TEXT( "AnimGraphNode" ) ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Class" ), GetClass()->GetName() ));
	OutNodeAttributes.Add( TKeyValuePair<FString, FString>( TEXT( "Name" ), GetName() ));
}

void UAnimGraphNode_Base::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UAnimGraphNode_Base::GetMenuCategory() const
{
	return FText::FromString(GetNodeCategory());
}

void UAnimGraphNode_Base::GetPinAssociatedProperty(const UScriptStruct* NodeType, const UEdGraphPin* InputPin, FProperty*& OutProperty, int32& OutIndex) const
{
	OutProperty = nullptr;
	OutIndex = INDEX_NONE;

	//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
	const FString PinNameStr = InputPin->PinName.ToString();
	const int32 UnderscoreIndex = PinNameStr.Find(TEXT("_"), ESearchCase::CaseSensitive);
	if (UnderscoreIndex != INDEX_NONE)
	{
		const FString ArrayName = PinNameStr.Left(UnderscoreIndex);

		if (FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(NodeType, *ArrayName))
		{
			const int32 ArrayIndex = FCString::Atoi(*(PinNameStr.Mid(UnderscoreIndex + 1)));

			OutProperty = ArrayProperty;
			OutIndex = ArrayIndex;
		}
	}
	
	// If the array check failed or we have no underscores
	if(OutProperty == nullptr)
	{
		if (FProperty* Property = FindFProperty<FProperty>(NodeType, InputPin->PinName))
		{
			OutProperty = Property;
			OutIndex = INDEX_NONE;
		}
	}
}

FPoseLinkMappingRecord UAnimGraphNode_Base::GetLinkIDLocation(const UScriptStruct* NodeType, UEdGraphPin* SourcePin)
{
	if (SourcePin->LinkedTo.Num() > 0)
	{
		if (UAnimGraphNode_Base* LinkedNode = Cast<UAnimGraphNode_Base>(FBlueprintEditorUtils::FindFirstCompilerRelevantNode(SourcePin->LinkedTo[0])))
		{
			//@TODO: Name-based hackery, avoid the roundtrip and better indicate when it's an array pose pin
			const FString SourcePinName = SourcePin->PinName.ToString();
			const int32 UnderscoreIndex = SourcePinName.Find(TEXT("_"), ESearchCase::CaseSensitive);
			if (UnderscoreIndex != INDEX_NONE)
			{
				const FString ArrayName = SourcePinName.Left(UnderscoreIndex);

				if (FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(NodeType, *ArrayName))
				{
					if (FStructProperty* Property = CastField<FStructProperty>(ArrayProperty->Inner))
					{
						if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
						{
							const int32 ArrayIndex = FCString::Atoi(*(SourcePinName.Mid(UnderscoreIndex + 1)));
							return FPoseLinkMappingRecord::MakeFromArrayEntry(this, LinkedNode, ArrayProperty, ArrayIndex);
						}
					}
				}
			}
			else
			{
				if (FStructProperty* Property = FindFProperty<FStructProperty>(NodeType, SourcePin->PinName))
				{
					if (Property->Struct->IsChildOf(FPoseLinkBase::StaticStruct()))
					{
						return FPoseLinkMappingRecord::MakeFromMember(this, LinkedNode, Property);
					}
				}
			}
		}
	}

	return FPoseLinkMappingRecord::MakeInvalid();
}

void UAnimGraphNode_Base::CreatePinsForPoseLink(FProperty* PoseProperty, int32 ArrayIndex)
{
	UScriptStruct* A2PoseStruct = FA2Pose::StaticStruct();

	// pose input
	const FName NewPinName = (ArrayIndex == INDEX_NONE) ? PoseProperty->GetFName() : *FString::Printf(TEXT("%s_%d"), *(PoseProperty->GetName()), ArrayIndex);
	CreatePin(EGPD_Input, UAnimationGraphSchema::PC_Struct, A2PoseStruct, NewPinName);
}

void UAnimGraphNode_Base::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	if (Pin->Direction == EGPD_Output)
	{
		if (Pin->PinName == TEXT("Pose"))
		{
			DisplayName.Reset();
		}
	}
}

bool UAnimGraphNode_Base::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UAnimationGraphSchema::StaticClass());
}

FString UAnimGraphNode_Base::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Animation");
}

void UAnimGraphNode_Base::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	if (UAnimationGraphSchema::IsLocalSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose");
	}
	else if (UAnimationGraphSchema::IsComponentSpacePosePin(Pin.PinType))
	{
		HoverTextOut = TEXT("Animation Pose (Component Space)");
	}
	else
	{
		Super::GetPinHoverText(Pin, HoverTextOut);
	}
}

void UAnimGraphNode_Base::ProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	UAnimBlueprintExtension_Base* Extension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_Base>(GetAnimBlueprint());

	// Record pose pins for later patchup and gather pins that have an associated evaluation handler
	Extension->ProcessNodePins(this, InCompilationContext, OutCompiledData);

	// Resolve functions
	GetFNode()->InitialUpdateFunction.SetFromFunction(InitialUpdateFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
	GetFNode()->BecomeRelevantFunction.SetFromFunction(BecomeRelevantFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
	GetFNode()->UpdateFunction.SetFromFunction(UpdateFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode())); 

	// Insert tag, if any
	if(Tag != NAME_None)
	{
		UAnimBlueprintExtension_Tag* TagExtension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_Tag>(GetAnimBlueprint());
		TagExtension->AddTaggedNode(this, InCompilationContext);
	}
	
	// Call the override point
	OnProcessDuringCompilation(InCompilationContext, OutCompiledData);
}

void UAnimGraphNode_Base::HandleAnimReferenceCollection(UAnimationAsset* AnimAsset, TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(AnimAsset)
	{
		AnimAsset->HandleAnimReferenceCollection(AnimationAssets, true);
	}
}

void UAnimGraphNode_Base::OnNodeSelected(bool bInIsSelected, FEditorModeTools& InModeTools, FAnimNode_Base* InRuntimeNode)
{
	const FEditorModeID ModeID = GetEditorMode();
	if (ModeID != NAME_None)
	{
		if (bInIsSelected)
		{
			InModeTools.ActivateMode(ModeID);
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				static_cast<IAnimNodeEditMode*>(EdMode)->EnterMode(this, InRuntimeNode);
			}
		}
		else
		{
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				static_cast<IAnimNodeEditMode*>(EdMode)->ExitMode();
			}
			InModeTools.DeactivateMode(ModeID);
		}
	}
}

void UAnimGraphNode_Base::OnPoseWatchChanged(const bool IsPoseWatchEnabled, TObjectPtr<class UPoseWatch> InPoseWatch, FEditorModeTools& InModeTools, FAnimNode_Base* InRuntimeNode)
{
	const FEditorModeID ModeID = GetEditorMode();
	if (ModeID != NAME_None)
	{
		if (IsPoseWatchEnabled)
		{
			InModeTools.ActivateMode(ModeID);
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				const bool bSupportsPoseWatch =
					   static_cast<IAnimNodeEditMode*>(EdMode)->SupportsPoseWatch()
					|| EdMode->GetID() == AnimNodeEditModes::AnimNode;

				if (bSupportsPoseWatch)
				{
					static_cast<IAnimNodeEditMode*>(EdMode)->RegisterPoseWatchedNode(this, InRuntimeNode);
				}
				else
				{
					InModeTools.DeactivateMode(ModeID);
				}
			}
		}
		else
		{
			if (FEdMode* EdMode = InModeTools.GetActiveMode(ModeID))
			{
				static_cast<IAnimNodeEditMode*>(EdMode)->ExitMode();
			}
			InModeTools.DeactivateMode(ModeID);
		}
	}
}

FEditorModeID UAnimGraphNode_Base::GetEditorMode() const
{
	return AnimNodeEditModes::AnimNode;
}

void UAnimGraphNode_Base::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp, const bool IsSelected, const bool IsPoseWatchEnabled) const
{
	if (IsSelected)
	{
		Draw(PDI, PreviewSkelMeshComp); 
	}
}

FAnimNode_Base* UAnimGraphNode_Base::FindDebugAnimNode(USkeletalMeshComponent* InPreviewSkelMeshComp) const
{
	FAnimNode_Base* DebugNode = nullptr;

	if (InPreviewSkelMeshComp != nullptr)
	{
		auto FindDebugNode = [this](UAnimInstance* InAnimInstance) -> FAnimNode_Base* 
		{
			if (!InAnimInstance)
			{
				return nullptr;
			}
			UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(InAnimInstance->GetClass());
			if (!AnimBlueprintClass)
			{
				return nullptr;
			}
			
			FAnimBlueprintDebugData& DebugData = AnimBlueprintClass->GetAnimBlueprintDebugData();
			int32* IndexPtr = DebugData.NodePropertyToIndexMap.Find(this);
			if (!IndexPtr)
			{
				return nullptr;
			}
			
			int32 AnimNodeIndex = *IndexPtr;
			// reverse node index temporarily because of a bug in NodeGuidToIndexMap
			AnimNodeIndex = AnimBlueprintClass->GetAnimNodeProperties().Num() - AnimNodeIndex - 1;

			return AnimBlueprintClass->GetAnimNodeProperties()[AnimNodeIndex]->ContainerPtrToValuePtr<FAnimNode_Base>(InAnimInstance);
		};
	
		// Try to find this node on the anim BP.
		DebugNode = FindDebugNode(InPreviewSkelMeshComp->GetAnimInstance());
		
		// Failing that, try the post-process BP instead.
		if (!DebugNode)
		{
			DebugNode = FindDebugNode(InPreviewSkelMeshComp->GetPostProcessInstance());			
		}
	}

	return DebugNode;
}

EAnimAssetHandlerType UAnimGraphNode_Base::SupportsAssetClass(const UClass* AssetClass) const
{
	return EAnimAssetHandlerType::NotSupported;
}


void UAnimGraphNode_Base::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	CopyPinDefaultsToNodeData(Pin);

	if(UAnimationGraph* AnimationGraph = Cast<UAnimationGraph>(GetGraph()))
	{
		AnimationGraph->OnPinDefaultValueChanged.Broadcast(Pin);
	}
}

FString UAnimGraphNode_Base::GetPinMetaData(FName InPinName, FName InKey)
{
	FString MetaData = Super::GetPinMetaData(InPinName, InKey);
	if(MetaData.IsEmpty())
	{
		// Check properties of our anim node
		if(FStructProperty* NodeStructProperty = GetFNodeProperty())
		{
			for (TFieldIterator<FProperty> It(NodeStructProperty->Struct); It; ++It)
			{
				const FProperty* Property = *It;
				if (Property && Property->GetFName() == InPinName)
				{
					return Property->GetMetaData(InKey);
				}
			}
		}
	}
	return MetaData;
}

void UAnimGraphNode_Base::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	const auto ConditionallyTagNodeFuncRef = [&OutTaggedMetaData](const FMemberReference& FuncMember, const FText& LocText)
	{
		if (IsPotentiallyBoundFunction(FuncMember))
		{
			const FText FunctionName = FText::FromName(FuncMember.GetMemberName());
			OutTaggedMetaData.Add(FSearchTagDataPair(LocText, FunctionName));
		}
	};

	// Conditionally include anim node function references as part of the node's search metadata
	ConditionallyTagNodeFuncRef(InitialUpdateFunction, LOCTEXT("InitialUpdateFunctionName", "Initial Update"));
	ConditionallyTagNodeFuncRef(BecomeRelevantFunction, LOCTEXT("BecomeRelevantFunctionName", "Become Relevant"));
	ConditionallyTagNodeFuncRef(UpdateFunction, LOCTEXT("UpdateFunctionName", "Update"));

	if(Tag != NAME_None)
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(LOCTEXT("Tag", "Tag"), FText::FromName(Tag)));
	}
}

void UAnimGraphNode_Base::AddPinSearchMetaDataInfo(const UEdGraphPin* InPin, TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddPinSearchMetaDataInfo(InPin, OutTaggedMetaData);
	
	FName BindingName;
	FProperty* PinProperty;
	int32 OptionalPinIndex;
	if(GetPinBindingInfo(InPin->GetFName(), BindingName, PinProperty, OptionalPinIndex))
	{
		if(const FAnimGraphNodePropertyBinding* BindingInfo = PropertyBindings.Find(BindingName))
		{
			OutTaggedMetaData.Add(FSearchTagDataPair(FText::FromString(TEXT("Binding")), BindingInfo->PathAsText));
		}
	}
}

bool UAnimGraphNode_Base::IsPinExposedAndLinked(const FString& InPinName, const EEdGraphPinDirection InDirection) const
{
	UEdGraphPin* Pin = FindPin(InPinName, InDirection);
	return Pin != nullptr && Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0] != nullptr;
}

bool UAnimGraphNode_Base::IsPinExposedAndBound(const FString& InPinName, const EEdGraphPinDirection InDirection) const
{
	UEdGraphPin* Pin = FindPin(InPinName, InDirection);
	return Pin != nullptr && Pin->LinkedTo.Num() == 0 && PropertyBindings.Find(Pin->GetFName()) != nullptr;
}

bool UAnimGraphNode_Base::IsPinUnlinkedUnboundAndUnset(const FString& InPinName, const EEdGraphPinDirection InDirection) const
{
	UEdGraphPin* Pin = FindPin(InPinName, InDirection);
	if (Pin)
	{
		return Pin->LinkedTo.Num() == 0 && PropertyBindings.Find(Pin->GetFName()) == nullptr && Pin->DefaultValue == Pin->AutogeneratedDefaultValue;
	}
	return false;
}

bool UAnimGraphNode_Base::IsPinBindable(const UEdGraphPin* InPin) const
{
	if(const FProperty* PinProperty = GetPinProperty(InPin->GetFName()))
	{
		const int32 OptionalPinIndex = ShowPinForProperties.IndexOfByPredicate([PinProperty](const FOptionalPinFromProperty& InOptionalPin)
		{
			return PinProperty->GetFName() == InOptionalPin.PropertyName;
		});

		return OptionalPinIndex != INDEX_NONE;
	}

	return false;
}

bool UAnimGraphNode_Base::GetPinBindingInfo(FName InPinName, FName& OutBindingName, FProperty*& OutPinProperty, int32& OutOptionalPinIndex) const
{
	OutPinProperty = GetPinProperty(InPinName);
	if(OutPinProperty)
	{
		OutOptionalPinIndex = ShowPinForProperties.IndexOfByPredicate([OutPinProperty](const FOptionalPinFromProperty& InOptionalPin)
		{
			return OutPinProperty->GetFName() == InOptionalPin.PropertyName;
		});

		OutBindingName = InPinName;
		return OutOptionalPinIndex != INDEX_NONE;
	}

	return false;
}

bool UAnimGraphNode_Base::HasBinding(FName InBindingName) const
{
	// Comparison without name index to deal with arrays
	const FName ComparisonName = FName(InBindingName, 0);

	for(const TPair<FName, FAnimGraphNodePropertyBinding>& BindingPair : PropertyBindings)
	{
		if(ComparisonName == FName(BindingPair.Key, 0))
		{
			return true;
		}
	}

	return false;
}

void UAnimGraphNode_Base::SetTag(FName InTag)
{
	FName OldTag = Tag;
	Tag = InTag;
	if(Tag != OldTag)
	{
		GetAnimBlueprint()->RequestRefreshExtensions();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetAnimBlueprint());
	}
}

FProperty* UAnimGraphNode_Base::GetPinProperty(const UEdGraphPin* InPin) const
{
	return GetPinProperty(InPin->GetFName());
}


FProperty* UAnimGraphNode_Base::GetPinProperty(FName InPinName) const
{
	// Compare FName without number to make sure we catch array properties that are split into multiple pins
	FName ComparisonName = InPinName;
	ComparisonName.SetNumber(0);
	
	return GetFNodeType()->FindPropertyByName(ComparisonName);
}

void UAnimGraphNode_Base::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if(Pin->LinkedTo.Num() > 0)
	{
		// If we have links, clear any bindings
		// Compare FName without number to make sure we catch array properties that are split into multiple pins
		FName ComparisonName = Pin->GetFName();
		ComparisonName.SetNumber(0);

		for(auto Iter = PropertyBindings.CreateIterator(); Iter; ++Iter)
		{
			if(ComparisonName == FName(Iter.Key(), 0))
			{
				Iter.RemoveCurrent();
			}
		}
	}
}

void UAnimGraphNode_Base::AutowireNewNode(UEdGraphPin* FromPin)
{
	// Ensure the pin is valid, a pose pin, and has a single link
	if (FromPin && UAnimationGraphSchema::IsPosePin(FromPin->PinType))
	{
		auto FindFirstPosePinInDirection = [this](EEdGraphPinDirection Direction) -> UEdGraphPin*
		{
			UEdGraphPin** PinToConnectTo = Pins.FindByPredicate([Direction](UEdGraphPin* Pin) -> bool
            {
                return Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType);
            });

			return PinToConnectTo ? *PinToConnectTo : nullptr;
		};
		
		// Get the linked pin, if valid, and ensure it iss also a pose pin
		UEdGraphPin* LinkedPin = FromPin->LinkedTo.Num() == 1 ? FromPin->LinkedTo[0] : nullptr;
		if (LinkedPin && UAnimationGraphSchema::IsPosePin(LinkedPin->PinType))
		{
			// Retrieve the first pin, of similar direction, from this node
			UEdGraphPin* PinToConnectTo = FindFirstPosePinInDirection(FromPin->Direction);
			if (PinToConnectTo)
			{
				ensure(GetSchema()->TryCreateConnection(LinkedPin, PinToConnectTo));
			}
		}

		// Link this node to the FromPin, so find the first pose pin of opposite direction on this node
		UEdGraphPin* PinToConnectTo = FindFirstPosePinInDirection(FromPin->Direction == EEdGraphPinDirection::EGPD_Input ? EEdGraphPinDirection::EGPD_Output : EEdGraphPinDirection::EGPD_Input);
		if (PinToConnectTo)
		{
			ensure(GetSchema()->TryCreateConnection(FromPin, PinToConnectTo));
		}
	}
	else
	{
		UK2Node::AutowireNewNode(FromPin);
	}
}

TSharedRef<SWidget> UAnimGraphNode_Base::MakePropertyBindingWidget(const FAnimPropertyBindingWidgetArgs& InArgs)
{
	UAnimGraphNode_Base* FirstAnimGraphNode = InArgs.Nodes[0];
	const bool bMultiSelect = InArgs.Nodes.Num() > 1;
	
	if(FirstAnimGraphNode->HasValidBlueprint() && IModularFeatures::Get().IsModularFeatureAvailable("PropertyAccessEditor"))
	{
		UBlueprint* Blueprint = FirstAnimGraphNode->GetAnimBlueprint();
		
		int32 PinArrayIndex = InArgs.PinName.GetNumber() - 1;
		const bool bIsArrayOrArrayElement = InArgs.PinProperty->IsA<FArrayProperty>();
		const bool bIsArrayElement = bIsArrayOrArrayElement && PinArrayIndex != INDEX_NONE && InArgs.bPropertyIsOnFNode;
		const bool bIsArray = bIsArrayOrArrayElement && PinArrayIndex == INDEX_NONE && InArgs.bPropertyIsOnFNode;

		FProperty* PropertyToBindTo = bIsArrayElement ? CastField<FArrayProperty>(InArgs.PinProperty)->Inner : InArgs.PinProperty;
		
		// Properties could potentially be removed underneath this widget, so keep a TFieldPath reference to them
		TFieldPath<FProperty> BindingPropertyPath(PropertyToBindTo);
		TFieldPath<FProperty> PinPropertyPath(InArgs.PinProperty);
		
		auto OnCanBindProperty = [BindingPropertyPath](FProperty* InProperty)
		{
			// Note: We support type promotion here
			IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
			FProperty* BindingProperty = BindingPropertyPath.Get();
			return BindingProperty && PropertyAccessEditor.GetPropertyCompatibility(InProperty, BindingProperty) != EPropertyAccessCompatibility::Incompatible;
		};

		auto OnCanBindFunction = [BindingPropertyPath](UFunction* InFunction)
		{
			IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
			FProperty* BindingProperty = BindingPropertyPath.Get();
			
			// Note: We support type promotion here
			return InFunction->NumParms == 1 
				&& BindingProperty != nullptr && PropertyAccessEditor.GetPropertyCompatibility(InFunction->GetReturnProperty(), BindingProperty) != EPropertyAccessCompatibility::Incompatible
				&& InFunction->HasAnyFunctionFlags(FUNC_BlueprintPure);
		};

		auto OnAddBinding = [InArgs, Blueprint, BindingPropertyPath, PinPropertyPath, bIsArrayElement, bIsArray](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				AnimGraphNode->Modify();

				// Reset to default so that references are not preserved
				FProperty* BindingProperty = BindingPropertyPath.Get();
				if(BindingProperty && BindingProperty->GetOwner<UStruct>() && AnimGraphNode->GetFNodeType()->IsChildOf(BindingProperty->GetOwner<UStruct>()) && BindingProperty->IsA<FObjectPropertyBase>())
				{
					void* PropertyAddress = BindingProperty->ContainerPtrToValuePtr<void>(AnimGraphNode->GetFNode());
					BindingProperty->InitializeValue(PropertyAddress);
				}
				
				// Pins are exposed if we have a binding or not - and after running this we do.
				InArgs.OnSetPinVisibility.ExecuteIfBound(AnimGraphNode, true, InArgs.OptionalPinIndex);

				// Need to break all pin links now we have a binding
				if(UEdGraphPin* Pin = AnimGraphNode->FindPin(InArgs.PinName))
				{
					Pin->BreakAllPinLinks();
				}

				if(bIsArray)
				{
					// Remove bindings for array elements if this is an array
					FName ComparisonName(InArgs.BindingName, 0);
					for(auto Iter = AnimGraphNode->PropertyBindings.CreateIterator(); Iter; ++Iter)
					{
						if(ComparisonName == FName(Iter.Key(), 0))
						{
							Iter.RemoveCurrent();
						}
					}
				}
				else if(bIsArrayElement)
				{
					// If we are an array element, remove only whole-array bindings
					FName ComparisonName(InArgs.BindingName, 0);
					for(auto Iter = AnimGraphNode->PropertyBindings.CreateIterator(); Iter; ++Iter)
					{
						if(ComparisonName == Iter.Key())
						{
							Iter.RemoveCurrent();
						}
					}
				}
				
				const FFieldVariant& LeafField = InBindingChain.Last().Field;

				FProperty* PinProperty = PinPropertyPath.Get();
				if(PinProperty && BindingProperty)
				{
					FAnimGraphNodePropertyBinding Binding;
					Binding.PropertyName = InArgs.BindingName;
					if(bIsArrayElement)
					{
						// Pull array index from the pin's FName if this is an array property
						Binding.ArrayIndex = InArgs.PinName.GetNumber() - 1;
					}
					PropertyAccessEditor.MakeStringPath(InBindingChain, Binding.PropertyPath);
					Binding.PathAsText = PropertyAccessEditor.MakeTextPath(Binding.PropertyPath, Blueprint->SkeletonGeneratedClass);
					Binding.Type = LeafField.IsA<UFunction>() ? EAnimGraphNodePropertyBindingType::Function : EAnimGraphNodePropertyBindingType::Property;
					Binding.bIsBound = true;
					AnimGraphNode->RecalculateBindingType(Binding);

					AnimGraphNode->PropertyBindings.Add(InArgs.BindingName, Binding);
				}
			}

			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		};

		auto OnRemoveBinding = [InArgs, Blueprint](FName InPropertyName)
		{
			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				AnimGraphNode->Modify();
				AnimGraphNode->PropertyBindings.Remove(InArgs.BindingName);
			}

			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		};

		auto CanRemoveBinding = [InArgs](FName InPropertyName)
		{
			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				if(AnimGraphNode->PropertyBindings.Contains(InArgs.BindingName))
				{
					return true;
				}
			}

			return false;
		}; 

		enum class ECurrentValueType : int32
		{
			None,
			Pin,
			Binding,
			Dynamic,
			MultipleValues,
		};

		auto CurrentBindingText = [InArgs]()
		{
			ECurrentValueType CurrentValueType = ECurrentValueType::None;

			const FText MultipleValues = LOCTEXT("MultipleValuesLabel", "Multiple Values");
			const FText Bind = LOCTEXT("BindLabel", "Bind");
			const FText ExposedAsPin = LOCTEXT("ExposedAsPinLabel", "Pin");
			const FText Dynamic = LOCTEXT("DynamicLabel", "Dynamic");
			FText CurrentValue = Bind;

			auto SetAssignValue = [&CurrentValueType, &CurrentValue, &MultipleValues](const FText& InValue, ECurrentValueType InType)
			{
				if(CurrentValueType != ECurrentValueType::MultipleValues)
				{
					if(CurrentValueType == ECurrentValueType::None)
					{
						CurrentValueType = InType;
						CurrentValue = InValue;
					}
					else if(CurrentValueType == InType)
					{
						if(!CurrentValue.EqualTo(InValue))
						{
							CurrentValueType = ECurrentValueType::MultipleValues;
							CurrentValue = MultipleValues;
						}
					}
					else
					{
						CurrentValueType = ECurrentValueType::MultipleValues;
						CurrentValue = MultipleValues;
					}
				}
			};

			const FName ComparisonName = FName(InArgs.PinName, 0);

			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				if(FAnimGraphNodePropertyBinding* BindingPtr = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
				{
					SetAssignValue(BindingPtr->PathAsText, ECurrentValueType::Binding);
				}
				else if(AnimGraphNode->AlwaysDynamicProperties.Find(ComparisonName))
				{
					SetAssignValue(Dynamic, ECurrentValueType::Dynamic);
				}
				else
				{
					TArrayView<FOptionalPinFromProperty> OptionalPins; 
					InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
					if(OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
					{
						SetAssignValue(InArgs.bOnGraphNode ? Bind : ExposedAsPin, ECurrentValueType::Pin);
					}
					else
					{
						SetAssignValue(Bind, ECurrentValueType::None);
					}
				}
			}

			return CurrentValue;
		};

		auto CurrentBindingToolTipText = [InArgs]()
		{
			ECurrentValueType CurrentValueType = ECurrentValueType::None;

			const FText MultipleValues = LOCTEXT("MultipleValuesToolTip", "Bindings Have Multiple Values");
			const FText ExposedAsPin = LOCTEXT("ExposedAsPinToolTip", "Exposed As a Pin on the Node");
			const FText BindValue = LOCTEXT("BindValueToolTip", "Bind This Value");
			const FText DynamicValue = LOCTEXT("DynamicValueToolTip", "Dynamic value that can be set externally");
			FText CurrentValue;
			
			auto SetAssignValue = [&CurrentValueType, &CurrentValue, &MultipleValues](const FText& InValue, ECurrentValueType InType)
			{
				if(CurrentValueType != ECurrentValueType::MultipleValues)
				{
					if(CurrentValueType == ECurrentValueType::None)
					{
						CurrentValueType = InType;
						CurrentValue = InValue;
					}
					else if(CurrentValueType == InType)
					{
						if(!CurrentValue.EqualTo(InValue))
						{
							CurrentValueType = ECurrentValueType::MultipleValues;
							CurrentValue = MultipleValues;
						}
					}
					else
					{
						CurrentValueType = ECurrentValueType::MultipleValues;
						CurrentValue = MultipleValues;
					}
				}
			};

			const FName ComparisonName = FName(InArgs.PinName, 0);

			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				if(FAnimGraphNodePropertyBinding* BindingPtr = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
				{
					if(BindingPtr->PathAsText.IsEmpty())
					{
						SetAssignValue(BindValue, ECurrentValueType::Binding);
					}
					else
					{
						IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
						const FText UnderlyingPath = PropertyAccessEditor.MakeTextPath(BindingPtr->PropertyPath);
						const FText& CompilationContext = BindingPtr->CompiledContext;
						const FText& CompilationContextDesc = BindingPtr->CompiledContextDesc;
						if(CompilationContext.IsEmpty() && CompilationContextDesc.IsEmpty())
						{
							SetAssignValue(FText::Format(LOCTEXT("BindingToolTipFormat", "Pin is bound to property '{0}'\nNative: {1}"), BindingPtr->PathAsText, UnderlyingPath), ECurrentValueType::Binding);
						}
						else
						{
							SetAssignValue(FText::Format(LOCTEXT("BindingToolTipFormatWithDesc", "Pin is bound to property '{0}'\nNative: {1}\n{2}\n{2}"), BindingPtr->PathAsText, UnderlyingPath, CompilationContext, CompilationContextDesc), ECurrentValueType::Binding);
						}
					}
				}
				else if(AnimGraphNode->AlwaysDynamicProperties.Find(ComparisonName))
				{
					SetAssignValue(DynamicValue, ECurrentValueType::Dynamic);
				}
				else
				{
					TArrayView<FOptionalPinFromProperty> OptionalPins; 
					InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
					if(OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
					{
						SetAssignValue(InArgs.bOnGraphNode ? BindValue : ExposedAsPin, ECurrentValueType::Pin);
					}
					else
					{
						SetAssignValue(BindValue, ECurrentValueType::None);
					}
				}
			}

			return CurrentValue;
		};

		auto CurrentBindingImage = [InArgs, BindingPropertyPath]() -> const FSlateBrush*
		{
			static FName PropertyIcon(TEXT("Kismet.VariableList.TypeIcon"));
			static FName FunctionIcon(TEXT("GraphEditor.Function_16x"));

			EAnimGraphNodePropertyBindingType BindingType = EAnimGraphNodePropertyBindingType::None;
			for(UObject* OuterObject : InArgs.Nodes)
			{
				if(UAnimGraphNode_Base* AnimGraphNode = Cast<UAnimGraphNode_Base>(OuterObject))
				{
					TArrayView<FOptionalPinFromProperty> OptionalPins; 
					InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
					if(OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
					{
						BindingType = EAnimGraphNodePropertyBindingType::None;
						break;
					}
					else if(FAnimGraphNodePropertyBinding* BindingPtr = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
					{
						if(BindingType == EAnimGraphNodePropertyBindingType::None)
						{
							BindingType = BindingPtr->Type;
						}
						else if(BindingType != BindingPtr->Type)
						{
							BindingType = EAnimGraphNodePropertyBindingType::None;
							break;
						}
					}
					else if(BindingType != EAnimGraphNodePropertyBindingType::None)
					{
						BindingType = EAnimGraphNodePropertyBindingType::None;
						break;
					}
				}
			}

			if (BindingType == EAnimGraphNodePropertyBindingType::Function)
			{
				return FAppStyle::GetBrush(FunctionIcon);
			}
			else
			{
				const UAnimationGraphSchema* AnimationGraphSchema = GetDefault<UAnimationGraphSchema>();
				FProperty* BindingProperty = BindingPropertyPath.Get();
				FEdGraphPinType PinType;
				if(BindingProperty != nullptr && AnimationGraphSchema->ConvertPropertyToPinType(BindingProperty, PinType))
				{
					return FBlueprintEditorUtils::GetIconFromPin(PinType, false);
				}
				else
				{
					return FAppStyle::GetBrush(PropertyIcon);
				}
			}
		};

		auto CurrentBindingColor = [InArgs, BindingPropertyPath]() -> FLinearColor
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

			FLinearColor BindingColor = FLinearColor::Gray;

			FProperty* BindingProperty = BindingPropertyPath.Get();
			FEdGraphPinType PinType;
			if(BindingProperty != nullptr && Schema->ConvertPropertyToPinType(BindingProperty, PinType))
			{
				BindingColor = Schema->GetPinTypeColor(PinType);

				enum class EPromotionState
				{
					NotChecked,
					NotPromoted,
					Promoted,
				} Promotion = EPromotionState::NotChecked;

				for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
				{
					TArrayView<FOptionalPinFromProperty> OptionalPins; 
					InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
					if(FAnimGraphNodePropertyBinding* BindingPtr = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
					{
						if(Promotion == EPromotionState::NotChecked)
						{
							if(BindingPtr->bIsPromotion)
							{
								Promotion = EPromotionState::Promoted;
								BindingColor = Schema->GetPinTypeColor(BindingPtr->PromotedPinType);
							}
							else
							{
								Promotion = EPromotionState::NotPromoted;
							}
						}
						else
						{
							EPromotionState NewPromotion = BindingPtr->bIsPromotion ? EPromotionState::Promoted : EPromotionState::NotPromoted;
							if(Promotion != NewPromotion)
							{
								BindingColor = FLinearColor::Gray;
								break;
							}
						}
					}
					else if(OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
					{
						if(Promotion == EPromotionState::NotChecked)
						{
							Promotion = EPromotionState::NotPromoted;
						}
						else if(Promotion == EPromotionState::Promoted)
						{
							BindingColor = FLinearColor::Gray;
							break;
						}
					}
				}
			}

			return BindingColor;
		};

		auto AddMenuExtension = [InArgs, Blueprint, BindingPropertyPath, PinPropertyPath](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.BeginSection("Pins", LOCTEXT("Pin", "Pin"));
			{
				auto ExposeAsPin = [InArgs, Blueprint]()
				{
					bool bHasBinding = false;

					for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
					{
						if(AnimGraphNode->HasBinding(InArgs.BindingName))
						{
							bHasBinding = true;
							break;
						}
					}

					{
						FScopedTransaction Transaction(LOCTEXT("PinExposure", "Pin Exposure"));

						// Switching from non-pin to pin, remove any bindings
						for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
						{
							AnimGraphNode->Modify();

							TArrayView<FOptionalPinFromProperty> OptionalPins; 
							InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
							const bool bVisible = OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin;
							InArgs.OnSetPinVisibility.ExecuteIfBound(AnimGraphNode, !bVisible || bHasBinding, InArgs.OptionalPinIndex);

							// Remove all bindings that match the property, array or array elements
							const FName ComparisonName = FName(InArgs.BindingName, 0);
							for(auto It = AnimGraphNode->PropertyBindings.CreateIterator(); It; ++It)
							{
								if(ComparisonName == FName(It.Key(), 0))
								{
									It.RemoveCurrent();
								}
							}
						}

						FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
					}
				}; 

				auto GetExposedAsPinCheckState = [InArgs]()
				{
					bool bPinShown = false;
					bool bHasBinding = false;

					for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
					{
						if(AnimGraphNode->HasBinding(InArgs.BindingName))
						{
							bHasBinding = true;
							break;
						}

						TArrayView<FOptionalPinFromProperty> OptionalPins; 
						InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
						bPinShown |= OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin;
					}

					// Pins are exposed if we have a binding or not, so treat as unchecked only if we have
					// no binding
					return bPinShown && !bHasBinding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				};
				
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("ExposeAsPin", "Expose As Pin"),
					LOCTEXT("ExposeAsPinTooltip", "Show/hide this property as a pin on the node"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.PinIcon"),
					FUIAction(
						FExecuteAction::CreateLambda(ExposeAsPin),
						FCanExecuteAction(),
						FGetActionCheckState::CreateLambda(GetExposedAsPinCheckState)
					),
					NAME_None,
					EUserInterfaceActionType::Check
				);

				if(InArgs.bPropertyIsOnFNode)
				{
					auto MakeDynamic = [InArgs, Blueprint]()
					{
						// Comparison without name index to deal with arrays
						const FName ComparisonName = FName(InArgs.PinName, 0);

						bool bIsAlwaysDynamic = false;
						for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
						{
							if(AnimGraphNode->AlwaysDynamicProperties.Contains(ComparisonName))
							{
								bIsAlwaysDynamic = true;
								break;
							}
						}

						{
							FScopedTransaction Transaction(LOCTEXT("AlwaysDynamic", "Always Dynamic"));

							for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
							{
								AnimGraphNode->Modify();

								if(bIsAlwaysDynamic)
								{
									AnimGraphNode->AlwaysDynamicProperties.Remove(ComparisonName);
								}
								else
								{
									AnimGraphNode->AlwaysDynamicProperties.Add(ComparisonName);
								}
							}

							FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
						}
					};

					auto GetDynamicCheckState = [InArgs]()
					{
						// Comparison without name index to deal with arrays
						const FName ComparisonName = FName(InArgs.PinName, 0);

						bool bIsAlwaysDynamic = false;
						for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
						{
							for(const FName& AlwaysDynamicPropertyName : AnimGraphNode->AlwaysDynamicProperties)
							{
								if(ComparisonName == FName(AlwaysDynamicPropertyName, 0))
								{
									bIsAlwaysDynamic = true;
									break;
								}
							}
						}
						
						return bIsAlwaysDynamic ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					};
					
					InMenuBuilder.AddMenuEntry(
						LOCTEXT("DynamicValue", "Dynamic Value"),
						LOCTEXT("DynamicValueTooltip", "Flag this value as dynamic. This way it can be set from functions even when not exposed as a pin."),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda(MakeDynamic),
							FCanExecuteAction(),
							FGetActionCheckState::CreateLambda(GetDynamicCheckState)
						),
						NAME_None,
						EUserInterfaceActionType::Check
					);
				}
			}
			InMenuBuilder.EndSection();
		};

		FPropertyBindingWidgetArgs Args;
		Args.Property = PropertyToBindTo;
		Args.OnCanBindProperty = FOnCanBindProperty::CreateLambda(OnCanBindProperty);
		Args.OnCanBindFunction = FOnCanBindFunction::CreateLambda(OnCanBindFunction);
		Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass){ return true; });
		Args.OnAddBinding = FOnAddBinding::CreateLambda(OnAddBinding);
		Args.OnRemoveBinding = FOnRemoveBinding::CreateLambda(OnRemoveBinding);
		Args.OnCanRemoveBinding = FOnCanRemoveBinding::CreateLambda(CanRemoveBinding);
		Args.CurrentBindingText = MakeAttributeLambda(CurrentBindingText);
		Args.CurrentBindingToolTipText = MakeAttributeLambda(CurrentBindingToolTipText);
		Args.CurrentBindingImage = MakeAttributeLambda(CurrentBindingImage);
		Args.CurrentBindingColor = MakeAttributeLambda(CurrentBindingColor);
		Args.MenuExtender = MakeShared<FExtender>();
		Args.MenuExtender->AddMenuExtension("BindingActions", EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateLambda(AddMenuExtension));
		if(InArgs.MenuExtender.IsValid())
		{
			Args.MenuExtender = FExtender::Combine( { Args.MenuExtender, InArgs.MenuExtender } );
		}

		IPropertyAccessBlueprintBinding::FContext BindingContext;
		BindingContext.Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(FirstAnimGraphNode);
		BindingContext.Graph = FirstAnimGraphNode->GetGraph();
		BindingContext.Node = FirstAnimGraphNode;
		BindingContext.Pin = FirstAnimGraphNode->FindPin(InArgs.PinName);

		auto OnSetPropertyAccessContextId = [InArgs, Blueprint](const FName& InContextId)
		{
			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				if(FAnimGraphNodePropertyBinding* Binding = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
				{
					AnimGraphNode->Modify();
					Binding->ContextId = InContextId;
				}
			}

			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		};

		auto OnCanSetPropertyAccessContextId = [InArgs, FirstAnimGraphNode](const FName& InContextId)
		{
			return FirstAnimGraphNode->PropertyBindings.Find(InArgs.BindingName) != nullptr;
		};
		
		auto OnGetPropertyAccessContextId = [InArgs]() -> FName
		{
			FName CurrentContext = NAME_None;
			for(UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
			{
				if(FAnimGraphNodePropertyBinding* Binding = AnimGraphNode->PropertyBindings.Find(InArgs.BindingName))
				{
					if(CurrentContext != NAME_None && CurrentContext != Binding->ContextId)
					{
						return NAME_None;
					}
					else
					{
						CurrentContext = Binding->ContextId;
					}
				}
			}
		
			return CurrentContext;
		};
		
		IPropertyAccessBlueprintBinding::FBindingMenuArgs MenuArgs;
		MenuArgs.OnSetPropertyAccessContextId = FOnSetPropertyAccessContextId::CreateLambda(OnSetPropertyAccessContextId);
		MenuArgs.OnCanSetPropertyAccessContextId = FOnCanSetPropertyAccessContextId::CreateLambda(OnCanSetPropertyAccessContextId);
		MenuArgs.OnGetPropertyAccessContextId = FOnGetPropertyAccessContextId::CreateLambda(OnGetPropertyAccessContextId);
		
		// Add the binding menu extenders
		TArray<TSharedPtr<FExtender>> Extenders( { Args.MenuExtender } );
		for(IPropertyAccessBlueprintBinding* Binding : IModularFeatures::Get().GetModularFeatureImplementations<IPropertyAccessBlueprintBinding>("PropertyAccessBlueprintBinding"))
		{
			TSharedPtr<FExtender> BindingExtender = Binding->MakeBindingMenuExtender(BindingContext, MenuArgs);
			if(BindingExtender)
			{
				Extenders.Add(BindingExtender);
			}
		}
		
		if(Extenders.Num() > 0)
		{
			Args.MenuExtender = FExtender::Combine(Extenders);
		}

		Args.bAllowNewBindings = false;
		Args.bAllowArrayElementBindings = !bIsArray;
		Args.bAllowUObjectFunctions = !bIsArray;
		Args.bAllowStructFunctions = !bIsArray;

		IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

		const FTextBlockStyle& TextBlockStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PropertyAccess.CompiledContext.Text");
		
		return
			SNew(SBox)
			.MaxDesiredWidth(200.0f)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.Padding(FMargin(1.0f, 3.0f, 1.0f, 1.0f))
					.Visibility(InArgs.bOnGraphNode ? EVisibility::Visible : EVisibility::Collapsed)
					.BorderImage(FAppStyle::GetBrush("PropertyAccess.CompiledContext.Border"))
					.RenderTransform_Lambda([InArgs, FirstAnimGraphNode, &TextBlockStyle]()
					{
						const FAnimGraphNodePropertyBinding* BindingPtr = FirstAnimGraphNode->PropertyBindings.Find(InArgs.BindingName);
						FVector2D TextSize(0.0f, 0.0f);
						if(BindingPtr)
						{
							const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
							TextSize = FontMeasureService->Measure(BindingPtr->CompiledContext, TextBlockStyle.Font);
						}
						return FSlateRenderTransform(FVector2D(0.0f, TextSize.Y - 1.0f));
					})	
					[
						SNew(STextBlock)
						.TextStyle(&TextBlockStyle)
						.Visibility_Lambda([InArgs, FirstAnimGraphNode, bMultiSelect]()
						{
							const FAnimGraphNodePropertyBinding* BindingPtr = FirstAnimGraphNode->PropertyBindings.Find(InArgs.BindingName);
							return bMultiSelect || BindingPtr == nullptr || BindingPtr->CompiledContext.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
						})
						.Text_Lambda([InArgs, FirstAnimGraphNode]()
						{
							const FAnimGraphNodePropertyBinding* BindingPtr = FirstAnimGraphNode->PropertyBindings.Find(InArgs.BindingName);
							return BindingPtr != nullptr ? BindingPtr->CompiledContext : FText::GetEmpty();
						})
					]
				]
				+SOverlay::Slot()
				[
					PropertyAccessEditor.MakePropertyBindingWidget(Blueprint, Args)
				]	
			];
	}
	else
	{
		return SNew(SSpacer);
	}
}

void UAnimGraphNode_Base::HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
{
	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	UClass* SkeletonVariableClass = FBlueprintEditorUtils::GetSkeletonClass(InVariableClass);
	
	// See if any of bindings reference the variable
	for(auto& BindingPair : PropertyBindings)
	{
		TArray<int32> RenameIndices;
		IPropertyAccessEditor::FResolvePropertyAccessArgs ResolveArgs;
		ResolveArgs.PropertyFunction = [InOldVarName, SkeletonVariableClass, &RenameIndices](int32 InSegmentIndex, FProperty* InProperty, int32 InStaticArrayIndex)
		{
			UClass* OwnerClass = InProperty->GetOwnerClass();
			if(OwnerClass && InProperty->GetFName() == InOldVarName && OwnerClass->IsChildOf(SkeletonVariableClass))
			{
				RenameIndices.Add(InSegmentIndex);
			}
		};
		
		PropertyAccessEditor.ResolvePropertyAccess(GetAnimBlueprint()->SkeletonGeneratedClass, BindingPair.Value.PropertyPath, ResolveArgs);

		// Rename any references we found
		for(const int32& RenameIndex : RenameIndices)
		{
			BindingPair.Value.PropertyPath[RenameIndex] = InNewVarName.ToString();
			BindingPair.Value.PathAsText = PropertyAccessEditor.MakeTextPath(BindingPair.Value.PropertyPath, GetAnimBlueprint()->SkeletonGeneratedClass);
		}
	}
}

void UAnimGraphNode_Base::ReplaceReferences(UBlueprint* InBlueprint, UBlueprint* InReplacementBlueprint, const FMemberReference& InSource, const FMemberReference& InReplacement)
{
	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
	
	UClass* SkeletonClass = InBlueprint->SkeletonGeneratedClass;

	FMemberReference Source = InSource;
	FProperty* SourceProperty = Source.ResolveMember<FProperty>(InBlueprint);
	FMemberReference Replacement = InReplacement;
	FProperty* ReplacementProperty = Replacement.ResolveMember<FProperty>(InReplacementBlueprint);
	
	// See if any of bindings reference the variable
	for(auto& BindingPair : PropertyBindings)
	{
		TArray<int32> ReplaceIndices;
		IPropertyAccessEditor::FResolvePropertyAccessArgs ResolveArgs;
		ResolveArgs.PropertyFunction = [SourceProperty, &ReplaceIndices](int32 InSegmentIndex, FProperty* InProperty, int32 InStaticArrayIndex)
		{
			if(InProperty == SourceProperty)
			{
				ReplaceIndices.Add(InSegmentIndex);
			}
		};
		
		PropertyAccessEditor.ResolvePropertyAccess(GetAnimBlueprint()->SkeletonGeneratedClass, BindingPair.Value.PropertyPath, ResolveArgs);

		// Replace any references we found
		for(const int32& RenameIndex : ReplaceIndices)
		{
			BindingPair.Value.PropertyPath[RenameIndex] = ReplacementProperty->GetName();
			BindingPair.Value.PathAsText = PropertyAccessEditor.MakeTextPath(BindingPair.Value.PropertyPath, GetAnimBlueprint()->SkeletonGeneratedClass);
		}
	}
}

bool UAnimGraphNode_Base::ReferencesVariable(const FName& InVarName, const UStruct* InScope) const
{
	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	const UClass* SkeletonVariableClass = FBlueprintEditorUtils::GetSkeletonClass(Cast<UClass>(InScope));
	
	// See if any of bindings reference the variable
	for(const auto& BindingPair : PropertyBindings)
	{
		bool bReferencesVariable = false;
		
		IPropertyAccessEditor::FResolvePropertyAccessArgs ResolveArgs;
		ResolveArgs.PropertyFunction = [InVarName, SkeletonVariableClass, &bReferencesVariable](int32 InSegmentIndex, FProperty* InProperty, int32 InStaticArrayIndex)
		{
			if(SkeletonVariableClass)
			{
				const UClass* OwnerSkeletonVariableClass = FBlueprintEditorUtils::GetSkeletonClass(Cast<UClass>(InProperty->GetOwnerStruct()));

				if(OwnerSkeletonVariableClass && InProperty->GetFName() == InVarName && OwnerSkeletonVariableClass->IsChildOf(SkeletonVariableClass))
				{
					bReferencesVariable = true;
				}
			}
			else if(InProperty->GetFName() == InVarName)
			{
				bReferencesVariable = true;
			}
		};
		
		PropertyAccessEditor.ResolvePropertyAccess(GetAnimBlueprint()->SkeletonGeneratedClass, BindingPair.Value.PropertyPath, ResolveArgs);

		if(bReferencesVariable)
		{
			return true;
		}
	}

	return false;
}

bool UAnimGraphNode_Base::ReferencesFunction(const FName& InFunctionName,const UStruct* InScope) const
{
	IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

	const UClass* SkeletonFunctionClass = FBlueprintEditorUtils::GetSkeletonClass(Cast<UClass>(InScope));

	// See if any of bindings reference the function
	for (const auto& BindingPair : PropertyBindings)
	{
		bool bReferencesFunction = false;

		IPropertyAccessEditor::FResolvePropertyAccessArgs ResolveArgs;
		ResolveArgs.FunctionFunction = [InFunctionName, SkeletonFunctionClass, &bReferencesFunction](int32 InSegmentIndex, UFunction* InFunction, FProperty* InProperty)
		{
			if (SkeletonFunctionClass)
			{
				const UClass* OwnerSkeletonFunctionClass = FBlueprintEditorUtils::GetSkeletonClass(InFunction->GetOuterUClass());

				if (OwnerSkeletonFunctionClass && InFunction->GetFName() == InFunctionName && OwnerSkeletonFunctionClass->IsChildOf(SkeletonFunctionClass))
				{
					bReferencesFunction = true;
				}
			}
			else if (InFunction->GetFName() == InFunctionName)
			{
				bReferencesFunction = true;
			}
		};

		PropertyAccessEditor.ResolvePropertyAccess(GetAnimBlueprint()->SkeletonGeneratedClass, BindingPair.Value.PropertyPath, ResolveArgs);

		if (bReferencesFunction)
		{
			return true;
		}
	}

	// Check private member anim node function binding 
	return InitialUpdateFunction.GetMemberName() == InFunctionName || BecomeRelevantFunction.GetMemberName() == InFunctionName || UpdateFunction.GetMemberName() == InFunctionName;
}

void UAnimGraphNode_Base::PostEditRefreshDebuggedComponent()
{
	if(FAnimNode_Base* DebuggedAnimNode = GetDebuggedAnimNode())
	{
		if(const IAnimClassInterface* AnimClassInterface = DebuggedAnimNode->GetAnimClassInterface())
		{
			if(const UAnimInstance* HostingInstance = Cast<UAnimInstance>(IAnimClassInterface::GetObjectPtrFromAnimNode(AnimClassInterface, DebuggedAnimNode)))
			{
				if(UWorld* World = HostingInstance->GetWorld())
				{
					if(World->IsPaused() || World->WorldType == EWorldType::Editor)
					{
						if(USkeletalMeshComponent* SkelMeshComponent = HostingInstance->GetSkelMeshComponent())
						{
							SkelMeshComponent->RefreshBoneTransforms();
						}
					}
				}
			}
		}
	}
}

UObject* UAnimGraphNode_Base::GetObjectBeingDebugged() const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		return Blueprint->GetObjectBeingDebugged();
	}

	return nullptr;
}

bool UAnimGraphNode_Base::IsPotentiallyBoundFunction(const FMemberReference& FunctionReference)
{
	return FunctionReference.GetMemberGuid().IsValid() || FunctionReference.GetMemberName() != NAME_None;
}

#undef LOCTEXT_NAMESPACE
