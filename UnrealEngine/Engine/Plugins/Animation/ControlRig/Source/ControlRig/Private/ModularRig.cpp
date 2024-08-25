// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRig.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "ControlRigObjectBinding.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRigComponent.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/Modules/RigUnit_ConnectorExecution.h"

#define LOCTEXT_NAMESPACE "ModularRig"

const FString UModularRig::NamespaceSeparator = TEXT(":");

////////////////////////////////////////////////////////////////////////////////
// FModuleInstanceHandle
////////////////////////////////////////////////////////////////////////////////

FModuleInstanceHandle::FModuleInstanceHandle(const UModularRig* InModularRig, const FString& InPath)
: ModularRig(InModularRig)
, Path(InPath)
{
}

FModuleInstanceHandle::FModuleInstanceHandle(const UModularRig* InModularRig, const FRigModuleInstance* InModule)
: ModularRig(InModularRig)
, Path(InModule->GetPath())
{
}

const FRigModuleInstance* FModuleInstanceHandle::Get() const
{
	if(const UModularRig* ResolvedRig = ModularRig.Get())
	{
		return ResolvedRig->FindModule(Path);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// UModularRig
////////////////////////////////////////////////////////////////////////////////

UModularRig::UModularRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UModularRig::OnObjectsReplaced);
#endif
}

void UModularRig::BeginDestroy()
{
	Super::BeginDestroy();
	
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.RemoveAll(this);
#endif
}

FString FRigModuleInstance::GetShortName() const
{
	if(const FRigModuleReference* ModuleReference = GetModuleReference())
	{
		const FString ShortName = ModuleReference->GetShortName();
		if(!ShortName.IsEmpty())
		{
			return ShortName;
		}
	}
	return Name.ToString();
}

FString FRigModuleInstance::GetPath() const
{
	if (!IsRootModule())
	{
		return URigHierarchy::JoinNameSpace(ParentPath, Name.ToString()); 
	}
	return Name.ToString();
}

FString FRigModuleInstance::GetNamespace() const
{
	return FString::Printf(TEXT("%s:"), *GetPath());
}

UControlRig* FRigModuleInstance::GetRig() const
{
	if(IsValid(RigPtr))
	{
		return RigPtr;
	}

	// reset the cache if it is not valid
	return RigPtr = nullptr;
}

void FRigModuleInstance::SetRig(UControlRig* InRig)
{
	UControlRig* PreviousRig = GetRig();
	if(PreviousRig && (PreviousRig != InRig))
	{
		UModularRig::DiscardModuleRig(PreviousRig);
	}

	// update the cache
	RigPtr = InRig;
}

bool FRigModuleInstance::ContainsRig(const UControlRig* InRig) const
{
	if(InRig == nullptr)
	{
		return false;
	}
	if(RigPtr == InRig)
	{
		return true;
	}
	return false;
}

const FRigModuleReference* FRigModuleInstance::GetModuleReference() const
{
	if(const UControlRig* Rig = GetRig())
	{
		if(const UModularRig* ModularRig = Cast<UModularRig>(Rig->GetParentRig()))
		{
			const FModularRigModel& Model = ModularRig->GetModularRigModel();
			return Model.FindModule(GetPath());
		}
	}
	return nullptr;
}

const FRigModuleInstance* FRigModuleInstance::GetParentModule() const
{
	if(ParentPath.IsEmpty())
	{
		return this;
	}
	if(const UControlRig* Rig = GetRig())
	{
		if(const UModularRig* ModularRig = Cast<UModularRig>(Rig->GetParentRig()))
		{
			return ModularRig->FindModule(ParentPath);
		}
	}
	return nullptr;
}

const FRigModuleInstance* FRigModuleInstance::GetRootModule() const
{
	if(ParentPath.IsEmpty())
	{
		return this;
	}
	if(const UControlRig* Rig = GetRig())
	{
		if(const UModularRig* ModularRig = Cast<UModularRig>(Rig->GetParentRig()))
		{
			FString RootPath = ParentPath;
			(void)URigHierarchy::SplitNameSpace(ParentPath, &RootPath, nullptr, false);
			return ModularRig->FindModule(RootPath);
		}
	}
	return nullptr;
}

const FRigConnectorElement* FRigModuleInstance::FindPrimaryConnector() const
{
	if(PrimaryConnector)
	{
		return PrimaryConnector;
	}
	
	if(const UControlRig* Rig = GetRig())
	{
		if(const URigHierarchy* Hierarchy = Rig->GetHierarchy())
		{
			const FString MyModulePath = GetPath();
			const TArray<FRigConnectorElement*> AllConnectors = Hierarchy->GetConnectors();
			for(const FRigConnectorElement* Connector : AllConnectors)
			{
				if(Connector->IsPrimary())
				{
					const FString ModulePath = Hierarchy->GetModulePath(Connector->GetKey());
					if(!ModulePath.IsEmpty())
					{
						if(ModulePath.Equals(MyModulePath, ESearchCase::CaseSensitive))
						{
							PrimaryConnector = Connector;
							return PrimaryConnector;
						}
					}
				}
			}
		}
	}
	return nullptr;
}

TArray<const FRigConnectorElement*> FRigModuleInstance::FindConnectors() const
{
	TArray<const FRigConnectorElement*> Connectors;
	if(const UControlRig* Rig = GetRig())
	{
		if(const URigHierarchy* Hierarchy = Rig->GetHierarchy())
		{
			const FString MyModulePath = GetPath();
			const TArray<FRigConnectorElement*> AllConnectors = Hierarchy->GetConnectors();
			for(const FRigConnectorElement* Connector : AllConnectors)
			{
				const FString ModulePath = Hierarchy->GetModulePath(Connector->GetKey());
				if(!ModulePath.IsEmpty())
				{
					if(ModulePath.Equals(MyModulePath, ESearchCase::CaseSensitive))
					{
						Connectors.Add(Connector);
					}
				}
			}
		}
	}
	return Connectors;
}

bool FRigModuleInstance::IsRootModule() const
{
	return ParentPath.IsEmpty();
}

void UModularRig::PostInitProperties()
{
	Super::PostInitProperties();

	ModularRigModel.UpdateCachedChildren();
	ModularRigModel.Connections.UpdateFromConnectionList();
	UpdateSupportedEvents();
}

void UModularRig::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
		ModularRigModel.UpdateCachedChildren();
		ModularRigModel.Connections.UpdateFromConnectionList();
	}
}

void UModularRig::PostLoad()
{
	Super::PostLoad();
	ModularRigModel.UpdateCachedChildren();
	ModularRigModel.Connections.UpdateFromConnectionList();
	ResetShortestDisplayPathCache();
}

void UModularRig::InitializeVMs(bool bRequestInit)
{
	URigVMHost::Initialize(bRequestInit);
	ForEachModule([bRequestInit](const FRigModuleInstance* Module) -> bool
	{
		if (UControlRig* ModuleRig = Module->GetRig())
		{
			ModuleRig->InitializeVMs(bRequestInit);
		}
		return true;
	});
}

bool UModularRig::InitializeVMs(const FName& InEventName)
{
	URigVMHost::InitializeVM(InEventName);
	UpdateModuleHierarchyFromCDO();

	ForEachModule([InEventName](const FRigModuleInstance* Module) -> bool
	{
		if (UControlRig* ModuleRig = Module->GetRig())
		{
			ModuleRig->InitializeVMs(InEventName);
		}
		return true;
	});
	return true;
}

void UModularRig::InitializeFromCDO()
{
	Super::InitializeFromCDO();
	UpdateModuleHierarchyFromCDO();
}

void UModularRig::UpdateModuleHierarchyFromCDO()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// keep the previous rigs around
		check(PreviousModuleRigs.IsEmpty());
		for (const FRigModuleInstance& Module : Modules)
        {
			if(UControlRig* ModuleRig = Module.GetRig())
			{
				if(IsValid(ModuleRig))
				{
					PreviousModuleRigs.Add(Module.GetPath(), ModuleRig);
				}
			}
        }

		// don't destroy the rigs when resetting
		ResetModules(false);

		// the CDO owns the model - when we ask for the model we'll always
		// get the model from the CDO. we'll now add UObject module instances
		// for each module (data only) reference in the model.
		// Note: The CDO does not contain any UObject module instances itself.
		const FModularRigModel& Model = GetModularRigModel();
		Model.ForEachModule([this, Model](const FRigModuleReference* InModuleReference) -> bool
		{
			check(InModuleReference);
			if (IsInGameThread() && !InModuleReference->Class.IsValid())
			{
				(void)InModuleReference->Class.LoadSynchronous();
			}
			if (InModuleReference->Class.IsValid())
			{
				(void)AddModuleInstance(
					InModuleReference->Name,
					InModuleReference->Class.Get(),
					FindModule(InModuleReference->ParentPath),
					Model.Connections.GetModuleConnectionMap(InModuleReference->GetPath()),
					InModuleReference->ConfigValues);
			}

			// continue to the next module
			return true;
		});

		// discard any remaining rigs
		for(const TPair<FString, UControlRig*>& Pair : PreviousModuleRigs)
		{
			DiscardModuleRig(Pair.Value);
		}
		PreviousModuleRigs.Reset();

		// update the module variable bindings now - since for this all
		// modules have to exist first
		ForEachModule([this, Model](const FRigModuleInstance* Module) -> bool
		{
			if(const FRigModuleReference* ModuleReference = Model.FindModule(Module->GetPath()))
			{
				(void)SetModuleVariableBindings(ModuleReference->GetPath(), ModuleReference->Bindings);
			}
			if(UControlRig* ModuleRig = Module->GetRig())
			{
				ModuleRig->Initialize();
			}
			return true;
		});

		UpdateCachedChildren();
		UpdateSupportedEvents();
	}
}

bool UModularRig::Execute_Internal(const FName& InEventName)
{
	if (VM)
	{
		FRigVMExtendedExecuteContext& ModularRigContext = GetRigVMExtendedExecuteContext();
		const FControlRigExecuteContext& PublicContext = ModularRigContext.GetPublicDataSafe<FControlRigExecuteContext>();
		const FRigUnitContext& UnitContext = PublicContext.UnitContext;
		const URigHierarchy* Hierarchy = GetHierarchy();

		ForEachModule([&InEventName, this, Hierarchy, UnitContext](FRigModuleInstance* Module) -> bool
		{
			if (const UControlRig* ModuleRig = Module->GetRig())
			{
				if (!ModuleRig->SupportsEvent(InEventName))
				{
					return true;
				}

				// Only emit interaction event on this module if any of the interaction elements
				// belong to the module's namespace
				if (InEventName == FRigUnit_InteractionExecution::EventName)
				{
					const FString ModuleNamespace = Module->GetNamespace();
					const bool bIsInteracting = UnitContext.ElementsBeingInteracted.ContainsByPredicate(
						[ModuleNamespace, Hierarchy](const FRigElementKey& InteractionElement)
						{
							return ModuleNamespace == Hierarchy->GetNameSpace(InteractionElement);
						});
					if (!bIsInteracting)
					{
						return true;
					}
				}

				ExecutionQueue.Add(FRigModuleExecutionElement(Module, InEventName));
			}
			return true;
		});

		ExecuteQueue();
		return true;
	}
	return false;
}

void UModularRig::Evaluate_AnyThread()
{
	ResetExecutionQueue();
	Super::Evaluate_AnyThread();
}

bool UModularRig::SupportsEvent(const FName& InEventName) const
{
	return GetSupportedEvents().Contains(InEventName);
}

const TArray<FName>& UModularRig::GetSupportedEvents() const
{
	if (SupportedEvents.IsEmpty())
	{
		UpdateSupportedEvents();
	}
	return SupportedEvents;
}

const FModularRigSettings& UModularRig::GetModularRigSettings() const
{
	if(HasAnyFlags(RF_ClassDefaultObject))
	{
		return ModularRigSettings;
	}
	if (const UModularRig* CDO = Cast<UModularRig>(GetClass()->GetDefaultObject()))
	{
		return CDO->GetModularRigSettings();
	}
	return ModularRigSettings;
}

void UModularRig::ExecuteQueue()
{
	FRigVMExtendedExecuteContext& Context = GetRigVMExtendedExecuteContext();
	FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
	URigHierarchy* Hierarchy = GetHierarchy();

#if WITH_EDITOR
	TMap<FRigModuleInstance*, FFirstEntryEventGuard> FirstModuleEvent;
#endif
	
	while(ExecutionQueue.IsValidIndex(ExecutionQueueFront))
	{
		FRigModuleExecutionElement& ExecutionElement = ExecutionQueue[ExecutionQueueFront];
		if (FRigModuleInstance* ModuleInstance = ExecutionElement.ModuleInstance)
		{
			if (UControlRig* ModuleRig = ModuleInstance->GetRig())
			{
				if (!ModuleRig->SupportsEvent(ExecutionElement.EventName))
				{
					ExecutionQueueFront++;
					continue;
				}

				// Make sure the hierarchy has the correct element redirector from this module rig
				FRigHierarchyRedirectorGuard ElementRedirectorGuard(ModuleRig);

				FRigVMExtendedExecuteContext& RigExtendedExecuteContext= ModuleRig->GetRigVMExtendedExecuteContext();

				// Make sure the hierarchy has the correct execute context with the rig module namespace
				FRigHierarchyExecuteContextBracket ExecuteContextBracket(Hierarchy, &RigExtendedExecuteContext);

				FControlRigExecuteContext& RigPublicContext = RigExtendedExecuteContext.GetPublicDataSafe<FControlRigExecuteContext>();
				FRigUnitContext& RigUnitContext = RigPublicContext.UnitContext;
				RigUnitContext = PublicContext.UnitContext;

				// forward important context info to each module
				RigPublicContext.SetDrawInterface(PublicContext.GetDrawInterface());
				RigPublicContext.SetDrawContainer(PublicContext.GetDrawContainer());
				RigPublicContext.RigModuleInstance = ExecutionElement.ModuleInstance;
				RigPublicContext.SetAbsoluteTime(PublicContext.GetAbsoluteTime());
				RigPublicContext.SetDeltaTime(PublicContext.GetDeltaTime());
				RigPublicContext.SetWorld(PublicContext.GetWorld());
				RigPublicContext.SetOwningActor(PublicContext.GetOwningActor());
				RigPublicContext.SetOwningComponent(PublicContext.GetOwningComponent());
#if WITH_EDITOR
				RigPublicContext.SetLog(PublicContext.GetLog());
#endif
				RigPublicContext.SetFramesPerSecond(PublicContext.GetFramesPerSecond());
				RigPublicContext.SetToWorldSpaceTransform(PublicContext.GetToWorldSpaceTransform());
				RigPublicContext.OnAddShapeLibraryDelegate = PublicContext.OnAddShapeLibraryDelegate;
				RigPublicContext.OnShapeExistsDelegate = PublicContext.OnShapeExistsDelegate;
				RigPublicContext.RuntimeSettings = PublicContext.RuntimeSettings;

#if WITH_EDITOR
				if (!FirstModuleEvent.Contains(ModuleInstance))
				{
					//ModuleRig->InstructionVisitInfo.FirstEntryEventInQueue = ExecutionElement.EventName;
					FirstModuleEvent.Add(ModuleInstance, FFirstEntryEventGuard(&ModuleRig->InstructionVisitInfo, ExecutionElement.EventName));
				}
#endif

				// re-initialize the module in case only the VM side got recompiled.
				// this happens when the user relies on auto recompilation when editing the
				// module (dependency) graph - by changing a value, add / remove nodes or links.
				if(ModuleRig->IsInitRequired())
				{
					const TGuardValue<float> AbsoluteTimeGuard(ModuleRig->AbsoluteTime, ModuleRig->AbsoluteTime);
					const TGuardValue<float> DeltaTimeGuard(ModuleRig->DeltaTime, ModuleRig->DeltaTime);
					if(!ModuleRig->InitializeVM(ExecutionElement.EventName))
					{
						ExecutionQueueFront++;
						continue;
					}

					// put the variable defaults back
					if(const FRigModuleReference* ModuleReference = GetModularRigModel().FindModule(ExecutionElement.ModulePath))
					{
						for (const TPair<FName, FString>& Variable : ModuleReference->ConfigValues)
						{
							ModuleRig->SetVariableFromString(Variable.Key, Variable.Value);
						}
					}
				}

				// Update the interaction elements to show only the ones belonging to this module
				const FString ModuleNamespace = FString::Printf(TEXT("%s:"), *ExecutionElement.ModulePath);
				RigUnitContext.ElementsBeingInteracted = RigUnitContext.ElementsBeingInteracted.FilterByPredicate(
					[ModuleNamespace, Hierarchy](const FRigElementKey& Key)
				{
					return ModuleNamespace == Hierarchy->GetNameSpace(Key);
				});
				RigUnitContext.InteractionType = RigUnitContext.ElementsBeingInteracted.IsEmpty() ?
					(uint8) EControlRigInteractionType::None
					: RigUnitContext.InteractionType;

				// Make sure the module's rig has the corrct user data
				// The rig will combine the user data of the
				// - skeleton
				// - skeletalmesh
				// - SkeletalMeshComponent
				// - default control rig module
				// - outer modular rig
				// - external variables
				{
					RigPublicContext.AssetUserData.Reset();
					if(const TArray<UAssetUserData*>* ControlRigUserDataArray = ModuleRig->GetAssetUserDataArray())
					{
						for(const UAssetUserData* ControlRigUserData : *ControlRigUserDataArray)
						{
							RigPublicContext.AssetUserData.Add(ControlRigUserData);
						}
					}
					RigPublicContext.AssetUserData.Remove(nullptr);
				}

				// Copy variable bindings
				for (TPair<FName, FRigVMExternalVariable>& Pair : ExecutionElement.ModuleInstance->VariableBindings)
				{
					const FRigVMExternalVariable TargetVariable = ExecutionElement.ModuleInstance->GetRig()->GetPublicVariableByName(Pair.Key);
					if(ensure(TargetVariable.Property))
					{
						if (RigVMTypeUtils::AreCompatible(Pair.Value.Property, TargetVariable.Property))
						{
							Pair.Value.Property->CopyCompleteValue(TargetVariable.Memory, Pair.Value.Memory);
						}
					}
				}
			
				ModuleRig->Execute_Internal(ExecutionElement.EventName);
				ExecutionElement.bExecuted = true;

				// Copy result of Connection event to the ModularRig's unit context
				if (ExecutionElement.EventName == FRigUnit_ConnectorExecution::EventName)
				{
					PublicContext.UnitContext.ConnectionResolve = RigPublicContext.UnitContext.ConnectionResolve;
				}
			}
		}
		
		ExecutionQueueFront++;
	}
}

void UModularRig::ResetExecutionQueue()
{
	ExecutionQueue.Reset();
	ExecutionQueueFront = 0;
}

void UModularRig::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	if(Modules.IsEmpty())
	{
		return;
	}
	
	bool bPerformedChange = false;
	for(const TPair<UObject*, UObject*>& Pair : OldToNewInstanceMap)
	{
		UObject* NewObject = Pair.Value;
		if((NewObject == nullptr) || (NewObject->GetOuter() != this) || !NewObject->IsA<UControlRig>())
		{
			continue;
		}

		UControlRig* NewRig = CastChecked<UControlRig>(NewObject);

		// relying on GetFName since URigVMHost is overloading GetName()
		const FString& Path = NewRig->GetFName().ToString();

		// if we find a matching module update it.
		// FRigModuleInstance::SetRig takes care of disregarding the previous module instance.
		if(FRigModuleInstance* Module = const_cast<FRigModuleInstance*>(FindModule(Path)))
		{
			Module->SetRig(NewRig);
			NewRig->bCopyHierarchyBeforeConstruction = false;
			NewRig->SetDynamicHierarchy(GetHierarchy());
			NewRig->Initialize(true);
			bPerformedChange = true;
		}
	}

	if(bPerformedChange)
	{
		UpdateSupportedEvents();
		RequestInit();
	}
}

void UModularRig::ResetModules(bool bDestroyModuleRigs)
{
	for (FRigModuleInstance& Module : Modules)
	{
		Module.CachedChildren.Reset();

		if(bDestroyModuleRigs)
		{
			if (const UControlRig* ModuleRig = Module.GetRig())
			{
				check(ModuleRig->GetOuter() == this);
				// takes care of renaming / moving the rig to the transient package
				Module.SetRig(nullptr);
			}
		}
	}
	
	RootModules.Reset();
	Modules.Reset();
	SupportedEvents.Reset();
	ResetShortestDisplayPathCache();
}

const FModularRigModel& UModularRig::GetModularRigModel() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		const UModularRig* CDO = GetClass()->GetDefaultObject<UModularRig>();
		return CDO->GetModularRigModel();
	}
	return ModularRigModel;
}

void UModularRig::UpdateCachedChildren()
{
	TMap<FString, FRigModuleInstance*> PathToModule;
	for (FRigModuleInstance& Module : Modules)
	{
		Module.CachedChildren.Reset();
		PathToModule.Add(Module.GetPath(), &Module);
	}
	
	RootModules.Reset();
	for (FRigModuleInstance& Module : Modules)
	{
		if (Module.IsRootModule())
		{
			RootModules.Add(&Module);
		}
		else
		{
			if (FRigModuleInstance** ParentModule = PathToModule.Find(Module.ParentPath))
			{
				(*ParentModule)->CachedChildren.Add(&Module);
			}
		}
	}
}

void UModularRig::UpdateSupportedEvents() const
{
	SupportedEvents.Reset();
	ModularRigModel.ForEachModule([this](const FRigModuleReference* Module) -> bool
	{
		if (Module->Class.IsValid())
		{
			if (UControlRig* CDO = Module->Class->GetDefaultObject<UControlRig>())
			{
				TArray<FName> Events = CDO->GetSupportedEvents();
				for (const FName& Event : Events)
				{
					SupportedEvents.AddUnique(Event);
				}
			}
		}
		return true;
	});
}

void UModularRig::RunPostConstructionEvent()
{
	RecomputeShortestDisplayPathCache();
	Super::RunPostConstructionEvent();
}

FRigModuleInstance* UModularRig::AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, const FRigModuleInstance* InParent,
                                                   const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues) 
{
	// Make sure there are no name clashes
	if (InParent)
	{
		for (const FRigModuleInstance* Child : InParent->CachedChildren)
		{
			if (Child->Name == InModuleName)
			{
				return nullptr;
			}
		}
	}
	else
	{
		for (const FRigModuleInstance* RootModule : RootModules)
		{
			if (RootModule->Name == InModuleName)
			{
				return nullptr;
			}
		}
	}

	// For now, lets only allow rig modules
	if (!InModuleClass->GetDefaultObject<UControlRig>()->IsRigModule())
	{
		return nullptr;
	}

	FRigModuleInstance& NewModule = Modules.Add_GetRef(FRigModuleInstance());
	NewModule.Name = InModuleName;
	if (InParent)
	{
		NewModule.ParentPath = InParent->GetPath();
	}
	const FString Name = NewModule.GetPath();

	UControlRig* NewModuleRig = nullptr;

	// reuse existing module rig instances first
	if(UControlRig** ExistingModuleRigPtr = PreviousModuleRigs.Find(Name))
	{
		if(UControlRig* ExistingModuleRig = *ExistingModuleRigPtr)
		{
			// again relying on GetFName since RigVMHost overloads GetName
			if(ExistingModuleRig->GetFName().ToString().Equals(Name) && ExistingModuleRig->GetClass() == InModuleClass)
			{
				NewModuleRig = ExistingModuleRig;
			}
			else
			{
				DiscardModuleRig(ExistingModuleRig);
			}
			PreviousModuleRigs.Remove(Name);
		}
	}

	if(NewModuleRig == nullptr)
	{
		NewModuleRig = NewObject<UControlRig>(this, InModuleClass, *Name);
	}
	
	NewModule.SetRig(NewModuleRig);

	UpdateCachedChildren();
	ResetShortestDisplayPathCache();
	for (const FName& EventName : NewModule.GetRig()->GetSupportedEvents())
	{
		SupportedEvents.AddUnique(EventName);
	}

	// Configure module
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		FRigVMExtendedExecuteContext& ModuleContext = NewModuleRig->GetRigVMExtendedExecuteContext();
		FControlRigExecuteContext& ModulePublicContext = ModuleContext.GetPublicDataSafe<FControlRigExecuteContext>();
		NewModuleRig->RequestInit();
		NewModuleRig->bCopyHierarchyBeforeConstruction = false;
		NewModuleRig->SetDynamicHierarchy(Hierarchy);
		ModulePublicContext.Hierarchy = Hierarchy;
		ModulePublicContext.RigModuleNameSpace = NewModuleRig->GetRigModuleNameSpace();
		ModulePublicContext.RigModuleNameSpaceHash = GetTypeHash(ModulePublicContext.RigModuleNameSpace);
		NewModuleRig->SetElementKeyRedirector(FRigElementKeyRedirector(InConnectionMap, Hierarchy));

		for (const TPair<FName, FString>& Variable : InVariableDefaultValues )
		{
			NewModule.GetRig()->SetVariableFromString(Variable.Key, Variable.Value);
		}
	}

	return &NewModule;
}

bool UModularRig::SetModuleVariableBindings(const FString& InModulePath, const TMap<FName, FString>& InVariableBindings)
{
	if(FRigModuleInstance* Module = const_cast<FRigModuleInstance*>(FindModule(InModulePath)))
	{
		Module->VariableBindings.Reset();
		
		for (const TPair<FName, FString>& Pair : InVariableBindings)
		{
			FString SourceModulePath, SourceVariableName = Pair.Value;
			(void)URigHierarchy::SplitNameSpace(Pair.Value, &SourceModulePath, &SourceVariableName);
			FRigVMExternalVariable SourceVariable;
			if (SourceModulePath.IsEmpty())
			{
				if (const FProperty* Property = GetClass()->FindPropertyByName(*SourceVariableName))
				{
					SourceVariable = FRigVMExternalVariable::Make(Property, (UObject*)this);
				}
			}
			else if(const FRigModuleInstance* SourceModule = FindModule(SourceModulePath))
			{
				SourceVariable = SourceModule->GetRig()->GetPublicVariableByName(*SourceVariableName);
			}

			if(SourceVariable.Property == nullptr)
			{
				// todo: report error
				return false;
			}
			
			SourceVariable.Name = *Pair.Value; // Adapt the name of the variable to contain the full path
			Module->VariableBindings.Add(Pair.Key, SourceVariable);
		}
		return true;
	}
	return false;
}

void UModularRig::DiscardModuleRig(UControlRig* InControlRig)
{
	if(InControlRig)
	{
		// rename the previous rig.
		// GC will pick it up eventually - since we won't have any
		// owning pointers to it anymore.
		InControlRig->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		InControlRig->MarkAsGarbage();
	}
}

void UModularRig::ResetShortestDisplayPathCache() const
{
	ElementKeyToShortestDisplayPath.Reset();
}

void UModularRig::RecomputeShortestDisplayPathCache() const
{
	ResetShortestDisplayPathCache();
	
	const URigHierarchy* Hierarchy = GetHierarchy();
	check(Hierarchy);

	const TArray<FRigElementKey> AllKeys = Hierarchy->GetAllKeys();

	auto GetNameForElement = [Hierarchy](const FRigElementKey& InElementKey)
	{
		const FName DesiredName = Hierarchy->GetNameMetadata(InElementKey, URigHierarchy::DesiredNameMetadataName, NAME_None);
		if(!DesiredName.IsNone())
		{
			return DesiredName;
		}
		return InElementKey.Name;
	};
	
	TMap<FName, bool> IsNameUniqueInHierarchy;
	for(const FRigElementKey& Key : AllKeys)
	{
		auto UpdateNameUniqueness = [](const FName& InName, TMap<FName, bool>& UniqueMap)
		{
			if(bool* Unique = UniqueMap.Find(InName))
			{
				*Unique = false;
			}
			else
			{
				UniqueMap.Add(InName, true);
			}
		};
		UpdateNameUniqueness(GetNameForElement(Key), IsNameUniqueInHierarchy);
	}

	const FModularRigModel& Model = GetModularRigModel();
	for(const FRigElementKey& Key : AllKeys)
	{
		const FName Name = GetNameForElement(Key);
		const FName ModulePath = Hierarchy->GetModulePathFName(Key);
		
		if(!ModulePath.IsNone())
		{
			if(const FRigModuleReference* Module = Model.FindModule(ModulePath.ToString()))
			{
				const FString NameString = Name.ToString();
				const FString ModuleShortName = Module->GetShortName();
				const FString NameSpacedName = URigHierarchy::JoinNameSpace(ModuleShortName, NameString);
				ElementKeyToShortestDisplayPath.Add(Key, { NameSpacedName, IsNameUniqueInHierarchy.FindChecked(Name) ? NameString : NameSpacedName });
				continue;
			}
		}

		if(IsNameUniqueInHierarchy.FindChecked(Name))
		{
			const FString NameString = Name.ToString();
			ElementKeyToShortestDisplayPath.Add(Key, { NameString, NameString});
		}
		else
		{
			const FString NameString = Key.ToString();
			ElementKeyToShortestDisplayPath.Add(Key, { Name.ToString(), Name.ToString()});
		}
	}
}

const FRigModuleInstance* UModularRig::FindModule(const FString& InPath) const
{
	if(InPath.EndsWith(NamespaceSeparator))
	{
		return FindModule(InPath.Left(InPath.Len() - 1));
	}
	return Modules.FindByPredicate([InPath](const FRigModuleInstance& Module)
	{
		return Module.GetPath() == InPath;
	});
}

const FRigModuleInstance* UModularRig::FindModule(const UControlRig* InModuleInstance) const
{
	const FRigModuleInstance* FoundModule = nullptr;
	ForEachModule([InModuleInstance, &FoundModule](const FRigModuleInstance* Module) -> bool
	{
		if(Module->GetRig() == InModuleInstance)
		{
			FoundModule = Module;
			// don't continue ForEachModule
			return false;
		}
		return true;
	});

	return FoundModule;
}

const FRigModuleInstance* UModularRig::FindModule(const FRigBaseElement* InElement) const
{
	if(InElement)
	{
		return FindModule(InElement->GetKey());
	}
	return nullptr;
}

const FRigModuleInstance* UModularRig::FindModule(const FRigElementKey& InElementKey) const
{
	if(const URigHierarchy* Hierarchy = GetHierarchy())
	{
		const FString ModulePath = Hierarchy->GetModulePath(InElementKey);
		if(!ModulePath.IsEmpty())
		{
			return FindModule(ModulePath);
		}
	}
	return nullptr;
}

FString UModularRig::GetParentPath(const FString& InPath) const
{
	if (const FRigModuleInstance* Element = FindModule(InPath))
	{
		return Element->ParentPath;
	}
	return FString();
}

FString UModularRig::GetShortestDisplayPathForElement(const FRigElementKey& InElementKey, bool bAlwaysShowNameSpace) const
{
	if(const TTuple<FString, FString>* ShortestDisplayPaths = ElementKeyToShortestDisplayPath.Find(InElementKey))
	{
		return bAlwaysShowNameSpace ? ShortestDisplayPaths->Get<0>() : ShortestDisplayPaths->Get<1>();
	}
	return FString();
}

void UModularRig::ForEachModule(TFunctionRef<bool(FRigModuleInstance*)> PerModuleFunction)
{
	TArray<FRigModuleInstance*> ModuleInstances = RootModules;
	for (int32 ModuleIndex = 0; ModuleIndex < ModuleInstances.Num(); ++ModuleIndex)
	{
		if (!PerModuleFunction(ModuleInstances[ModuleIndex]))
		{
			break;
		}
		ModuleInstances.Append(ModuleInstances[ModuleIndex]->CachedChildren);
	}
}

void UModularRig::ForEachModule(TFunctionRef<bool(const FRigModuleInstance*)> PerModuleFunction) const
{
	TArray<FRigModuleInstance*> ModuleInstances = RootModules;
	for (int32 ModuleIndex = 0; ModuleIndex < ModuleInstances.Num(); ++ModuleIndex)
	{
		if (!PerModuleFunction(ModuleInstances[ModuleIndex]))
		{
			break;
		}
		ModuleInstances.Append(ModuleInstances[ModuleIndex]->CachedChildren);
	}
}

void UModularRig::ExecuteConnectorEvent(const FRigElementKey& InConnector, const FRigModuleInstance* InModuleInstance, const FRigElementKeyRedirector* InRedirector, TArray<FRigElementResolveResult>& InOutCandidates)
{
	if (!InModuleInstance)
	{
		InOutCandidates.Reset();
		return;
	}

	if (!InRedirector)
	{
		InOutCandidates.Reset();
		return;
	}
	
	FRigModuleInstance* Module = Modules.FindByPredicate([InModuleInstance](FRigModuleInstance& Instance)
	{
		return &Instance == InModuleInstance;
	});
	if (!Module)
	{
		InOutCandidates.Reset();
		return;
	}

	TArray<FRigElementResolveResult> Candidates = InOutCandidates;
	
	FControlRigExecuteContext& PublicContext = GetRigVMExtendedExecuteContext().GetPublicDataSafe<FControlRigExecuteContext>();

	FString ShortConnectorName = InConnector.Name.ToString();
	ShortConnectorName.RemoveFromStart(Module->GetNamespace());
	TGuardValue<FRigElementKey> ConnectorGuard(PublicContext.UnitContext.ConnectionResolve.Connector, FRigElementKey(*ShortConnectorName, InConnector.Type));
	TGuardValue<TArray<FRigElementResolveResult>> CandidatesGuard(PublicContext.UnitContext.ConnectionResolve.Matches, Candidates);
	TGuardValue<TArray<FRigElementResolveResult>> MatchesGuard(PublicContext.UnitContext.ConnectionResolve.Excluded, {});
	
	FRigModuleExecutionElement ExecutionElement(Module, FRigUnit_ConnectorExecution::EventName);
	TGuardValue<TArray<FRigModuleExecutionElement>> ExecutionGuard(ExecutionQueue, {ExecutionElement});
	TGuardValue<int32> ExecutionFrontGuard(ExecutionQueueFront, 0);
	TGuardValue<FRigElementKeyRedirector> RedirectorGuard(ElementKeyRedirector, *InRedirector);
	ExecuteQueue();

	InOutCandidates = PublicContext.UnitContext.ConnectionResolve.Matches;
}

#undef LOCTEXT_NAMESPACE
