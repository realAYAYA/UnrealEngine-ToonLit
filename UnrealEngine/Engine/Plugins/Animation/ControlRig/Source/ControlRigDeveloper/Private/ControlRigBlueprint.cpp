// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprint.h"

#include "RigVMBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "Engine/SkeletalMesh.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "ControlRig.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphSchema.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectGlobals.h"
#include "ControlRigObjectVersion.h"
#include "BlueprintCompilationManager.h"
#include "ModularRig.h"
#include "ModularRigController.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Hierarchy/RigUnit_SetBoneTransform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RigVMPythonUtils.h"
#include "RigVMTypeUtils.h"
#include "RigVMModel/Nodes/RigVMAggregateNode.h"
#include "Rigs/RigControlHierarchy.h"
#include "Settings/ControlRigSettings.h"
#include "Units/ControlRigNodeWorkflow.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_DynamicHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigBlueprint)

#if WITH_EDITOR
#include "IControlRigEditorModule.h"
#include "Kismet2/WatchedPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/Transactor.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "ScopedTransaction.h"
#include "Algo/Count.h"
#endif//WITH_EDITOR

#define LOCTEXT_NAMESPACE "ControlRigBlueprint"

TArray<UControlRigBlueprint*> UControlRigBlueprint::sCurrentlyOpenedRigBlueprints;
#if WITH_EDITOR
const FName UControlRigBlueprint::ControlRigPanelNodeFactoryName(TEXT("FControlRigGraphPanelPinFactory"));
#endif

UControlRigBlueprint::UControlRigBlueprint(const FObjectInitializer& ObjectInitializer)
	: URigVMBlueprint(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	GizmoLibrary_DEPRECATED = nullptr;
	ShapeLibraries.Add(UControlRigSettings::Get()->DefaultShapeLibrary);
#endif

	Validator = ObjectInitializer.CreateDefaultSubobject<UControlRigValidator>(this, TEXT("ControlRigValidator"));
	DebugBoneRadius = 1.f;

	bExposesAnimatableControls = false;

	Hierarchy = CreateDefaultSubobject<URigHierarchy>(TEXT("Hierarchy"));
	URigHierarchyController* Controller = Hierarchy->GetController(true);
	// give BP a chance to propagate hierarchy changes to available control rig instances
	Controller->OnModified().AddUObject(this, &UControlRigBlueprint::HandleHierarchyModified);

	if(GetClass() == UControlRigBlueprint::StaticClass())
	{
		CommonInitialization(ObjectInitializer);
	}

	ModularRigModel.SetOuterClientHost(this);
	UModularRigController* ModularController = ModularRigModel.GetController();
	ModularController->OnModified().AddUObject(this, &UControlRigBlueprint::HandleRigModulesModified);
	
}

UControlRigBlueprint::UControlRigBlueprint()
{
	ModulesRecompilationBracket = 0;
}

UClass* UControlRigBlueprint::RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO)
{
	UClass* Result = Super::RegenerateClass(ClassToRegenerate, PreviousCDO);
	Hierarchy->CleanupInvalidCaches();
	PropagateHierarchyFromBPToInstances();
	return Result;
}

bool UControlRigBlueprint::RequiresForceLoadMembers(UObject* InObject) const
{
	// old assets don't support preload filtering
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RemoveParameters)
	{
		return UBlueprint::RequiresForceLoadMembers(InObject);
	}
	
	return Super::RequiresForceLoadMembers(InObject);
}

void UControlRigBlueprint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// if this is any of our external variables we need to request construction so that the rig rebuilds itself
	if(NewVariables.ContainsByPredicate([&PropertyChangedEvent](const FBPVariableDescription& Variable)
	{
		return Variable.VarName == PropertyChangedEvent.GetMemberPropertyName();
	}))
	{
		if(UControlRig* DebuggedControlRig = Cast<UControlRig>(GetObjectBeingDebugged()))
		{
			if(const FProperty* PropertyOnRig = DebuggedControlRig->GetClass()->FindPropertyByName(PropertyChangedEvent.MemberProperty->GetFName()))
			{
				if(PropertyOnRig->SameType(PropertyChangedEvent.MemberProperty))
				{
					UControlRig* CDO = DebuggedControlRig->GetClass()->GetDefaultObject<UControlRig>();
					const uint8* SourceMemory = PropertyOnRig->ContainerPtrToValuePtr<uint8>(CDO);
					uint8* TargetMemory = PropertyOnRig->ContainerPtrToValuePtr<uint8>(DebuggedControlRig);
					PropertyOnRig->CopyCompleteValue(TargetMemory, SourceMemory);
				}
			}
			DebuggedControlRig->RequestConstruction();
		}
	}
}

void UControlRigBlueprint::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Propagate shape libraries
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, ShapeLibraries))
	{
		URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(false /* create if needed */));

		TArray<UObject*> ArchetypeInstances;
		CDO->GetArchetypeInstances(ArchetypeInstances);
		ArchetypeInstances.Add(CDO);

		// Propagate libraries to archetypes
		for (UObject* Instance : ArchetypeInstances)
		{
			if (UControlRig* InstanceRig = Cast<UControlRig>(Instance))
			{
				InstanceRig->ShapeLibraries = ShapeLibraries;
			}
		}
	}
}

UClass* UControlRigBlueprint::GetControlRigClass() const
{
	return GetRigVMHostClass();
}

bool UControlRigBlueprint::IsModularRig() const
{
	if(const UClass* Class = GetControlRigClass())
	{
		return Class->IsChildOf(UModularRig::StaticClass());
	}
	return false;
}

USkeletalMesh* UControlRigBlueprint::GetPreviewMesh() const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

#if WITH_EDITORONLY_DATA
	if (!PreviewSkeletalMesh.IsValid())
	{
		(void)PreviewSkeletalMesh.LoadSynchronous();
	}

	return PreviewSkeletalMesh.Get();
#else
	return nullptr;
#endif
}

bool UControlRigBlueprint::IsControlRigModule() const
{
	return RigModuleSettings.Identifier.IsValid();
}

#if WITH_EDITORONLY_DATA

bool UControlRigBlueprint::CanTurnIntoControlRigModule(bool InAutoConvertHierarchy, FString* OutErrorMessage) const
{
	if(IsControlRigModule())
	{
		if(OutErrorMessage)
		{
			static const FString Message = TEXT("This asset is already a Control Rig Module.");
			*OutErrorMessage = Message;
		}
		return false;
	}

	if (GetRigVMHostClass()->IsChildOf<UModularRig>())
	{
		if(OutErrorMessage)
		{
			static const FString Message = TEXT("This asset is a Modular Rig.");
			*OutErrorMessage = Message;
		}
		return false;
	}

	if(Hierarchy == nullptr)
	{
		if(OutErrorMessage)
		{
			static const FString Message = TEXT("This asset contains no hierarchy.");
			*OutErrorMessage = Message;
		}
		return false;
	}

	const TArray<FRigElementKey> Keys = Hierarchy->GetAllKeys(true);
	for(const FRigElementKey& Key : Keys)
	{
		if(!InAutoConvertHierarchy)
		{
			if(Key.Type != ERigElementType::Bone &&
				Key.Type != ERigElementType::Curve &&
				Key.Type != ERigElementType::Connector)
			{
				if(OutErrorMessage)
				{
					static constexpr TCHAR Format[] = TEXT("The hierarchy contains elements other than bones (for example '%s'). Modules only allow imported bones and user authored connectors.");
					*OutErrorMessage = FString::Printf(Format, *Key.ToString());
				}
				return false;
			}

			if(Key.Type == ERigElementType::Bone)
			{
				if(Hierarchy->FindChecked<FRigBoneElement>(Key)->BoneType != ERigBoneType::Imported)
				{
					if(OutErrorMessage)
					{
						static constexpr TCHAR Format[] = TEXT("The hierarchy contains a user defined bone ('%s') - only imported bones are allowed.");
						*OutErrorMessage = FString::Printf(Format, *Key.ToString());
					}
					return false;
				}
			}
		}
	}
	
	return true;
}

bool UControlRigBlueprint::TurnIntoControlRigModule(bool InAutoConvertHierarchy, FString* OutErrorMessage)
{
	if(!CanTurnIntoControlRigModule(InAutoConvertHierarchy, OutErrorMessage))
	{
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("TurnIntoControlRigModule", "Turn Rig into Module"));

	Modify();
	RigModuleSettings.Identifier = FRigModuleIdentifier();
	RigModuleSettings.Identifier.Name = GetName();

	if(Hierarchy)
	{
		Hierarchy->Modify();
		URigHierarchyController* Controller = Hierarchy->GetController(true);

		// create a copy of this hierarchy
		URigHierarchy* CopyOfHierarchy = NewObject<URigHierarchy>(GetTransientPackage());
		CopyOfHierarchy->CopyHierarchy(Hierarchy);

		// also create a hierarchy based on the preview mesh
		URigHierarchy* PreviewMeshHierarchy = NewObject<URigHierarchy>(GetTransientPackage());
		if(PreviewSkeletalMesh)
		{
			PreviewMeshHierarchy->GetController(true)->ImportBones(PreviewSkeletalMesh->GetSkeleton());
		}

		// disable compilation
		{
			FRigVMBlueprintCompileScope CompileScope(this);

			// remove everything from the hierarchy
			Hierarchy->Reset();

			const TArray<FRigElementKey> AllKeys = CopyOfHierarchy->GetAllKeys(true);
			TArray<FRigElementKey> KeysToSpawn;

			for(const FRigElementKey& Key : AllKeys)
			{
				if(Key.Type == ERigElementType::Curve)
				{
					continue;
				}
				if(Key.Type == ERigElementType::Bone)
				{
					if(PreviewMeshHierarchy->Contains(Key))
					{
						continue;
					}
				}
				KeysToSpawn.Add(Key);
			}

			(void)ConvertHierarchyElementsToSpawnerNodes(CopyOfHierarchy, KeysToSpawn, false);

			if(Hierarchy->Num(ERigElementType::Connector) == 0)
			{
				static const FName RootName = TEXT("Root");
				static const FString RootDescription = TEXT("This is the default temporary socket used for the root connection.");
				const FRigElementKey ConnectorKey = Controller->AddConnector(RootName);
				const FRigElementKey SocketKey = Controller->AddSocket(RootName, FRigElementKey(), FTransform::Identity, false, FRigSocketElement::SocketDefaultColor, RootDescription, false);
				(void)ResolveConnector(ConnectorKey, SocketKey);
			}
		}
	}

	OnRigTypeChangedDelegate.Broadcast(this);
	return true;
}

bool UControlRigBlueprint::CanTurnIntoStandaloneRig(FString* OutErrorMessage) const
{
	return IsControlRigModule();
}

bool UControlRigBlueprint::TurnIntoStandaloneRig(FString* OutErrorMessage)
{
	if(!CanTurnIntoStandaloneRig(OutErrorMessage))
	{
		return false;
	}

	FScopedTransaction Transaction(LOCTEXT("TurnIntoStandaloneRig", "Turn Module into Rig"));

	Modify();
	RigModuleSettings = FRigModuleSettings();

	if(Hierarchy)
	{
		Hierarchy->Modify();
		Hierarchy->Reset();
		if(PreviewSkeletalMesh)
		{
			Hierarchy->GetController(true)->ImportBones(PreviewSkeletalMesh->GetSkeleton());
		}
	}

	OnRigTypeChangedDelegate.Broadcast(this);
	return true;
}

TArray<URigVMNode*> UControlRigBlueprint::ConvertHierarchyElementsToSpawnerNodes(URigHierarchy* InHierarchy, TArray<FRigElementKey> InKeys, bool bRemoveElements)
{
	TArray<URigVMNode*> SpawnerNodes;

	// find the construction event 
	const URigVMNode* EventNode = nullptr;
	for(const URigVMGraph* Graph : GetRigVMClient()->GetAllModels(false, false))
	{
		for(const URigVMNode* Node : Graph->GetNodes())
		{
			if(Node->IsEvent() && Node->GetEventName() == FRigUnit_PrepareForExecution::EventName)
			{
				EventNode = Node;
				break;
			}
		}
		if(EventNode)
		{
			break;
		}
	}

	FVector2D NodePosition = FVector2D::ZeroVector;
	const FVector2D NodePositionIncrement = FVector2D(400, 0);
	
	// if we didn't find the construction event yet, create it
	if(EventNode == nullptr)
	{
		const URigVMGraph* ConstructionGraph = GetRigVMClient()->AddModel(TEXT("ConstructionGraph"), true);
		URigVMController* GraphController = GetRigVMClient()->GetOrCreateController(ConstructionGraph);
		EventNode = GraphController->AddUnitNode(FRigUnit_PrepareForExecution::StaticStruct(), FRigUnit::GetMethodName(), NodePosition);
		NodePosition += NodePositionIncrement;
	}

	const URigVMPin* LastPin = EventNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_PrepareForExecution, ExecuteContext));
	if(LastPin)
	{
		// follow the node's execution links to find the last one
		bool bCarryOn = true;
		while(bCarryOn)
		{
			static const TArray<FString> ExecutePinPaths = {
				FRigVMStruct::ControlFlowCompletedName.ToString(),
				FRigVMStruct::ExecuteContextName.ToString()
			};

			for(const FString& ExecutePinPath : ExecutePinPaths)
			{
				if(const URigVMPin* ExecutePin = LastPin->GetNode()->FindPin(ExecutePinPath))
				{
					const TArray<URigVMPin*> TargetPins = ExecutePin->GetLinkedTargetPins();
					if(TargetPins.IsEmpty())
					{
						bCarryOn = false;
						break;
					}
					LastPin = TargetPins[0];
					NodePosition = LastPin->GetNode()->GetPosition() + NodePositionIncrement;
				}
			}
		}
	}

	const URigVMGraph* ConstructionGraph = EventNode->GetGraph();
	URigVMController* GraphController = GetRigVMClient()->GetOrCreateController(ConstructionGraph);

	auto GetParentAndTransformDefaults = [InHierarchy](const FRigElementKey& InKey, FString& OutParentDefault, FString& OutTransformDefault)
	{
		const FRigElementKey Parent = InHierarchy->GetFirstParent(InKey);
		OutParentDefault.Reset();
		FRigElementKey::StaticStruct()->ExportText(OutParentDefault, &Parent, nullptr, nullptr, PPF_None, nullptr);

		const FTransform Transform = InHierarchy->GetInitialLocalTransform(InKey);
		OutTransformDefault.Reset();
		TBaseStructure<FTransform>::Get()->ExportText(OutTransformDefault, &Transform, nullptr, nullptr, PPF_None, nullptr);
	};

	TMap<FRigElementKey, const URigVMPin*> ParentItemPinMap;
	auto AddParentItemLink = [GraphController, InHierarchy, &SpawnerNodes, &ParentItemPinMap]
		(const FRigElementKey& Key, URigVMNode* Node)
		{
			SpawnerNodes.Add(Node);
			ParentItemPinMap.Add(Key, Node->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Item)));

			if(const URigVMPin** SourcePin = ParentItemPinMap.Find(InHierarchy->GetFirstParent(Key)))
			{
				if(const URigVMPin* TargetPin = Node->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Parent)))
				{
					GraphController->AddLink((*SourcePin)->GetPinPath(), TargetPin->GetPinPath(), true);
				}
			}
		};
	
	for(const FRigElementKey& Key : InKeys)
	{
		if(Key.Type == ERigElementType::Bone)
		{
			FString ParentDefault, TransformDefault;
			GetParentAndTransformDefaults(Key, ParentDefault, TransformDefault);

			URigVMNode* AddBoneNode = GraphController->AddUnitNode(FRigUnit_HierarchyAddBone::StaticStruct(), FRigUnit::GetMethodName(), NodePosition);
			NodePosition += NodePositionIncrement;
			AddParentItemLink(Key, AddBoneNode);

			if(LastPin)
			{
				if(const URigVMPin* NextPin = AddBoneNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, ExecuteContext)))
				{
					GraphController->AddLink(LastPin->GetPinPath(), NextPin->GetPinPath(), true);
					LastPin = NextPin;
				}
			}

			GraphController->SetPinDefaultValue(AddBoneNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Name))->GetPinPath(), Key.Name.ToString(), true, true);
			GraphController->SetPinDefaultValue(AddBoneNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Parent))->GetPinPath(), ParentDefault, true, true);
			GraphController->SetPinDefaultValue(AddBoneNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, Space))->GetPinPath(), TEXT("LocalSpace"), true, true);
			GraphController->SetPinDefaultValue(AddBoneNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, Transform))->GetPinPath(), TransformDefault, true, true);
		}
		else if(Key.Type == ERigElementType::Null)
		{
			FString ParentDefault, TransformDefault;
			GetParentAndTransformDefaults(Key, ParentDefault, TransformDefault);

			URigVMNode* AddNullNode = GraphController->AddUnitNode(FRigUnit_HierarchyAddNull::StaticStruct(), FRigUnit::GetMethodName(), NodePosition);
			NodePosition += NodePositionIncrement;
			AddParentItemLink(Key, AddNullNode);
			SpawnerNodes.Add(AddNullNode);

			if(LastPin)
			{
				if(const URigVMPin* NextPin = AddNullNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, ExecuteContext)))
				{
					GraphController->AddLink(LastPin->GetPinPath(), NextPin->GetPinPath(), true);
					LastPin = NextPin;
				}
			}

			GraphController->SetPinDefaultValue(AddNullNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Name))->GetPinPath(), Key.Name.ToString(), true, true);
			GraphController->SetPinDefaultValue(AddNullNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Parent))->GetPinPath(), ParentDefault, true, true);
			GraphController->SetPinDefaultValue(AddNullNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddNull, Space))->GetPinPath(), TEXT("LocalSpace"), true, true);
			GraphController->SetPinDefaultValue(AddNullNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddNull, Transform))->GetPinPath(), TransformDefault, true, true);
		}
		else if(Key.Type == ERigElementType::Control)
		{
			FRigControlElement* ControlElement = InHierarchy->FindChecked<FRigControlElement>(Key);
			
			FString ParentDefault, TransformDefault;
			GetParentAndTransformDefaults(Key, ParentDefault, TransformDefault);

			const FTransform OffsetTransform = InHierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::InitialLocal);
			FString OffsetDefault;
			TBaseStructure<FTransform>::Get()->ExportText(OffsetDefault, &OffsetTransform, nullptr, nullptr, PPF_None, nullptr);
			
			if(ControlElement->Settings.AnimationType == ERigControlAnimationType::AnimationChannel)
			{
				UScriptStruct* UnitNodeStruct = nullptr;
				TRigVMTypeIndex TypeIndex = INDEX_NONE;
				FString InitialValue, MinimumValue, MaximumValue, SettingsValue;
				switch(ControlElement->Settings.ControlType)
				{
					case ERigControlType::Bool:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelBool::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Bool;
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<float>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<float>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<float>();
						break;
					}
					case ERigControlType::Float:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelFloat::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Float;
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<float>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<float>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<float>();

						if(ControlElement->Settings.LimitEnabled.Num() == 1)
						{
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings Settings;
							Settings.Enabled = ControlElement->Settings.LimitEnabled[0];
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::ScaleFloat:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelScaleFloat::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Float;
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<float>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<float>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<float>();

						if(ControlElement->Settings.LimitEnabled.Num() == 1)
						{
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings Settings;
							Settings.Enabled = ControlElement->Settings.LimitEnabled[0];
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::Integer:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelInteger::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Int32; 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<int32>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<int32>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<int32>();

						if(ControlElement->Settings.LimitEnabled.Num() == 1)
						{
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings Settings;
							Settings.Enabled = ControlElement->Settings.LimitEnabled[0];
							FRigUnit_HierarchyAddAnimationChannelSingleLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::Vector2D:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelVector2D::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FVector2D>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FVector2D>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<FVector2D>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<FVector2D>();

						if(ControlElement->Settings.LimitEnabled.Num() == 2)
						{
							FRigUnit_HierarchyAddAnimationChannel2DLimitSettings Settings;
							Settings.X = ControlElement->Settings.LimitEnabled[0];
							Settings.Y = ControlElement->Settings.LimitEnabled[1];
							FRigUnit_HierarchyAddAnimationChannel2DLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::Position:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelVector::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FVector>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FVector>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<FVector>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<FVector>();

						if(ControlElement->Settings.LimitEnabled.Num() == 3)
						{
							FRigUnit_HierarchyAddAnimationChannelVectorLimitSettings Settings;
							Settings.X = ControlElement->Settings.LimitEnabled[0];
							Settings.Y = ControlElement->Settings.LimitEnabled[1];
							Settings.Z = ControlElement->Settings.LimitEnabled[2];
							FRigUnit_HierarchyAddAnimationChannelVectorLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::Scale:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelScaleVector::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FVector>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FVector>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<FVector>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<FVector>();

						if(ControlElement->Settings.LimitEnabled.Num() == 3)
						{
							FRigUnit_HierarchyAddAnimationChannelVectorLimitSettings Settings;
							Settings.X = ControlElement->Settings.LimitEnabled[0];
							Settings.Y = ControlElement->Settings.LimitEnabled[1];
							Settings.Z = ControlElement->Settings.LimitEnabled[2];
							FRigUnit_HierarchyAddAnimationChannelVectorLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					case ERigControlType::Rotator:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddAnimationChannelRotator::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FRotator>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FRotator>();
						MinimumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Minimum).ToString<FRotator>();
						MaximumValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Maximum).ToString<FRotator>();

						if(ControlElement->Settings.LimitEnabled.Num() == 3)
						{
							FRigUnit_HierarchyAddAnimationChannelRotatorLimitSettings Settings;
							Settings.Pitch = ControlElement->Settings.LimitEnabled[0];
							Settings.Yaw = ControlElement->Settings.LimitEnabled[1];
							Settings.Roll = ControlElement->Settings.LimitEnabled[2];
							FRigUnit_HierarchyAddAnimationChannelRotatorLimitSettings::StaticStruct()->ExportText(SettingsValue, &Settings, &Settings, nullptr, PPF_None, nullptr);
						}
						break;
					}
					default:
					{
						break;
					}
				}

				if(UnitNodeStruct == nullptr)
				{
					continue;
				}

				URigVMNode* AddControlNode = GraphController->AddUnitNode(UnitNodeStruct, FRigUnit::GetMethodName(), NodePosition);
				NodePosition += NodePositionIncrement;
				AddParentItemLink(Key, AddControlNode);

				if(LastPin)
				{
					if(const URigVMPin* NextPin = AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, ExecuteContext)))
					{
						GraphController->AddLink(LastPin->GetPinPath(), NextPin->GetPinPath(), true);
						LastPin = NextPin;
					}
				}

				GraphController->ResolveWildCardPin(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, InitialValue))->GetPinPath(), TypeIndex, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, Name))->GetPinPath(), Key.Name.ToString(), true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, Parent))->GetPinPath(), ParentDefault, true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, InitialValue))->GetPinPath(), InitialValue, true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, MinimumValue))->GetPinPath(), MinimumValue, true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, MaximumValue))->GetPinPath(), MaximumValue, true, true);

				if(!SettingsValue.IsEmpty())
				{
					GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddAnimationChannelFloat, LimitsEnabled))->GetPinPath(), SettingsValue, true, true);
				}
			}
			else
			{
				UScriptStruct* UnitNodeStruct = nullptr;
				TRigVMTypeIndex TypeIndex = INDEX_NONE;
				FString InitialValue;
				switch(ControlElement->Settings.ControlType)
				{
					case ERigControlType::Float:
					case ERigControlType::ScaleFloat:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlFloat::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Float;
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<float>();
						break;
					}
					case ERigControlType::Integer:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlInteger::StaticStruct();
						TypeIndex = RigVMTypeUtils::TypeIndex::Int32; 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<int32>();
						break;
					}
					case ERigControlType::Vector2D:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlVector2D::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FVector2D>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FVector2D>();
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlVector::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FVector>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FVector>();
						break;
					}
					case ERigControlType::Rotator:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlRotator::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FRotator>(); 
						InitialValue = InHierarchy->GetControlValue(Key, ERigControlValueType::Initial).ToString<FRotator>();
						break;
					}
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						UnitNodeStruct = FRigUnit_HierarchyAddControlTransform::StaticStruct();
						TypeIndex = FRigVMRegistry::Get().GetTypeIndex<FTransform>(); 
						const FTransform InitialTransform = InHierarchy->GetInitialLocalTransform(Key);
						TBaseStructure<FTransform>::Get()->ExportText(InitialValue, &InitialTransform, nullptr, nullptr, PPF_None, nullptr);
						break;
					}
					default:
					{
						break;
					}
				}

				if(UnitNodeStruct == nullptr)
				{
					continue;
				}

				URigVMNode* AddControlNode = GraphController->AddUnitNode(UnitNodeStruct, FRigUnit::GetMethodName(), NodePosition);
				NodePosition += NodePositionIncrement;
				AddParentItemLink(Key, AddControlNode);

				if(LastPin)
				{
					if(const URigVMPin* NextPin = AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, ExecuteContext)))
					{
						GraphController->AddLink(LastPin->GetPinPath(), NextPin->GetPinPath(), true);
						LastPin = NextPin;
					}
				}

				GraphController->ResolveWildCardPin(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddControlInteger, InitialValue))->GetPinPath(), TypeIndex, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Name))->GetPinPath(), Key.Name.ToString(), true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Parent))->GetPinPath(), ParentDefault, true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddControlElement, OffsetSpace))->GetPinPath(), TEXT("LocalSpace"), true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddControlElement, OffsetTransform))->GetPinPath(), OffsetDefault, true, true);
				GraphController->SetPinDefaultValue(AddControlNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddControlInteger, InitialValue))->GetPinPath(), InitialValue, true, true);

				if(const FStructProperty* SettingsProperty = CastField<FStructProperty>(UnitNodeStruct->FindPropertyByName(TEXT("Settings"))))
				{
					UScriptStruct* SettingsStruct = CastChecked<UScriptStruct>(SettingsProperty->Struct);
					FStructOnScope SettingsScope(SettingsStruct);
					FRigUnit_HierarchyAddControl_Settings* Settings = (FRigUnit_HierarchyAddControl_Settings*)SettingsScope.GetStructMemory();
					Settings->ConfigureFrom(ControlElement, ControlElement->Settings);
					FString SettingsDefault;
					SettingsStruct->ExportText(SettingsDefault, Settings, nullptr, nullptr, PPF_None, nullptr);

					GraphController->SetPinDefaultValue(AddControlNode->FindPin(SettingsProperty->GetName())->GetPinPath(), SettingsDefault, true, true);
				}
			}
		}
		else if(Key.Type == ERigElementType::Socket)
		{
			FString ParentDefault, TransformDefault;
			GetParentAndTransformDefaults(Key, ParentDefault, TransformDefault);

			URigVMNode* AddSocketNode = GraphController->AddUnitNode(FRigUnit_HierarchyAddSocket::StaticStruct(), FRigUnit::GetMethodName(), NodePosition);
			NodePosition += NodePositionIncrement;
			AddParentItemLink(Key, AddSocketNode);
			SpawnerNodes.Add(AddSocketNode);

			if(LastPin)
			{
				if(const URigVMPin* NextPin = AddSocketNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddBone, ExecuteContext)))
				{
					GraphController->AddLink(LastPin->GetPinPath(), NextPin->GetPinPath(), true);
					LastPin = NextPin;
				}
			}

			GraphController->SetPinDefaultValue(AddSocketNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Name))->GetPinPath(), Key.Name.ToString(), true, true);
			GraphController->SetPinDefaultValue(AddSocketNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddElement, Parent))->GetPinPath(), ParentDefault, true, true);
			GraphController->SetPinDefaultValue(AddSocketNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddNull, Space))->GetPinPath(), TEXT("LocalSpace"), true, true);
			GraphController->SetPinDefaultValue(AddSocketNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_HierarchyAddNull, Transform))->GetPinPath(), TransformDefault, true, true);
		}
	}

	if(bRemoveElements && InHierarchy)
	{
		InHierarchy->Modify();
		for(const FRigElementKey& Key : InKeys)
		{
			InHierarchy->GetController(true)->RemoveElement(Key, true);
		}
	}

	return SpawnerNodes;
}

#endif // WITH_EDITORONLY_DATA

UTexture2D* UControlRigBlueprint::GetRigModuleIcon() const
{
	if(IsControlRigModule())
	{
		if(UTexture2D* Icon = Cast<UTexture2D>(RigModuleSettings.Icon.TryLoad()))
		{
			return Icon;
		}
	}
	return nullptr;
}

void UControlRigBlueprint::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
#if WITH_EDITORONLY_DATA
	if(bMarkAsDirty)
	{
		Modify();
	}

	PreviewSkeletalMesh = PreviewMesh;
#endif
}

void UControlRigBlueprint::Serialize(FArchive& Ar)
{
	if(IsValid(this))
	{
		RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, RigVMClient));
		ModularRigModel.SetOuterClientHost(this);
	}
	
	Super::Serialize(Ar);

	if(Ar.IsObjectReferenceCollector())
	{
		Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);


#if WITH_EDITORONLY_DATA
		if (Ar.IsCooking() && ReferencedObjectPathsStored)
		{
			for (FSoftObjectPath ObjectPath : ReferencedObjectPaths)
			{
				ObjectPath.Serialize(Ar);
			}
		}
		else
#endif
		{
			TArray<IRigVMGraphFunctionHost*> ReferencedFunctionHosts = GetReferencedFunctionHosts(false);

			for(IRigVMGraphFunctionHost* ReferencedFunctionHost : ReferencedFunctionHosts)
			{
				if (URigVMBlueprintGeneratedClass* BPGeneratedClass = Cast<URigVMBlueprintGeneratedClass>(ReferencedFunctionHost))
				{
					Ar << BPGeneratedClass;
				}
			}

			for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibraryPtr : ShapeLibraries)
			{
				if(ShapeLibraryPtr.IsValid())
				{
					UControlRigShapeLibrary* ShapeLibrary = ShapeLibraryPtr.Get();
					Ar << ShapeLibrary;
				}
			}
		}
	}

	if(Ar.IsLoading())
	{
		if(Model_DEPRECATED || FunctionLibrary_DEPRECATED)
		{
			TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
			RigVMClient.SetFromDeprecatedData(Model_DEPRECATED, FunctionLibrary_DEPRECATED);
		}

		ModularRigModel.UpdateCachedChildren();
		ModularRigModel.Connections.UpdateFromConnectionList();
	}
}

void UControlRigBlueprint::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	// make sure to save the VM with high performance settings
	// so that during cooking we reach small footprints.
	// these settings may have changed during the user session.
	VMCompileSettings.ASTSettings.bFoldAssignments = true;
	VMCompileSettings.ASTSettings.bFoldLiterals = true;

	bExposesAnimatableControls = false;
	Hierarchy->ForEach<FRigControlElement>([this](FRigControlElement* ControlElement) -> bool
    {
		if (Hierarchy->IsAnimatable(ControlElement))
		{
			bExposesAnimatableControls = true;
			return false;
		}
		return true;
	});

	if(IsControlRigModule())
	{
		URigHierarchy* DebuggedHierarchy = Hierarchy;
		if(UControlRig* DebuggedRig = Cast<UControlRig>(GetObjectBeingDebugged()))
		{
			DebuggedHierarchy = DebuggedRig->GetHierarchy();
		}

		TGuardValue<bool> SuspendNotifGuard(Hierarchy->GetSuspendNotificationsFlag(), true);
		TGuardValue<bool> SuspendNotifGuardOnDebuggedHierarchy(DebuggedHierarchy->GetSuspendNotificationsFlag(), true);

		UpdateExposedModuleConnectors();
	}

	if (IsControlRigModule())
	{
		ControlRigType = EControlRigType::RigModule;
		ItemTypeDisplayName = TEXT("Rig Module");
	}
	else if (GetControlRigClass()->IsChildOf(UModularRig::StaticClass()))
	{
		ControlRigType = EControlRigType::ModularRig;
		ItemTypeDisplayName = TEXT("Modular Rig");
	}
	else
	{
		ControlRigType = EControlRigType::IndependentRig;
		ItemTypeDisplayName = TEXT("Control Rig");
	}
}

void UControlRigBlueprint::UpdateExposedModuleConnectors() const
{
	UControlRigBlueprint* MutableThis = ((UControlRigBlueprint*)this);
	MutableThis->RigModuleSettings.ExposedConnectors.Reset();
	Hierarchy->ForEach<FRigConnectorElement>([MutableThis](const FRigConnectorElement* ConnectorElement) -> bool
	{
		FRigModuleConnector ExposedConnector;
		ExposedConnector.Name = ConnectorElement->GetName();
		ExposedConnector.Settings = ConnectorElement->Settings;
		MutableThis->RigModuleSettings.ExposedConnectors.Add(ExposedConnector);
		return true;
	});
	PropagateHierarchyFromBPToInstances();
}

bool UControlRigBlueprint::ResolveConnector(const FRigElementKey& DraggedKey, const FRigElementKey& TargetKey, bool bSetupUndoRedo)
{
	FScopedTransaction Transaction(LOCTEXT("ResolveConnector", "Resolve connector"));

	if(bSetupUndoRedo)
	{
		Modify();
	}
	if(TargetKey.IsValid())
	{
		FRigElementKey& ExistingTargetKey = ConnectionMap.FindOrAdd(DraggedKey);
		if(ExistingTargetKey == TargetKey)
		{
			return false;
		}
		ExistingTargetKey = TargetKey;

		if (IsModularRig())
		{
			// Add connection to the model
			if (UModularRigController* Controller = GetModularRigController())
			{
				Controller->ConnectConnectorToElement(DraggedKey, TargetKey, bSetupUndoRedo, ModularRigSettings.bAutoResolve);
			}
		}
		else
		{
			ConnectionMap.FindOrAdd(DraggedKey) = TargetKey;
		}
	}
	else
	{
		if (IsModularRig())
		{
			// Add connection to the model
			if (UModularRigController* Controller = GetModularRigController())
			{
				Controller->DisconnectConnector(DraggedKey, false, bSetupUndoRedo);
			}
		}
		else
		{
			ConnectionMap.Remove(DraggedKey);
		}
	}

	RecompileModularRig();

	PropagateHierarchyFromBPToInstances();

	if(UControlRig* ControlRig = Cast<UControlRig>(GetObjectBeingDebugged()))
	{
		for (UEdGraph* Graph : UbergraphPages)
		{
			UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
			if (RigGraph == nullptr)
			{
				continue;
			}
			RigGraph->CacheNameLists(ControlRig->GetHierarchy(), &DrawContainer, ShapeLibraries);
		}
	}

	return true;
}

void UControlRigBlueprint::UpdateConnectionMapFromModel()
{
	if (IsModularRig())
	{
		ConnectionMap.Reset();

		for (const FModularRigSingleConnection& Connection : ModularRigModel.Connections)
		{
			ConnectionMap.Add(Connection.Connector, Connection.Target);
		}
	}
}

void UControlRigBlueprint::PostLoad()
{
	Super::PostLoad();

	{
#if WITH_EDITOR
		
		// correct the offset transforms
		if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::ControlOffsetTransform)
		{
			HierarchyContainer_DEPRECATED.ControlHierarchy.PostLoad();
			if (HierarchyContainer_DEPRECATED.ControlHierarchy.Num() > 0)
			{
				MarkDirtyDuringLoad();
			}

			for (FRigControl& Control : HierarchyContainer_DEPRECATED.ControlHierarchy)
			{
				const FTransform PreviousOffsetTransform = Control.GetTransformFromValue(ERigControlValueType::Initial);
				Control.OffsetTransform = PreviousOffsetTransform;
				Control.InitialValue = Control.Value;

				if (Control.ControlType == ERigControlType::Transform)
				{
					Control.InitialValue = FRigControlValue::Make<FTransform>(FTransform::Identity);
				}
				else if (Control.ControlType == ERigControlType::TransformNoScale)
				{
					Control.InitialValue = FRigControlValue::Make<FTransformNoScale>(FTransformNoScale::Identity);
				}
				else if (Control.ControlType == ERigControlType::EulerTransform)
				{
					Control.InitialValue = FRigControlValue::Make<FEulerTransform>(FEulerTransform::Identity);
				}
			}
		}

		// convert the hierarchy from V1 to V2
		if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RigHierarchyV2)
		{
			Modify();
			
			TGuardValue<bool> SuspendNotifGuard(Hierarchy->GetSuspendNotificationsFlag(), true);
			
			Hierarchy->Reset();
			GetHierarchyController()->ImportFromHierarchyContainer(HierarchyContainer_DEPRECATED, false);
		}

		// perform backwards compat value upgrades
		TArray<URigVMGraph*> GraphsToValidate = GetAllModels();
		for (int32 GraphIndex = 0; GraphIndex < GraphsToValidate.Num(); GraphIndex++)
		{
			URigVMGraph* GraphToValidate = GraphsToValidate[GraphIndex];
			if(GraphToValidate == nullptr)
			{
				continue;
			}

			for(URigVMNode* Node : GraphToValidate->GetNodes())
			{
				TArray<URigVMPin*> Pins = Node->GetAllPinsRecursively();
				for(URigVMPin* Pin : Pins)
				{
					if(Pin->GetCPPTypeObject() == StaticEnum<ERigElementType>())
					{
						if(Pin->GetDefaultValue() == TEXT("Space"))
						{
							if(URigVMController* Controller = GetController(GraphToValidate))
							{
								FRigVMControllerNotifGuard NotifGuard(Controller, true);
								Controller->SetPinDefaultValue(Pin->GetPinPath(), TEXT("Null"), false, false, false);
							}
						}
					}
				}
			}
		}

#endif
	}

	// upgrade the gizmo libraries to shape libraries
	if(!GizmoLibrary_DEPRECATED.IsNull() || GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RenameGizmoToShape)
	{
		// if it's an older file and it doesn't have the GizmoLibrary stored,
		// refer to the previous default.
		ShapeLibraries.Reset();

		if(!GizmoLibrary_DEPRECATED.IsNull())
		{
			ShapeLibrariesToLoadOnPackageLoaded.Add(GizmoLibrary_DEPRECATED.ToString());
		}
		else
		{
			static const FString DefaultGizmoLibraryPath = TEXT("/ControlRig/Controls/DefaultGizmoLibrary.DefaultGizmoLibrary");
			ShapeLibrariesToLoadOnPackageLoaded.Add(DefaultGizmoLibraryPath);
		}

		URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(false /* create if needed */));

		TArray<UObject*> ArchetypeInstances;
		CDO->GetArchetypeInstances(ArchetypeInstances);
		ArchetypeInstances.Insert(CDO, 0);

		for (UObject* Instance : ArchetypeInstances)
		{
			if (UControlRig* InstanceRig = Cast<UControlRig>(Instance))
			{
				InstanceRig->ShapeLibraries.Reset();
				InstanceRig->GizmoLibrary_DEPRECATED.Reset();
			}
		}
	}

#if WITH_EDITOR
	if(IsControlRigModule() && Hierarchy)
	{
		// backwards compat - makes sure to only ever allow one primary connector
		TArray<FRigConnectorElement*> Connectors = Hierarchy->GetConnectors();
		const int32 NumPrimaryConnectors = Algo::CountIf(Connectors, [](const FRigConnectorElement* InConnector) -> bool
		{
			return InConnector->IsPrimary();
		});
		if(NumPrimaryConnectors > 1)
		{
			bool bHasSeenPrimary = false;
			for(FRigConnectorElement* Connector : Connectors)
			{
				if(bHasSeenPrimary)
				{
					Connector->Settings.Type = EConnectorType::Secondary;
				}
				else
				{
					bHasSeenPrimary = Connector->IsPrimary();
				}
			}
			UpdateExposedModuleConnectors();
		}
	}
#endif

	ModularRigModel.PatchModelsOnLoad();
	UpdateModularDependencyDelegates();
}

#if WITH_EDITOR

void UControlRigBlueprint::HandlePackageDone()
{
	if (ShapeLibrariesToLoadOnPackageLoaded.Num() > 0)
	{
		for(const FString& ShapeLibraryToLoadOnPackageLoaded : ShapeLibrariesToLoadOnPackageLoaded)
		{
			ShapeLibraries.Add(LoadObject<UControlRigShapeLibrary>(nullptr, *ShapeLibraryToLoadOnPackageLoaded));
		}

		URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(false /* create if needed */));

		TArray<UObject*> ArchetypeInstances;
		CDO->GetArchetypeInstances(ArchetypeInstances);
		ArchetypeInstances.Insert(CDO, 0);

		for (UObject* Instance : ArchetypeInstances)
		{
			if (UControlRig* InstanceRig = Cast<UControlRig>(Instance))
			{
				InstanceRig->ShapeLibraries = ShapeLibraries;
			}
		}

		ShapeLibrariesToLoadOnPackageLoaded.Reset();
	}

	PropagateHierarchyFromBPToInstances();

	Super::HandlePackageDone();
	
	if(IsModularRig())
	{
		// force load all dependencies
		ModularRigModel.ForEachModule([](const FRigModuleReference* Element) -> bool
		{
			(void)Element->Class.LoadSynchronous();
			return true;
		});

		RecompileModularRig();
	}
}

void UControlRigBlueprint::HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure)
{
	Super::HandleConfigureRigVMController(InClient, InControllerToConfigure);

	TWeakObjectPtr<URigVMBlueprint> WeakThis(this);
	InControllerToConfigure->ConfigureWorkflowOptionsDelegate.BindLambda([WeakThis](URigVMUserWorkflowOptions* Options)
	{
		if(UControlRigWorkflowOptions* ControlRigNodeWorkflowOptions = Cast<UControlRigWorkflowOptions>(Options))
		{
			ControlRigNodeWorkflowOptions->Hierarchy = nullptr;
			ControlRigNodeWorkflowOptions->Selection.Reset();
			
			if(const URigVMBlueprint* StrongThis = WeakThis.Get())
			{
				if(UControlRig* ControlRig = Cast<UControlRig>(StrongThis->GetObjectBeingDebugged()))
				{
					ControlRigNodeWorkflowOptions->Hierarchy = ControlRig->GetHierarchy();
					ControlRigNodeWorkflowOptions->Selection = ControlRig->GetHierarchy()->GetSelectedKeys();
				}
			}
		}
	});
}

#endif

UClass* UControlRigBlueprint::GetRigVMEdGraphNodeClass() const
{
	return UControlRigGraphNode::StaticClass();
}

UClass* UControlRigBlueprint::GetRigVMEdGraphSchemaClass() const
{
	return UControlRigGraphSchema::StaticClass();
}

UClass* UControlRigBlueprint::GetRigVMEdGraphClass() const
{
	return UControlRigGraph::StaticClass();
}

UClass* UControlRigBlueprint::GetRigVMEditorSettingsClass() const
{
	return UControlRigEditorSettings::StaticClass();
}

void UControlRigBlueprint::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	for (FRigModuleReference& Module : ModularRigModel.Modules)
	{
		OutDeps.Add(Module.Class.Get());
	}
}

#if WITH_EDITOR
const FName& UControlRigBlueprint::GetPanelPinFactoryName() const
{
	return ControlRigPanelNodeFactoryName;
}

IRigVMEditorModule* UControlRigBlueprint::GetEditorModule() const
{
	return &IControlRigEditorModule::Get();
}
#endif

TArray<FString> UControlRigBlueprint::GeneratePythonCommands(const FString InNewBlueprintName)
{
	TArray<FString> InternalCommands;
	InternalCommands.Add(TEXT("import unreal"));
	InternalCommands.Add(TEXT("unreal.load_module('ControlRigDeveloper')"));
	InternalCommands.Add(TEXT("factory = unreal.ControlRigBlueprintFactory"));
	InternalCommands.Add(FString::Printf(TEXT("blueprint = factory.create_new_control_rig_asset(desired_package_path = '%s')"), *InNewBlueprintName));
	InternalCommands.Add(TEXT("hierarchy = blueprint.hierarchy"));
	InternalCommands.Add(TEXT("hierarchy_controller = hierarchy.get_controller()"));

	// Hierarchy
	InternalCommands.Append(Hierarchy->GetController(true)->GeneratePythonCommands());

#if WITH_EDITORONLY_DATA
	const FString PreviewMeshPath = GetPreviewMesh()->GetPathName();
	InternalCommands.Add(FString::Printf(TEXT("blueprint.set_preview_mesh(unreal.load_object(name='%s', outer=None))"),
		*PreviewMeshPath));
#endif

	InternalCommands.Append(Super::GeneratePythonCommands(InNewBlueprintName));
	return InternalCommands;
}

void UControlRigBlueprint::GetTypeActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	IControlRigEditorModule::Get().GetTypeActions((UControlRigBlueprint*)this, ActionRegistrar);
}

void UControlRigBlueprint::GetInstanceActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	IControlRigEditorModule::Get().GetInstanceActions((UControlRigBlueprint*)this, ActionRegistrar);
}

void UControlRigBlueprint::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		TArray<FName> PropertiesChanged = TransactionEvent.GetChangedProperties();
		int32 TransactionIndex = GEditor->Trans->FindTransactionIndex(TransactionEvent.GetTransactionId());
		const FTransaction* Transaction = GEditor->Trans->GetTransaction(TransactionIndex);
		if (Transaction && Transaction->ContainsObject(Hierarchy))
		{
			if (Transaction->GetTitle().BuildSourceString() == TEXT("Transform Gizmo"))
			{
				PropagatePoseFromBPToInstances();
				return;
			}

			PropagateHierarchyFromBPToInstances();

			// make sure the bone name list is up 2 date for the editor graph
			for (UEdGraph* Graph : UbergraphPages)
			{
				UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
				if (RigGraph == nullptr)
				{
					continue;
				}
				RigGraph->CacheNameLists(Hierarchy, &DrawContainer, ShapeLibraries);
			}

			RequestAutoVMRecompilation();
			(void)MarkPackageDirty();
		}

		if (PropertiesChanged.Contains(GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, ModularRigModel)))
		{
			if (IsModularRig())
			{
				ModularRigModel.UpdateCachedChildren();
				ModularRigModel.Connections.UpdateFromConnectionList();
				RecompileModularRig();
			}
		}
		
		if (PropertiesChanged.Contains(GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, DrawContainer)))
		{
			PropagateDrawInstructionsFromBPToInstances();
		}

		if (PropertiesChanged.Contains(GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, ConnectionMap)))
		{
			PropagateHierarchyFromBPToInstances();
		}
	}
}

void UControlRigBlueprint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	
	if (URigHierarchyController* Controller = Hierarchy->GetController(true))
	{
		Controller->OnModified().RemoveAll(this);
		Controller->OnModified().AddUObject(this, &UControlRigBlueprint::HandleHierarchyModified);
	}

	if (UModularRigController* ModularController = ModularRigModel.GetController())
	{
		ModularController->OnModified().RemoveAll(this);
		ModularController->OnModified().AddUObject(this, &UControlRigBlueprint::HandleRigModulesModified);
	}

	// update the rig module identifier after save-as or duplicate asset
	if(IsControlRigModule())
	{
		RigModuleSettings.Identifier.Name = URigHierarchy::GetSanitizedName(FRigName(GetName())).ToString();
	}

	ModularRigModel.UpdateCachedChildren();
	ModularRigModel.Connections.UpdateFromConnectionList();
}

void UControlRigBlueprint::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// update the rig module identifier after renaming the asset
	if(IsControlRigModule())
	{
		RigModuleSettings.Identifier.Name = URigHierarchy::GetSanitizedName(FRigName(GetName())).ToString();
	}
}

TArray<UControlRigBlueprint*> UControlRigBlueprint::GetCurrentlyOpenRigBlueprints()
{
	return sCurrentlyOpenedRigBlueprints;
}

#if WITH_EDITOR

const FControlRigShapeDefinition* UControlRigBlueprint::GetControlShapeByName(const FName& InName) const
{
	TMap<FString, FString> LibraryNameMap;
	if(UControlRig* ControlRig = Cast<UControlRig>(GetObjectBeingDebugged()))
	{
		LibraryNameMap = ControlRig->ShapeLibraryNameMap;
	}
	return UControlRigShapeLibrary::GetShapeByName(InName, ShapeLibraries, LibraryNameMap);
}

FName UControlRigBlueprint::AddTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget)
{
	TUniquePtr<FControlValueScope> ValueScope;
	if (!UControlRigEditorSettings::Get()->bResetControlsOnPinValueInteraction) // if we need to retain the controls
	{
		ValueScope = MakeUnique<FControlValueScope>(this);
	}

	// for now we only allow one pin control at the same time
	ClearTransientControls();

	URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
	UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

	FName ReturnName = NAME_None;
	TArray<UObject*> ArchetypeInstances;
	CDO->GetArchetypeInstances(ArchetypeInstances);
	for (UObject* ArchetypeInstance : ArchetypeInstances)
	{
		UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
		if (InstancedControlRig)
		{
			FName ControlName = InstancedControlRig->AddTransientControl(InNode, InTarget);
			if (ReturnName == NAME_None)
			{
				ReturnName = ControlName;
			}
		}
	}

	return ReturnName;
}

FName UControlRigBlueprint::RemoveTransientControl(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget)
{
	TUniquePtr<FControlValueScope> ValueScope;
	if (!UControlRigEditorSettings::Get()->bResetControlsOnPinValueInteraction) // if we need to retain the controls
	{
		ValueScope = MakeUnique<FControlValueScope>(this);
	}

	URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
	UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

	FName RemovedName = NAME_None;
	TArray<UObject*> ArchetypeInstances;
	CDO->GetArchetypeInstances(ArchetypeInstances);
	for (UObject* ArchetypeInstance : ArchetypeInstances)
	{
		UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
		if (InstancedControlRig)
		{
			FName Name = InstancedControlRig->RemoveTransientControl(InNode, InTarget);
			if (RemovedName == NAME_None)
	{
				RemovedName = Name;
			}
		}
	}

	return RemovedName;
}

FName UControlRigBlueprint::AddTransientControl(const FRigElementKey& InElement)
{
	TUniquePtr<FControlValueScope> ValueScope;
	if (!UControlRigEditorSettings::Get()->bResetControlsOnPinValueInteraction) // if we need to retain the controls
	{
		ValueScope = MakeUnique<FControlValueScope>(this);
	}
	URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
	UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

	FName ReturnName = NAME_None;
	TArray<UObject*> ArchetypeInstances;
	CDO->GetArchetypeInstances(ArchetypeInstances);

	// hierarchy transforms will be reset when ClearTransientControls() is called,
	// so to retain any bone transform modifications we have to save them
	TMap<UObject*, FTransform> SavedElementLocalTransforms;
	for (UObject* ArchetypeInstance : ArchetypeInstances)
	{
		UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
		if (InstancedControlRig)
		{
			if (InstancedControlRig->DynamicHierarchy)
			{ 
				SavedElementLocalTransforms.FindOrAdd(InstancedControlRig) = InstancedControlRig->DynamicHierarchy->GetLocalTransform(InElement);
			}
		}
	}

	// for now we only allow one pin control at the same time
	ClearTransientControls();
	
	for (UObject* ArchetypeInstance : ArchetypeInstances)
	{
		UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
		if (InstancedControlRig)
		{
			// restore the element transforms so that transient controls are created at the right place
			if (const FTransform* SavedTransform = SavedElementLocalTransforms.Find(InstancedControlRig))
			{
				if (InstancedControlRig->DynamicHierarchy)
				{ 
					InstancedControlRig->DynamicHierarchy->SetLocalTransform(InElement, *SavedTransform);
				}
			}
			
			FName ControlName = InstancedControlRig->AddTransientControl(InElement);
			if (ReturnName == NAME_None)
			{
				ReturnName = ControlName;
			}
		}
	}

	return ReturnName;

}

FName UControlRigBlueprint::RemoveTransientControl(const FRigElementKey& InElement)
{
	TUniquePtr<FControlValueScope> ValueScope;
	if (!UControlRigEditorSettings::Get()->bResetControlsOnPinValueInteraction) // if we need to retain the controls
	{
		ValueScope = MakeUnique<FControlValueScope>(this);
	}

	URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
	UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

	FName RemovedName = NAME_None;
	TArray<UObject*> ArchetypeInstances;
	CDO->GetArchetypeInstances(ArchetypeInstances);
	for (UObject* ArchetypeInstance : ArchetypeInstances)
	{
		UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
		if (InstancedControlRig)
		{
			FName Name = InstancedControlRig->RemoveTransientControl(InElement);
			if (RemovedName == NAME_None)
			{
				RemovedName = Name;
			}
		}
	}

	return RemovedName;
}

void UControlRigBlueprint::ClearTransientControls()
{
	TUniquePtr<FControlValueScope> ValueScope;
	if (!UControlRigEditorSettings::Get()->bResetControlsOnPinValueInteraction) // if we need to retain the controls
	{
		ValueScope = MakeUnique<FControlValueScope>(this);
	}

	if (URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass())
	{
		UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

		TArray<UObject*> ArchetypeInstances;
		CDO->GetArchetypeInstances(ArchetypeInstances);
		for (UObject* ArchetypeInstance : ArchetypeInstances)
		{
			UControlRig* InstancedControlRig = Cast<UControlRig>(ArchetypeInstance);
			if (InstancedControlRig)
			{
				InstancedControlRig->ClearTransientControls();
			}
		}
	}
}

UModularRigController* UControlRigBlueprint::GetModularRigController() 
{
	if (!GetControlRigClass()->IsChildOf(UModularRig::StaticClass()))
	{
		return nullptr;
	}

	return ModularRigModel.GetController();
}

void UControlRigBlueprint::RecompileModularRig()
{
	RefreshModuleConnectors();
	OnModularRigPreCompiled().Broadcast(this);
	if (const UClass* MyControlRigClass = GeneratedClass)
	{
		if (UModularRig* DefaultObject = Cast<UModularRig>(MyControlRigClass->GetDefaultObject(false)))
		{
			PropagateModuleHierarchyFromBPToInstances();
		}
	}
	UpdateModularDependencyDelegates();
	OnModularRigCompiled().Broadcast(this);
}

#endif

void UControlRigBlueprint::SetupDefaultObjectDuringCompilation(URigVMHost* InCDO)
{
	Super::SetupDefaultObjectDuringCompilation(InCDO);
	CastChecked<UControlRig>(InCDO)->GetHierarchy()->CopyHierarchy(Hierarchy);
}

void UControlRigBlueprint::SetupPinRedirectorsForBackwardsCompatibility()
{
	for(URigVMGraph* Model : RigVMClient)
	{
		for (URigVMNode* Node : Model->GetNodes())
		{
			if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
			{
				UScriptStruct* Struct = UnitNode->GetScriptStruct();
				if (Struct == FRigUnit_SetBoneTransform::StaticStruct())
				{
					URigVMPin* TransformPin = UnitNode->FindPin(TEXT("Transform"));
					URigVMPin* ResultPin = UnitNode->FindPin(TEXT("Result"));
					GetOrCreateController()->AddPinRedirector(false, true, TransformPin->GetPinPath(), ResultPin->GetPinPath());
				}
			}
		}
	}
}

void UControlRigBlueprint::PathDomainSpecificContentOnLoad()
{
	PatchRigElementKeyCacheOnLoad();
	PatchPropagateToChildren();
}

void UControlRigBlueprint::PatchRigElementKeyCacheOnLoad()
{
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::RigElementKeyCache)
	{
		for (URigVMGraph* Graph : GetAllModels())
		{
			URigVMController* Controller = GetOrCreateController(Graph);
			TGuardValue<bool> DisablePinDefaultValueValidation(Controller->bValidatePinDefaults, false);
			FRigVMControllerNotifGuard NotifGuard(Controller, true);
			for (URigVMNode* Node : Graph->GetNodes())
			{
				if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
				{
					UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct();
					FString FunctionName = FString::Printf(TEXT("%s::%s"), *ScriptStruct->GetStructCPPName(), *UnitNode->GetMethodName().ToString());
					const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*FunctionName);
					check(Function);
					for (TFieldIterator<FProperty> It(Function->Struct); It; ++It)
					{
						if (It->GetCPPType() == TEXT("FCachedRigElement"))
						{
							if (URigVMPin* Pin = Node->FindPin(It->GetName()))
							{
								int32 BoneIndex = FCString::Atoi(*Pin->GetDefaultValue());
								FRigElementKey Key = Hierarchy->GetKey(BoneIndex);
								FCachedRigElement DefaultValueElement(Key, Hierarchy);
								FString Result;
								TBaseStructure<FCachedRigElement>::Get()->ExportText(Result, &DefaultValueElement, nullptr, nullptr, PPF_None, nullptr);								
								Controller->SetPinDefaultValue(Pin->GetPinPath(), Result, true, false, false);
								MarkDirtyDuringLoad();
							}							
						}
					}
				}
			}
		}
	}
}

// change the default value form False to True for transform nodes
void UControlRigBlueprint::PatchPropagateToChildren()
{
	// no need to update default value past this version
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) >= FControlRigObjectVersion::RenameGizmoToShape)
	{
		return;
	}
	
	auto IsNullOrControl = [](const URigVMPin* InPin)
	{
		const bool bHasItem = InPin->GetCPPTypeObject() == FRigElementKey::StaticStruct() && InPin->GetName() == "Item";
		if (!bHasItem)
		{
			return false;
		}

		if (const URigVMPin* TypePin = InPin->FindSubPin(TEXT("Type")))
		{
			const FString& TypeValue = TypePin->GetDefaultValue();
			return TypeValue == TEXT("Null") || TypeValue == TEXT("Space") || TypeValue == TEXT("Control");
		}
		
		return false;
	};

	auto IsPropagateChildren = [](const URigVMPin* InPin)
	{
		return InPin->GetCPPType() == TEXT("bool") && InPin->GetName() == TEXT("bPropagateToChildren");
	};

	auto FindPropagatePin = [IsNullOrControl, IsPropagateChildren](const URigVMNode* InNode)-> URigVMPin*
	{
		URigVMPin* PropagatePin = nullptr;
		URigVMPin* ItemPin = nullptr;  
		for (URigVMPin* Pin: InNode->GetPins())
		{
			// look for Item pin
			if (!ItemPin && IsNullOrControl(Pin))
			{
				ItemPin = Pin;
			}

			// look for bPropagateToChildren pin
			if (!PropagatePin && IsPropagateChildren(Pin))
			{
				PropagatePin = Pin;
			}

			// return propagation pin if both found
			if (ItemPin && PropagatePin)
			{
				return PropagatePin;
			}
		}
		return nullptr;
	};

	for (URigVMGraph* Graph : GetAllModels())
	{
		TArray< const URigVMPin* > PinsToUpdate;
		for (const URigVMNode* Node : Graph->GetNodes())
		{
			if (const URigVMPin* PropagatePin = FindPropagatePin(Node))
			{
				PinsToUpdate.Add(PropagatePin);
			}
		}
		
		if (URigVMController* Controller = GetOrCreateController(Graph))
		{
			FRigVMControllerNotifGuard NotifGuard(Controller, true);
			for (const URigVMPin* Pin: PinsToUpdate)
			{
				Controller->SetPinDefaultValue(Pin->GetPinPath(), TEXT("True"), false, false, false);
			}
		}
	}
}

void UControlRigBlueprint::PatchFunctionsOnLoad()
{
	URigVMBlueprintGeneratedClass* CRGeneratedClass = GetRigVMBlueprintGeneratedClass();
	FRigVMGraphFunctionStore& Store = CRGeneratedClass->GraphFunctionStore;
	const URigVMFunctionLibrary* Library = GetLocalFunctionLibrary();

	TMap<URigVMLibraryNode*, FRigVMGraphFunctionHeader> OldHeaders;

	// Backwards compatibility. Store public access in the model
	TArray<FName> BackwardsCompatiblePublicFunctions;
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::StoreFunctionsInGeneratedClass)
	{
		for (const FRigVMOldPublicFunctionData& OldPublicFunction : PublicFunctions_DEPRECATED)
		{
			BackwardsCompatiblePublicFunctions.Add(OldPublicFunction.Name);
		}
	}
	else
	{
		if (GetLinkerCustomVersion(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMSaveFunctionAccessInModel)
		{
			for (const FRigVMGraphFunctionData& FunctionData : Store.PublicFunctions)
			{
				BackwardsCompatiblePublicFunctions.Add(FunctionData.Header.Name);
				URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(FunctionData.Header.LibraryPointer.LibraryNode.ResolveObject());
				OldHeaders.Add(LibraryNode, FunctionData.Header);
			}
		}
	}

	// Addressing issue where PublicGraphFunctions is populated, but the model PublicFunctionNames is not
	URigVMFunctionLibrary* FunctionLibrary = GetLocalFunctionLibrary();
	if (FunctionLibrary)
	{
		if (PublicGraphFunctions.Num() > FunctionLibrary->PublicFunctionNames.Num())
		{
			for (const FRigVMGraphFunctionHeader& PublicHeader : PublicGraphFunctions)
			{
				BackwardsCompatiblePublicFunctions.Add(PublicHeader.Name);
			}
		}
	}

	// Lets rebuild the FunctionStore from the model
	if (FunctionLibrary)
	{
		Store.PublicFunctions.Reset();
		Store.PrivateFunctions.Reset();

		for (URigVMLibraryNode* LibraryNode : FunctionLibrary->GetFunctions())
		{
			bool bIsPublic = FunctionLibrary->IsFunctionPublic(LibraryNode->GetFName());
			if (!bIsPublic)
			{
				bIsPublic = BackwardsCompatiblePublicFunctions.Contains(LibraryNode->GetFName());
				if (bIsPublic)
				{
					FunctionLibrary->PublicFunctionNames.Add(LibraryNode->GetFName());
				}
			}

			FRigVMGraphFunctionHeader Header = LibraryNode->GetFunctionHeader(CRGeneratedClass);
			if (FRigVMGraphFunctionHeader* OldHeader = OldHeaders.Find(LibraryNode))
			{				
				Header.ExternalVariables = OldHeader->ExternalVariables;
				Header.Dependencies = OldHeader->Dependencies;
			}
			Store.AddFunction(Header, bIsPublic);
			
		}
	}

	// Update dependencies and external variables if needed
	for (URigVMLibraryNode* LibraryNode : Library->GetFunctions())
	{
		GetRigVMClient()->UpdateExternalVariablesForFunction(LibraryNode);
		GetRigVMClient()->UpdateDependenciesForFunction(LibraryNode);
	}
}

void UControlRigBlueprint::CreateMemberVariablesOnLoad()
{
#if WITH_EDITOR

	const int32 LinkerVersion = GetLinkerCustomVersion(FControlRigObjectVersion::GUID);
	if (LinkerVersion < FControlRigObjectVersion::SwitchedToRigVM)
	{
		// ignore errors during the first potential compile of the VM
		// since that this point variable nodes may still be ill-formed.
		TGuardValue<FRigVMReportDelegate> SuspendReportDelegate(VMCompileSettings.ASTSettings.ReportDelegate,
			FRigVMReportDelegate::CreateLambda([](EMessageSeverity::Type,  UObject*, const FString&)
			{
				// do nothing
			})
		);
		InitializeModelIfRequired();
	}

	AddedMemberVariableMap.Reset();

	for (int32 VariableIndex = 0; VariableIndex < NewVariables.Num(); VariableIndex++)
	{
		AddedMemberVariableMap.Add(NewVariables[VariableIndex].VarName, VariableIndex);
	}

	if (RigVMClient.Num() == 0)
	{
		return;
	}

	// setup variables on the blueprint based on the previous "parameters"
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::BlueprintVariableSupport)
	{
		TSharedPtr<FKismetNameValidator> NameValidator = MakeShareable(new FKismetNameValidator(this, NAME_None, nullptr));

		auto CreateVariable = [this, NameValidator](const URigVMVariableNode* InVariableNode)
		{
			if (!InVariableNode)
			{
				return;
			}
			
			static const FString VariableString = TEXT("Variable");
			if (URigVMPin* VariablePin = InVariableNode->FindPin(VariableString))
			{
				if (VariablePin->GetDirection() != ERigVMPinDirection::Visible)
				{
					return;
				}
			}

			const FRigVMGraphVariableDescription Description = InVariableNode->GetVariableDescription();
			if (AddedMemberVariableMap.Contains(Description.Name))
			{
				return;
			}

			const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(Description.ToExternalVariable());
			if (!PinType.PinCategory.IsValid())
			{
				return;
			}

			const FName VarName = FindHostMemberVariableUniqueName(NameValidator, Description.Name.ToString());
			const int32 VariableIndex = AddHostMemberVariable(this, VarName, PinType, false, false, FString());
			if (VariableIndex != INDEX_NONE)
			{
				AddedMemberVariableMap.Add(Description.Name, VariableIndex);
				MarkDirtyDuringLoad();
			}
		};

		auto CreateParameter = [this, NameValidator](const URigVMParameterNode* InParameterNode)
		{
			if (!InParameterNode)
			{
				return;
			}

			static const FString ParameterString = TEXT("Parameter");
			if (const URigVMPin* ParameterPin = InParameterNode->FindPin(ParameterString))
			{
				if (ParameterPin->GetDirection() != ERigVMPinDirection::Visible)
				{
					return;
				}
			}

			const FRigVMGraphParameterDescription Description = InParameterNode->GetParameterDescription();
			if (AddedMemberVariableMap.Contains(Description.Name))
			{
				return;
			}

			const FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(Description.ToExternalVariable());
			if (!PinType.PinCategory.IsValid())
			{
				return;
			}

			const FName VarName = FindHostMemberVariableUniqueName(NameValidator, Description.Name.ToString());
			const int32 VariableIndex = AddHostMemberVariable(this, VarName, PinType, true, !Description.bIsInput, FString());
			
			if (VariableIndex != INDEX_NONE)
			{
				AddedMemberVariableMap.Add(Description.Name, VariableIndex);
				MarkDirtyDuringLoad();
			}
		};
		
		for (const URigVMGraph* Model : RigVMClient)
		{
			const TArray<URigVMNode*>& Nodes = Model->GetNodes();
			for (const URigVMNode* Node : Nodes)
			{
				if (const URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
				{
					CreateVariable(VariableNode);
				}

				// Leaving this for backwards compatibility, even though we don't support parameters anymore
				// When a parameter node is found, we will create a variable
				else if (const URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(Node))
				{
					CreateParameter(ParameterNode);
				}
			}
		}
	}

#endif
}

void UControlRigBlueprint::PatchVariableNodesOnLoad()
{
#if WITH_EDITOR

	// setup variables on the blueprint based on the previous "parameters"
	if (GetLinkerCustomVersion(FControlRigObjectVersion::GUID) < FControlRigObjectVersion::BlueprintVariableSupport)
	{
		TGuardValue<bool> GuardNotifsSelf(bSuspendModelNotificationsForSelf, true);
		
		check(GetDefaultModel());

		auto PatchVariableNode = [this](const URigVMVariableNode* InVariableNode)
		{
			if (!InVariableNode)
			{
				return;
			}

			const FRigVMGraphVariableDescription Description = InVariableNode->GetVariableDescription();
			if (!AddedMemberVariableMap.Contains(Description.Name))
			{
				return;
			}
			
			const int32 VariableIndex = AddedMemberVariableMap.FindChecked(Description.Name);
			const FName VarName = NewVariables[VariableIndex].VarName;
			
			GetOrCreateController()->RefreshVariableNode(
				InVariableNode->GetFName(), VarName, Description.CPPType, Description.CPPTypeObject, false);
			
			MarkDirtyDuringLoad();			
		};

		auto PatchParameterNode = [this](const URigVMParameterNode* InParameterNode)
		{
			if (!InParameterNode)
			{
				return;
			}
			
			const FRigVMGraphParameterDescription Description = InParameterNode->GetParameterDescription();
			if (!AddedMemberVariableMap.Contains(Description.Name))
			{
				return;
			}

			const int32 VariableIndex = AddedMemberVariableMap.FindChecked(Description.Name);
			const FName VarName = NewVariables[VariableIndex].VarName;
			
			GetOrCreateController()->ReplaceParameterNodeWithVariable(
				InParameterNode->GetFName(), VarName, Description.CPPType, Description.CPPTypeObject, false);

			MarkDirtyDuringLoad();	
		};
		
		for(const URigVMGraph* Model : RigVMClient)
		{
			TArray<URigVMNode*> Nodes = Model->GetNodes();
			for (URigVMNode* Node : Nodes)
			{
				if (const URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
				{
					PatchVariableNode(VariableNode);
				}
				else if (const URigVMParameterNode* ParameterNode = Cast<URigVMParameterNode>(Node))
				{
					PatchParameterNode(ParameterNode);
				}
			}
		}
	}

#endif

	Super::PatchVariableNodesOnLoad();
}

void UControlRigBlueprint::UpdateElementKeyRedirector(UControlRig* InControlRig) const
{
	InControlRig->HierarchySettings = HierarchySettings;
	InControlRig->RigModuleSettings = RigModuleSettings;
	InControlRig->ElementKeyRedirector = FRigElementKeyRedirector(ConnectionMap, InControlRig->GetHierarchy());
}

void UControlRigBlueprint::PropagatePoseFromInstanceToBP(UControlRig* InControlRig) const
{
	check(InControlRig);
	// current transforms in BP and CDO are meaningless, no need to copy them
	// we use BP hierarchy to initialize CDO and instances' hierarchy, 
	// so it should always be in the initial state.
	Hierarchy->CopyPose(InControlRig->GetHierarchy(), false, true, false, true);
}

void UControlRigBlueprint::PropagatePoseFromBPToInstances() const
{
	if (UClass* MyControlRigClass = GeneratedClass)
	{
		if (UControlRig* DefaultObject = Cast<UControlRig>(MyControlRigClass->GetDefaultObject(false)))
		{
			DefaultObject->PostInitInstanceIfRequired();
			DefaultObject->GetHierarchy()->CopyPose(Hierarchy, true, true, true);

			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);
			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (UControlRig* InstanceRig = Cast<UControlRig>(ArchetypeInstance))
				{
					InstanceRig->PostInitInstanceIfRequired();
					if(!InstanceRig->IsRigModuleInstance())
					{
						InstanceRig->GetHierarchy()->CopyPose(Hierarchy, true, true, true);
					}
				}
			}
		}
	}
}

void UControlRigBlueprint::PropagateHierarchyFromBPToInstances() const
{
	if (UClass* MyControlRigClass = GeneratedClass)
	{
		if (UControlRig* DefaultObject = Cast<UControlRig>(MyControlRigClass->GetDefaultObject(false)))
		{
			DefaultObject->PostInitInstanceIfRequired();
			DefaultObject->GetHierarchy()->CopyHierarchy(Hierarchy);

			UpdateElementKeyRedirector(DefaultObject);

			if (!DefaultObject->HasAnyFlags(RF_NeedPostLoad)) // If CDO is loading, skip Init, it will be done later
			{
				DefaultObject->Initialize(true);
			}

			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);
			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (UControlRig* InstanceRig = Cast<UControlRig>(ArchetypeInstance))
				{
					if (InstanceRig->IsRigModuleInstance())
					{
						if (UModularRig* ModularRig = Cast<UModularRig>(InstanceRig->GetOuter()))
						{
							ModularRig->RequestInit();
						}
					}
					else
					{
						InstanceRig->PostInitInstanceIfRequired();
						InstanceRig->GetHierarchy()->CopyHierarchy(Hierarchy);
						InstanceRig->HierarchySettings = HierarchySettings;
						UpdateElementKeyRedirector(InstanceRig);
						InstanceRig->Initialize(true);
					}
				}
			}
		}
	}
}

void UControlRigBlueprint::PropagateDrawInstructionsFromBPToInstances() const
{
	if (UClass* MyControlRigClass = GeneratedClass)
	{
		if (UControlRig* DefaultObject = Cast<UControlRig>(MyControlRigClass->GetDefaultObject(false)))
	{
			DefaultObject->DrawContainer = DrawContainer;

			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);

			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (UControlRig* InstanceRig = Cast<UControlRig>(ArchetypeInstance))
	{
					InstanceRig->DrawContainer = DrawContainer;
				}
			}
		}
	}


	// make sure the bone name list is up 2 date for the editor graph
	for (UEdGraph* Graph : UbergraphPages)
	{
		UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
		if (RigGraph == nullptr)
		{
			continue;
		}
		RigGraph->CacheNameLists(Hierarchy, &DrawContainer, ShapeLibraries);
	}
}

void UControlRigBlueprint::PropagatePropertyFromBPToInstances(FRigElementKey InRigElement, const FProperty* InProperty) const
{
	int32 ElementIndex = Hierarchy->GetIndex(InRigElement);
	ensure(ElementIndex != INDEX_NONE);
	check(InProperty);

	if (UClass* MyControlRigClass = GeneratedClass)
	{
		if (UControlRig* DefaultObject = Cast<UControlRig>(MyControlRigClass->GetDefaultObject(false)))
		{
			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);

			const int32 PropertyOffset = InProperty->GetOffset_ReplaceWith_ContainerPtrToValuePtr();
			const int32 PropertySize = InProperty->GetSize();

			uint8* Source = ((uint8*)Hierarchy->Get(ElementIndex)) + PropertyOffset;
			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (UControlRig* InstanceRig = Cast<UControlRig>(ArchetypeInstance))
				{
					InstanceRig->PostInitInstanceIfRequired();
					uint8* Dest = ((uint8*)InstanceRig->GetHierarchy()->Get(ElementIndex)) + PropertyOffset;
					FMemory::Memcpy(Dest, Source, PropertySize);
				}
			}
		}
	}
}

void UControlRigBlueprint::PropagatePropertyFromInstanceToBP(FRigElementKey InRigElement, const FProperty* InProperty, UControlRig* InInstance) const
{
	const int32 ElementIndex = Hierarchy->GetIndex(InRigElement);
	ensure(ElementIndex != INDEX_NONE);
	check(InProperty);

	const int32 PropertyOffset = InProperty->GetOffset_ReplaceWith_ContainerPtrToValuePtr();
	const int32 PropertySize = InProperty->GetSize();
	uint8* Source = ((uint8*)InInstance->GetHierarchy()->Get(ElementIndex)) + PropertyOffset;
	uint8* Dest = ((uint8*)Hierarchy->Get(ElementIndex)) + PropertyOffset;
	FMemory::Memcpy(Dest, Source, PropertySize);
}

void UControlRigBlueprint::PropagateModuleHierarchyFromBPToInstances() const
{
	if (const UClass* MyControlRigClass = GeneratedClass)
	{
		if (UModularRig* DefaultObject = Cast<UModularRig>(MyControlRigClass->GetDefaultObject(false)))
		{
			// We need to first transfer the model from the blueprint to the CDO
			// We then ask instances to initialize which will provoke a call UModularRig::UpdateModuleHierarchyFromCDO
			
			DefaultObject->ResetModules();

			// copy the model over to the CDO.
			// non-CDO instances are going to instantiate the model into a
			// UObject module instance tree. CDO's are data only to avoid bugs / 
			// behaviors in the blueprint re-instancer - which is disregarding any
			// object under a CDO.
			DefaultObject->ModularRigModel = ModularRigModel;
			DefaultObject->ModularRigModel.SetOuterClientHost(DefaultObject);
			DefaultObject->ModularRigSettings = ModularRigSettings;
			
			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);
			for (UObject* ArchetypeInstance : ArchetypeInstances)
			{
				if (UControlRig* InstanceRig = Cast<UControlRig>(ArchetypeInstance))
				{
					// this will provoke a call to InitializeFromCDO
					InstanceRig->Initialize(true);
				}
			}
		}
	}
}

void UControlRigBlueprint::UpdateModularDependencyDelegates()
{
	TArray<const UBlueprint*> VisitList;
	ModularRigModel.ForEachModule([&VisitList, this](const FRigModuleReference* Element) -> bool
	{
		if(const UClass* Class = Element->Class.Get())
		{
			if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Class->ClassGeneratedBy))
			{
				if(!VisitList.Contains(Blueprint))
				{
					Blueprint->OnVMCompiled().RemoveAll(this);
					Blueprint->OnModularRigCompiled().RemoveAll(this);
					Blueprint->OnVMCompiled().AddUObject(this, &UControlRigBlueprint::OnModularDependencyVMCompiled);
					Blueprint->OnModularRigCompiled().AddUObject(this, &UControlRigBlueprint::OnModularDependencyChanged);
					VisitList.Add(Blueprint);
				}
			}
		}
		return true;
	});
}

void UControlRigBlueprint::OnModularDependencyVMCompiled(UObject* InBlueprint, URigVM* InVM, FRigVMExtendedExecuteContext& InExecuteContext)
{
	if(URigVMBlueprint* RigVMBlueprint = Cast<URigVMBlueprint>(InBlueprint))
	{
		OnModularDependencyChanged(RigVMBlueprint);
	}
}

void UControlRigBlueprint::OnModularDependencyChanged(URigVMBlueprint* InBlueprint)
{
	RefreshModuleVariables();
	RefreshModuleConnectors();
	RecompileModularRig();
}

void UControlRigBlueprint::RequestConstructionOnAllModules()
{
	// the rig will perform initialize itself - but we should request construction
	check(IsModularRig());
	
	const URigVMBlueprintGeneratedClass* RigClass = GetRigVMBlueprintGeneratedClass();
	check(RigClass);
	
	UControlRig* CDO = Cast<UControlRig>(RigClass->GetDefaultObject(true /* create if needed */));

	TArray<UObject*> ArchetypeInstances;
	CDO->GetArchetypeInstances(ArchetypeInstances);

	// visit all or our instances and request construction
	for (UObject* Instance : ArchetypeInstances)
	{
		if (UModularRig* InstanceRig = Cast<UModularRig>(Instance))
		{
			InstanceRig->RequestConstruction();
		}
	}

}

void UControlRigBlueprint::RefreshModuleVariables()
{
	if(!IsModularRig())
	{
		return;
	}

	if (UModularRigController* Controller = GetModularRigController())
	{
		ModularRigModel.ForEachModule([this, Controller](const FRigModuleReference* Element) -> bool
	   {
		   TGuardValue<bool> NotificationsGuard(Controller->bSuspendNotifications, true);
		   RefreshModuleVariables(Element);
		   return true;
	   });
	}
}

void UControlRigBlueprint::RefreshModuleVariables(const FRigModuleReference* InModule)
{
	if(!IsModularRig())
	{
		return;
	}

	// avoid dead class pointers
	const UClass* ModuleClass = InModule->Class.Get();
	if(ModuleClass == nullptr)
	{
		return;
	}

	// Make sure the provided module belongs to our ModularRigModel
	const FString& ModulePath = InModule->GetPath();
	FRigModuleReference* Module = ModularRigModel.FindModule(ModulePath);
	if (Module != InModule)
	{
		return;
	}

	Modify();

	for (TFieldIterator<FProperty> PropertyIt(ModuleClass); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		
		// remove advanced, private or not editable properties
		const bool bIsAdvanced = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
		const bool bIsPublic = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst);
		const bool bIsInstanceEditable = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
		if (bIsAdvanced || !bIsPublic || !bIsInstanceEditable)
		{
			Module->ConfigValues.Remove(Property->GetFName());
			Module->Bindings.Remove(Property->GetFName());
		}
	}

	// Make sure all the types are valid
	if (UModularRigController* Controller = GetModularRigController())
	{
		const TMap<FName, FString> ConfigValues = Module->ConfigValues;
		const TMap<FName, FString> Bindings = Module->Bindings;
		Module->ConfigValues.Reset();
		Module->Bindings.Reset();
		for (const TPair<FName, FString>& Pair : ConfigValues)
		{
			Controller->SetConfigValueInModule(ModulePath, Pair.Key, Pair.Value, false);
		}
		for (const TPair<FName, FString>& Pair : Bindings)
		{
			Controller->BindModuleVariable(ModulePath, Pair.Key, Pair.Value, false);
		}
	}
}

void UControlRigBlueprint::RefreshModuleConnectors()
{
	if(!IsModularRig())
	{
		return;
	}

	if (UModularRigController* Controller = GetModularRigController())
	{
		TGuardValue<bool> NotificationsGuard(Controller->bSuspendNotifications, true);
		ModularRigModel.ForEachModule([this](const FRigModuleReference* Element) -> bool
		{
			RefreshModuleConnectors(Element);
			return true;
		});
	}
}

void UControlRigBlueprint::RefreshModuleConnectors(const FRigModuleReference* InModule)
{
	if(!IsModularRig())
	{
		return;
	}

	// avoid dead class pointers
	if(InModule->Class.Get() == nullptr)
	{
		return;
	}
	
	const bool bRemoveAllConnectors = !ModularRigModel.FindModule(InModule->GetPath());
	
	if (URigHierarchyController* Controller = GetHierarchyController())
	{
		if (UControlRig* CDO = GetControlRigClass()->GetDefaultObject<UControlRig>())
		{
			Hierarchy->Modify();
			bool bAnyModification = false;

			const FString Namespace = InModule->GetNamespace();
			const TArray<FRigElementKey> AllConnectors = Hierarchy->GetKeysOfType<FRigConnectorElement>();
			const TArray<FRigElementKey> ExistingConnectors = AllConnectors.FilterByPredicate([Namespace](const FRigElementKey& ConnectorKey) -> bool
			{
				const FString ConnectorName = ConnectorKey.Name.ToString();
				FString ConnectorNamespace;
				(void)URigHierarchy::SplitNameSpace(ConnectorName, &ConnectorNamespace, nullptr);
				ConnectorNamespace.Append(UModularRig::NamespaceSeparator);
				return ConnectorNamespace.Equals(Namespace, ESearchCase::CaseSensitive);
			});

			// setup the module information. this is needed so that newly added
			// connectors result in the right namespace metadata etc
			FRigVMExtendedExecuteContext& Context = CDO->GetRigVMExtendedExecuteContext();
			FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
			const UControlRig* ModuleCDO = InModule->Class->GetDefaultObject<UControlRig>();
			const TArray<FRigModuleConnector>& ExpectedConnectors = ModuleCDO->GetRigModuleSettings().ExposedConnectors;

			// remove the obsolete connectors
			for(const FRigElementKey& Connector : ExistingConnectors)
			{
				const FString ConnectorNameString = Connector.Name.ToString();
				FString ConnectorNamespace, ShortName = ConnectorNameString;
				(void)URigHierarchy::SplitNameSpace(ConnectorNameString, &ConnectorNamespace, &ShortName);
				const bool bConnectorExpected = ExpectedConnectors.ContainsByPredicate(
					[ShortName](const FRigModuleConnector& ExpectedConnector) -> bool
					{
						return ExpectedConnector.Name.Equals(ShortName, ESearchCase::CaseSensitive);
					}
				);
				
				if(bRemoveAllConnectors || !bConnectorExpected)
				{
					bAnyModification = true;
					(void)Controller->RemoveElement(Connector);
					ConnectionMap.Remove(Connector);
				}
			}

			// add the missing expected connectors
			if(!bRemoveAllConnectors)
			{
				for (const FRigModuleConnector& Connector : ExpectedConnectors)
				{
					const FName ConnectorName = *Connector.Name;
					const FName ConnectorNameWithNameSpace = *URigHierarchy::JoinNameSpace(InModule->GetNamespace(), Connector.Name);
					const FRigElementKey ConnectorKeyWithNameSpace(ConnectorNameWithNameSpace, ERigElementType::Connector);
					if(!Hierarchy->Contains(ConnectorKeyWithNameSpace))
					{
						bAnyModification = true;
						FRigHierarchyExecuteContextBracket HierarchyContextGuard(Hierarchy, &Context);
						FControlRigExecuteContextRigModuleGuard RigModuleGuard(PublicContext, InModule->GetNamespace());
						(void)Controller->AddConnector(ConnectorName, Connector.Settings);
					}
					else
					{
						// copy the connector settings
						FRigConnectorElement* ExistingConnector = Hierarchy->FindChecked<FRigConnectorElement>(ConnectorKeyWithNameSpace);
						ExistingConnector->Settings = Connector.Settings;
					}
				}
			}

			if (bAnyModification)
			{
				PropagateHierarchyFromBPToInstances();
			}
		}
	}
}

void UControlRigBlueprint::HandleHierarchyModified(ERigHierarchyNotification InNotification, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
#if WITH_EDITOR

	if(bSuspendAllNotifications)
	{
		return;
	}

	switch(InNotification)
	{
		case ERigHierarchyNotification::ElementRemoved:
		{
			Modify();
			Influences.OnKeyRemoved(InElement->GetKey());
			PropagateHierarchyFromBPToInstances();
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		{
			Modify();
			const FRigElementKey OldKey(InHierarchy->GetPreviousName(InElement->GetKey()), InElement->GetType());
			Influences.OnKeyRenamed(OldKey, InElement->GetKey());
			if (IsControlRigModule() && InElement->IsTypeOf(ERigElementType::Connector))
			{
				if (FRigElementKey* Target = ConnectionMap.Find(OldKey))
				{
					ConnectionMap.FindOrAdd(InElement->GetKey(), *Target);
					ConnectionMap.Remove(OldKey);
				}
			}
			PropagateHierarchyFromBPToInstances();
			break;
		}
		case ERigHierarchyNotification::ElementAdded:
		case ERigHierarchyNotification::ParentChanged:
		case ERigHierarchyNotification::ElementReordered:
		case ERigHierarchyNotification::HierarchyReset:
		{
			Modify();
			PropagateHierarchyFromBPToInstances();
			break;
		}
		case ERigHierarchyNotification::ElementSelected:
		{
			bool bClearTransientControls = true;
			if (const FRigControlElement* ControlElement = Cast<FRigControlElement>(InElement))
			{
				if (ControlElement->Settings.bIsTransientControl)
				{
					bClearTransientControls = false;
				}
			}

			if(bClearTransientControls)
			{
				if(UControlRig* RigBeingDebugged = Cast<UControlRig>(GetObjectBeingDebugged()))
				{
					const FName TransientControlName = UControlRig::GetNameForTransientControl(InElement->GetKey());
					const FRigElementKey TransientControlKey(TransientControlName, ERigElementType::Control);
					if (const FRigControlElement* ControlElement = RigBeingDebugged->GetHierarchy()->Find<FRigControlElement>(TransientControlKey))
					{
						if (ControlElement->Settings.bIsTransientControl)
						{
							bClearTransientControls = false;
						}
					}
				}
			}

			if(bClearTransientControls)
			{
				ClearTransientControls();
			}
			break;
		}
		case ERigHierarchyNotification::ElementDeselected:
		{
			if (const FRigControlElement* ControlElement = Cast<FRigControlElement>(InElement))
			{
				if (ControlElement->Settings.bIsTransientControl)
				{
					ClearTransientControls();
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}

	HierarchyModifiedEvent.Broadcast(InNotification, InHierarchy, InElement);
	
#endif
}

void UControlRigBlueprint::HandleRigModulesModified(EModularRigNotification InNotification, const FRigModuleReference* InModule)
{
	bool bRecompile = true;
	switch (InNotification)
	{
		case EModularRigNotification::ModuleAdded:
		{
			if (InModule)
			{
				RefreshModuleConnectors(InModule);
				UpdateModularDependencyDelegates();
			}
			break;
		}
		case EModularRigNotification::ModuleRenamed:
		case EModularRigNotification::ModuleReparented:
		{
			if (InModule)
			{
				if (URigHierarchyController* Controller = GetHierarchyController())
				{
					if (UControlRig* CDO = GetControlRigClass()->GetDefaultObject<UControlRig>())
					{
						Hierarchy->Modify();
						
						struct ConnectionInfo
						{
							FString NewPath;
							FRigElementKey TargetConnection;
							FRigConnectorSettings Settings;
						};
						FString OldNamespace;
						if (InNotification == EModularRigNotification::ModuleRenamed)
						{
							OldNamespace = (InModule->ParentPath.IsEmpty()) ? *InModule->PreviousName.ToString() : URigHierarchy::JoinNameSpace(InModule->ParentPath, InModule->PreviousName.ToString());
						}
						else if(InNotification == EModularRigNotification::ModuleReparented)
						{
							OldNamespace = (InModule->PreviousParentPath.IsEmpty()) ? *InModule->PreviousName.ToString() : URigHierarchy::JoinNameSpace(InModule->PreviousParentPath, InModule->PreviousName.ToString());
						}
						FString NewNamespace = (InModule->ParentPath.IsEmpty()) ? *InModule->Name.ToString() : URigHierarchy::JoinNameSpace(InModule->ParentPath, InModule->Name.ToString());
						OldNamespace.Append(UModularRig::NamespaceSeparator);
						NewNamespace.Append(UModularRig::NamespaceSeparator);
						
						TArray<FRigElementKey> Connectors = Controller->GetHierarchy()->GetKeysOfType<FRigConnectorElement>();
						TMap<FRigElementKey, ConnectionInfo> RenamedConnectors; // old key -> new key
						for (const FRigElementKey& Connector : Connectors)
						{
							FString OldConnectorName = Connector.Name.ToString();
							FString OldConnectorNamespace, ShortName;
							(void)URigHierarchy::SplitNameSpace(OldConnectorName, &OldConnectorNamespace, &ShortName);
							OldConnectorNamespace.Append(UModularRig::NamespaceSeparator);
							if (OldConnectorNamespace.StartsWith(OldNamespace))
							{
								ConnectionInfo& Info = RenamedConnectors.FindOrAdd(Connector);
								Info.NewPath = OldConnectorName.Replace(*OldNamespace, *NewNamespace);
								Info.Settings = CastChecked<FRigConnectorElement>(Controller->GetHierarchy()->FindChecked(Connector))->Settings;
								if (FRigElementKey* TargetKey = ConnectionMap.Find(Connector))
								{
									Info.TargetConnection = *TargetKey;
								}
							}
						}

						// Remove connectors
						for (TPair<FRigElementKey, ConnectionInfo>& Pair : RenamedConnectors)
						{
							Controller->RemoveElement(Pair.Key);
						}

						// Add connectors
						{
							FRigVMExtendedExecuteContext& Context = CDO->GetRigVMExtendedExecuteContext();
							FRigHierarchyExecuteContextBracket HierarchyContextGuard(Controller->GetHierarchy(), &Context);
							FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
							for (TPair<FRigElementKey, ConnectionInfo>& Pair : RenamedConnectors)
							{
								FString Namespace, ConnectorName;
								(void)URigHierarchy::SplitNameSpace(Pair.Value.NewPath, &Namespace, &ConnectorName);
								Namespace.Append(UModularRig::NamespaceSeparator);
								FControlRigExecuteContextRigModuleGuard RigModuleGuard(PublicContext, Namespace);
								const TGuardValue<bool> DisableErrors(Controller->bReportWarningsAndErrors, false);
								Controller->AddConnector(*ConnectorName, Pair.Value.Settings);
							}
						}

						UpdateConnectionMapFromModel();
						PropagateHierarchyFromBPToInstances();
					}
				}
			}
			break;
		}
		case EModularRigNotification::ModuleRemoved:
		{
			if (InModule)
			{
				RefreshModuleConnectors(InModule);
				UpdateConnectionMapFromModel();
				UpdateModularDependencyDelegates();
			}
			break;
		}
		case EModularRigNotification::ConnectionChanged:
		{
			Hierarchy->Modify();
			
			UpdateConnectionMapFromModel();
			HierarchyModifiedEvent.Broadcast(ERigHierarchyNotification::HierarchyReset, Hierarchy, nullptr);
			break;
		}
		case EModularRigNotification::ModuleShortNameChanged:
		{
			bRecompile = false;
			break;
		}
		case EModularRigNotification::ModuleConfigValueChanged:
		{
			bRecompile = false;
			PropagateModuleHierarchyFromBPToInstances();
			RequestConstructionOnAllModules();
			break;
		}
		case EModularRigNotification::InteractionBracketOpened:
		{
			ModulesRecompilationBracket++;
			break;
		}
		case EModularRigNotification::InteractionBracketClosed:
		case EModularRigNotification::InteractionBracketCanceled:
		{
			ModulesRecompilationBracket--;
			break;
		}
		default:
		{
			break;
		}
	}

	if (bRecompile && ModulesRecompilationBracket == 0)
	{
		RecompileModularRig();
	}
}

UControlRigBlueprint::FControlValueScope::FControlValueScope(UControlRigBlueprint* InBlueprint)
: Blueprint(InBlueprint)
{
#if WITH_EDITOR
	check(Blueprint);

	if (UControlRig* CR = Cast<UControlRig>(Blueprint->GetObjectBeingDebugged()))
	{
		TArray<FRigControlElement*> Controls = CR->AvailableControls();
		for (FRigControlElement* ControlElement : Controls)
		{
			ControlValues.Add(ControlElement->GetFName(), CR->GetControlValue(ControlElement->GetFName()));
		}
	}
#endif
}

UControlRigBlueprint::FControlValueScope::~FControlValueScope()
{
#if WITH_EDITOR
	check(Blueprint);

	if (UControlRig* CR = Cast<UControlRig>(Blueprint->GetObjectBeingDebugged()))
	{
		for (const TPair<FName, FRigControlValue>& Pair : ControlValues)
		{
			if (CR->FindControl(Pair.Key))
			{
				CR->SetControlValue(Pair.Key, Pair.Value);
			}
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE


