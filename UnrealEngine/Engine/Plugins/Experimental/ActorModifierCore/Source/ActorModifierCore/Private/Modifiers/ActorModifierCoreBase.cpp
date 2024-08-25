// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/ActorModifierCoreBase.h"

#include "Modifiers/ActorModifierCoreSharedObject.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogActorModifierCoreBase, Log, All);

UActorModifierCoreSharedObject* UActorModifierCoreBase::GetShared(TSubclassOf<UActorModifierCoreSharedObject> InClass, bool bInCreateIfNone) const
{
	if (!InClass.Get())
	{
		return nullptr;
	}

	const AActor* ActorModified = GetModifiedActor();
	if (!ActorModified)
	{
		return nullptr;
	}

	if (const UActorModifierCoreSubsystem* Subsystem = UActorModifierCoreSubsystem::Get())
	{
		ULevel* Level = ActorModified->GetLevel();
		return Subsystem->GetModifierSharedObject(Level, InClass, bInCreateIfNone);
	}

	return nullptr;
}

TFuture<bool> UActorModifierCoreBase::ExecuteModifier()
{
	ExecutePromise = MakeShared<TPromise<bool>>();
	TFuture<bool> ExecuteFuture = ExecutePromise->GetFuture();

	const bool bValidActor = IsValid(GetModifiedActor());
	const bool bModifierReady = IsModifierReady();

	if (bModifierIdle && bValidActor && bModifierReady)
	{
		if (bModifierEnabled || IsModifierStack())
		{
			// lock current execution state
			BeginModifierExecution();

			// save the state before this modifier is execute to restore it later
			SavePreState();

			// run modifier logic, and update the modifier dirty state once the modifier logic completes
			TWeakObjectPtr<UActorModifierCoreBase> WeakThis(this);
			ExecuteApply()
			.Then([WeakThis](TFuture<bool> ApplyResult)
			{
				UActorModifierCoreBase* This = WeakThis.Get();
				if (!This)
				{
					return;
				}

				const bool bApplyResult = ApplyResult.Get();

				// If it succeeded then unmark modifier dirty and set state as applied (for restore)
				if (bApplyResult)
				{
					This->bModifierDirty = false;
					This->bModifierApplied = true;
				}

				This->ApplyPromise.Reset();

				// unlock current execution state
				This->EndModifierExecution();

				// return result execution
				This->ExecutePromise->SetValue(bApplyResult);
				This->ExecutePromise.Reset();
			});
		}
		// disable
		else
		{
			// Skipped
			Status = FActorModifierCoreStatus(EActorModifierCoreStatus::Success, FText::GetEmpty());

			bModifierDirty = false;

			ExecutePromise->SetValue(true);
			ExecutePromise.Reset();
		}
	}
	// invalid
	else
	{
		ExecutePromise->SetValue(false);
		ExecutePromise.Reset();
	}

	return ExecuteFuture;
}

void UActorModifierCoreBase::Apply()
{
	checkNoEntry();
	Fail(FText::FromString(TEXT("Apply not implemented")));
}

void UActorModifierCoreBase::Next()
{
	if (!bModifierIdle && ApplyPromise.IsValid())
	{
		// Success
		Status = FActorModifierCoreStatus(EActorModifierCoreStatus::Success, FText::GetEmpty());

		ApplyPromise->SetValue(true);
	}
	else
	{
		LogModifier(TEXT("Next is called again after execution is done"), true);
		checkNoEntry()
	}
}

void UActorModifierCoreBase::Fail(const FText& InFailReason)
{
	if (!bModifierIdle && ApplyPromise.IsValid())
	{
		// Provide a valid failing reason
		check(!InFailReason.IsEmpty())

		// Failed
		Status = FActorModifierCoreStatus(EActorModifierCoreStatus::Error, InFailReason);

		ApplyPromise->SetValue(false);
	}
	else
	{
		LogModifier(TEXT("Fail is called again after execution is done"), true);
		checkNoEntry()
	}
}

void UActorModifierCoreBase::Unapply()
{
	LogModifier(TEXT("Unapplying modifier"));

	// only restore if this modifier was already applied previously
	if (bModifierApplied)
	{
		RestorePreState();
		bModifierApplied = false;
	}
}

void UActorModifierCoreBase::OnModifierDirty(UActorModifierCoreBase* DirtyModifier, bool bExecute)
{
	if (ModifierStack.IsValid())
	{
		ModifierStack->OnModifierDirty(DirtyModifier, bExecute);
	}
}

const FActorModifierCoreMetadata& UActorModifierCoreBase::GetModifierMetadata() const
{
	return *Metadata;
}

FName UActorModifierCoreBase::GetModifierName() const
{
	return Metadata->GetName();
}

FName UActorModifierCoreBase::GetModifierCategory() const
{
	return Metadata->GetCategory();
}

bool UActorModifierCoreBase::IsModifierStack() const
{
	return Metadata->IsStack();
}

AActor* UActorModifierCoreBase::GetModifiedActor() const
{
	if (!ModifiedActor.IsValid())
	{
		const_cast<UActorModifierCoreBase*>(this)->ModifiedActor = Cast<AActor>(GetOuter());
	}
	return ModifiedActor.Get();
}

UActorModifierCoreStack* UActorModifierCoreBase::GetRootModifierStack() const
{
	// we are not the root stack
	if (const UActorModifierCoreStack* Stack = GetModifierStack())
	{
		return Stack->GetRootModifierStack();
	}

	// we are the root stack
	UActorModifierCoreBase* This = const_cast<UActorModifierCoreBase*>(this);
	return Cast<UActorModifierCoreStack>(This);
}

const UActorModifierCoreBase* UActorModifierCoreBase::GetPreviousModifier() const
{
	const UActorModifierCoreBase* PreviousNameModifier = nullptr;
	if (const UActorModifierCoreStack* RootStack = GetRootModifierStack())
	{
		RootStack->ProcessFunction([&PreviousNameModifier, this](const UActorModifierCoreBase* InModifier)->bool
		{
			// stop when we are the current modifier
			if (InModifier == this)
			{
				return false;
			}

			PreviousNameModifier = InModifier;

			return true;
		});
	}
	return PreviousNameModifier;
}

const UActorModifierCoreBase* UActorModifierCoreBase::GetNextModifier() const
{
	const UActorModifierCoreBase* NextNameModifier = nullptr;
	if (const UActorModifierCoreStack* RootStack = GetRootModifierStack())
	{
		bool bStartSearch = false;
		RootStack->ProcessFunction([&NextNameModifier, &bStartSearch, this](const UActorModifierCoreBase* InModifier)->bool
		{
			if (bStartSearch)
			{
				NextNameModifier = InModifier;

				// stop we have found our next modifier
				return false;
			}

			if (InModifier == this)
			{
				bStartSearch = true;
			}

			// keep going
			return true;
		});
	}
	return NextNameModifier;
}

const UActorModifierCoreBase* UActorModifierCoreBase::GetPreviousNameModifier(const FName& InModifierName) const
{
	const UActorModifierCoreBase* PreviousNameModifier = nullptr;
	if (const UActorModifierCoreStack* RootStack = GetRootModifierStack())
	{
		RootStack->ProcessFunction([&PreviousNameModifier, InModifierName, this](const UActorModifierCoreBase* InModifier)->bool
		{
			// stop when we are the current modifier
			if (InModifier == this)
			{
				return false;
			}

			// is it the name we are looking for
			if (InModifier->GetModifierName() == InModifierName)
			{
				PreviousNameModifier = InModifier;
			}

			// keep going since we want the closest one
			return true;
		});
	}
	return PreviousNameModifier;
}

const UActorModifierCoreBase* UActorModifierCoreBase::GetNextNameModifier(const FName& InModifierName) const
{
	const UActorModifierCoreBase* NextNameModifier = nullptr;
	if (const UActorModifierCoreStack* RootStack = GetRootModifierStack())
	{
		bool bStartSearch = false;
		RootStack->ProcessFunction([&NextNameModifier, &bStartSearch, InModifierName, this](const UActorModifierCoreBase* InModifier)->bool
		{
			if (bStartSearch)
			{
				// is this the name we are looking for
				if (InModifier->GetModifierName() == InModifierName)
				{
					NextNameModifier = InModifier;

					// stop we have found our next modifier
					return false;
				}
			}

			if (InModifier == this)
			{
				bStartSearch = true;
			}

			// keep going
			return true;
		});
	}
	return NextNameModifier;
}

void UActorModifierCoreBase::MarkModifierDirty(bool bExecute)
{
	const UActorModifierCoreStack* Stack = GetRootModifierStack();

	if (!Stack || !Stack->IsModifierIdle() || !IsModifierInitialized())
	{
		return;
	}

	if (!bModifierDirty || bExecute)
	{
		bModifierDirty = true;

		OnModifierDirty(this, bExecute);
	}
}

bool UActorModifierCoreBase::IsModifierEnabled() const
{
	if (const UActorModifierCoreStack* Stack = GetModifierStack())
	{
		if (!Stack->IsModifierEnabled())
		{
			return false;
		}
	}
	return bModifierEnabled;
}

void UActorModifierCoreBase::ProcessLockFunction(TFunctionRef<void()> InFunction)
{
	LockModifierExecution();
	InFunction();
	UnlockModifierExecution();
}

void UActorModifierCoreBase::LockModifierExecution()
{
	if (!bModifierExecutionLocked)
	{
		bModifierExecutionLocked = true;

		LogModifier(TEXT("Locking modifier execution"));
	}
}

void UActorModifierCoreBase::UnlockModifierExecution()
{
	if (bModifierExecutionLocked)
	{
		bModifierExecutionLocked = false;

		LogModifier(TEXT("Unlocking modifier execution"));

		if (IsModifierDirty())
		{
			MarkModifierDirty(true);
		}
	}
}

void UActorModifierCoreBase::AddExtensionInternal(const FName& InExtensionType, TSharedPtr<FActorModifierCoreExtension> InExtension)
{
	if (!InExtension.IsValid())
	{
		return;
	}

	LogModifier(FString::Printf(TEXT("Adding modifier extension %s"), *InExtensionType.ToString()));

	ModifierExtensions.Emplace(InExtensionType, InExtension);

	InExtension->ConstructInternal(this, InExtensionType);

	if (bModifierEnabled)
	{
		InExtension->EnableExtension(EActorModifierCoreEnableReason::User);
	}
}

void UActorModifierCoreBase::SetModifierEnabled(bool bInEnabled)
{
	if (bModifierEnabled == bInEnabled)
	{
		return;
	}

#if WITH_EDITOR
	Modify();
#endif

	bModifierEnabled = bInEnabled;
	OnModifierEnabledChanged(/** Execute */true);
}

bool UActorModifierCoreBase::IsModifierProfiling() const
{
	if (const UActorModifierCoreStack* Stack = GetRootModifierStack())
	{
		return Stack->bModifierProfiling;
	}

	return false;
}

bool UActorModifierCoreBase::ProcessFunction(TFunctionRef<bool(const UActorModifierCoreBase*)> InFunction, const FActorModifierCoreStackSearchOp& InSearchOptions) const
{
	return InFunction(this);
}

UActorModifierCoreBase::UActorModifierCoreBase()
{
	// Copy metadata from CDO
	if (!IsTemplate())
	{
		const UActorModifierCoreBase* CDO = GetClass()->GetDefaultObject<UActorModifierCoreBase>();
		Metadata = CDO->Metadata;
	}
}

void UActorModifierCoreBase::PostLoad()
{
	Super::PostLoad();

	// Begin batch operation to avoid updating every time a modifier is loaded
	UActorModifierCoreStack* Stack = GetRootModifierStack();
	if (!Stack->IsModifierExecutionLocked() && !Stack->IsModifierStackInitialized())
	{
		Stack->LockModifierExecution();
	}

	// Bind to world delegate, tick will be called when all actors have been loaded, unbind when actors have been loaded
	FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
	FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UActorModifierCoreBase::PostModifierWorldLoad);
}

void UActorModifierCoreBase::PostEditImport()
{
	Super::PostEditImport();

	InitializeModifier(EActorModifierCoreEnableReason::Duplicate);

	// Execute stack to update modifiers after duplication process
	if (IsModifierStack())
	{
		OnModifierEnabledChanged(true);
	}
}

void UActorModifierCoreBase::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	InitializeModifier(EActorModifierCoreEnableReason::Duplicate);
}

#if WITH_EDITOR
void UActorModifierCoreBase::PreEditUndo()
{
	Super::PreEditUndo();

	if (UActorModifierCoreStack* RootStack = GetRootModifierStack())
	{
		if (RootStack->IsModifierIdle())
		{
			RootStack->LockModifierExecution();
			RootStack->MarkModifierDirty(false);
			RootStack->RestorePreState();
		}
	}
}

void UActorModifierCoreBase::PostEditUndo()
{
	Super::PostEditUndo();

	// is it an undo remove or undo add operation ?
	const bool bModifierInStack = ModifierStack.IsValid() && ModifierStack->Modifiers.Contains(this);
	const bool bStackRegistered = IsModifierStack() && !ModifierStack.IsValid() && ModifiedActor.IsValid();
	const bool bModifierValid = bModifierInStack || bStackRegistered;

	if (!bModifierValid)
	{
		UninitializeModifier(EActorModifierCoreDisableReason::Undo);
	}
	else
	{
		InitializeModifier(EActorModifierCoreEnableReason::Undo);
	}

	// refresh the whole stack
	if (UActorModifierCoreStack* InStack = GetRootModifierStack())
	{
		if (InStack->IsModifierStackInitialized())
		{
			InStack->UnlockModifierExecution();
		}
	}
}

void UActorModifierCoreBase::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	static const FName ModifierEnabledName = GET_MEMBER_NAME_CHECKED(UActorModifierCoreBase, bModifierEnabled);

	if (InPropertyChangedEvent.GetMemberPropertyName() == ModifierEnabledName)
	{
		OnModifierEnabledChanged();
	}
}
#endif

FActorModifierCoreExtension* UActorModifierCoreBase::GetExtension(const FName& InExtensionType) const
{
	const TSharedPtr<FActorModifierCoreExtension>* Extension = ModifierExtensions.Find(InExtensionType);
	return Extension ? Extension->Get() : nullptr;
}

bool UActorModifierCoreBase::RemoveExtension(const FName& InExtensionType)
{
	if (const TSharedPtr<FActorModifierCoreExtension>* Extension = ModifierExtensions.Find(InExtensionType))
	{
		LogModifier(FString::Printf(TEXT("Removing modifier extension %s"), *InExtensionType.ToString()));

		if (bModifierEnabled)
		{
			Extension->Get()->DisableExtension(EActorModifierCoreDisableReason::User);
		}

		return ModifierExtensions.Remove(InExtensionType) > 0;
	}

	return false;
}

void UActorModifierCoreBase::LogModifier(const FString& InLog, bool bInForce) const
{
	if (IsModifierProfiling() || bInForce)
	{
		const FString ActorLabel = ModifiedActor.IsValid() ? *GetModifiedActor()->GetActorNameOrLabel() : TEXT("Invalid actor");
		const FString ModifierLabel = GetModifierName().ToString();

		UE_LOG(LogActorModifierCoreBase, Log, TEXT("[%s][%s] %s"), *ActorLabel, *ModifierLabel, *InLog);
	}
}

void UActorModifierCoreBase::TickModifier(float InDeltaTime)
{
	if (!Metadata->IsTickAllowed())
	{
		return;
	}

	if (IsModifierDirtyable())
	{
		MarkModifierDirty();
	}
}

void UActorModifierCoreBase::PostModifierCreation(UActorModifierCoreStack* InStack)
{
	// initialize once, called by the subsystem itself
	if (!ModifierStack.IsValid())
	{
		if (const UActorModifierCoreBase* CDO = GetClass()->GetDefaultObject<UActorModifierCoreBase>())
		{
			Metadata = CDO->Metadata;
		}

		ModifierStack = InStack;
		ModifiedActor = GetModifiedActor();
		bModifierInitialized = false;
	}
}

void UActorModifierCoreBase::PostModifierCDOCreation()
{
	if (IsTemplate())
	{
		Metadata = MakeShared<FActorModifierCoreMetadata>(this);
		OnModifierCDOSetup(*Metadata);
	}
}

void UActorModifierCoreBase::PostModifierWorldLoad(UWorld* InWorld, ELevelTick InType, float InDelta)
{
	const AActor* Actor = GetModifiedActor();

	// Check actor is in the world loaded and does not need post load and is not in async loading
	if (Actor
		&& InWorld == Actor->GetWorld()
		&& !Actor->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading)
		&& !Actor->HasAnyFlags(EObjectFlags::RF_NeedPostLoad)
		&& !Actor->HasAnyFlags(EObjectFlags::RF_NeedPostLoadSubobjects))
	{
		// Check that components are post loaded and ready to be used
		const bool bActorComponentsPostLoaded = ForEachComponent<UActorComponent>([](UActorComponent* InComponent)->bool
		{
			return InComponent
				&& !InComponent->HasAnyFlags(EObjectFlags::RF_NeedPostLoad)
				&& !InComponent->HasAnyFlags(EObjectFlags::RF_NeedPostLoadSubobjects);
		}
		, EActorModifierCoreComponentType::All
		, EActorModifierCoreLookup::Self);

		if (!bActorComponentsPostLoaded)
		{
			return;
		}

		// Remove handle
		FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);

		// Initialize now that all actors of world have been post loaded
		InitializeModifier(EActorModifierCoreEnableReason::Load);

		// End batch operation and execute all modifiers at once if all stack is initialized
		if (IsModifierStack() && !ModifierStack.IsValid())
		{
			if (IsModifierExecutionLocked())
			{
				UnlockModifierExecution();
			}
			else
			{
				MarkModifierDirty();
			}
		}
	}
}

void UActorModifierCoreBase::EnableModifier(EActorModifierCoreEnableReason InReason)
{
	OnModifierEnabled(InReason);

	// Enable extensions
	for (const TPair<FName, TSharedPtr<FActorModifierCoreExtension>>& ExtensionPair : ModifierExtensions)
	{
		if (ExtensionPair.Value.IsValid())
		{
			ExtensionPair.Value->EnableExtension(InReason);
		}
	}
}

void UActorModifierCoreBase::DisableModifier(EActorModifierCoreDisableReason InReason)
{
	OnModifierDisabled(InReason);

	// Disable extensions
	for (const TPair<FName, TSharedPtr<FActorModifierCoreExtension>>& ExtensionPair : ModifierExtensions)
	{
		if (ExtensionPair.Value.IsValid())
		{
			ExtensionPair.Value->DisableExtension(InReason);
		}
	}
}

void UActorModifierCoreBase::InitializeModifier(EActorModifierCoreEnableReason InReason)
{
	const bool bModifierValid = !bModifierInitialized;

	// is the modifier correctly setup
	if (bModifierValid)
	{
#if WITH_EDITOR
		// to be able to track property changes and stack updates
		if (!HasAnyFlags(RF_Transactional))
		{
			SetFlags(GetFlags() | RF_Transactional);
		}
#endif

		// set new actor
		ModifiedActor = Cast<AActor>(GetOuter());

		// Initialize profiler
		if (!Profiler.IsValid())
		{
			Profiler = Metadata->CreateProfilerInstance(this);
		}

		LogModifier(FString::Printf(TEXT("Initializing modifier with reason %i"), InReason));

		// add the modifier to our new actor stack
		OnModifierAdded(InReason);

		// if the original state was enable, enable it
		if (bModifierEnabled)
		{
			EnableModifier(InReason);
		}

		bModifierInitialized = true;

		MarkModifierDirty(false);

		UActorModifierCoreStack::OnModifierAddedDelegate.Broadcast(this);
	}
}

void UActorModifierCoreBase::UninitializeModifier(EActorModifierCoreDisableReason InReason)
{
	if (bModifierInitialized)
	{
		bModifierInitialized = false;

		LogModifier(FString::Printf(TEXT("Uninitializing modifier with reason %i"), InReason));

		const bool bWasModifierEnabled = bModifierEnabled;

		MarkModifierDirty(false);

		// if modifier is enable we need to disable it first
		if (bModifierEnabled)
		{
			bModifierEnabled = false;
			DisableModifier(InReason);
		}

		// lets remove it now from old actor
		OnModifierRemoved(InReason);

		// set new actor
		ModifiedActor = Cast<AActor>(GetOuter());

		// recover old enabled state
		bModifierEnabled = bWasModifierEnabled;

		// Ensure promises are fulfilled
		if (InReason == EActorModifierCoreDisableReason::Destroyed)
		{
			if (ApplyPromise.IsValid())
			{
				ApplyPromise->SetValue(false);
				ApplyPromise.Reset();
			}

			if (ExecutePromise.IsValid())
			{
				ExecutePromise->SetValue(false);
				ExecutePromise.Reset();
			}
		}

		UActorModifierCoreStack::OnModifierRemovedDelegate.Broadcast(this);
	}
}

TFuture<bool> UActorModifierCoreBase::ExecuteApply()
{
	ApplyPromise = MakeShared<TPromise<bool>>();
	Apply();
	return ApplyPromise->GetFuture();
}

void UActorModifierCoreBase::OnModifierEnabledChanged(bool bInExecute)
{
	LogModifier(FString::Printf(TEXT("Modifier %s"), bModifierEnabled ? TEXT("enabled") : TEXT("disabled")), true);

	if (bModifierEnabled)
	{
		EnableModifier(EActorModifierCoreEnableReason::User);
	}
	else
	{
		DisableModifier(EActorModifierCoreDisableReason::User);
	}

	MarkModifierDirty(bInExecute);
}

void UActorModifierCoreBase::BeginModifierExecution()
{
	bModifierIdle = false;

	const UActorModifierCoreStack* Stack = GetRootModifierStack();

	if (IsModifierEnabled()
		&& Stack && Stack->IsModifierEnabled() && Stack->IsModifierProfiling()
		&& Profiler.IsValid())
	{
		Profiler->BeginProfiling();
	}

	LogModifier(TEXT("Appling modifier"));
}

void UActorModifierCoreBase::EndModifierExecution()
{
	bModifierIdle = true;

	const UActorModifierCoreStack* Stack = GetRootModifierStack();

	if (IsModifierEnabled()
		&& Stack && Stack->IsModifierEnabled() && Stack->IsModifierProfiling()
		&& Profiler.IsValid())
	{
		Profiler->EndProfiling();
	}

	if (Status.GetStatus() != EActorModifierCoreStatus::Success)
	{
		LogModifier(FString::Printf(TEXT("Modifier execution failed due to reason : %s"), *Status.GetStatusMessage().ToString()), true);
	}
}

