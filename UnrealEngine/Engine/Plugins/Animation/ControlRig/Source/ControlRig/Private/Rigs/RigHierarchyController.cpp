// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyController.h"
#include "ControlRig.h"
#include "AnimationCoreLibrary.h"
#include "UObject/Package.h"
#include "ModularRig.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "Engine/SkeletalMesh.h"
#include "RigVMPythonUtils.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigHierarchyController)

////////////////////////////////////////////////////////////////////////////////
// URigHierarchyController
////////////////////////////////////////////////////////////////////////////////

URigHierarchyController::~URigHierarchyController()
{
}

void URigHierarchyController::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
    {
		URigHierarchy* OuterHierarchy = Cast<URigHierarchy>(GetOuter());
		SetHierarchy(OuterHierarchy);
    }
}

URigHierarchy* URigHierarchyController::GetHierarchy() const
{
	return Cast<URigHierarchy>(GetOuter());;
}

void URigHierarchyController::SetHierarchy(URigHierarchy* InHierarchy)
{
	// since we changed the controller to be a property of the hierarchy,
	// controlling a different hierarchy is no longer allowed
	if (ensure(InHierarchy == GetOuter()))
	{
		InHierarchy->OnModified().RemoveAll(this);
		InHierarchy->OnModified().AddUObject(this, &URigHierarchyController::HandleHierarchyModified);
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("Invalid API Usage, Called URigHierarchyController::SetHierarchy(...) with a Hierarchy that is not the outer of the controller"));
	}
}

bool URigHierarchyController::SelectElement(FRigElementKey InKey, bool bSelect, bool bClearSelection)
{
	if(!IsValid())
	{
		return false;
	}

	if(bClearSelection)
	{
		TArray<FRigElementKey> KeysToSelect;
		KeysToSelect.Add(InKey);
		return SetSelection(KeysToSelect);
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	if(URigHierarchy* HierarchyForSelection = Hierarchy->HierarchyForSelectionPtr.Get())
	{
		if(URigHierarchyController* ControllerForSelection = HierarchyForSelection->GetController())
		{
			return ControllerForSelection->SelectElement(InKey, bSelect, bClearSelection);
		}
	}

	FRigElementKey Key = InKey;
	if(Hierarchy->ElementKeyRedirector)
	{
		if(const FCachedRigElement* Cache = Hierarchy->ElementKeyRedirector->Find(Key))
		{
			if(const_cast<FCachedRigElement*>(Cache)->UpdateCache(Hierarchy))
			{
				Key = Cache->GetKey();
			}
		}
	}

	FRigBaseElement* Element = Hierarchy->Find(Key);
	if(Element == nullptr)
	{
		return false;
	}

	const bool bSelectionState = Hierarchy->OrderedSelection.Contains(Key);
	ensure(bSelectionState == Element->bSelected);
	if(Element->bSelected == bSelect)
	{
		return false;
	}

	Element->bSelected = bSelect;
	if(bSelect)
	{
		Hierarchy->OrderedSelection.Add(Key);
	}
	else
	{
		Hierarchy->OrderedSelection.Remove(Key);
	}

	if(Element->bSelected)
	{
		Notify(ERigHierarchyNotification::ElementSelected, Element);
	}
	else
	{
		Notify(ERigHierarchyNotification::ElementDeselected, Element);
	}

	Hierarchy->UpdateVisibilityOnProxyControls();

	return true;
}

bool URigHierarchyController::SetSelection(const TArray<FRigElementKey>& InKeys, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	if(URigHierarchy* HierarchyForSelection = Hierarchy->HierarchyForSelectionPtr.Get())
	{
		if(URigHierarchyController* ControllerForSelection = HierarchyForSelection->GetController())
		{
			return ControllerForSelection->SetSelection(InKeys);
		}
	}

	TArray<FRigElementKey> PreviousSelection = Hierarchy->GetSelectedKeys();
	bool bResult = true;

	{
		// disable python printing here as we only want to print a single command instead of one per selected item
		const TGuardValue<bool> Guard(bSuspendPythonPrinting, true);

		for(const FRigElementKey& KeyToDeselect : PreviousSelection)
		{
			if(!InKeys.Contains(KeyToDeselect))
			{
				if(!SelectElement(KeyToDeselect, false))
				{
					bResult = false;
				}
			}
		}

		for(const FRigElementKey& KeyToSelect : InKeys)
		{
			if(!PreviousSelection.Contains(KeyToSelect))
			{
				if(!SelectElement(KeyToSelect, true))
				{
					bResult = false;
				}
			}
		}
	}

#if WITH_EDITOR
	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			const FString Selection = FString::JoinBy( InKeys, TEXT(", "), [](const FRigElementKey& Key)
			{
				return Key.ToPythonString();
			});
			
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.set_selection([%s])"),
				*Selection ) );
		}
	}
#endif

	return bResult;
}

FRigElementKey URigHierarchyController::AddBone(FName InName, FRigElementKey InParent, FTransform InTransform,
                                                bool bTransformInGlobal, ERigBoneType InBoneType, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Bone", "Add Bone"));
        Hierarchy->Modify();
	}
#endif

	FRigBoneElement* NewElement = MakeElement<FRigBoneElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);
		NewElement->Key.Type = ERigElementType::Bone;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->BoneType = InBoneType;
		AddElement(NewElement, Hierarchy->Get(Hierarchy->GetIndex(InParent)), true, InName);

		if(bTransformInGlobal)
		{
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialGlobal, true, false);
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::CurrentGlobal, true, false);
		}
		else
		{
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialLocal, true, false);
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::CurrentLocal, true, false);
		}

		NewElement->Pose.Current = NewElement->Pose.Initial;
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddBonePythonCommands(NewElement);
			for (const FString& Command : Commands)
			{			
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();
		
	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddNull(FName InName, FRigElementKey InParent, FTransform InTransform,
                                                bool bTransformInGlobal, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Null", "Add Null"));
		Hierarchy->Modify();
	}
#endif

	FRigNullElement* NewElement = MakeElement<FRigNullElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);		
		NewElement->Key.Type = ERigElementType::Null;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		AddElement(NewElement, Hierarchy->Get(Hierarchy->GetIndex(InParent)), false, InName);

		if(bTransformInGlobal)
		{
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialGlobal, true, false);
		}
		else
		{
			Hierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialLocal, true, false);
		}

		NewElement->Pose.Current = NewElement->Pose.Initial;
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddNullPythonCommands(NewElement);
			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddControl(
	FName InName,
	FRigElementKey InParent,
	FRigControlSettings InSettings,
	FRigControlValue InValue,
	FTransform InOffsetTransform,
	FTransform InShapeTransform,
	bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Control", "Add Control"));
		Hierarchy->Modify();
	}
#endif

	FRigControlElement* NewElement = MakeElement<FRigControlElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);		
		NewElement->Key.Type = ERigElementType::Control;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->Settings = InSettings;
		if(NewElement->Settings.LimitEnabled.IsEmpty())
		{
			NewElement->Settings.SetupLimitArrayForType();
		}

		if(!NewElement->Settings.DisplayName.IsNone())
		{
			NewElement->Settings.DisplayName = Hierarchy->GetSafeNewDisplayName(InParent, NewElement->Settings.DisplayName); 
		}
		else if(Hierarchy->HasExecuteContext())
		{
			const FControlRigExecuteContext& CRContext = Hierarchy->ExecuteContext->GetPublicData<FControlRigExecuteContext>();
			if(!CRContext.GetRigModuleNameSpace().IsEmpty())
			{
				NewElement->Settings.DisplayName = Hierarchy->GetSafeNewDisplayName(InParent, InName);
			}
		}
		
		AddElement(NewElement, Hierarchy->Get(Hierarchy->GetIndex(InParent)), false, InName);
		
		NewElement->Offset.Set(ERigTransformType::InitialLocal, InOffsetTransform);  
		NewElement->Shape.Set(ERigTransformType::InitialLocal, InShapeTransform);  
		Hierarchy->SetControlValue(NewElement, InValue, ERigControlValueType::Initial, false);
		const FTransform LocalTransform = Hierarchy->GetTransform(NewElement, ERigTransformType::InitialLocal);
		Hierarchy->SetControlPreferredEulerAngles(NewElement, LocalTransform);

		NewElement->Offset.MarkDirty(ERigTransformType::InitialGlobal);
		NewElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
		NewElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
		NewElement->Offset.Current = NewElement->Offset.Initial;
		NewElement->Pose.Current = NewElement->Pose.Initial;
		NewElement->PreferredEulerAngles.Current = NewElement->PreferredEulerAngles.Initial;
		NewElement->Shape.Current = NewElement->Shape.Initial;
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddControlPythonCommands(NewElement);
			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddAnimationChannel(FName InName, FRigElementKey InParentControl,
	FRigControlSettings InSettings, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	if(const FRigControlElement* ParentControl = Hierarchy->Find<FRigControlElement>(InParentControl))
	{
		InSettings.AnimationType = ERigControlAnimationType::AnimationChannel;
		InSettings.bGroupWithParentControl = true;

		return AddControl(InName, ParentControl->GetKey(), InSettings, InSettings.GetIdentityValue(),
			FTransform::Identity, FTransform::Identity, bSetupUndo, bPrintPythonCommand);
	}

	return FRigElementKey();
}

FRigElementKey URigHierarchyController::AddCurve(FName InName, float InValue, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Curve", "Add Curve"));
		Hierarchy->Modify();
	}
#endif

	FRigCurveElement* NewElement = MakeElement<FRigCurveElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);		
		NewElement->Key.Type = ERigElementType::Curve;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->Value = InValue;
		AddElement(NewElement, nullptr, false, InName);
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddCurvePythonCommands(NewElement);
			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddRigidBody(FName InName, FRigElementKey InParent,
                                                     FRigRigidBodySettings InSettings, FTransform InLocalTransform, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add RigidBody", "Add RigidBody"));
		Hierarchy->Modify();
	}
#endif

	FRigRigidBodyElement* NewElement = MakeElement<FRigRigidBodyElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);		
		NewElement->Key.Type = ERigElementType::RigidBody;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->Settings = InSettings;
		AddElement(NewElement, Hierarchy->Get(Hierarchy->GetIndex(InParent)), true, InName);

		Hierarchy->SetTransform(NewElement, InLocalTransform, ERigTransformType::InitialLocal, true, false);
		NewElement->Pose.Current = NewElement->Pose.Initial;
	}
	
#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddRigidBodyPythonCommands(NewElement);
			for (const FString& Command : Commands)
			{
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	GetHierarchy()->EnsureCacheValidity();

	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddReference(FName InName, FRigElementKey InParent,
	FRigReferenceGetWorldTransformDelegate InDelegate, bool bSetupUndo)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Reference", "Add Reference"));
		Hierarchy->Modify();
	}
#endif

	FRigReferenceElement* NewElement = MakeElement<FRigReferenceElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);		
		NewElement->Key.Type = ERigElementType::Reference;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->GetWorldTransformDelegate = InDelegate;
		AddElement(NewElement, Hierarchy->Get(Hierarchy->GetIndex(InParent)), true, InName);

		Hierarchy->SetTransform(NewElement, FTransform::Identity, ERigTransformType::InitialLocal, true, false);
		NewElement->Pose.Current = NewElement->Pose.Initial;
	}

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	Hierarchy->EnsureCacheValidity();

	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddConnector(FName InName, FRigConnectorSettings InSettings, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	// only allow to add one primary connector
	if(InSettings.Type == EConnectorType::Primary)
	{
		const TArray<FRigConnectorElement*>& Connectors = Hierarchy->GetConnectors();
		for(const FRigConnectorElement* Connector : Connectors)
		{
			if(Connector->IsPrimary())
			{
				if(Hierarchy->HasExecuteContext())
				{
					const FControlRigExecuteContext& CRContext = Hierarchy->ExecuteContext->GetPublicData<FControlRigExecuteContext>();
					const FString NameSpace = CRContext.GetRigModuleNameSpace();
					if(!NameSpace.IsEmpty())
					{
						const FString ConnectorNameSpace = Hierarchy->GetNameSpace(Connector->GetKey());
						if(!ConnectorNameSpace.IsEmpty() && ConnectorNameSpace.Equals(NameSpace, ESearchCase::CaseSensitive))
						{
							static constexpr TCHAR Format[] = TEXT("Cannot add connector '%s' - there already is a primary connector.");
							ReportAndNotifyErrorf(Format, *InName.ToString());
							return FRigElementKey();
						}
					}
				}
			}
		}

		if(InSettings.bOptional)
		{
			static constexpr TCHAR Format[] = TEXT("Cannot add connector '%s' - primary connectors cannot be optional.");
			ReportAndNotifyErrorf(Format, *InName.ToString());
			return FRigElementKey();
		}
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Connector", "Add Connector"));
		Hierarchy->Modify();
	}
#endif

	FRigConnectorElement* NewElement = MakeElement<FRigConnectorElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(Hierarchy->bEnableCacheValidityCheck, false);
		NewElement->Key.Type = ERigElementType::Connector;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		NewElement->Settings = InSettings;
		AddElement(NewElement, nullptr, true, InName);
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddConnectorPythonCommands(NewElement);
			for (const FString& Command : Commands)
			{			
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();
		
	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddSocket(FName InName, FRigElementKey InParent, FTransform InTransform,
	bool bTransformInGlobal, const FLinearColor& InColor, const FString& InDescription, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* CurrentHierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Socket", "Add Socket"));
		CurrentHierarchy->Modify();
	}
#endif

	FRigSocketElement* NewElement = MakeElement<FRigSocketElement>();
	{
		TGuardValue<bool> DisableCacheValidityChecks(CurrentHierarchy->bEnableCacheValidityCheck, false);
		NewElement->Key.Type = ERigElementType::Socket;
		NewElement->Key.Name = GetSafeNewName(InName, NewElement->Key.Type);
		AddElement(NewElement, CurrentHierarchy->Get(CurrentHierarchy->GetIndex(InParent)), true, InName);

		if(bTransformInGlobal)
		{
			CurrentHierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialGlobal, true, false);
			CurrentHierarchy->SetTransform(NewElement, InTransform, ERigTransformType::CurrentGlobal, true, false);
		}
		else
		{
			CurrentHierarchy->SetTransform(NewElement, InTransform, ERigTransformType::InitialLocal, true, false);
			CurrentHierarchy->SetTransform(NewElement, InTransform, ERigTransformType::CurrentLocal, true, false);
		}

		NewElement->Pose.Current = NewElement->Pose.Initial;

		NewElement->SetColor(InColor, CurrentHierarchy);
		NewElement->SetDescription(InDescription, CurrentHierarchy);
		CurrentHierarchy->SetRigElementKeyMetadata(NewElement->Key, FRigSocketElement::DesiredParentMetaName, InParent);
	}

#if WITH_EDITOR
	TransactionPtr.Reset();

	if (bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			TArray<FString> Commands = GetAddSocketPythonCommands(NewElement);
			for (const FString& Command : Commands)
			{			
				RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
					FString::Printf(TEXT("%s"), *Command));
			}
		}
	}
#endif

	CurrentHierarchy->EnsureCacheValidity();
		
	return NewElement->Key;
}

FRigElementKey URigHierarchyController::AddDefaultRootSocket()
{
	FRigElementKey SocketKey;
	if(const URigHierarchy* CurrentHierarchy = GetHierarchy())
	{
		static const FRigElementKey RootSocketKey(TEXT("Root"), ERigElementType::Socket);
		if(CurrentHierarchy->Contains(RootSocketKey))
		{
			return RootSocketKey;
		}

		CurrentHierarchy->ForEach<FRigBoneElement>([this, CurrentHierarchy, &SocketKey](const FRigBoneElement* Bone) -> bool
		{
			// find first root bone
			if(CurrentHierarchy->GetNumberOfParents(Bone) == 0)
			{
				SocketKey = AddSocket(RootSocketKey.Name, Bone->GetKey(), FTransform::Identity);

				// stop
				return false;
			}

			// continue with the search
			return true;
		});
	}
	return SocketKey; 
}

FRigControlSettings URigHierarchyController::GetControlSettings(FRigElementKey InKey) const
{
	if(!IsValid())
	{
		return FRigControlSettings();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InKey);
	if(ControlElement == nullptr)
	{
		return FRigControlSettings();
	}

	return ControlElement->Settings;
}

bool URigHierarchyController::SetControlSettings(FRigElementKey InKey, FRigControlSettings InSettings, bool bSetupUndo) const
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InKey);
	if(ControlElement == nullptr)
	{
		return false;
	}

	#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "SetControlSettings", "Set Control Settings"));
		Hierarchy->Modify();
	}
#endif

	ControlElement->Settings = InSettings;
	if(ControlElement->Settings.LimitEnabled.IsEmpty())
	{
		ControlElement->Settings.SetupLimitArrayForType(false, false, false);
	}

	FRigControlValue InitialValue = Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Initial);
	FRigControlValue CurrentValue = Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Current);

	ControlElement->Settings.ApplyLimits(InitialValue);
	ControlElement->Settings.ApplyLimits(CurrentValue);

	Hierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);

	Hierarchy->SetControlValue(ControlElement, InitialValue, ERigControlValueType::Initial, bSetupUndo);
	Hierarchy->SetControlValue(ControlElement, CurrentValue, ERigControlValueType::Current, bSetupUndo);

#if WITH_EDITOR
    TransactionPtr.Reset();
#endif

	Hierarchy->EnsureCacheValidity();
	
	return true;
}

TArray<FRigElementKey> URigHierarchyController::ImportBones(const FReferenceSkeleton& InSkeleton,
                                                            const FName& InNameSpace, bool bReplaceExistingBones, bool bRemoveObsoleteBones, bool bSelectBones,
                                                            bool bSetupUndo)
{
	TArray<FRigElementKey> AddedBones;

	if(!IsValid())
	{
		return AddedBones;
	}
	
	TArray<FRigElementKey> BonesToSelect;
	TMap<FName, FName> BoneNameMap;

	URigHierarchy* Hierarchy = GetHierarchy();

	Hierarchy->ResetPoseToInitial();

	const TArray<FMeshBoneInfo>& BoneInfos = InSkeleton.GetRefBoneInfo();
	const TArray<FTransform>& BonePoses = InSkeleton.GetRefBonePose();

	struct Local
	{
		static FName DetermineBoneName(const FName& InBoneName, const FName& InLocalNameSpace)
		{
			if (InLocalNameSpace == NAME_None || InBoneName == NAME_None)
			{
				return InBoneName;
			}
			return *FString::Printf(TEXT("%s_%s"), *InLocalNameSpace.ToString(), *InBoneName.ToString());
		}
	};

	if (bReplaceExistingBones)
	{
		TArray<FRigBoneElement*> AllBones = GetHierarchy()->GetElementsOfType<FRigBoneElement>(true);
		for(FRigBoneElement* BoneElement : AllBones)
		{
			BoneNameMap.Add(BoneElement->GetFName(), BoneElement->GetFName());
		}

		for (int32 Index = 0; Index < BoneInfos.Num(); ++Index)
		{
			const FRigElementKey ExistingBoneKey(BoneInfos[Index].Name, ERigElementType::Bone);
			const int32 ExistingBoneIndex = Hierarchy->GetIndex(ExistingBoneKey);
			
			const FName DesiredBoneName = Local::DetermineBoneName(BoneInfos[Index].Name, InNameSpace);
			FName ParentName = (BoneInfos[Index].ParentIndex != INDEX_NONE) ? BoneInfos[BoneInfos[Index].ParentIndex].Name : NAME_None;
			ParentName = Local::DetermineBoneName(ParentName, InNameSpace);

			const FName* MappedParentNamePtr = BoneNameMap.Find(ParentName);
			if (MappedParentNamePtr)
			{
				ParentName = *MappedParentNamePtr;
			}

			const FRigElementKey ParentKey(ParentName, ERigElementType::Bone);

			// if this bone already exists
			if (ExistingBoneIndex != INDEX_NONE)
			{
				const int32 ParentIndex = Hierarchy->GetIndex(ParentKey);
				
				// check it's parent
				if (ParentIndex != INDEX_NONE)
				{
					SetParent(ExistingBoneKey, ParentKey, bSetupUndo);
				}

				Hierarchy->SetInitialLocalTransform(ExistingBoneIndex, BonePoses[Index], true, bSetupUndo);
				Hierarchy->SetLocalTransform(ExistingBoneIndex, BonePoses[Index], true, bSetupUndo);

				BonesToSelect.Add(ExistingBoneKey);
			}
			else
			{
				const FRigElementKey AddedBoneKey = AddBone(DesiredBoneName, ParentKey, BonePoses[Index], false, ERigBoneType::Imported, bSetupUndo);
				BoneNameMap.Add(DesiredBoneName, AddedBoneKey.Name);
				AddedBones.Add(AddedBoneKey);
				BonesToSelect.Add(AddedBoneKey);
			}
		}

	}
	else // import all as new
	{
		for (int32 Index = 0; Index < BoneInfos.Num(); ++Index)
		{
			FName DesiredBoneName = Local::DetermineBoneName(BoneInfos[Index].Name, InNameSpace);
			FName ParentName = (BoneInfos[Index].ParentIndex != INDEX_NONE) ? BoneInfos[BoneInfos[Index].ParentIndex].Name : NAME_None;
			ParentName = Local::DetermineBoneName(ParentName, InNameSpace);

			const FName* MappedParentNamePtr = BoneNameMap.Find(ParentName);
			if (MappedParentNamePtr)
			{
				ParentName = *MappedParentNamePtr;
			}

			const FRigElementKey ParentKey(ParentName, ERigElementType::Bone);
			const FRigElementKey AddedBoneKey = AddBone(DesiredBoneName, ParentKey, BonePoses[Index], false, ERigBoneType::Imported, bSetupUndo);
			BoneNameMap.Add(DesiredBoneName, AddedBoneKey.Name);
			AddedBones.Add(AddedBoneKey);
			BonesToSelect.Add(AddedBoneKey);
		}
	}

	if (bReplaceExistingBones && bRemoveObsoleteBones)
	{
		TMap<FName, int32> BoneNameToIndexInSkeleton;
		for (const FMeshBoneInfo& BoneInfo : BoneInfos)
		{
			FName DesiredBoneName = Local::DetermineBoneName(BoneInfo.Name, InNameSpace);
			BoneNameToIndexInSkeleton.Add(DesiredBoneName, BoneNameToIndexInSkeleton.Num());
		}
		
		TArray<FRigElementKey> BonesToDelete;
		TArray<FRigBoneElement*> AllBones = GetHierarchy()->GetElementsOfType<FRigBoneElement>(true);
		for(FRigBoneElement* BoneElement : AllBones)
        {
            if (!BoneNameToIndexInSkeleton.Contains(BoneElement->GetFName()))
			{
				if (BoneElement->BoneType == ERigBoneType::Imported)
				{
					BonesToDelete.Add(BoneElement->GetKey());
				}
			}
		}

		for (const FRigElementKey& BoneToDelete : BonesToDelete)
		{
			TArray<FRigElementKey> Children = Hierarchy->GetChildren(BoneToDelete);
			Algo::Reverse(Children);
			
			for (const FRigElementKey& Child : Children)
			{
				if(BonesToDelete.Contains(Child))
				{
					continue;
				}
				RemoveAllParents(Child, true, bSetupUndo);
			}
		}

		for (const FRigElementKey& BoneToDelete : BonesToDelete)
		{
			RemoveElement(BoneToDelete);
			BonesToSelect.Remove(BoneToDelete);
		}

		// update the sub index to match the bone index in the skeleton
		for (int32 Index = 0; Index < BoneInfos.Num(); ++Index)
		{
			FName DesiredBoneName = Local::DetermineBoneName(BoneInfos[Index].Name, InNameSpace);
			const FRigElementKey Key(DesiredBoneName, ERigElementType::Bone);
			if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(Key))
			{
				BoneElement->SubIndex = Index;
			}
		}		
	}

	if (bSelectBones)
	{
		SetSelection(BonesToSelect);
	}

	Hierarchy->EnsureCacheValidity();

	return AddedBones;
}

TArray<FRigElementKey> URigHierarchyController::ImportBones(USkeleton* InSkeleton, FName InNameSpace,
                                                            bool bReplaceExistingBones, bool bRemoveObsoleteBones,
                                                            bool bSelectBones, bool bSetupUndo,
                                                            bool bPrintPythonCommand)
{
	FReferenceSkeleton EmptySkeleton;
	FReferenceSkeleton& RefSkeleton = EmptySkeleton; 
	if (InSkeleton != nullptr)
	{
		RefSkeleton = InSkeleton->GetReferenceSkeleton();
	}

	const TArray<FRigElementKey> BoneKeys = ImportBones(RefSkeleton, InNameSpace, bReplaceExistingBones, bRemoveObsoleteBones,
	                   bSelectBones, bSetupUndo);

#if WITH_EDITOR
	if (!BoneKeys.IsEmpty() && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.import_bones_from_asset('%s', '%s', %s, %s, %s)"),
				(InSkeleton != nullptr) ? *InSkeleton->GetPathName() : TEXT(""),
				*InNameSpace.ToString(),
				(bReplaceExistingBones) ? TEXT("True") : TEXT("False"),
				(bRemoveObsoleteBones) ? TEXT("True") : TEXT("False"),
				(bSelectBones) ? TEXT("True") : TEXT("False")));
		}
	}
#endif
	return BoneKeys;
}

#if WITH_EDITOR

TArray<FRigElementKey> URigHierarchyController::ImportBonesFromAsset(FString InAssetPath, FName InNameSpace,
	bool bReplaceExistingBones, bool bRemoveObsoleteBones, bool bSelectBones, bool bSetupUndo)
{
	if(USkeleton* Skeleton = GetSkeletonFromAssetPath(InAssetPath))
	{
		return ImportBones(Skeleton, InNameSpace, bReplaceExistingBones, bRemoveObsoleteBones, bSelectBones, bSetupUndo);
	}
	return TArray<FRigElementKey>();
}

TArray<FRigElementKey> URigHierarchyController::ImportCurvesFromAsset(FString InAssetPath, FName InNameSpace,
	bool bSelectCurves, bool bSetupUndo)
{
	if(USkeleton* Skeleton = GetSkeletonFromAssetPath(InAssetPath))
	{
		return ImportCurves(Skeleton, InNameSpace, bSelectCurves, bSetupUndo);
	}
	return TArray<FRigElementKey>();
}

USkeleton* URigHierarchyController::GetSkeletonFromAssetPath(const FString& InAssetPath)
{
	UObject* AssetObject = StaticLoadObject(UObject::StaticClass(), NULL, *InAssetPath, NULL, LOAD_None, NULL);
	if(AssetObject == nullptr)
	{
		return nullptr;
    }

	if(USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetObject))
	{
		return SkeletalMesh->GetSkeleton();
	}

	if(USkeleton* Skeleton = Cast<USkeleton>(AssetObject))
	{
		return Skeleton;
	}

	return nullptr;
}

#endif

TArray<FRigElementKey> URigHierarchyController::ImportCurves(USkeleton* InSkeleton, FName InNameSpace,
                                                             bool bSelectCurves, bool bSetupUndo, bool bPrintPythonCommand)
{
	if (InSkeleton == nullptr)
	{
		return TArray<FRigElementKey>();
	}

	TArray<FRigElementKey> Keys;
	if(!IsValid())
	{
		return Keys;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	InSkeleton->ForEachCurveMetaData([this, Hierarchy, InNameSpace, &Keys, bSetupUndo](const FName& InCurveName, const FCurveMetaData& InMetaData)
	{
		FName Name = InCurveName;
		if (!InNameSpace.IsNone())
		{
			Name = *FString::Printf(TEXT("%s::%s"), *InNameSpace.ToString(), *InCurveName.ToString());
		}

		const FRigElementKey ExpectedKey(Name, ERigElementType::Curve);
		if(Hierarchy->Contains(ExpectedKey))
		{
			Keys.Add(ExpectedKey);
			return;
		}
		
		const FRigElementKey CurveKey = AddCurve(Name, 0.f, bSetupUndo);
		Keys.Add(FRigElementKey(Name, ERigElementType::Curve));
	});

	if(bSelectCurves)
	{
		SetSelection(Keys);
	}
	
#if WITH_EDITOR
	if (!Keys.IsEmpty() && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.import_curves_from_asset('%s', '%s', %s)"),
				*InSkeleton->GetPathName(),
				*InNameSpace.ToString(),
				(bSelectCurves) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	return Keys;
}

FString URigHierarchyController::ExportSelectionToText() const
{
	if(!IsValid())
	{
		return FString();
	}

	URigHierarchy* Hierarchy = GetHierarchy();
	return ExportToText(Hierarchy->GetSelectedKeys());
}

FString URigHierarchyController::ExportToText(TArray<FRigElementKey> InKeys) const
{
	if(!IsValid() || InKeys.IsEmpty())
	{
		return FString();
	}

	URigHierarchy* Hierarchy = GetHierarchy();
	Hierarchy->ComputeAllTransforms();

	// sort the keys by traversal order
	TArray<FRigElementKey> Keys = Hierarchy->SortKeys(InKeys);

	FRigHierarchyCopyPasteContent Data;
	for (const FRigElementKey& Key : Keys)
	{
		FRigBaseElement* Element = Hierarchy->Find(Key);
		if(Element == nullptr)
		{
			continue;
		}

		FRigHierarchyCopyPasteContentPerElement PerElementData;
		PerElementData.Key = Key;
		PerElementData.Parents = Hierarchy->GetParents(Key);

		if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Element))
		{
			ensure(PerElementData.Parents.Num() == MultiParentElement->ParentConstraints.Num());

			for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
			{
				PerElementData.ParentWeights.Add(ParentConstraint.Weight);
			}
		}
		else
		{
			PerElementData.ParentWeights.SetNumZeroed(PerElementData.Parents.Num());
			if(PerElementData.ParentWeights.Num() > 0)
			{
				PerElementData.ParentWeights[0] = 1.f;
			}
		}

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
		{
			PerElementData.Pose.Initial.Local.Set(Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialLocal), PerElementData.Pose.Initial.bDirty[FRigLocalAndGlobalTransform::ELocal]);
			PerElementData.Pose.Initial.Global.Set(Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialGlobal), PerElementData.Pose.Initial.bDirty[FRigLocalAndGlobalTransform::EGlobal]);
			PerElementData.Pose.Current.Local.Set(Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentLocal), PerElementData.Pose.Current.bDirty[FRigLocalAndGlobalTransform::ELocal]);
			PerElementData.Pose.Current.Global.Set(Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentGlobal), PerElementData.Pose.Current.bDirty[FRigLocalAndGlobalTransform::EGlobal]);
		}

		switch (Key.Type)
		{
			case ERigElementType::Bone:
			{
				FRigBoneElement DefaultElement;
				FRigBoneElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Control:
			{
				FRigControlElement DefaultElement;
				FRigControlElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Null:
			{
				FRigNullElement DefaultElement;
				FRigNullElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Curve:
			{
				FRigCurveElement DefaultElement;
				FRigCurveElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::RigidBody:
			{
				FRigRigidBodyElement DefaultElement;
				FRigRigidBodyElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Reference:
			{
				FRigReferenceElement DefaultElement;
				FRigReferenceElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Connector:
			{
				FRigConnectorElement DefaultElement;
				FRigConnectorElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			case ERigElementType::Socket:
			{
				FRigSocketElement DefaultElement;
				FRigSocketElement::StaticStruct()->ExportText(PerElementData.Content, Element, &DefaultElement, nullptr, PPF_None, nullptr);
				break;
			}
			default:
			{
				ensure(false);
				break;
			}
		}

		Data.Elements.Add(PerElementData);
	}

	FString ExportedText;
	FRigHierarchyCopyPasteContent DefaultContent;
	FRigHierarchyCopyPasteContent::StaticStruct()->ExportText(ExportedText, &Data, &DefaultContent, nullptr, PPF_None, nullptr);
	return ExportedText;
}

class FRigHierarchyImportErrorContext : public FOutputDevice
{
public:

	int32 NumErrors;

	FRigHierarchyImportErrorContext()
        : FOutputDevice()
        , NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		UE_LOG(LogControlRig, Error, TEXT("Error Importing To Hierarchy: %s"), V);
		NumErrors++;
	}
};

TArray<FRigElementKey> URigHierarchyController::ImportFromText(FString InContent, bool bReplaceExistingElements, bool bSelectNewElements, bool bSetupUndo, bool bPrintPythonCommands)
{
	return ImportFromText(InContent, ERigElementType::All, bReplaceExistingElements, bSelectNewElements, bSetupUndo, bPrintPythonCommands);
}

TArray<FRigElementKey> URigHierarchyController::ImportFromText(FString InContent, ERigElementType InAllowedTypes, bool bReplaceExistingElements, bool bSelectNewElements, bool bSetupUndo, bool bPrintPythonCommands)
{
	TArray<FRigElementKey> PastedKeys;
	if(!IsValid())
	{
		return PastedKeys;
	}

	FRigHierarchyCopyPasteContent Data;
	FRigHierarchyImportErrorContext ErrorPipe;
	FRigHierarchyCopyPasteContent::StaticStruct()->ImportText(*InContent, &Data, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigHierarchyCopyPasteContent::StaticStruct()->GetName(), true);
	if (ErrorPipe.NumErrors > 0)
	{
		return PastedKeys;
	}

	if (Data.Elements.Num() == 0)
	{
		// check if this is a copy & paste buffer from pre-5.0
		if(Data.Contents.Num() > 0)
		{
			const int32 OriginalNumElements = Data.Elements.Num();
			for (int32 i=0; i<Data.Types.Num(); )
			{
				if (((uint8)InAllowedTypes & (uint8)Data.Types[i]) == 0)
				{
					Data.Contents.RemoveAt(i);
					Data.Types.RemoveAt(i);
					Data.LocalTransforms.RemoveAt(i);
					Data.GlobalTransforms.RemoveAt(i);
					continue;
				}
				++i;
			}
			if (OriginalNumElements > Data.Types.Num())
			{
				ReportAndNotifyErrorf(TEXT("Some elements were not allowed to be pasted."));
			}
			FRigHierarchyContainer OldHierarchy;
			if(OldHierarchy.ImportFromText(Data).Num() > 0)
			{
				return ImportFromHierarchyContainer(OldHierarchy, true);
			}
		}
		
		return PastedKeys;
	}

	const int32 OriginalNumElements = Data.Elements.Num();
	Data.Elements = Data.Elements.FilterByPredicate([InAllowedTypes](const FRigHierarchyCopyPasteContentPerElement& Element)
	{
		return ((uint8)InAllowedTypes & (uint8)Element.Key.Type) != 0;
	});
	if (OriginalNumElements > Data.Elements.Num())
	{
		ReportAndNotifyErrorf(TEXT("Some elements were not allowed to be pasted."));
	}

	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Elements", "Add Elements"));
		Hierarchy->Modify();
	}
#endif

	TMap<FRigElementKey, FRigElementKey> KeyMap;
	for(FRigBaseElement* Element : *Hierarchy)
	{
		KeyMap.Add(Element->GetKey(), Element->GetKey());
	}

	FRigHierarchyInteractionBracket InteractionBracket(Hierarchy);

	for(const FRigHierarchyCopyPasteContentPerElement& PerElementData : Data.Elements)
	{
		ErrorPipe.NumErrors = 0;

		FRigBaseElement* NewElement = nullptr;

		switch (PerElementData.Key.Type)
		{
			case ERigElementType::Bone:
			{
				NewElement = MakeElement<FRigBoneElement>();
				FRigBoneElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigBoneElement::StaticStruct()->GetName(), true);
				CastChecked<FRigBoneElement>(NewElement)->BoneType = ERigBoneType::User;
				break;
			}
			case ERigElementType::Null:
			{
				NewElement = MakeElement<FRigNullElement>();
				FRigNullElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigNullElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::Control:
			{
				NewElement = MakeElement<FRigControlElement>();
				FRigControlElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigControlElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::Curve:
			{
				NewElement = MakeElement<FRigCurveElement>();
				FRigCurveElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigCurveElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::RigidBody:
			{
				NewElement = MakeElement<FRigRigidBodyElement>();
				FRigRigidBodyElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigRigidBodyElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::Reference:
			{
				NewElement = MakeElement<FRigReferenceElement>();
				FRigReferenceElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigReferenceElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::Connector:
			{
				NewElement = MakeElement<FRigConnectorElement>();
				FRigConnectorElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigConnectorElement::StaticStruct()->GetName(), true);
				break;
			}
			case ERigElementType::Socket:
			{
				NewElement = MakeElement<FRigSocketElement>();
				FRigSocketElement::StaticStruct()->ImportText(*PerElementData.Content, NewElement, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigSocketElement::StaticStruct()->GetName(), true);
				break;
			}
			default:
			{
				ensure(false);
				break;
			}
		}

		check(NewElement);
		NewElement->Key = PerElementData.Key;

		if(bReplaceExistingElements)
		{
			if(FRigBaseElement* ExistingElement = Hierarchy->Find(NewElement->GetKey()))
			{
				ExistingElement->CopyPose(NewElement, true, true, false);

				if(FRigControlElement* ControlElement = Cast<FRigControlElement>(ExistingElement))
				{
					Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
					Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::InitialLocal);
					ControlElement->Shape.MarkDirty(ERigTransformType::CurrentGlobal);
					ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
				}
				
				TArray<FRigElementKey> CurrentParents = Hierarchy->GetParents(NewElement->GetKey());

				bool bUpdateParents = CurrentParents.Num() != PerElementData.Parents.Num();
				if(!bUpdateParents)
				{
					for(const FRigElementKey& CurrentParent : CurrentParents)
					{
						if(!PerElementData.Parents.Contains(CurrentParent))
						{
							bUpdateParents = true;
							break;
						}
					}
				}

				if(bUpdateParents)
				{
					RemoveAllParents(ExistingElement->GetKey(), true, bSetupUndo);

					for(const FRigElementKey& NewParent : PerElementData.Parents)
					{
						AddParent(ExistingElement->GetKey(), NewParent, 0.f, true, bSetupUndo);
					}
				}
				
				for(int32 ParentIndex = 0; ParentIndex < PerElementData.ParentWeights.Num(); ParentIndex++)
				{
					Hierarchy->SetParentWeight(ExistingElement, ParentIndex, PerElementData.ParentWeights[ParentIndex], true, true);
					Hierarchy->SetParentWeight(ExistingElement, ParentIndex, PerElementData.ParentWeights[ParentIndex], false, true);
				}

				PastedKeys.Add(ExistingElement->GetKey());

				Hierarchy->DestroyElement(NewElement);
				continue;
			}
		}

		const FName DesiredName = NewElement->Key.Name;
		NewElement->Key.Name = GetSafeNewName(DesiredName, NewElement->Key.Type);
		AddElement(NewElement, nullptr, true, DesiredName);

		KeyMap.FindOrAdd(PerElementData.Key) = NewElement->Key;

		for(const FRigElementKey& OriginalParent : PerElementData.Parents)
		{
			FRigElementKey Parent = OriginalParent;
			if(const FRigElementKey* RemappedParent = KeyMap.Find(Parent))
			{
				Parent = *RemappedParent;
			}

			AddParent(NewElement->GetKey(), Parent, 0.f, true, bSetupUndo);
		}
		
		for(int32 ParentIndex = 0; ParentIndex < PerElementData.ParentWeights.Num(); ParentIndex++)
		{
			Hierarchy->SetParentWeight(NewElement, ParentIndex, PerElementData.ParentWeights[ParentIndex], true, true);
			Hierarchy->SetParentWeight(NewElement, ParentIndex, PerElementData.ParentWeights[ParentIndex], false, true);
		}

		PastedKeys.Add(NewElement->GetKey());
	}

	for(const FRigHierarchyCopyPasteContentPerElement& PerElementData : Data.Elements)
	{
		FRigElementKey MappedKey = KeyMap.FindChecked(PerElementData.Key);
		FRigBaseElement* Element = Hierarchy->FindChecked(MappedKey);

		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
		{
			Hierarchy->SetTransform(TransformElement, PerElementData.Pose.Initial.Local.Transform, ERigTransformType::InitialLocal, true, true);
			Hierarchy->SetTransform(TransformElement, PerElementData.Pose.Current.Local.Transform, ERigTransformType::CurrentLocal, true, true);
		}
	}
	
#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	if(bSelectNewElements)
	{
		SetSelection(PastedKeys);
	}

#if WITH_EDITOR
	if (bPrintPythonCommands && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			FString PythonContent = InContent.Replace(TEXT("\\\""), TEXT("\\\\\""));
		
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.import_from_text('%s', %s, %s)"),
				*PythonContent,
				(bReplaceExistingElements) ? TEXT("True") : TEXT("False"),
				(bSelectNewElements) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	return PastedKeys;
}

TArray<FRigElementKey> URigHierarchyController::ImportFromHierarchyContainer(const FRigHierarchyContainer& InContainer, bool bIsCopyAndPaste)
{
	URigHierarchy* Hierarchy = GetHierarchy();

	TMap<FRigElementKey, FRigElementKey> KeyMap;;
	FRigHierarchyInteractionBracket InteractionBracket(Hierarchy);

	for(const FRigBone& Bone : InContainer.BoneHierarchy)
	{
		const FRigElementKey OriginalParentKey = Bone.GetParentElementKey(true);
		const FRigElementKey* ParentKey = nullptr;
		if(OriginalParentKey.IsValid())
		{
			ParentKey = KeyMap.Find(OriginalParentKey);
		}
		if(ParentKey == nullptr)
		{
			ParentKey = &OriginalParentKey;
		}

		const FRigElementKey Key = AddBone(Bone.Name, *ParentKey, Bone.InitialTransform, true, bIsCopyAndPaste ? ERigBoneType::User : Bone.Type, false);
		KeyMap.Add(Bone.GetElementKey(), Key);
	}
	for(const FRigSpace& Space : InContainer.SpaceHierarchy)
	{
		const FRigElementKey Key = AddNull(Space.Name, FRigElementKey(), Space.InitialTransform, false, false);
		KeyMap.Add(Space.GetElementKey(), Key);
	}
	for(const FRigControl& Control : InContainer.ControlHierarchy)
	{
		FRigControlSettings Settings;
		Settings.ControlType = Control.ControlType;
		Settings.DisplayName = Control.DisplayName;
		Settings.PrimaryAxis = Control.PrimaryAxis;
		Settings.bIsCurve = Control.bIsCurve;
		Settings.SetAnimationTypeFromDeprecatedData(Control.bAnimatable, Control.bGizmoEnabled);
		Settings.SetupLimitArrayForType(Control.bLimitTranslation, Control.bLimitRotation, Control.bLimitScale);
		Settings.bDrawLimits = Control.bDrawLimits;
		Settings.MinimumValue = Control.MinimumValue;
		Settings.MaximumValue = Control.MaximumValue;
		Settings.bShapeVisible = Control.bGizmoVisible;
		Settings.ShapeName = Control.GizmoName;
		Settings.ShapeColor = Control.GizmoColor;
		Settings.ControlEnum = Control.ControlEnum;
		Settings.bGroupWithParentControl = Settings.IsAnimatable() && (
			Settings.ControlType == ERigControlType::Bool ||
			Settings.ControlType == ERigControlType::Float ||
			Settings.ControlType == ERigControlType::ScaleFloat ||
			Settings.ControlType == ERigControlType::Integer ||
			Settings.ControlType == ERigControlType::Vector2D
		);

		if(Settings.ShapeName == FRigControl().GizmoName)
		{
			Settings.ShapeName = FControlRigShapeDefinition().ShapeName; 
		}

		FRigControlValue InitialValue = Control.InitialValue;

#if WITH_EDITORONLY_DATA
		if(!InitialValue.IsValid())
		{
			InitialValue.SetFromTransform(InitialValue.Storage_DEPRECATED, Settings.ControlType, Settings.PrimaryAxis);
		}
#endif
		
		const FRigElementKey Key = AddControl(
			Control.Name,
			FRigElementKey(),
			Settings,
			InitialValue,
			Control.OffsetTransform,
			Control.GizmoTransform,
			false);

		KeyMap.Add(Control.GetElementKey(), Key);
	}
	
	for(const FRigCurve& Curve : InContainer.CurveContainer)
	{
		const FRigElementKey Key = AddCurve(Curve.Name, Curve.Value, false);
		KeyMap.Add(Curve.GetElementKey(), Key);
	}

	for(const FRigSpace& Space : InContainer.SpaceHierarchy)
	{
		const FRigElementKey SpaceKey = KeyMap.FindChecked(Space.GetElementKey());
		const FRigElementKey OriginalParentKey = Space.GetParentElementKey();
		if(OriginalParentKey.IsValid())
		{
			FRigElementKey ParentKey;
			if(const FRigElementKey* ParentKeyPtr = KeyMap.Find(OriginalParentKey))
			{
				ParentKey = *ParentKeyPtr;
			}
			SetParent(SpaceKey, ParentKey, false, false);
		}
	}

	for(const FRigControl& Control : InContainer.ControlHierarchy)
	{
		const FRigElementKey ControlKey = KeyMap.FindChecked(Control.GetElementKey());
		FRigElementKey OriginalParentKey = Control.GetParentElementKey();
		const FRigElementKey SpaceKey = Control.GetSpaceElementKey();
		OriginalParentKey = SpaceKey.IsValid() ? SpaceKey : OriginalParentKey;
		if(OriginalParentKey.IsValid())
		{
			FRigElementKey ParentKey;
			if(const FRigElementKey* ParentKeyPtr = KeyMap.Find(OriginalParentKey))
			{
				ParentKey = *ParentKeyPtr;
			}
			SetParent(ControlKey, ParentKey, false, false);
		}
	}

#if WITH_EDITOR
	if(!IsRunningCommandlet()) // don't show warnings like this if we are cooking
	{
		for(const TPair<FRigElementKey, FRigElementKey>& Pair: KeyMap)
		{
			if(Pair.Key != Pair.Value)
			{
				check(Pair.Key.Type == Pair.Value.Type);
				const FText TypeLabel = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Pair.Key.Type);
				ReportWarningf(TEXT("%s '%s' was renamed to '%s' during load (fixing invalid name)."), *TypeLabel.ToString(), *Pair.Key.Name.ToString(), *Pair.Value.Name.ToString());
			}
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();

	TArray<FRigElementKey> AddedKeys;
	KeyMap.GenerateValueArray(AddedKeys);
	return AddedKeys;
}

#if WITH_EDITOR
TArray<FString> URigHierarchyController::GeneratePythonCommands()
{
	URigHierarchy* Hierarchy = GetHierarchy();

	TArray<FString> Commands;
	Hierarchy->Traverse([&](FRigBaseElement* Element, bool& bContinue)
	{
		Commands.Append(GetAddElementPythonCommands(Element));
		
		bContinue = true;
		return;
	});

	return Commands;
}

TArray<FString> URigHierarchyController::GetAddElementPythonCommands(FRigBaseElement* Element) const
{
	if(FRigBoneElement* BoneElement = Cast<FRigBoneElement>(Element))
	{
		return GetAddBonePythonCommands(BoneElement);		
	}
	else if(FRigNullElement* NullElement = Cast<FRigNullElement>(Element))
	{
		return GetAddNullPythonCommands(NullElement);
	}
	else if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
	{
		return GetAddControlPythonCommands(ControlElement);
	}
	else if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Element))
	{
		return GetAddCurvePythonCommands(CurveElement);
	}
	else if(FRigRigidBodyElement* RigidBodyElement = Cast<FRigRigidBodyElement>(Element))
	{
		return GetAddRigidBodyPythonCommands(RigidBodyElement);
	}
	else if(FRigReferenceElement* ReferenceElement = Cast<FRigReferenceElement>(Element))
	{
		ensure(false);
	}
	else if(FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Element))
	{
		return GetAddConnectorPythonCommands(ConnectorElement);
	}
	else if(FRigSocketElement* SocketElement = Cast<FRigSocketElement>(Element))
	{
		return GetAddSocketPythonCommands(SocketElement);
	}
	return TArray<FString>();
}

TArray<FString> URigHierarchyController::GetAddBonePythonCommands(FRigBoneElement* Bone) const
{
	TArray<FString> Commands;
	if (!Bone)
	{
		return Commands;
	}
	
	FString TransformStr = RigVMPythonUtils::TransformToPythonString(Bone->Pose.Initial.Local.Transform);
	FString ParentKeyStr = "''";
	if (Bone->ParentElement)
	{
		ParentKeyStr = Bone->ParentElement->GetKey().ToPythonString();
	}
	
	// AddBone(FName InName, FRigElementKey InParent, FTransform InTransform, bool bTransformInGlobal = true, ERigBoneType InBoneType = ERigBoneType::User, bool bSetupUndo = false);
	Commands.Add(FString::Printf(TEXT("hierarchy_controller.add_bone('%s', %s, %s, False, %s)"),
		*Bone->GetName(),
		*ParentKeyStr,
		*TransformStr,
		*RigVMPythonUtils::EnumValueToPythonString<ERigBoneType>((int64)Bone->BoneType)
	));

	return Commands;
}

TArray<FString> URigHierarchyController::GetAddNullPythonCommands(FRigNullElement* Null) const
{
	FString TransformStr = RigVMPythonUtils::TransformToPythonString(Null->Pose.Initial.Local.Transform);

	FString ParentKeyStr = "''";
	if (Null->ParentConstraints.Num() > 0)
	{
		ParentKeyStr = Null->ParentConstraints[0].ParentElement->GetKey().ToPythonString();		
	}
		
	// AddNull(FName InName, FRigElementKey InParent, FTransform InTransform, bool bTransformInGlobal = true, bool bSetupUndo = false);
	return {FString::Printf(TEXT("hierarchy_controller.add_null('%s', %s, %s, False)"),
		*Null->GetName(),
		*ParentKeyStr,
		*TransformStr)};
}

TArray<FString> URigHierarchyController::GetAddControlPythonCommands(FRigControlElement* Control) const
{
	TArray<FString> Commands;
	FString TransformStr = RigVMPythonUtils::TransformToPythonString(Control->Pose.Initial.Local.Transform);

	FString ParentKeyStr = "''";
	if (Control->ParentConstraints.Num() > 0)
	{
		ParentKeyStr = Control->ParentConstraints[0].ParentElement->GetKey().ToPythonString();
	}

	FRigControlSettings& Settings = Control->Settings;
	FString SettingsStr;
	{
		FString ControlNamePythonized = RigVMPythonUtils::PythonizeName(Control->GetName());
		SettingsStr = FString::Printf(TEXT("control_settings_%s"),
			*ControlNamePythonized);
			
		Commands.Append(URigHierarchy::ControlSettingsToPythonCommands(Settings, SettingsStr));	
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigControlValue Value = Hierarchy->GetControlValue(Control->GetKey(), ERigControlValueType::Initial);
	FString ValueStr = Value.ToPythonString(Settings.ControlType);
	
	// AddControl(FName InName, FRigElementKey InParent, FRigControlSettings InSettings, FRigControlValue InValue, bool bSetupUndo = true);
	Commands.Add(FString::Printf(TEXT("hierarchy_controller.add_control('%s', %s, %s, %s)"),
		*Control->GetName(),
		*ParentKeyStr,
		*SettingsStr,
		*ValueStr));

	Commands.Append(GetSetControlShapeTransformPythonCommands(Control, Control->Shape.Initial.Local.Transform, true));
	Commands.Append(GetSetControlValuePythonCommands(Control, Settings.MinimumValue, ERigControlValueType::Minimum));
	Commands.Append(GetSetControlValuePythonCommands(Control, Settings.MaximumValue, ERigControlValueType::Maximum));
	Commands.Append(GetSetControlOffsetTransformPythonCommands(Control, Control->Offset.Initial.Local.Transform, true, true));
	Commands.Append(GetSetControlValuePythonCommands(Control, Value, ERigControlValueType::Current));

	return Commands;
}

TArray<FString> URigHierarchyController::GetAddCurvePythonCommands(FRigCurveElement* Curve) const
{
	URigHierarchy* Hierarchy = GetHierarchy();

	// FRigElementKey AddCurve(FName InName, float InValue = 0.f, bool bSetupUndo = true);
	return {FString::Printf(TEXT("hierarchy_controller.add_curve('%s', %f)"),
		*Curve->GetName(),
		Hierarchy->GetCurveValue(Curve))};
}

TArray<FString> URigHierarchyController::GetAddRigidBodyPythonCommands(FRigRigidBodyElement* RigidBody) const
{
	TArray<FString> Commands;
	FString TransformStr = RigVMPythonUtils::TransformToPythonString(RigidBody->Pose.Initial.Local.Transform);
	
	FString ParentKeyStr = "''";
	if (RigidBody->ParentElement)
	{
		ParentKeyStr = RigidBody->ParentElement->GetKey().ToPythonString();
	}

	FString RigidBodyNamePythonized = RigVMPythonUtils::PythonizeName(RigidBody->GetName());
	FRigRigidBodySettings& Settings = RigidBody->Settings;
	FString SettingsStr;
	{
		SettingsStr =FString::Printf(TEXT("rigid_body_settings_%s"),
			*RigidBodyNamePythonized);
			
		Commands.Add(FString::Printf(TEXT("rigid_body_settings_%s = unreal.RigRigidBodySettings()"),
			*RigidBodyNamePythonized));

		Commands.Add(FString::Printf(TEXT("control_settings_%s.mass = %f"),
			*RigidBodyNamePythonized,
			Settings.Mass));	
	}
	
	// FRigElementKey AddRigidBody(FName InName, FRigElementKey InParent, FRigRigidBodySettings InSettings, FTransform InLocalTransform, bool bSetupUndo = false);
	Commands.Add(FString::Printf(TEXT("hierarchy_controller.add_rigid_body('%s', %s, %s, %s)"),
		*RigidBody->GetName(),
		*ParentKeyStr,
		*SettingsStr,
		*TransformStr));

	return Commands;
}

TArray<FString> URigHierarchyController::GetAddConnectorPythonCommands(FRigConnectorElement* Connector) const
{
	TArray<FString> Commands;

	FRigConnectorSettings& Settings = Connector->Settings;
	FString SettingsStr;
	{
		FString ConnectorNamePythonized = RigVMPythonUtils::PythonizeName(Connector->GetName());
		SettingsStr = FString::Printf(TEXT("connector_settings_%s"),
			*ConnectorNamePythonized);
			
		Commands.Append(URigHierarchy::ConnectorSettingsToPythonCommands(Settings, SettingsStr));	
	}

	// AddConnector(FName InName, FRigConnectorSettings InSettings = FRigConnectorSettings(), bool bSetupUndo = false);
	Commands.Add(FString::Printf(TEXT("hierarchy_controller.add_connector('%s', %s)"),
		*Connector->GetName(),
		*SettingsStr
	));

	return Commands;
}

TArray<FString> URigHierarchyController::GetAddSocketPythonCommands(FRigSocketElement* Socket) const
{
	TArray<FString> Commands;
	FString TransformStr = RigVMPythonUtils::TransformToPythonString(Socket->Pose.Initial.Local.Transform);

	FString ParentKeyStr = "''";
	if (Socket->ParentElement)
	{
		ParentKeyStr = Socket->ParentElement->GetKey().ToPythonString();
	}

	const URigHierarchy* CurrentHierarchy = GetHierarchy();

	// AddSocket(FName InName, FRigElementKey InParent, FTransform InTransform, bool bTransformInGlobal = true, FLinearColor Color, FString Description, bool bSetupUndo = false);
	Commands.Add(FString::Printf(TEXT("hierarchy_controller.add_socket('%s', %s, %s, False, %s, '%s')"),
		*Socket->GetName(),
		*ParentKeyStr,
		*TransformStr,
		*RigVMPythonUtils::LinearColorToPythonString(Socket->GetColor(CurrentHierarchy)),
		*Socket->GetDescription(CurrentHierarchy)
	));

	return Commands;
}

TArray<FString> URigHierarchyController::GetSetControlValuePythonCommands(const FRigControlElement* Control, const FRigControlValue& Value,
                                                                          const ERigControlValueType& Type) const
{
	return {FString::Printf(TEXT("hierarchy.set_control_value(%s, %s, %s)"),
		*Control->GetKey().ToPythonString(),
		*Value.ToPythonString(Control->Settings.ControlType),
		*RigVMPythonUtils::EnumValueToPythonString<ERigControlValueType>((int64)Type))};
}

TArray<FString> URigHierarchyController::GetSetControlOffsetTransformPythonCommands(const FRigControlElement* Control,
                                                                                    const FTransform& Offset, bool bInitial, bool bAffectChildren) const
{
	//SetControlOffsetTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false)
	return {FString::Printf(TEXT("hierarchy.set_control_offset_transform(%s, %s, %s, %s)"),
		*Control->GetKey().ToPythonString(),
		*RigVMPythonUtils::TransformToPythonString(Offset),
		bInitial ? TEXT("True") : TEXT("False"),
		bAffectChildren ? TEXT("True") : TEXT("False"))};
}

TArray<FString> URigHierarchyController::GetSetControlShapeTransformPythonCommands(const FRigControlElement* Control,
	const FTransform& InTransform, bool bInitial) const
{
	//SetControlShapeTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bSetupUndo = false)
	return {FString::Printf(TEXT("hierarchy.set_control_shape_transform(%s, %s, %s)"),
		*Control->GetKey().ToPythonString(),
		*RigVMPythonUtils::TransformToPythonString(InTransform),
		bInitial ? TEXT("True") : TEXT("False"))};
}
#endif

void URigHierarchyController::Notify(ERigHierarchyNotification InNotifType, const FRigBaseElement* InElement)
{
	if(!IsValid())
	{
		return;
	}
	if(bSuspendAllNotifications)
	{
		return;
	}
	if(bSuspendSelectionNotifications)
	{
		if(InNotifType == ERigHierarchyNotification::ElementSelected ||
			InNotifType == ERigHierarchyNotification::ElementDeselected)
		{
			return;
		}
	}	
	GetHierarchy()->Notify(InNotifType, InElement);
}

void URigHierarchyController::HandleHierarchyModified(ERigHierarchyNotification InNotifType, URigHierarchy* InHierarchy, const FRigBaseElement* InElement) const
{
	if(bSuspendAllNotifications)
	{
		return;
	}
	ensure(IsValid());
	ensure(InHierarchy == GetHierarchy());
	ModifiedEvent.Broadcast(InNotifType, InHierarchy, InElement);
}

bool URigHierarchyController::IsValid() const
{
	if(::IsValid(this))
	{
		return ::IsValid(GetHierarchy());
	}
	return false;
}

FName URigHierarchyController::GetSafeNewName(const FName& InDesiredName, ERigElementType InElementType, bool bAllowNameSpace) const
{
	FRigName Name(InDesiredName);

	// remove potential namespaces from it
	if(!bAllowNameSpace)
	{
		int32 Index = INDEX_NONE;
		if(Name.GetName().FindLastChar(TEXT(':'), Index))
		{
			Name.SetName(Name.GetName().Mid(Index + 1));
		}
	}
	
	return GetHierarchy()->GetSafeNewName(Name, InElementType, bAllowNameSpace).GetFName();
}

int32 URigHierarchyController::AddElement(FRigBaseElement* InElementToAdd, FRigBaseElement* InFirstParent, bool bMaintainGlobalTransform, const FName& InDesiredName)
{
	ensure(IsValid());

	URigHierarchy* Hierarchy = GetHierarchy();

	InElementToAdd->CachedNameString.Reset();
	InElementToAdd->SubIndex = Hierarchy->Num(InElementToAdd->Key.Type);
	InElementToAdd->Index = Hierarchy->Elements.Add(InElementToAdd);
	Hierarchy->ElementsPerType[URigHierarchy::RigElementTypeToFlatIndex(InElementToAdd->GetKey().Type)].Add(InElementToAdd);

	Hierarchy->IndexLookup.Add(InElementToAdd->Key, InElementToAdd->Index);
	Hierarchy->IncrementTopologyVersion();

	FRigName DesiredName = InDesiredName;
	URigHierarchy::SanitizeName(DesiredName);
	
	FRigName LastSegmentName;
	if(URigHierarchy::SplitNameSpace(DesiredName, nullptr, &LastSegmentName, true))
	{
		DesiredName = LastSegmentName;
	}

	if(!InDesiredName.IsNone() &&
		!InElementToAdd->GetFName().IsEqual(DesiredName.GetFName(), ENameCase::CaseSensitive))
	{
		Hierarchy->SetNameMetadata(InElementToAdd->Key, URigHierarchy::DesiredNameMetadataName, DesiredName.GetFName());
		Hierarchy->SetRigElementKeyMetadata(InElementToAdd->Key, URigHierarchy::DesiredKeyMetadataName, FRigElementKey(DesiredName.GetFName(), InElementToAdd->Key.Type));
	}
	
	if(Hierarchy->HasExecuteContext())
	{
		const FControlRigExecuteContext& CRContext = Hierarchy->ExecuteContext->GetPublicData<FControlRigExecuteContext>();

		if(!CRContext.GetRigModuleNameSpace().IsEmpty())
		{
			if(InElementToAdd->GetName().StartsWith(CRContext.GetRigModuleNameSpace(), ESearchCase::CaseSensitive))
			{
				Hierarchy->SetNameMetadata(InElementToAdd->Key, URigHierarchy::NameSpaceMetadataName, *CRContext.GetRigModuleNameSpace());
				Hierarchy->SetNameMetadata(InElementToAdd->Key, URigHierarchy::ModuleMetadataName, *CRContext.GetRigModuleNameSpace().LeftChop(1));

				if(Hierarchy->ElementKeyRedirector)
				{
					Hierarchy->ElementKeyRedirector->Add(FRigElementKey(DesiredName.GetFName(), InElementToAdd->Key.Type), InElementToAdd->Key, Hierarchy);
				}
			}

			// also try to find the live module and retrieve the shortname from that to update the element's short name
			if(const FRigModuleInstance* Module = CRContext.GetRigModuleInstance())
			{
				const FString ModuleShortName = Module->GetShortName();
				if(!ModuleShortName.IsEmpty())
				{
					Hierarchy->SetNameMetadata(InElementToAdd->Key, URigHierarchy::ShortModuleNameMetadataName, *ModuleShortName);
				}
			}
		}
	}
	
	{
		const TGuardValue<bool> Guard(bSuspendAllNotifications, true);
		SetParent(InElementToAdd, InFirstParent, bMaintainGlobalTransform);
	}

	if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InElementToAdd))
	{
		Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
		Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::InitialLocal);
		ControlElement->Shape.MarkDirty(ERigTransformType::CurrentGlobal);
		ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
	}

	// only notify once at the end
	Notify(ERigHierarchyNotification::ElementAdded, InElementToAdd);

	return InElementToAdd->Index;
}

bool URigHierarchyController::RemoveElement(FRigElementKey InElement, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Element = Hierarchy->Find(InElement);
	if(Element == nullptr)
	{
		ReportWarningf(TEXT("Cannot Remove Element: '%s' not found."), *InElement.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Remove Element", "Remove Element"));
		Hierarchy->Modify();
	}
#endif

	const bool bRemoved = RemoveElement(Element);
	
#if WITH_EDITOR
	if(!bRemoved && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bRemoved && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.remove_element(%s)"),
				*InElement.ToPythonString()));
		}
	}
#endif

	return bRemoved;
}

bool URigHierarchyController::RemoveElement(FRigBaseElement* InElement)
{
	if(InElement == nullptr)
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	// make sure this element is part of this hierarchy
	ensure(Hierarchy->FindChecked(InElement->Key) == InElement);
	ensure(InElement->OwnedInstances == 1);

	// deselect if needed
	if(InElement->IsSelected())
	{
		SelectElement(InElement->GetKey(), false);
	}

	// if this is a transform element - make sure to allow dependents to store their global transforms
	if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(InElement))
	{
		FRigTransformElement::FElementsToDirtyArray PreviousElementsToDirty = TransformElement->ElementsToDirty; 
		for(const FRigTransformElement::FElementToDirty& ElementToDirty : PreviousElementsToDirty)
		{
			if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(ElementToDirty.Element))
			{
				if(SingleParentElement->ParentElement == InElement)
				{
					RemoveParent(SingleParentElement, InElement, true);
				}
			}
			else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(ElementToDirty.Element))
			{
				for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
				{
					if(ParentConstraint.ParentElement == InElement)
					{
						RemoveParent(MultiParentElement, InElement, true);
						break;
					}
				}
			}
		}
	}

	const int32 NumElementsRemoved = Hierarchy->Elements.Remove(InElement);
	ensure(NumElementsRemoved == 1);

	const int32 NumTypeElementsRemoved = Hierarchy->ElementsPerType[URigHierarchy::RigElementTypeToFlatIndex(InElement->GetKey().Type)].Remove(InElement);
	ensure(NumTypeElementsRemoved == 1);

	const int32 NumLookupsRemoved = Hierarchy->IndexLookup.Remove(InElement->Key);
	ensure(NumLookupsRemoved == 1);
	for(TPair<FRigElementKey, int32>& Pair : Hierarchy->IndexLookup)
	{
		if(Pair.Value > InElement->Index)
		{
			Pair.Value--;
		}
	}

	// update the indices of all other elements
	for (FRigBaseElement* RemainingElement : Hierarchy->Elements)
	{
		if(RemainingElement->Index > InElement->Index)
		{
			RemainingElement->Index--;
		}
	}

	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InElement))
	{
		RemoveElementToDirty(SingleParentElement->ParentElement, InElement);
	}
	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InElement))
	{
		for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
		{
			RemoveElementToDirty(ParentConstraint.ParentElement, InElement);
		}
	}

	if(InElement->SubIndex != INDEX_NONE)
	{
		for(FRigBaseElement* Element : Hierarchy->Elements)
		{
			if(Element->SubIndex > InElement->SubIndex && Element->GetType() == InElement->GetType())
			{
				Element->SubIndex--;
			}
		}
	}

	for(FRigBaseElement* Element : Hierarchy->Elements)
	{
		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			ControlElement->Settings.Customization.AvailableSpaces.Remove(InElement->GetKey());
			ControlElement->Settings.Customization.RemovedSpaces.Remove(InElement->GetKey());
			ControlElement->Settings.DrivenControls.Remove(InElement->GetKey());
		}
	}
	
	Hierarchy->IncrementTopologyVersion();

	Notify(ERigHierarchyNotification::ElementRemoved, InElement);
	if(Hierarchy->Num() == 0)
	{
		Notify(ERigHierarchyNotification::HierarchyReset, nullptr);
	}

	if (InElement->OwnedInstances == 1)
	{
		Hierarchy->DestroyElement(InElement);
	}

	Hierarchy->EnsureCacheValidity();

	return NumElementsRemoved == 1;
}

FRigElementKey URigHierarchyController::RenameElement(FRigElementKey InElement, FName InName, bool bSetupUndo, bool bPrintPythonCommand, bool bClearSelection)
{
	if(!IsValid())
	{
		return FRigElementKey();
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Element = Hierarchy->Find(InElement);
	if(Element == nullptr)
	{
		ReportWarningf(TEXT("Cannot Rename Element: '%s' not found."), *InElement.ToString());
		return FRigElementKey();
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Rename Element", "Rename Element"));
		Hierarchy->Modify();
	}
#endif

	const bool bRenamed = RenameElement(Element, InName, bClearSelection);

#if WITH_EDITOR
	if(!bRenamed && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bRenamed && bClearSelection)
	{
		ClearSelection();
	}

	if (bRenamed && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(), 
				FString::Printf(TEXT("hierarchy_controller.rename_element(%s, '%s')"),
				*InElement.ToPythonString(),
				*InName.ToString()));
		}
	}
#endif

	return bRenamed ? Element->GetKey() : FRigElementKey();
}

bool URigHierarchyController::ReorderElement(FRigElementKey InElement, int32 InIndex, bool bSetupUndo,
	bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Element = Hierarchy->Find(InElement);
	if(Element == nullptr)
	{
		ReportWarningf(TEXT("Cannot Reorder Element: '%s' not found."), *InElement.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Reorder Element", "Reorder Element"));
		Hierarchy->Modify();
	}
#endif

	const bool bReordered = ReorderElement(Element, InIndex);

#if WITH_EDITOR
	if(!bReordered && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bReordered && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(), 
				FString::Printf(TEXT("hierarchy_controller.reorder_element(%s, %d)"),
				*InElement.ToPythonString(),
				InIndex));
		}
	}
#endif

	return bReordered;
}

FName URigHierarchyController::SetDisplayName(FRigElementKey InControl, FName InDisplayName, bool bRenameElement, bool bSetupUndo,
                                              bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return NAME_None;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InControl);
	if(ControlElement == nullptr)
	{
		ReportWarningf(TEXT("Cannot Rename Control: '%s' not found."), *InControl.ToString());
		return NAME_None;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Set Display Name on Control", "Set Display Name on Control"));
		Hierarchy->Modify();
	}
#endif

	const FName NewDisplayName = SetDisplayName(ControlElement, InDisplayName, bRenameElement);
	const bool bDisplayNameChanged = !NewDisplayName.IsNone();

#if WITH_EDITOR
	if(!bDisplayNameChanged && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bDisplayNameChanged && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(), 
				FString::Printf(TEXT("hierarchy_controller.set_display_name(%s, '%s', %s)"),
				*InControl.ToPythonString(),
				*InDisplayName.ToString(),
				(bRenameElement) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	return NewDisplayName;
}

bool URigHierarchyController::RenameElement(FRigBaseElement* InElement, const FName &InName, bool bClearSelection)
{
	if(InElement == nullptr)
	{
		return false;
	}

	if (InElement->GetFName().IsEqual(InName, ENameCase::CaseSensitive))
	{
		return false;
	}

	const FRigElementKey OldKey = InElement->GetKey();

	URigHierarchy* Hierarchy = GetHierarchy();

	// deselect the key that no longer exists
	// no need to trigger a reselect since we always clear selection after rename
	const bool bWasSelected = Hierarchy->IsSelected(InElement); 
	if (Hierarchy->IsSelected(InElement))
	{
		DeselectElement(OldKey);
	}

	{
		// create a temp copy of the map and remove the current item's key
		TMap<FRigElementKey, int32> TemporaryMap = Hierarchy->IndexLookup;
		TemporaryMap.Remove(OldKey);
   
		TGuardValue<TMap<FRigElementKey, int32>> MapGuard(Hierarchy->IndexLookup, TemporaryMap);
		InElement->Key.Name = GetSafeNewName(InName, InElement->GetType());
		InElement->CachedNameString.Reset();
	}
	
	const FRigElementKey NewKey = InElement->GetKey();

	Hierarchy->IndexLookup.Remove(OldKey);
	Hierarchy->IndexLookup.Add(NewKey, InElement->Index);

	// update all multi parent elements' index lookups
	for (FRigBaseElement* Element : Hierarchy->Elements)
	{
		if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(Element))
		{
			const int32* ExistingIndexPtr = MultiParentElement->IndexLookup.Find(OldKey);
			if(ExistingIndexPtr)
			{
				const int32 ExistingIndex = *ExistingIndexPtr;
				MultiParentElement->IndexLookup.Remove(OldKey);
				MultiParentElement->IndexLookup.Add(NewKey, ExistingIndex);
			}
		}

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
		{
			for(FRigElementKey& Favorite : ControlElement->Settings.Customization.AvailableSpaces)
			{
				if(Favorite == OldKey)
				{
					Favorite.Name = NewKey.Name;
				}
			}

			for(FRigElementKey& DrivenControl : ControlElement->Settings.DrivenControls)
			{
				if(DrivenControl == OldKey)
				{
					DrivenControl.Name = NewKey.Name;
				}
			}
		}
	}

	Hierarchy->PreviousNameMap.FindOrAdd(NewKey) = OldKey;
	Hierarchy->IncrementTopologyVersion();
	Notify(ERigHierarchyNotification::ElementRenamed, InElement);

	if (!bClearSelection && bWasSelected)
	{
		SelectElement(InElement->GetKey(), true);
	}

	return true;
}

bool URigHierarchyController::ReorderElement(FRigBaseElement* InElement, int32 InIndex)
{
	if(InElement == nullptr)
	{
		return false;
	}

	InIndex = FMath::Max<int32>(InIndex, 0);

	URigHierarchy* Hierarchy = GetHierarchy();

	TArray<FRigBaseElement*> LocalElements;
	if(const FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(InElement))
	{
		LocalElements.Append(Hierarchy->GetChildren(ParentElement));
	}
	else
	{
		const TArray<FRigBaseElement*> RootElements = Hierarchy->GetRootElements();
		LocalElements.Append(RootElements);
	}

	const int32 CurrentIndex = LocalElements.Find(InElement);
	if(CurrentIndex == INDEX_NONE || CurrentIndex == InIndex)
	{
		return false;
	}

	Hierarchy->IncrementTopologyVersion();

	TArray<int32> GlobalIndices;
	GlobalIndices.Reserve(LocalElements.Num());
	for(const FRigBaseElement* Element : LocalElements)
	{
		GlobalIndices.Add(Element->GetIndex());
	}

	LocalElements.RemoveAt(CurrentIndex);
	if(InIndex >= LocalElements.Num())
	{
		LocalElements.Add(InElement);
	}
	else
	{
		LocalElements.Insert(InElement, InIndex);
	}

	InIndex = FMath::Min<int32>(InIndex, LocalElements.Num() - 1);
	const int32 LowerBound = FMath::Min<int32>(InIndex, CurrentIndex);
	const int32 UpperBound = FMath::Max<int32>(InIndex, CurrentIndex);
	for(int32 LocalIndex = LowerBound; LocalIndex <= UpperBound; LocalIndex++)
	{
		const int32 GlobalIndex = GlobalIndices[LocalIndex];
		FRigBaseElement* Element = LocalElements[LocalIndex];
		Hierarchy->Elements[GlobalIndex] = Element;
		Element->Index = GlobalIndex;
		Hierarchy->IndexLookup.FindOrAdd(Element->Key) = GlobalIndex;
	}

	Notify(ERigHierarchyNotification::ElementReordered, InElement);

	return true;
}

FName URigHierarchyController::SetDisplayName(FRigControlElement* InControlElement, const FName& InDisplayName, bool bRenameElement)
{
	if(InControlElement == nullptr)
	{
		return NAME_None;
	}

	if (InControlElement->Settings.DisplayName.IsEqual(InDisplayName, ENameCase::CaseSensitive))
	{
		return NAME_None;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigElementKey ParentElementKey;
	if(const FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(InControlElement))
	{
		ParentElementKey = ParentElement->GetKey();
	}

	const FName DisplayName = Hierarchy->GetSafeNewDisplayName(ParentElementKey, InDisplayName);
	InControlElement->Settings.DisplayName = DisplayName;

	Hierarchy->IncrementTopologyVersion();
	Notify(ERigHierarchyNotification::ControlSettingChanged, InControlElement);

	if(bRenameElement)
	{
		RenameElement(InControlElement, InControlElement->Settings.DisplayName, false);
	}
#if WITH_EDITOR
	else
	{
		// if we are merely setting the display name - we want to update all listening hierarchies
		for(URigHierarchy::FRigHierarchyListener& Listener : Hierarchy->ListeningHierarchies)
		{
			if(URigHierarchy* ListeningHierarchy = Listener.Hierarchy.Get())
			{
				if(URigHierarchyController* ListeningController = ListeningHierarchy->GetController())
				{
					const TGuardValue<bool> Guard(ListeningController->bSuspendAllNotifications, true);
					ListeningController->SetDisplayName(InControlElement->GetKey(), InDisplayName, bRenameElement, false, false);
				}
			}
		}
	}
#endif
	return InControlElement->Settings.DisplayName;
}

bool URigHierarchyController::AddParent(FRigElementKey InChild, FRigElementKey InParent, float InWeight, bool bMaintainGlobalTransform, bool bSetupUndo)
{
	if(!IsValid())
	{
		return false;
	}

	if(InParent.Type == ERigElementType::Socket)
	{
		ReportWarningf(TEXT("Cannot parent Child '%s' under a Socket parent."), *InChild.ToString());
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Child = Hierarchy->Find(InChild);
	if(Child == nullptr)
	{
		ReportWarningf(TEXT("Cannot Add Parent, Child '%s' not found."), *InChild.ToString());
		return false;
	}

	FRigBaseElement* Parent = Hierarchy->Find(InParent);
	if(Parent == nullptr)
	{
		ReportWarningf(TEXT("Cannot Add Parent, Parent '%s' not found."), *InParent.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Add Parent", "Add Parent"));
		Hierarchy->Modify();
	}
#endif

	const bool bAdded = AddParent(Child, Parent, InWeight, bMaintainGlobalTransform);

#if WITH_EDITOR
	if(!bAdded && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();
#endif
	
	return bAdded;
}

bool URigHierarchyController::AddParent(FRigBaseElement* InChild, FRigBaseElement* InParent, float InWeight, bool bMaintainGlobalTransform, bool bRemoveAllParents)
{
	if(InChild == nullptr || InParent == nullptr)
	{
		return false;
	}

	// single parent children can't be parented multiple times
	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InChild))
	{
		if(SingleParentElement->ParentElement == InParent)
		{
			return false;
		}
		bRemoveAllParents = true;
	}

	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		for(const FRigElementParentConstraint& ParentConstraint : MultiParentElement->ParentConstraints)
		{
			if(ParentConstraint.ParentElement == InParent)
			{
				return false;
			}
		}
	}

	// we can only parent things to controls which are not animation channels (animation channels are not 3D things)
	if(FRigControlElement* ParentControlElement = Cast<FRigControlElement>(InParent))
	{
		if(ParentControlElement->IsAnimationChannel())
		{
			return false;
		}
	}

	// we can only reparent animation channels - we cannot add a parent to them
	if(FRigControlElement* ChildControlElement = Cast<FRigControlElement>(InChild))
	{
		if(ChildControlElement->IsAnimationChannel())
		{
			if(!bRemoveAllParents)
			{
				if (ChildControlElement->ParentConstraints.Num() > 0)
				{
					ReportErrorf(TEXT("Cannot add multiple parents to animation channel '%s'."), *InChild->Key.ToString());
					return false;
				}
			}

			bMaintainGlobalTransform = false;
		}

		if(ChildControlElement->Settings.bRestrictSpaceSwitching)
		{
			if(ChildControlElement->Settings.Customization.AvailableSpaces.Contains(InParent->GetKey()))
			{
				return false;
			}
		}
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	if(Hierarchy->IsParentedTo(InParent, InChild))
	{
		ReportErrorf(TEXT("Cannot parent '%s' to '%s' - would cause a cycle."), *InChild->Key.ToString(), *InParent->Key.ToString());
		return false;
	}

	Hierarchy->EnsureCacheValidity();

	if(bRemoveAllParents)
	{
		RemoveAllParents(InChild, bMaintainGlobalTransform);		
	}

	if(InWeight > SMALL_NUMBER || bRemoveAllParents)
	{
		if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(InChild))
		{
			if(bMaintainGlobalTransform)
			{
				Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentGlobal);
				Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialGlobal);
				TransformElement->Pose.MarkDirty(ERigTransformType::CurrentLocal);
				TransformElement->Pose.MarkDirty(ERigTransformType::InitialLocal);
			}
			else
			{
				Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentLocal);
				Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialLocal);
				TransformElement->Pose.MarkDirty(ERigTransformType::CurrentGlobal);
				TransformElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
			}
		}

		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InChild))
		{
			Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
			Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::InitialLocal);
		}
	}

	FRigElementParentConstraint Constraint;
	Constraint.ParentElement = Cast<FRigTransformElement>(InParent);
	if(Constraint.ParentElement == nullptr)
	{
		return false;
	}
	Constraint.InitialWeight = InWeight;
	Constraint.Weight = InWeight;

	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InChild))
	{
		AddElementToDirty(Constraint.ParentElement, SingleParentElement);
		SingleParentElement->ParentElement = Constraint.ParentElement;

		if(Cast<FRigSocketElement>(SingleParentElement))
		{
			Hierarchy->SetRigElementKeyMetadata(SingleParentElement->GetKey(), FRigSocketElement::DesiredParentMetaName, Constraint.ParentElement->GetKey());
			Hierarchy->Notify(ERigHierarchyNotification::SocketDesiredParentChanged, SingleParentElement);
		}

		Hierarchy->IncrementTopologyVersion();

		if(!bMaintainGlobalTransform)
		{
			Hierarchy->PropagateDirtyFlags(SingleParentElement, true, true);
			Hierarchy->PropagateDirtyFlags(SingleParentElement, false, true);
		}

		Notify(ERigHierarchyNotification::ParentChanged, SingleParentElement);
		
		Hierarchy->EnsureCacheValidity();
		
		return true;
	}
	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		if(FRigControlElement* ControlElement = Cast<FRigControlElement>(InChild))
		{
			if(!ControlElement->Settings.DisplayName.IsNone())
			{
				ControlElement->Settings.DisplayName =
					Hierarchy->GetSafeNewDisplayName(
						InParent->GetKey(),
						ControlElement->Settings.DisplayName);
			}
		}
		
		AddElementToDirty(Constraint.ParentElement, MultiParentElement);

		const int32 ParentIndex = MultiParentElement->ParentConstraints.Add(Constraint);
		MultiParentElement->IndexLookup.Add(Constraint.ParentElement->GetKey(), ParentIndex);

		if(InWeight > SMALL_NUMBER)
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
			{
				ControlElement->Offset.MarkDirty(ERigTransformType::CurrentGlobal);  
				ControlElement->Offset.MarkDirty(ERigTransformType::InitialGlobal);
				ControlElement->Shape.MarkDirty(ERigTransformType::CurrentGlobal);  
				ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
			}
		}

		Hierarchy->IncrementTopologyVersion();

		if(InWeight > SMALL_NUMBER)
		{
			if(!bMaintainGlobalTransform)
			{
				Hierarchy->PropagateDirtyFlags(MultiParentElement, true, true);
				Hierarchy->PropagateDirtyFlags(MultiParentElement, false, true);
			}
		}

		if(FRigControlElement* ChildControlElement = Cast<FRigControlElement>(InChild))
		{
			const FTransform LocalTransform = Hierarchy->GetTransform(ChildControlElement, ERigTransformType::InitialLocal);
			Hierarchy->SetControlPreferredEulerAngles(ChildControlElement, LocalTransform, true);
			ChildControlElement->PreferredEulerAngles.Current = ChildControlElement->PreferredEulerAngles.Initial;
		}

		Notify(ERigHierarchyNotification::ParentChanged, MultiParentElement);

		Hierarchy->EnsureCacheValidity();
		
		return true;
	}
	
	return false;
}

bool URigHierarchyController::RemoveParent(FRigElementKey InChild, FRigElementKey InParent, bool bMaintainGlobalTransform, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Child = Hierarchy->Find(InChild);
	if(Child == nullptr)
	{
		ReportWarningf(TEXT("Cannot Remove Parent, Child '%s' not found."), *InChild.ToString());
		return false;
	}

	FRigBaseElement* Parent = Hierarchy->Find(InParent);
	if(Parent == nullptr)
	{
		ReportWarningf(TEXT("Cannot Remove Parent, Parent '%s' not found."), *InParent.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Remove Parent", "Remove Parent"));
		Hierarchy->Modify();
	}
#endif

	const bool bRemoved = RemoveParent(Child, Parent, bMaintainGlobalTransform);

#if WITH_EDITOR
	if(!bRemoved && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bRemoved && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.remove_parent(%s, %s, %s)"),
				*InChild.ToPythonString(),
				*InParent.ToPythonString(),
				(bMaintainGlobalTransform) ? TEXT("True") : TEXT("False")));
		}
	}
#endif
	
	return bRemoved;
}

bool URigHierarchyController::RemoveParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bMaintainGlobalTransform)
{
	if(InChild == nullptr || InParent == nullptr)
	{
		return false;
	}

	FRigTransformElement* ParentTransformElement = Cast<FRigTransformElement>(InParent);
	if(ParentTransformElement == nullptr)
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	// single parent children can't be parented multiple times
	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InChild))
	{
		if(SingleParentElement->ParentElement == ParentTransformElement)
		{
			if(bMaintainGlobalTransform)
			{
				Hierarchy->GetTransform(SingleParentElement, ERigTransformType::CurrentGlobal);
				Hierarchy->GetTransform(SingleParentElement, ERigTransformType::InitialGlobal);
				SingleParentElement->Pose.MarkDirty(ERigTransformType::CurrentLocal);
				SingleParentElement->Pose.MarkDirty(ERigTransformType::InitialLocal);
			}
			else
			{
				Hierarchy->GetTransform(SingleParentElement, ERigTransformType::CurrentLocal);
				Hierarchy->GetTransform(SingleParentElement, ERigTransformType::InitialLocal);
				SingleParentElement->Pose.MarkDirty(ERigTransformType::CurrentGlobal);
				SingleParentElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
			}

			const FRigElementKey PreviousParentKey = SingleParentElement->ParentElement->GetKey();
			Hierarchy->PreviousParentMap.FindOrAdd(SingleParentElement->GetKey()) = PreviousParentKey;
			
			// remove the previous parent
			SingleParentElement->ParentElement = nullptr;

			if(Cast<FRigSocketElement>(SingleParentElement))
			{
				Hierarchy->RemoveMetadata(SingleParentElement->GetKey(), FRigSocketElement::DesiredParentMetaName);
				Hierarchy->Notify(ERigHierarchyNotification::SocketDesiredParentChanged, SingleParentElement);
			}

			RemoveElementToDirty(InParent, SingleParentElement); 
			Hierarchy->IncrementTopologyVersion();

			if(!bMaintainGlobalTransform)
			{
				Hierarchy->PropagateDirtyFlags(SingleParentElement, true, true);
				Hierarchy->PropagateDirtyFlags(SingleParentElement, false, true);
			}

			Notify(ERigHierarchyNotification::ParentChanged, SingleParentElement);

			Hierarchy->EnsureCacheValidity();
			
			return true;
		}
	}

	// single parent children can't be parented multiple times
	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		int32 ParentIndex = INDEX_NONE;
		for(int32 ConstraintIndex = 0; ConstraintIndex < MultiParentElement->ParentConstraints.Num(); ConstraintIndex++)
		{
			if(MultiParentElement->ParentConstraints[ConstraintIndex].ParentElement == ParentTransformElement)
			{
				ParentIndex = ConstraintIndex;
				break;
			}
		}
				
		if(MultiParentElement->ParentConstraints.IsValidIndex(ParentIndex))
		{
			if(bMaintainGlobalTransform)
			{
				Hierarchy->GetTransform(MultiParentElement, ERigTransformType::CurrentGlobal);
				Hierarchy->GetTransform(MultiParentElement, ERigTransformType::InitialGlobal);
				MultiParentElement->Pose.MarkDirty(ERigTransformType::CurrentLocal);
				MultiParentElement->Pose.MarkDirty(ERigTransformType::InitialLocal);
			}
			else
			{
				Hierarchy->GetTransform(MultiParentElement, ERigTransformType::CurrentLocal);
				Hierarchy->GetTransform(MultiParentElement, ERigTransformType::InitialLocal);
				MultiParentElement->Pose.MarkDirty(ERigTransformType::CurrentGlobal);
				MultiParentElement->Pose.MarkDirty(ERigTransformType::InitialGlobal);
			}

			// remove the previous parent
			RemoveElementToDirty(InParent, MultiParentElement); 

			const FRigElementKey PreviousParentKey = MultiParentElement->ParentConstraints[ParentIndex].ParentElement->GetKey();
			Hierarchy->PreviousParentMap.FindOrAdd(MultiParentElement->GetKey()) = PreviousParentKey;

			MultiParentElement->ParentConstraints.RemoveAt(ParentIndex);
			MultiParentElement->IndexLookup.Remove(ParentTransformElement->GetKey());
			for(TPair<FRigElementKey, int32>& Pair : MultiParentElement->IndexLookup)
			{
				if(Pair.Value > ParentIndex)
				{
					Pair.Value--;
				}
			}

			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(MultiParentElement))
			{
				ControlElement->Offset.MarkDirty(ERigTransformType::CurrentGlobal);  
				ControlElement->Offset.MarkDirty(ERigTransformType::InitialGlobal);
				ControlElement->Shape.MarkDirty(ERigTransformType::CurrentGlobal);  
				ControlElement->Shape.MarkDirty(ERigTransformType::InitialGlobal);
			}

			Hierarchy->IncrementTopologyVersion();

			if(!bMaintainGlobalTransform)
			{
				Hierarchy->PropagateDirtyFlags(MultiParentElement, true, true);
				Hierarchy->PropagateDirtyFlags(MultiParentElement, false, true);
			}

			Notify(ERigHierarchyNotification::ParentChanged, MultiParentElement);

			Hierarchy->EnsureCacheValidity();
			
			return true;
		}
	}

	return false;
}

bool URigHierarchyController::RemoveAllParents(FRigElementKey InChild, bool bMaintainGlobalTransform, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Child = Hierarchy->Find(InChild);
	if(Child == nullptr)
	{
		ReportWarningf(TEXT("Cannot Remove All Parents, Child '%s' not found."), *InChild.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Remove Parent", "Remove Parent"));
		Hierarchy->Modify();
	}
#endif

	const bool bRemoved = RemoveAllParents(Child, bMaintainGlobalTransform);

#if WITH_EDITOR
	if(!bRemoved && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();
	
	if (bRemoved && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.remove_all_parents(%s, %s)"),
				*InChild.ToPythonString(),
				(bMaintainGlobalTransform) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	return bRemoved;
}

bool URigHierarchyController::RemoveAllParents(FRigBaseElement* InChild, bool bMaintainGlobalTransform)
{
	if(FRigSingleParentElement* SingleParentElement = Cast<FRigSingleParentElement>(InChild))
	{
		return RemoveParent(SingleParentElement, SingleParentElement->ParentElement, bMaintainGlobalTransform);
	}
	else if(FRigMultiParentElement* MultiParentElement = Cast<FRigMultiParentElement>(InChild))
	{
		bool bSuccess = true;

		FRigElementParentConstraintArray ParentConstraints = MultiParentElement->ParentConstraints;
		for(const FRigElementParentConstraint& ParentConstraint : ParentConstraints)
		{
			if(!RemoveParent(MultiParentElement, ParentConstraint.ParentElement, bMaintainGlobalTransform))
			{
				bSuccess = false;
			}
		}

		return bSuccess;
	}
	return false;
}

bool URigHierarchyController::SetParent(FRigElementKey InChild, FRigElementKey InParent, bool bMaintainGlobalTransform, bool bSetupUndo, bool bPrintPythonCommand)
{
	if(!IsValid())
	{
		return false;
	}

	URigHierarchy* Hierarchy = GetHierarchy();

	FRigBaseElement* Child = Hierarchy->Find(InChild);
	if(Child == nullptr)
	{
		ReportWarningf(TEXT("Cannot Set Parent, Child '%s' not found."), *InChild.ToString());
		return false;
	}

	FRigBaseElement* Parent = Hierarchy->Find(InParent);
	if(Parent == nullptr)
	{
		if(InChild.Type == ERigElementType::Socket)
		{
			Hierarchy->SetRigElementKeyMetadata(InChild, FRigSocketElement::DesiredParentMetaName, InParent);
			Hierarchy->Notify(ERigHierarchyNotification::SocketDesiredParentChanged, Child);
			return true;
		}
		ReportWarningf(TEXT("Cannot Set Parent, Parent '%s' not found."), *InParent.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if(bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("RigHierarchyController", "Set Parent", "Set Parent"));
		Hierarchy->Modify();
	}
#endif

	const bool bParentSet = SetParent(Child, Parent, bMaintainGlobalTransform);

#if WITH_EDITOR
	if(!bParentSet && TransactionPtr.IsValid())
	{
		TransactionPtr->Cancel();
	}
	TransactionPtr.Reset();

	if (bParentSet && bPrintPythonCommand && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.set_parent(%s, %s, %s)"),
				*InChild.ToPythonString(),
				*InParent.ToPythonString(),
				(bMaintainGlobalTransform) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	return bParentSet;
}

TArray<FRigElementKey> URigHierarchyController::DuplicateElements(TArray<FRigElementKey> InKeys, bool bSelectNewElements, bool bSetupUndo, bool bPrintPythonCommands)
{
	const FString Content = ExportToText(InKeys);
	TArray<FRigElementKey> Result = ImportFromText(Content, false, bSelectNewElements, bSetupUndo);

#if WITH_EDITOR
	if (!Result.IsEmpty() && bPrintPythonCommands && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			FString ArrayStr = TEXT("[");
			for (auto It = InKeys.CreateConstIterator(); It; ++It)
			{
				ArrayStr += It->ToPythonString();
				if (It.GetIndex() < InKeys.Num() - 1)
				{
					ArrayStr += TEXT(", ");
				}
			}
			ArrayStr += TEXT("]");		
		
			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.duplicate_elements(%s, %s)"),
				*ArrayStr,
				(bSelectNewElements) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	GetHierarchy()->EnsureCacheValidity();
	
	return Result;
}

TArray<FRigElementKey> URigHierarchyController::MirrorElements(TArray<FRigElementKey> InKeys, FRigVMMirrorSettings InSettings, bool bSelectNewElements, bool bSetupUndo, bool bPrintPythonCommands)
{
	URigHierarchy* Hierarchy = GetHierarchy();
	FRigHierarchyInteractionBracket InteractionBracket(Hierarchy);

	TArray<FRigElementKey> OriginalKeys = Hierarchy->SortKeys(InKeys);
	TArray<FRigElementKey> DuplicatedKeys = DuplicateElements(OriginalKeys, bSelectNewElements, bSetupUndo);

	if (DuplicatedKeys.Num() != OriginalKeys.Num())
	{
		return DuplicatedKeys;
	}

	for (int32 Index = 0; Index < OriginalKeys.Num(); Index++)
	{
		if (DuplicatedKeys[Index].Type != OriginalKeys[Index].Type)
		{
			return DuplicatedKeys;
		}
	}

	// mirror the transforms
	for (int32 Index = 0; Index < OriginalKeys.Num(); Index++)
	{
		FTransform GlobalTransform = Hierarchy->GetGlobalTransform(OriginalKeys[Index]);
		FTransform InitialTransform = Hierarchy->GetInitialGlobalTransform(OriginalKeys[Index]);

		// also mirror the offset, limits and shape transform
		if (OriginalKeys[Index].Type == ERigElementType::Control)
		{
			if(FRigControlElement* DuplicatedControlElement = Hierarchy->Find<FRigControlElement>(DuplicatedKeys[Index]))
			{
				TGuardValue<TArray<FRigControlLimitEnabled>> DisableLimits(DuplicatedControlElement->Settings.LimitEnabled, TArray<FRigControlLimitEnabled>());

				// mirror offset
				FTransform OriginalGlobalOffsetTransform = Hierarchy->GetGlobalControlOffsetTransform(OriginalKeys[Index]);
				FTransform ParentTransform = Hierarchy->GetParentTransform(DuplicatedKeys[Index]);
				FTransform OffsetTransform = InSettings.MirrorTransform(OriginalGlobalOffsetTransform).GetRelativeTransform(ParentTransform);
				Hierarchy->SetControlOffsetTransform(DuplicatedKeys[Index], OffsetTransform, true, false, true);
				Hierarchy->SetControlOffsetTransform(DuplicatedKeys[Index], OffsetTransform, false, false, true);

				// mirror limits
				FTransform DuplicatedGlobalOffsetTransform = Hierarchy->GetGlobalControlOffsetTransform(DuplicatedKeys[Index]);

				for (ERigControlValueType ValueType = ERigControlValueType::Minimum;
                    ValueType <= ERigControlValueType::Maximum;
                    ValueType = (ERigControlValueType)(uint8(ValueType) + 1)
                    )
				{
					const FRigControlValue LimitValue = Hierarchy->GetControlValue(DuplicatedKeys[Index], ValueType);
					const FTransform LocalLimitTransform = LimitValue.GetAsTransform(DuplicatedControlElement->Settings.ControlType, DuplicatedControlElement->Settings.PrimaryAxis);
					FTransform GlobalLimitTransform = LocalLimitTransform * OriginalGlobalOffsetTransform;
					FTransform DuplicatedLimitTransform = InSettings.MirrorTransform(GlobalLimitTransform).GetRelativeTransform(DuplicatedGlobalOffsetTransform);
					FRigControlValue DuplicatedValue;
					DuplicatedValue.SetFromTransform(DuplicatedLimitTransform, DuplicatedControlElement->Settings.ControlType, DuplicatedControlElement->Settings.PrimaryAxis);
					Hierarchy->SetControlValue(DuplicatedControlElement, DuplicatedValue, ValueType, false);
				}

				// we need to do this here to make sure that the limits don't apply ( the tguardvalue is still active within here )
				Hierarchy->SetGlobalTransform(DuplicatedKeys[Index], InSettings.MirrorTransform(GlobalTransform), true, false, true);
				Hierarchy->SetGlobalTransform(DuplicatedKeys[Index], InSettings.MirrorTransform(GlobalTransform), false, false, true);

				// mirror shape transform
				FTransform GlobalShapeTransform = Hierarchy->GetControlShapeTransform(DuplicatedControlElement, ERigTransformType::InitialLocal) * OriginalGlobalOffsetTransform;
				Hierarchy->SetControlShapeTransform(DuplicatedControlElement, InSettings.MirrorTransform(GlobalShapeTransform).GetRelativeTransform(DuplicatedGlobalOffsetTransform), ERigTransformType::InitialLocal, true);
				Hierarchy->SetControlShapeTransform(DuplicatedControlElement, InSettings.MirrorTransform(GlobalShapeTransform).GetRelativeTransform(DuplicatedGlobalOffsetTransform), ERigTransformType::CurrentLocal, true);
			}
		}
		else
		{
			Hierarchy->SetGlobalTransform(DuplicatedKeys[Index], InSettings.MirrorTransform(GlobalTransform), true, false, true);
			Hierarchy->SetGlobalTransform(DuplicatedKeys[Index], InSettings.MirrorTransform(GlobalTransform), false, false, true);
		}
	}

	// correct the names
	if (!InSettings.SearchString.IsEmpty() && !InSettings.ReplaceString.IsEmpty())
	{
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);
		
		for (int32 Index = 0; Index < DuplicatedKeys.Num(); Index++)
		{
			FName OldName = OriginalKeys[Index].Name;
			FString OldNameStr = OldName.ToString();
			FString NewNameStr = OldNameStr.Replace(*InSettings.SearchString, *InSettings.ReplaceString, ESearchCase::CaseSensitive);
			if (NewNameStr != OldNameStr)
			{
				Controller->RenameElement(DuplicatedKeys[Index], *NewNameStr, true);
			}
		}
	}

#if WITH_EDITOR
	if (!DuplicatedKeys.IsEmpty() && bPrintPythonCommands && !bSuspendPythonPrinting)
	{
		UBlueprint* Blueprint = GetTypedOuter<UBlueprint>();
		if (Blueprint)
		{
			FString ArrayStr = TEXT("[");
			for (auto It = InKeys.CreateConstIterator(); It; ++It)
			{
				ArrayStr += It->ToPythonString();
				if (It.GetIndex() < InKeys.Num() - 1)
				{
					ArrayStr += TEXT(", ");
				}
			}
			ArrayStr += TEXT("]");

			RigVMPythonUtils::Print(Blueprint->GetFName().ToString(),
				FString::Printf(TEXT("hierarchy_controller.mirror_elements(%s, unreal.RigMirrorSettings(%s, %s, '%s', '%s'), %s)"),
				*ArrayStr,
				*RigVMPythonUtils::EnumValueToPythonString<EAxis::Type>((int64)InSettings.MirrorAxis.GetValue()),
				*RigVMPythonUtils::EnumValueToPythonString<EAxis::Type>((int64)InSettings.AxisToFlip.GetValue()),
				*InSettings.SearchString,
				*InSettings.ReplaceString,
				(bSelectNewElements) ? TEXT("True") : TEXT("False")));
		}
	}
#endif

	Hierarchy->EnsureCacheValidity();
	
	return DuplicatedKeys;
}

bool URigHierarchyController::SetParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bMaintainGlobalTransform)
{
	if(InChild == nullptr || InParent == nullptr)
	{
		return false;
	}
	return AddParent(InChild, InParent, 1.f, bMaintainGlobalTransform, true);
}

void URigHierarchyController::AddElementToDirty(FRigBaseElement* InParent, FRigBaseElement* InElementToAdd, int32 InHierarchyDistance) const
{
	if(InParent == nullptr)
	{
		return;
	} 

	FRigTransformElement* ElementToAdd = Cast<FRigTransformElement>(InElementToAdd);
	if(ElementToAdd == nullptr)
	{
		return;
	}

	if(FRigTransformElement* TransformParent = Cast<FRigTransformElement>(InParent))
	{
		const FRigTransformElement::FElementToDirty ElementToDirty(ElementToAdd, InHierarchyDistance);
		TransformParent->ElementsToDirty.AddUnique(ElementToDirty);
	}
}

void URigHierarchyController::RemoveElementToDirty(FRigBaseElement* InParent, FRigBaseElement* InElementToRemove) const
{
	if(InParent == nullptr)
	{
		return;
	}

	FRigTransformElement* ElementToRemove = Cast<FRigTransformElement>(InElementToRemove);
	if(ElementToRemove == nullptr)
	{
		return;
	}

	if(FRigTransformElement* TransformParent = Cast<FRigTransformElement>(InParent))
	{
		TransformParent->ElementsToDirty.Remove(ElementToRemove);
	}
}

void URigHierarchyController::ReportWarning(const FString& InMessage) const
{
	if(!bReportWarningsAndErrors)
	{
		return;
	}

	if(LogFunction)
	{
		LogFunction(EMessageSeverity::Warning, InMessage);
		return;
	}

	FString Message = InMessage;
	if (const URigHierarchy* Hierarchy = GetHierarchy())
	{
		if (const UPackage* Package = Cast<UPackage>(Hierarchy->GetOutermost()))
		{
			Message = FString::Printf(TEXT("%s : %s"), *Package->GetPathName(), *InMessage);
		}
	}

	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Warning, *Message, *FString());
}

void URigHierarchyController::ReportError(const FString& InMessage) const
{
	if(!bReportWarningsAndErrors)
	{
		return;
	}

	if(LogFunction)
	{
		LogFunction(EMessageSeverity::Error, InMessage);
		return;
	}

	FString Message = InMessage;
	if (const URigHierarchy* Hierarchy = GetHierarchy())
	{
		if (const UPackage* Package = Cast<UPackage>(Hierarchy->GetOutermost()))
		{
			Message = FString::Printf(TEXT("%s : %s"), *Package->GetPathName(), *InMessage);
		}
	}

	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, *Message, *FString());
}

void URigHierarchyController::ReportAndNotifyError(const FString& InMessage) const
{
	if (!bReportWarningsAndErrors)
	{
		return;
	}

	ReportError(InMessage);

#if WITH_EDITOR
	FNotificationInfo Info(FText::FromString(InMessage));
	Info.bUseSuccessFailIcons = true;
	Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Warning"));
	Info.bFireAndForget = true;
	Info.bUseThrobber = true;
	// longer message needs more time to read
	Info.FadeOutDuration = FMath::Clamp(0.1f * InMessage.Len(), 5.0f, 20.0f);
	Info.ExpireDuration = Info.FadeOutDuration;
	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	if (NotificationPtr)
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
	}
#endif
}

