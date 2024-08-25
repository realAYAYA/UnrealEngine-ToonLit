// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnitContext.h"
#include "ControlRig.h"
#include "ModularRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnitContext)

FName FControlRigExecuteContext::AddRigModuleNameSpace(const FName& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	return *AddRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::AddRigModuleNameSpace(const FString& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	check(!RigModuleNameSpace.IsEmpty());
	return RigModuleNameSpace + InName;
}

FName FControlRigExecuteContext::RemoveRigModuleNameSpace(const FName& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	return *RemoveRigModuleNameSpace(InName.ToString());
}

FString FControlRigExecuteContext::RemoveRigModuleNameSpace(const FString& InName) const
{
	if(IsRigModule())
	{
		return InName;
	}
	check(!RigModuleNameSpace.IsEmpty());

	if(InName.StartsWith(RigModuleNameSpace, ESearchCase::CaseSensitive))
	{
		return InName.Mid(RigModuleNameSpace.Len());
	}
	return InName;
}

FString FControlRigExecuteContext::GetElementNameSpace(ERigMetaDataNameSpace InNameSpaceType) const
{
	if(IsRigModule())
	{
		// prefix the meta data name with the namespace to allow modules to store their
		// metadata in a way that doesn't collide with other modules' metadata.
		switch(InNameSpaceType)
		{
			case ERigMetaDataNameSpace::Self:
			{
				return GetRigModuleNameSpace();
			}
			case ERigMetaDataNameSpace::Parent:
			{
				check(GetRigModuleNameSpace().EndsWith(UModularRig::NamespaceSeparator));
				FString ParentNameSpace;
				URigHierarchy::SplitNameSpace(GetRigModuleNameSpace().LeftChop(1), &ParentNameSpace, nullptr, true);
				if(ParentNameSpace.IsEmpty())
				{
					return GetRigModuleNameSpace();
				}
				return ParentNameSpace + UModularRig::NamespaceSeparator;
			}
			case ERigMetaDataNameSpace::Root:
			{
				FString RootNameSpace;
				URigHierarchy::SplitNameSpace(GetRigModuleNameSpace(), &RootNameSpace, nullptr, false);
				if(RootNameSpace.IsEmpty())
				{
					return GetRigModuleNameSpace();
				}
				return RootNameSpace + UModularRig::NamespaceSeparator;
			}
			default:
			{
				break;
			}
		}
	}
	else
	{
		// prefix the meta data with some mockup namespaces
		// so we can test this even without a module present.
		switch(InNameSpaceType)
		{
			case ERigMetaDataNameSpace::Self:
			{
				// if we are storing on self and this is not a modular
				// rig let's just not use a namespace.
				break;
			}
			case ERigMetaDataNameSpace::Parent:
			{
				static const FString ParentNameSpace = TEXT("Parent:");
				return ParentNameSpace;
			}
			case ERigMetaDataNameSpace::Root:
			{
				static const FString RootNameSpace = TEXT("Root:");
				return RootNameSpace;
			}
			default:
			{
				break;
			}
		}
	}

	return FString();
}

const FRigModuleInstance* FControlRigExecuteContext::GetRigModuleInstance(ERigMetaDataNameSpace InNameSpaceType) const
{
	if(RigModuleInstance)
	{
		switch(InNameSpaceType)
		{
			case ERigMetaDataNameSpace::Self:
			{
				return RigModuleInstance;
			}
			case ERigMetaDataNameSpace::Parent:
			{
				return RigModuleInstance->GetParentModule();
			}
			case ERigMetaDataNameSpace::Root:
			{
				return RigModuleInstance->GetRootModule();
			}
			case ERigMetaDataNameSpace::None:
			default:
			{
				break;
			}
		}
		
	}
	return nullptr;
}

FName FControlRigExecuteContext::AdaptMetadataName(ERigMetaDataNameSpace InNameSpaceType, const FName& InMetadataName) const
{
	// only if we are within a rig module let's adapt the meta data name
	const bool bUseNameSpace = InNameSpaceType != ERigMetaDataNameSpace::None;
	if(bUseNameSpace && !InMetadataName.IsNone())
	{
		// if the metadata name already contains a namespace - we are just going
		// to use it as is. this means that modules have access to other module's metadata,
		// and that's ok. the user will require the full path to it anyway so it is a
		// conscious user decision.
		const FString MetadataNameString = InMetadataName.ToString();
		int32 Index = INDEX_NONE;
		if(MetadataNameString.FindChar(TEXT(':'), Index))
		{
			return InMetadataName;
		}

		if(IsRigModule())
		{
			// prefix the meta data name with the namespace to allow modules to store their
			// metadata in a way that doesn't collide with other modules' metadata.
			switch(InNameSpaceType)
			{
				case ERigMetaDataNameSpace::Self:
				case ERigMetaDataNameSpace::Parent:
				case ERigMetaDataNameSpace::Root:
				{
					return *URigHierarchy::JoinNameSpace(GetElementNameSpace(InNameSpaceType), MetadataNameString);
				}
				default:
				{
					break;
				}
			}
		}
		else
		{
			// prefix the meta data with some mockup namespaces
			// so we can test this even without a module present.
			switch(InNameSpaceType)
			{
				case ERigMetaDataNameSpace::Self:
				{
					// if we are storing on self and this is not a modular
					// rig let's just not use a namespace.
					break;
				}
				case ERigMetaDataNameSpace::Parent:
				case ERigMetaDataNameSpace::Root:
				{
					return *URigHierarchy::JoinNameSpace(GetElementNameSpace(InNameSpaceType), MetadataNameString);
				}
				default:
				{
					break;
				}
			}
		}
	}
	return InMetadataName;
}

FControlRigExecuteContextRigModuleGuard::FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const UControlRig* InControlRig)
	: Context(InContext)
	, PreviousRigModuleNameSpace(InContext.RigModuleNameSpace)
	, PreviousRigModuleNameSpaceHash(InContext.RigModuleNameSpaceHash)
{
	Context.RigModuleNameSpace = InControlRig->GetRigModuleNameSpace();
	Context.RigModuleNameSpaceHash = GetTypeHash(Context.RigModuleNameSpace);
}

FControlRigExecuteContextRigModuleGuard::FControlRigExecuteContextRigModuleGuard(FControlRigExecuteContext& InContext, const FString& InNewModuleNameSpace)
	: Context(InContext)
	, PreviousRigModuleNameSpace(InContext.RigModuleNameSpace)
	, PreviousRigModuleNameSpaceHash(InContext.RigModuleNameSpaceHash)
{
	Context.RigModuleNameSpace = InNewModuleNameSpace;
	Context.RigModuleNameSpaceHash = GetTypeHash(Context.RigModuleNameSpace);
}

FControlRigExecuteContextRigModuleGuard::~FControlRigExecuteContextRigModuleGuard()
{
	Context.RigModuleNameSpace = PreviousRigModuleNameSpace;
	Context.RigModuleNameSpaceHash = PreviousRigModuleNameSpaceHash; 
}
