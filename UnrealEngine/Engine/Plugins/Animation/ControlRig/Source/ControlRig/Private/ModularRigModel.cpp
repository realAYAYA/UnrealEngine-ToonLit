// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigModel.h"
#include "ModularRigController.h"
#include "ModularRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigModel)

FString FRigModuleReference::GetShortName() const
{
	if(!ShortName.IsEmpty())
	{
		return ShortName;
	}
	return GetPath();
}

FString FRigModuleReference::GetPath() const
{
	if (ParentPath.IsEmpty())
	{
		return Name.ToString();
	}
	return URigHierarchy::JoinNameSpace(ParentPath, Name.ToString());
}

FString FRigModuleReference::GetNamespace() const
{
	return GetPath() + UModularRig::NamespaceSeparator;
}

const FRigConnectorElement* FRigModuleReference::FindPrimaryConnector(const URigHierarchy* InHierarchy) const
{
	if(InHierarchy)
	{
		const FString MyModulePath = GetPath();
		const TArray<FRigConnectorElement*> AllConnectors = InHierarchy->GetConnectors();
		for(const FRigConnectorElement* Connector : AllConnectors)
		{
			if(Connector->IsPrimary())
			{
				const FString ModulePath = InHierarchy->GetModulePath(Connector->GetKey());
				if(!ModulePath.IsEmpty())
				{
					if(ModulePath.Equals(MyModulePath, ESearchCase::CaseSensitive))
					{
						return Connector;
					}
				}
			}
		}
	}
	return nullptr;
}

TArray<const FRigConnectorElement*> FRigModuleReference::FindConnectors(const URigHierarchy* InHierarchy) const
{
	TArray<const FRigConnectorElement*> Connectors;
	if(InHierarchy)
	{
		const FString MyModulePath = GetPath();
		const TArray<FRigConnectorElement*> AllConnectors = InHierarchy->GetConnectors();
		for(const FRigConnectorElement* Connector : AllConnectors)
		{
			const FString ModulePath = InHierarchy->GetModulePath(Connector->GetKey());
			if(!ModulePath.IsEmpty())
			{
				if(ModulePath.Equals(MyModulePath, ESearchCase::CaseSensitive))
				{
					Connectors.Add(Connector);
				}
			}
		}
	}
	return Connectors;
}

TMap<FRigElementKey, FRigElementKey> FModularRigConnections::GetModuleConnectionMap(const FString& InModulePath) const
{
	TMap<FRigElementKey, FRigElementKey> Result;
	for (const FModularRigSingleConnection& Connection : ConnectionList)
	{
		FString Path, Name;
		URigHierarchy::SplitNameSpace(Connection.Connector.Name.ToString(), &Path, &Name);

		// Exactly the same path (do not return connectors from child modules)
		if (Path == InModulePath)
		{
			Result.Add(FRigElementKey(*Name, ERigElementType::Connector), Connection.Target);
		}
	}
	return Result;
}

void FModularRigModel::PatchModelsOnLoad()
{
	if (Connections.IsEmpty())
	{
		ForEachModule([this](const FRigModuleReference* Module) -> bool
		{
			FString ModuleNamespace = Module->GetNamespace();
			for (const TTuple<FRigElementKey, FRigElementKey>& Connection : Module->Connections_DEPRECATED)
			{
				const FString ConnectorPath = FString::Printf(TEXT("%s%s"), *ModuleNamespace, *Connection.Key.Name.ToString());
				FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
				Connections.AddConnection(ConnectorKey, Connection.Value);
			}
			return true;
		});
	}

	Connections.UpdateFromConnectionList();
}

UModularRigController* FModularRigModel::GetController(bool bCreateIfNeeded)
{
	if (bCreateIfNeeded && Controller == nullptr)
	{
		const FName SafeControllerName = *FString::Printf(TEXT("%s_ModularRig_Controller"), *GetOuter()->GetPathName());
		UModularRigController* NewController = NewObject<UModularRigController>(GetOuter(), UModularRigController::StaticClass(), SafeControllerName);
		NewController->SetModel(this);
		Controller = NewController;
	}
	return Cast<UModularRigController>(Controller);
}

void FModularRigModel::SetOuterClientHost(UObject* InOuterClientHost)
{
	OuterClientHost = InOuterClientHost;
}

void FModularRigModel::UpdateCachedChildren()
{
	TMap<FString, FRigModuleReference*> PathToModule;
	for (FRigModuleReference& Module : Modules)
	{
		Module.CachedChildren.Reset();
		PathToModule.Add(Module.GetPath(), &Module);
	}
	
	RootModules.Reset();
	for (FRigModuleReference& Module : Modules)
	{
		if (Module.ParentPath.IsEmpty())
		{
			RootModules.Add(&Module);
		}
		else
		{
			if (FRigModuleReference** ParentModule = PathToModule.Find(Module.ParentPath))
			{
				(*ParentModule)->CachedChildren.Add(&Module);
			}
		}
	}
}

FRigModuleReference* FModularRigModel::FindModule(const FString& InPath)
{
	const TArray<FRigModuleReference*>* Children = &RootModules;

	FString Left = InPath, Right;
	while (URigHierarchy::SplitNameSpace(Left, &Left, &Right, false))
	{
		FRigModuleReference* const * Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name.ToString() == Left;
		});
		
		if (!Child)
		{
			return nullptr;
		}
		
		Children = &(*Child)->CachedChildren;
		Left = Right;
	}

	FRigModuleReference* const * Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name.ToString() == Left;
		});

	if (!Child)
	{
		return nullptr;
	}
	return *Child;
}

const FRigModuleReference* FModularRigModel::FindModule(const FString& InPath) const
{
	FRigModuleReference* Module = const_cast<FModularRigModel*>(this)->FindModule(InPath);
	return const_cast<FRigModuleReference*>(Module);
}

FRigModuleReference* FModularRigModel::GetParentModule(const FString& InPath)
{
	const FString ParentPath = GetParentPath(InPath);
	if(!ParentPath.IsEmpty())
	{
		return FindModule(ParentPath);
	}
	return nullptr;
}

const FRigModuleReference* FModularRigModel::GetParentModule(const FString& InPath) const
{
	FRigModuleReference* Module = const_cast<FModularRigModel*>(this)->GetParentModule(InPath);
	return const_cast<FRigModuleReference*>(Module);
}

FRigModuleReference* FModularRigModel::GetParentModule(const FRigModuleReference* InChildModule)
{
	if(InChildModule)
	{
		return FindModule(InChildModule->ParentPath);
	}
	return nullptr;
}

const FRigModuleReference* FModularRigModel::GetParentModule(const FRigModuleReference* InChildModule) const
{
	FRigModuleReference* Module = const_cast<FModularRigModel*>(this)->GetParentModule(InChildModule);
	return const_cast<FRigModuleReference*>(Module);
}

bool FModularRigModel::IsModuleParentedTo(const FString& InChildModulePath, const FString& InParentModulePath) const
{
	const FRigModuleReference* ChildModule = FindModule(InChildModulePath);
	const FRigModuleReference* ParentModule = FindModule(InParentModulePath);
	if(ChildModule == nullptr || ParentModule == nullptr)
	{
		return false;
	}
	return IsModuleParentedTo(ChildModule, ParentModule);
}

bool FModularRigModel::IsModuleParentedTo(const FRigModuleReference* InChildModule, const FRigModuleReference* InParentModule) const
{
	if(InChildModule == nullptr || InParentModule == nullptr)
	{
		return false;
	}
	if(InChildModule == InParentModule)
	{
		return false;
	}
	
	while(InChildModule)
	{
		if(InChildModule == InParentModule)
		{
			return true;
		}
		InChildModule = GetParentModule(InChildModule);
	}

	return false;
}

FString FModularRigModel::GetParentPath(const FString& InPath) const
{
	if (const FRigModuleReference* Element = FindModule(InPath))
	{
		return Element->ParentPath;
	}
	return FString();
}

void FModularRigModel::ForEachModule(TFunction<bool(const FRigModuleReference*)> PerModule) const
{
	TArray<FRigModuleReference*> ModuleInstances = RootModules;
	for (int32 Index=0; Index < ModuleInstances.Num(); ++Index)
	{
		if (!PerModule(ModuleInstances[Index]))
		{
			break;
		}
		ModuleInstances.Append(ModuleInstances[Index]->CachedChildren);
	}
}

TArray<FString> FModularRigModel::SortPaths(const TArray<FString>& InPaths) const
{
	TArray<FString> Result;
	ForEachModule([InPaths, &Result](const FRigModuleReference* Element) -> bool
	{
		const FString& Path = Element->GetPath();
		if(InPaths.Contains(Path))
		{
			Result.AddUnique(Path);
		}
		return true;
	});
	return Result;
}
