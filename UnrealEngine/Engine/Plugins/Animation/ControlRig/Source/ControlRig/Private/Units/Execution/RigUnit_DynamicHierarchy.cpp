// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_DynamicHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"
#include "ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DynamicHierarchy)

bool FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(
	const FRigUnitContext& InContext,
	const FControlRigExecuteContext& InExecuteContext,
	bool bAllowOnlyConstructionEvent,
	FString* OutErrorMessage)
{
	if((InContext.State != EControlRigState::Update) ||
		(InContext.Hierarchy == nullptr) ||
		(InExecuteContext.Hierarchy == nullptr))
	{
		return false;
	}

	if(bAllowOnlyConstructionEvent)
	{
		if(InExecuteContext.GetEventName() != FRigUnit_PrepareForExecution::EventName)
		{
			if(OutErrorMessage)
			{
				static constexpr TCHAR ErrorMessageFormat[] = TEXT("Node can only run in %s Event");
				*OutErrorMessage = FString::Printf(ErrorMessageFormat, *FRigUnit_PrepareForExecution::EventName.ToString());
			}
			return false;
		}
	}

	if(InContext.Hierarchy->Num() >= InContext.HierarchySettings.ProceduralElementLimit)
	{
		if(OutErrorMessage)
		{
			static constexpr TCHAR ErrorMessageFormat[] = TEXT("Node has hit the Procedural Element Limit. Check the Class Settings under Hierarchy.");
			*OutErrorMessage = FString::Printf(ErrorMessageFormat, *FRigUnit_PrepareForExecution::EventName.ToString());
		}
		return false;
	}

	return true;
}

FRigUnit_AddParent_Execute()
{
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true))
	{
		return;
	}
	
	FRigTransformElement* ChildElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Child);
	if(ChildElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Child item %s does not exist."), *Child.ToString())
		return;
	}

	FRigTransformElement* ParentElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Parent);
	if(ParentElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Parent item %s does not exist."), *Parent.ToString())
		return;
	}

	FRigHierarchyEnableControllerBracket EnableController(ExecuteContext.Hierarchy, true);
	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		Controller->AddParent(ChildElement, ParentElement, 0.f, true, false);
	}
}

FRigUnit_SetDefaultParent_Execute()
{
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true))
	{
		return;
	}
	
	FRigTransformElement* ChildElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Child);
	if(ChildElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Child item %s does not exist."), *Child.ToString())
		return;
	}

	FRigTransformElement* ParentElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Parent);
	if(ParentElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Parent item %s does not exist."), *Parent.ToString())
		return;
	}

	FRigHierarchyEnableControllerBracket EnableController(ExecuteContext.Hierarchy, true);
	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		Controller->AddParent(ChildElement, ParentElement, 1.0f, true, true);
	}
}

FRigUnit_SwitchParent_Execute()
{
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, false))
	{
		return;
	}

	FRigTransformElement* ChildElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Child);
	if(ChildElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Child item %s does not exist."), *Child.ToString())
		return;
	}
	if(!ChildElement->IsA<FRigMultiParentElement>())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Child item %s cannot be space switched (only Nulls and Controls can)."), *Child.ToString())
		return;
	}

	FRigTransformElement* ParentElement = nullptr;

	if(Mode == ERigSwitchParentMode::ParentItem)
	{
		ParentElement = ExecuteContext.Hierarchy->Find<FRigTransformElement>(Parent);
		if(ParentElement == nullptr)
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Parent item %s does not exist."), *Parent.ToString())
			return;
		}

		if(!ParentElement->IsA<FRigTransformElement>())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Parent item %s does not have a transform."), *Parent.ToString())
			return;
		}
	}

	const ERigTransformType::Type TransformTypeToMaintain =
		bMaintainGlobal ? ERigTransformType::CurrentGlobal : ERigTransformType::CurrentLocal;
	
	const FTransform Transform = ExecuteContext.Hierarchy->GetTransform(ChildElement, TransformTypeToMaintain);

	switch(Mode)
	{
		case ERigSwitchParentMode::World:
		{
			if(!ExecuteContext.Hierarchy->SwitchToWorldSpace(ChildElement, false, true))
			{
				return;
			}
			break;
		}
		case ERigSwitchParentMode::DefaultParent:
		{
			if(!ExecuteContext.Hierarchy->SwitchToDefaultParent(ChildElement, false, true))
			{
				return;
			}
			break;
		}
		case ERigSwitchParentMode::ParentItem:
		default:
		{
			FString FailureReason;
			static const URigHierarchy::TElementDependencyMap EmptyDependencyMap;
			if(!ExecuteContext.Hierarchy->SwitchToParent(ChildElement, ParentElement, false, true, EmptyDependencyMap, &FailureReason))
			{
				if(!FailureReason.IsEmpty())
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("%s"), *FailureReason);
				}
				return;
			}

			// during construction event also change the initial weights
			if(ExecuteContext.GetEventName() == FRigUnit_PrepareForExecution::EventName)
			{
				if(!ExecuteContext.Hierarchy->SwitchToParent(ChildElement, ParentElement, true, true, EmptyDependencyMap, &FailureReason))
				{
					if(!FailureReason.IsEmpty())
					{
						UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("%s"), *FailureReason);
					}
					return;
				}
			}
			break;
		}
	}
	
	ExecuteContext.Hierarchy->SetTransform(ChildElement, Transform, TransformTypeToMaintain, true);
}

FRigUnit_HierarchyGetParentWeights_Execute()
{
	FRigUnit_HierarchyGetParentWeightsArray::StaticExecute(RigVMExecuteContext, Child, Weights, Parents.Keys, Context);
}

FRigVMStructUpgradeInfo FRigUnit_HierarchyGetParentWeights::GetUpgradeInfo() const
{
	FRigUnit_HierarchyGetParentWeightsArray NewNode;
	NewNode.Child = Child;
	NewNode.Weights = Weights;
	NewNode.Parents = Parents.Keys;

	return FRigVMStructUpgradeInfo();
}

FRigUnit_HierarchyGetParentWeightsArray_Execute()
{
	if((Context.State != EControlRigState::Update) || (Context.Hierarchy == nullptr))
	{
		return;
	}

	FRigBaseElement* ChildElement = Context.Hierarchy->Find(Child);
	if(ChildElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Item %s does not exist."), *Child.ToString())
		return;
	}
	
	Weights = Context.Hierarchy->GetParentWeightArray(ChildElement, false);
	Parents = Context.Hierarchy->GetParents(ChildElement->GetKey(), false);
}

FRigUnit_HierarchySetParentWeights_Execute()
{
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, false))
	{
		return;
	}

	FRigBaseElement* ChildElement = Context.Hierarchy->Find(Child);
	if(ChildElement == nullptr)
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Item %s does not exist."), *Child.ToString())
		return;
	}

	if(Weights.Num() != ExecuteContext.Hierarchy->GetNumberOfParents(ChildElement))
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Provided incorrect number of weights(%d) for %s - expected %d."), Weights.Num(), *Child.ToString(), ExecuteContext.Hierarchy->GetNumberOfParents(Child))
		return;
	}

	ExecuteContext.Hierarchy->SetParentWeightArray(ChildElement, Weights, false, true);

	// during construction event also change the initial weights
	if(ExecuteContext.GetEventName() == FRigUnit_PrepareForExecution::EventName)
	{
		ExecuteContext.Hierarchy->SetParentWeightArray(ChildElement, Weights, true, true);
	}
}

FRigUnit_HierarchyReset_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}
	ExecuteContext.Hierarchy->ResetToDefault();
}

FRigUnit_HierarchyImportFromSkeleton_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Items.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		
		if(const USkeletalMeshComponent* SkelMeshComponent = Context.DataSourceRegistry->RequestSource<USkeletalMeshComponent>(UControlRig::OwnerComponent))
		{
			if(SkelMeshComponent->GetSkeletalMeshAsset())
			{
				const FReferenceSkeleton& ReferenceSkeleton = SkelMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
				Items = Controller->ImportBones(ReferenceSkeleton, NameSpace, false, false, false, false);
				if(bIncludeCurves)
				{
					if(USkeleton* Skeleton = SkelMeshComponent->GetSkeletalMeshAsset()->GetSkeleton())
					{
						Items.Append(Controller->ImportCurves(Skeleton, NameSpace, false, false, false));
					}
				}
			}
		}
	}
}

FRigUnit_HierarchyRemoveElement_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}
	
	bSuccess = false;

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		bSuccess = Controller->RemoveElement(Item);
	}
}

FRigUnit_HierarchyAddBone_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddBone(Name, Parent, Transform, Space == EBoneGetterSetterMode::GlobalSpace, ERigBoneType::Imported, false, false);
	}
}

FRigUnit_HierarchyAddNull_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddNull(Name, Parent, Transform, Space == EBoneGetterSetterMode::GlobalSpace, false, false);
	}
}

void FRigUnit_HierarchyAddControl_Settings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.DisplayName = DisplayName;
}

void FRigUnit_HierarchyAddControl_ShapeSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.bShapeVisible = bVisible;
	OutSettings.ShapeName = Name;
	OutSettings.ShapeColor = Color;
}

void FRigUnit_HierarchyAddControl_ProxySettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.AnimationType = bIsProxy ? ERigControlAnimationType::ProxyControl : ERigControlAnimationType::AnimationControl;
	OutSettings.DrivenControls = DrivenControls;
	OutSettings.ShapeVisibility = ShapeVisibility;
}

void FRigUnit_HierarchyAddControlFloat_LimitSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.SetupLimitArrayForType(false, false, false);
	OutSettings.LimitEnabled[0] = Limit;
	OutSettings.MinimumValue = FRigControlValue::Make<float>(MinValue);
	OutSettings.MaximumValue = FRigControlValue::Make<float>(MaxValue);
	OutSettings.bDrawLimits = bDrawLimits;
}

void FRigUnit_HierarchyAddControlFloat_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = ERigControlType::Float;
	OutSettings.PrimaryAxis = PrimaryAxis;

	Proxy.Configure(OutSettings);
	Limits.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlFloat_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FRigControlValue Value = FRigControlValue::Make<float>(InitialValue);
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

void FRigUnit_HierarchyAddControlInteger_LimitSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.SetupLimitArrayForType(false, false, false);
	OutSettings.LimitEnabled[0] = Limit;
	OutSettings.MinimumValue = FRigControlValue::Make<int32>(MinValue);
	OutSettings.MaximumValue = FRigControlValue::Make<int32>(MaxValue);
	OutSettings.bDrawLimits = bDrawLimits;
}

void FRigUnit_HierarchyAddControlInteger_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = ERigControlType::Integer;
	OutSettings.PrimaryAxis = PrimaryAxis;

	Proxy.Configure(OutSettings);
	Limits.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlInteger_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FRigControlValue Value = FRigControlValue::Make<int32>(InitialValue);
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

void FRigUnit_HierarchyAddControlVector2D_LimitSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.SetupLimitArrayForType(false, false, false);
	OutSettings.LimitEnabled[0] = LimitX;
	OutSettings.LimitEnabled[1] = LimitY;
	OutSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinValue.X, MinValue.Y, 0.f));
	OutSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaxValue.X, MaxValue.Y, 0.f));
	OutSettings.bDrawLimits = bDrawLimits;
}

void FRigUnit_HierarchyAddControlVector2D_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = ERigControlType::Vector2D;
	OutSettings.PrimaryAxis = PrimaryAxis;

	Proxy.Configure(OutSettings);
	Limits.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlVector2D_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue.X, InitialValue.Y, 0.f));
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

void FRigUnit_HierarchyAddControlVector_LimitSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.SetupLimitArrayForType(false, false, false);
	OutSettings.LimitEnabled[0] = LimitX;
	OutSettings.LimitEnabled[1] = LimitY;
	OutSettings.LimitEnabled[2] = LimitZ;
	OutSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinValue));
	OutSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaxValue));
	OutSettings.bDrawLimits = bDrawLimits;
}

void FRigUnit_HierarchyAddControlVector_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = bIsPosition ? ERigControlType::Position : ERigControlType::Scale;

	Proxy.Configure(OutSettings);
	Limits.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlVector_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue));
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

void FRigUnit_HierarchyAddControlRotator_LimitSettings::Configure(FRigControlSettings& OutSettings) const
{
	OutSettings.SetupLimitArrayForType(false, false, false);
	OutSettings.LimitEnabled[0] = LimitPitch;
	OutSettings.LimitEnabled[1] = LimitYaw;
	OutSettings.LimitEnabled[2] = LimitRoll;
	OutSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinValue.Euler()));
	OutSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaxValue.Euler()));
	OutSettings.bDrawLimits = bDrawLimits;
}

void FRigUnit_HierarchyAddControlRotator_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = ERigControlType::Rotator;

	Proxy.Configure(OutSettings);
	Limits.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlRotator_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue.Euler()));
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

void FRigUnit_HierarchyAddControlTransform_Settings::Configure(FRigControlSettings& OutSettings) const
{
	Super::Configure(OutSettings);
	
	OutSettings.ControlType = ERigControlType::EulerTransform;

	Proxy.Configure(OutSettings);
	Shape.Configure(OutSettings);
}

FRigUnit_HierarchyAddControlTransform_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		Settings.Configure(ControlSettings);

		const FEulerTransform EulerTransform(InitialValue);
		const FRigControlValue Value = FRigControlValue::Make<FRigControlValue::FEulerTransform_Float>(EulerTransform);
		const FTransform ShapeTransform = Settings.Shape.Transform;
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddControl(Name, Parent, ControlSettings, Value, OffsetTransform, ShapeTransform, false, false);
	}
}

FRigUnit_HierarchyAddAnimationChannelBool_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Bool;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<bool>(MinimumValue);
		ControlSettings.MaximumValue = FRigControlValue::Make<bool>(MaximumValue);
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<bool>(InitialValue);

		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

FRigUnit_HierarchyAddAnimationChannelFloat_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Float;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<float>(MinimumValue);
		ControlSettings.MaximumValue = FRigControlValue::Make<float>(MaximumValue);
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<float>(InitialValue);
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

FRigUnit_HierarchyAddAnimationChannelInteger_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Integer;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<int32>(MinimumValue);
		ControlSettings.MaximumValue = FRigControlValue::Make<int32>(MaximumValue);
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<int32>(InitialValue);
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

FRigUnit_HierarchyAddAnimationChannelVector2D_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Vector2D;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinimumValue.X, MinimumValue.Y, 0.f));
		ControlSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaximumValue.X, MaximumValue.Y, 0.f));
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue.X, InitialValue.Y, 0.f));
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

FRigUnit_HierarchyAddAnimationChannelVector_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Position;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinimumValue));
		ControlSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaximumValue));
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue));
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

FRigUnit_HierarchyAddAnimationChannelRotator_Execute()
{
	FString ErrorMessage;
	if(!FRigUnit_DynamicHierarchyBase::IsValidToRunInContext(Context, ExecuteContext, true, &ErrorMessage))
	{
		if(!ErrorMessage.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("%s"), *ErrorMessage);
		}
		return;
	}

	Item.Reset();

	if(URigHierarchyController* Controller = ExecuteContext.Hierarchy->GetController(true))
	{
		FRigControlSettings ControlSettings;
		ControlSettings.ControlType = ERigControlType::Rotator;
		ControlSettings.SetupLimitArrayForType(true, true, true);
		ControlSettings.MinimumValue = FRigControlValue::Make<FVector3f>(FVector3f(MinimumValue.Euler()));
		ControlSettings.MaximumValue = FRigControlValue::Make<FVector3f>(FVector3f(MaximumValue.Euler()));
		ControlSettings.DisplayName = Controller->GetHierarchy()->GetSafeNewDisplayName(Parent, Name.ToString());
		const FRigControlValue Value = FRigControlValue::Make<FVector3f>(FVector3f(InitialValue.Euler()));
		
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, RigVMExecuteContext.GetInstructionIndex());
		Item = Controller->AddAnimationChannel(Name, Parent, ControlSettings, false, false);

		if(Item.IsValid())
		{
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Initial, false, false);
			ExecuteContext.Hierarchy->SetControlValue(Item, Value, ERigControlValueType::Current, false, false);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel could not be added. Is Parent valid?"));
		}
	}
}

