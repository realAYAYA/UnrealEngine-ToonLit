// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigConnectionRules.h"
#include "Rigs/RigHierarchyElements.h"
#include "Rigs/RigHierarchy.h"
#include "ModularRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigConnectionRules)

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRuleStash
////////////////////////////////////////////////////////////////////////////////

FRigConnectionRuleStash::FRigConnectionRuleStash()
{
}

FRigConnectionRuleStash::FRigConnectionRuleStash(const FRigConnectionRule* InRule)
{
	check(InRule);
	ScriptStructPath = InRule->GetScriptStruct()->GetPathName();
	InRule->GetScriptStruct()->ExportText(ExportedText, InRule, InRule, nullptr, PPF_None, nullptr);
}

void FRigConnectionRuleStash::Save(FArchive& Ar)
{
	Ar << ScriptStructPath;
	Ar << ExportedText;
}

void FRigConnectionRuleStash::Load(FArchive& Ar)
{
	Ar << ScriptStructPath;
	Ar << ExportedText;
}

bool FRigConnectionRuleStash::IsValid() const
{
	return !ScriptStructPath.IsEmpty() && !ExportedText.IsEmpty();
}

UScriptStruct* FRigConnectionRuleStash::GetScriptStruct() const
{
	if(!ScriptStructPath.IsEmpty())
	{
		return FindObject<UScriptStruct>(nullptr, *ScriptStructPath);
	}
	return nullptr;
}

TSharedPtr<FStructOnScope> FRigConnectionRuleStash::Get() const
{
	class FErrorPipe : public FOutputDevice
	{
	public:

		int32 NumErrors;

		FErrorPipe()
			: FOutputDevice()
			, NumErrors(0)
		{
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			NumErrors++;
		}
	};

	UScriptStruct* ScriptStruct = GetScriptStruct();
	check(ScriptStruct);
	TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(ScriptStruct));
	FErrorPipe ErrorPipe;
	ScriptStruct->ImportText(*ExportedText, StructOnScope->GetStructMemory(), nullptr, PPF_None, &ErrorPipe, ScriptStruct->GetName());
	return StructOnScope;
}

const FRigConnectionRule* FRigConnectionRuleStash::Get(TSharedPtr<FStructOnScope>& InOutStorage) const
{
	InOutStorage = Get();
	if(InOutStorage.IsValid() && InOutStorage->IsValid())
	{
		check(InOutStorage->GetStruct()->IsChildOf(FRigConnectionRule::StaticStruct()));
		return reinterpret_cast<const FRigConnectionRule*>(InOutStorage->GetStructMemory());
	}
	return nullptr;
}

bool FRigConnectionRuleStash::operator==(const FRigConnectionRuleStash& InOther) const
{
	return ScriptStructPath.Equals(InOther.ScriptStructPath, ESearchCase::CaseSensitive) &&
		ExportedText.Equals(InOther.ExportedText, ESearchCase::CaseSensitive);
}

uint32 GetTypeHash(const FRigConnectionRuleStash& InRuleStash)
{
	return HashCombine(GetTypeHash(InRuleStash.ScriptStructPath), GetTypeHash(InRuleStash.ExportedText));
}

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRuleInput
////////////////////////////////////////////////////////////////////////////////

const FRigConnectorElement* FRigConnectionRuleInput::FindPrimaryConnector(FText* OutErrorMessage) const
{
	check(Hierarchy);
	check(Module);

	const FName ModuleNameSpace = *Module->GetNamespace();

	const FRigConnectorElement* PrimaryConnector = nullptr;
	Hierarchy->ForEach<FRigConnectorElement>(
		[this, ModuleNameSpace, &PrimaryConnector](FRigConnectorElement* Connector) -> bool
		{
			if(Connector->IsPrimary())
			{
				const FName ConnectorNameSpace = Hierarchy->GetNameSpaceFName(Connector->GetKey());
				if(!ConnectorNameSpace.IsNone() && ConnectorNameSpace.IsEqual(ModuleNameSpace, ENameCase::CaseSensitive))
				{
					PrimaryConnector = Connector;
					return false; // stop the search
				}
			}
			return true; // continue the search
		}
	);

	if(PrimaryConnector == nullptr)
	{
		if(OutErrorMessage)
		{
			static constexpr TCHAR Format[] = TEXT("No primary connector found for module '%s'.");
			*OutErrorMessage = FText::FromString(FString::Printf(Format, *Module->GetPath()));
		}
	}

	return PrimaryConnector;
}

TArray<const FRigConnectorElement*> FRigConnectionRuleInput::FindSecondaryConnectors(bool bOptional, FText* OutErrorMessage) const
{
	check(Hierarchy);
	check(Module);

	const FName ModuleNameSpace = *Module->GetNamespace();

	TArray<const FRigConnectorElement*> SecondaryConnectors;
	Hierarchy->ForEach<FRigConnectorElement>(
		[this, bOptional, ModuleNameSpace, &SecondaryConnectors](const FRigConnectorElement* Connector) -> bool
		{
			if(Connector->IsSecondary() && Connector->IsOptional() == bOptional)
			{
				const FName ConnectorNameSpace = Hierarchy->GetNameSpaceFName(Connector->GetKey());
				if(!ConnectorNameSpace.IsNone() && ConnectorNameSpace.IsEqual(ModuleNameSpace, ENameCase::CaseSensitive))
				{
					SecondaryConnectors.Add(Connector);
				}
			}
			return true; // continue the search
		}
	);

	return SecondaryConnectors;
}

const FRigTransformElement* FRigConnectionRuleInput::ResolveConnector(const FRigConnectorElement* InConnector, FText* OutErrorMessage) const
{
	check(Redirector);

	if(const FCachedRigElement* Target = Redirector->Find(InConnector->GetKey()))
	{
		return Cast<FRigTransformElement>(Target->GetElement());
	}

	if(OutErrorMessage)
	{
		static constexpr TCHAR Format[] = TEXT("Resolved target not found for connector '%s'.");
		*OutErrorMessage = FText::FromString(FString::Printf(Format, *InConnector->GetName()));
	}
	return nullptr;
}

const FRigTransformElement* FRigConnectionRuleInput::ResolvePrimaryConnector(FText* OutErrorMessage) const
{
	if(const FRigConnectorElement* Connector = FindPrimaryConnector(OutErrorMessage))
	{
		return ResolveConnector(Connector, OutErrorMessage);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// FRigConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetInvalidTarget(FText());
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigAndConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigAndConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();
	
	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			Result = Rule->Resolve(InTarget, InRuleInput);
			if(!Result.IsValid())
			{
				return Result;
			}
		}
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigOrConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigOrConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();
	
	TSharedPtr<FStructOnScope> Storage;
	for(const FRigConnectionRuleStash& ChildRule : ChildRules)
	{
		if(const FRigConnectionRule* Rule = ChildRule.Get(Storage))
		{
			Result = Rule->Resolve(InTarget, InRuleInput);
			if(Result.IsValid())
			{
				return Result;
			}
		}
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTypeConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigTypeConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();

	if(!InTarget->GetKey().IsTypeOf(ElementType))
	{
		const FString ExpectedType = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ElementType).ToString();
		static constexpr TCHAR Format[] = TEXT("Element '%s' is not of the expected type (%s).");
		Result.SetInvalidTarget(FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *ExpectedType)));
	}
	
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigTagConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigTagConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();

	if(!InRuleInput.GetHierarchy()->HasTag(InTarget->GetKey(), Tag))
	{
		static constexpr TCHAR Format[] = TEXT("Element '%s' does not contain tag '%s'.");
		Result.SetInvalidTarget(FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *Tag.ToString())));
	}
	
	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigChildOfPrimaryConnectionRule
////////////////////////////////////////////////////////////////////////////////

FRigElementResolveResult FRigChildOfPrimaryConnectionRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result(InTarget->GetKey());
	Result.SetPossibleTarget();

	// find the primary resolved target
	FText ErrorMessage;
	const FRigTransformElement* PrimaryTarget = InRuleInput.ResolvePrimaryConnector(&ErrorMessage);
	if(PrimaryTarget == nullptr)
	{
		Result.SetInvalidTarget(ErrorMessage);
		return Result;
	}

	static constexpr TCHAR IsNotAChildOfFormat[] = TEXT("Target '%s' is not a child of the primary.");
	static constexpr TCHAR IsAlreadyUsedForPrimaryFormat[] = TEXT("Target '%s' is already used for the primary.");
	if(InTarget == PrimaryTarget)
	{
		Result.SetInvalidTarget(FText::FromString(FString::Printf(IsAlreadyUsedForPrimaryFormat, *InTarget->GetKey().ToString())));
		return Result;
	}

	// for sockets we use the parent for resolve
	if(PrimaryTarget->GetType() == ERigElementType::Socket)
	{
		if(const FRigTransformElement* FirstParent =
			Cast<FRigTransformElement>(InRuleInput.GetHierarchy()->GetFirstParent(PrimaryTarget)))
		{
			PrimaryTarget = FirstParent;

			if(InTarget == PrimaryTarget)
			{
				Result.SetInvalidTarget(FText::FromString(FString::Printf(IsNotAChildOfFormat, *InTarget->GetKey().ToString())));
				return Result;
			}
		}
	}

	static const URigHierarchy::TElementDependencyMap EmptyDependencyMap;
	if(!InRuleInput.GetHierarchy()->IsParentedTo(
		const_cast<FRigBaseElement*>(InTarget), const_cast<FRigTransformElement*>(PrimaryTarget), EmptyDependencyMap))
	{
		Result.SetInvalidTarget(FText::FromString(FString::Printf(IsNotAChildOfFormat, *InTarget->GetKey().ToString())));
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////
// FRigOnChainRule
////////////////////////////////////////////////////////////////////////////////

/*
FRigElementResolveResult FRigOnChainRule::Resolve(const FRigBaseElement* InTarget, const FRigConnectionRuleInput& InRuleInput) const
{
	FRigElementResolveResult Result;
	Result.State = ERigElementResolveState::PossibleTarget;

	if(!InHierarchy->HasTag(InTarget->GetKey(), Tag))
	{
		static constexpr TCHAR Format[] = TEXT("Element '%s' does not contain tag '%s'.");
		Result.State = ERigElementResolveState::InvalidTarget;
		Result.Message = FText::FromString(FString::Printf(Format, *InTarget->GetKey().ToString(), *Tag.ToString()));
	}
	
	return Result;
}
*/