// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionBuilder/PCGWorldPartitionBuilder.h"

#include "PCGComponent.h"
#include "PCGGraph.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/OutputDevice.h"
#include "Modules/ModuleManager.h"
#include "UObject/Linker.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPCGWorldPartitionBuilder, All, All);

namespace PCGWorldPartitionBuilder
{
	/** Grab all original PCG components in world, applying given filter. */
	void CollectComponentsToGenerate(UWorld* InWorld, TFunctionRef<bool(const UPCGComponent*)> ComponentFilter, TArray<TWeakObjectPtr<UPCGComponent>>& OutComponents);
	
	/** Generate the given components. Optionally generate only one component at a time with a wait on async processes after each. Optionally apply given filter to select components. */
	bool GenerateComponents(TArray<TWeakObjectPtr<UPCGComponent>>& Components, UWorld* InWorld, bool bOneComponentAtATime, TArray<UPackage*>& InOutDeletedActorPackages, bool& bOutGenerationErrors);
	bool GenerateComponents(
		TArray<TWeakObjectPtr<UPCGComponent>>& Components,
		UWorld* InWorld,
		bool bOneComponentAtATime,
		TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
		TArray<UPackage*>& InOutDeletedActorPackages,
		bool& bOutGenerationErrors);

	/** Generate a component. Applies correct editing mode if necessary. */
	void GenerateComponent(UPCGComponent* InComponent, UWorld* InWorld);

	/** Waits for background activity to be quiet. */
	void WaitForAllAsyncEditorProcesses(UWorld* InWorld);

	/** Runs builder on current editor world. */
	void Build(const TArray<FString>& Args);

	static FAutoConsoleCommand CommandBuildComponents(
		TEXT("pcg.BuildComponents"),
		TEXT("Runs PCG world builder on PCG components in current world. Arguments (multiple values separated with ';'):\n"
			"\t[-IncludeGraphNames=PCG_GraphA;PCG_GraphB]\n"
			"\t[-GenerateComponentEditingModeNormal]\n"
			"\t[-GenerateComponentEditingModePreview]\n"
			"\t[-IgnoreGenerationErrors]\n"
			"\t[-IncludeActorIDs=MyActor1_UAID1234678;MyActor2_UAID1234678]\n"
			"\t[-OneComponentAtATime]\n"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { PCGWorldPartitionBuilder::Build(Args); }));
};

/** Output device to capture presence of errors during generation. */
struct FPCGDetectErrorsInScope : public FOutputDevice
{
	FPCGDetectErrorsInScope()
	{
		if (GLog)
		{
			GLog->AddOutputDevice(this);
		}
	}

	virtual ~FPCGDetectErrorsInScope()
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	//~Begin FOutputDevice interface
	virtual bool IsMemoryOnly() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
	{
		if (Verbosity <= ELogVerbosity::Error)
		{
			bErrorOccurred = true;
		}
	}
	//~End FOutputDevice interface

	bool GetErrorOccurred() const { return bErrorOccurred; }

private:
	std::atomic<bool> bErrorOccurred = false;
};

UPCGWorldPartitionBuilder::UPCGWorldPartitionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		bGenerateEditingModeNormalComponents = HasParam("GenerateComponentEditingModeNormal");

		bGenerateEditingModePreviewComponents = HasParam("GenerateComponentEditingModePreview");

		bOneComponentAtATime = HasParam("OneComponentAtATime");

		bIgnoreGenerationErrors = HasParam("IgnoreGenerationErrors");

		FString IncludeGraphNamesValue;
		if (GetParamValue("IncludeGraphNames=", IncludeGraphNamesValue) && !IncludeGraphNamesValue.IsEmpty())
		{
			TArray<FString> IncludeGraphNameStrings;
			IncludeGraphNamesValue.ParseIntoArray(IncludeGraphNameStrings, TEXT(";"), true);

			IncludeGraphNames.Append(IncludeGraphNameStrings);
		}

		FString IncludeActorIDsValue;
		if (GetParamValue("IncludeActorIDs=", IncludeActorIDsValue) && !IncludeActorIDsValue.IsEmpty())
		{
			TArray<FString> IncludeActorIDStrings;
			IncludeActorIDsValue.ParseIntoArray(IncludeActorIDStrings, TEXT(";"), true);

			IncludeActorIDs.Append(IncludeActorIDStrings);
		}
	}
}

bool UPCGWorldPartitionBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	if (!ensure(GEngine) || !ensure(World))
	{
		return false;
	}

	// Try to eliminate as much background noise as possible before getting started.
	PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(World);

	// 1. Collect components of interest once upfront.

	TArray<TWeakObjectPtr<UPCGComponent>> ComponentsToGenerate;
	{
		auto ComponentFilter = [this](const UPCGComponent* InComponent)
		{
			if (!InComponent || !InComponent->GetOwner())
			{
				return false;
			}

			// Check actor in inclusion list if provided.
			if (!IncludeActorIDs.IsEmpty() && !IncludeActorIDs.Contains(InComponent->GetOwner()->GetName()))
			{
				return false;
			}

			// Accept based on editing mode.
			return (InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview)
				|| (bGenerateEditingModeNormalComponents && InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::Normal)
				|| (bGenerateEditingModePreviewComponents && InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::Preview);
		};

		PCGWorldPartitionBuilder::CollectComponentsToGenerate(World, ComponentFilter, ComponentsToGenerate);
	}

	// 2. Clear the dirty flag on any packages that dirtied themselves while loading. We're not interested in saving these.

	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyPackages(DirtyPackages);
		DirtyPackages.RemoveSwap(nullptr);
		for (UPackage* DirtyPackage : DirtyPackages)
		{
			// Make clean as it's not a change we have made.
			if (!PendingDirtyPackages.Contains(DirtyPackage->GetPersistentGuid()))
			{
				DirtyPackage->SetDirtyFlag(false);
			}
		}
		DirtyPackages.Reset();
	}

	const int32 NumPendingDirtyPackagesBefore = PendingDirtyPackages.Num();

	// 3. Generate components.

	bool bGeneratedAnyComponent = false;

	if (!IncludeGraphNames.IsEmpty())
	{
		// Generate all components with graph of each name, in specified order.
		for (const FName& IncludeGraphName : IncludeGraphNames)
		{
			auto FilterOnGraphName = [IncludeGraphName](const UPCGComponent* InComponent)
			{
				const UPCGGraph* Graph = InComponent ? InComponent->GetGraph() : nullptr;
				return Graph && Graph->GetName() == IncludeGraphName;
			};

			bool bErrorsOccurred = false;
			bGeneratedAnyComponent |= PCGWorldPartitionBuilder::GenerateComponents(ComponentsToGenerate, World, bOneComponentAtATime, FilterOnGraphName, DeletedActorPackages, bErrorsOccurred);

			bErrorOccurredWhileGenerating |= bErrorsOccurred;
		}
	}
	else
	{
		bool bErrorsOccurred = false;
		bGeneratedAnyComponent |= PCGWorldPartitionBuilder::GenerateComponents(ComponentsToGenerate, World, bOneComponentAtATime, DeletedActorPackages, bErrorsOccurred);

		bErrorOccurredWhileGenerating |= bErrorsOccurred;
	}

	// 4. Get packages that were dirtied during generation and record them for saving later.

	if (bGeneratedAnyComponent)
	{
		TArray<UPackage*> DirtyPackages;
		FEditorFileUtils::GetDirtyPackages(DirtyPackages);
		
		DirtyPackages.RemoveSwap(nullptr);
		for (UPackage* DirtyPackage : DirtyPackages)
		{
			PendingDirtyPackages.Add(DirtyPackage->GetPersistentGuid(), DirtyPackage);
		}
	}
	
	const int NumPackagesDirtied = PendingDirtyPackages.Num() - NumPendingDirtyPackagesBefore;
	if (!bGeneratedAnyComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("No components found to generate. No packages will be saved."));
	}
	else if (NumPackagesDirtied <= 0)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("At least one component was generated but no additional packages were dirtied."));
	}
	else
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generation complete, %d packages dirtied."), NumPackagesDirtied);
	}

	if (!bGeneratedAnyComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Dirty package detection and save skipped due to trivial run"));
		return !bErrorOccurredWhileGenerating;
	}

	// TODO: Review the save flow when we have iterative loading.
	return SaveDirtyPackages(World, PackageHelper);
}

bool UPCGWorldPartitionBuilder::SaveDirtyPackages(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	// Check whether an error was thrown while generating components and fail the builder if the ignore argument is not provided.
	if (bErrorOccurredWhileGenerating)
	{
		if (!bIgnoreGenerationErrors)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Dirty package detection and save skipped due to errors during generation."));

			return !bErrorOccurredWhileGenerating;
		}
		else
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generation errors ignored, dirty packages will be saved."));
		}
	}

	// Save/delete pending packages.

	TArray<UPackage*> DirtyPackages;
	PendingDirtyPackages.GenerateValueArray(DirtyPackages);

	// Empty packages should be deleted - mirrors logic in InternalPromptForCheckoutAndSave()
	TArray<UPackage*> PackagesToDelete;
	for (int i = DirtyPackages.Num() - 1; i >= 0; --i)
	{
		if (UPackage::IsEmptyPackage(DirtyPackages[i]))
		{
			PackagesToDelete.Add(DirtyPackages[i]);
			DirtyPackages.RemoveAtSwap(i);
		}
	}

	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PostRun: %d packages modified, %d actor packages deleted, %d empty packages marked for delete."),
		DirtyPackages.Num(),
		DeletedActorPackages.Num(),
		PackagesToDelete.Num());

	// Combine the empty-deleted packages with the actor-deleted packages.
	for (UPackage* DeletedPackage : DeletedActorPackages)
	{
		PackagesToDelete.AddUnique(DeletedPackage);
	}

	// Log final changes after combining packages (there may have been duplicates in the two deleted package lists).
	UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PostRun: Final package changes: %d modified, %d deleted."), DirtyPackages.Num(), PackagesToDelete.Num());

	if (!SavePackages(DirtyPackages, PackageHelper))
	{
		return false;
	}
	
	if (!PackagesToDelete.IsEmpty())
	{
		// Release any file handles and prepare for delete.
		for (UPackage* PackageToDelete : PackagesToDelete)
		{
			ResetLoaders(PackageToDelete);
		}

		if (!DeletePackages(PackagesToDelete, PackageHelper))
		{
			return false;
		}
	}

	TArray<FString> FilesToSubmit;
	FilesToSubmit.Append(SourceControlHelpers::PackageFilenames(DirtyPackages));
	FilesToSubmit.Append(SourceControlHelpers::PackageFilenames(PackagesToDelete));

	const FString ChangeDescription = FString::Printf(TEXT("Generated PCG components for world '%s'"), *World->GetName());
	return OnFilesModified(FilesToSubmit, ChangeDescription);
}

void PCGWorldPartitionBuilder::CollectComponentsToGenerate(
	UWorld* InWorld,
	TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
	TArray<TWeakObjectPtr<UPCGComponent>>& OutComponents)
{
	check(InWorld);

	TArray<UObject*> AllComponents;
	GetObjectsOfClass(UPCGComponent::StaticClass(), AllComponents, /*bIncludeDerivedClasses=*/true);
	for (UObject* ComponentObject : AllComponents)
	{
		if (!IsValid(ComponentObject))
		{
			continue;
		}

		UPCGComponent* Component = Cast<UPCGComponent>(ComponentObject);
		if (!Component || !Component->GetOwner() || Component->GetWorld() != InWorld)
		{
			continue;
		}

		if (Component->IsLocalComponent())
		{
			// Skip LCs, rely on original component to generate everything.
			continue;
		}

		if (!ComponentFilter(Component))
		{
			continue;
		}

		OutComponents.Add(Component);
	}
}

bool PCGWorldPartitionBuilder::GenerateComponents(TArray<TWeakObjectPtr<UPCGComponent>>& Components, UWorld* InWorld, bool bOneComponentAtATime, TArray<UPackage*>& InOutDeletedActorPackages, bool& bOutGenerationErrors)
{
	return PCGWorldPartitionBuilder::GenerateComponents(Components, InWorld, bOneComponentAtATime, [](const UPCGComponent*) { return true; }, InOutDeletedActorPackages, bOutGenerationErrors);
}

bool PCGWorldPartitionBuilder::GenerateComponents(
	TArray<TWeakObjectPtr<UPCGComponent>>& Components,
	UWorld* InWorld,
	bool bOneComponentAtATime,
	TFunctionRef<bool(const UPCGComponent*)> ComponentFilter,
	TArray<UPackage*>& InOutDeletedActorPackages,
	bool& bOutGenerationErrors)
{
	if (!bOneComponentAtATime)
	{
		PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(InWorld);
	}

	const FPCGDetectErrorsInScope DetectErrors;

	auto WaitForComponentGeneration = [InWorld](const UPCGComponent* InComponent)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Completing generation on PCG component on actor '%s' label '%s', graph '%s'"),
			*InComponent->GetOwner()->GetName(),
			*InComponent->GetOwner()->GetActorNameOrLabel(),
			*InComponent->GetGraph()->GetName());

		while (InComponent->IsGenerating())
		{
			FWorldPartitionHelpers::FakeEngineTick(InWorld);
		}

		// Can be useful to let some things flush/update after generation.
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	};

	// Hook actor deleted events and track any corresponding deleted packages, as the packages can be GC'd if PCG triggers a GC
	// before generation.
	FDelegateHandle ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([&InOutDeletedActorPackages](AActor* InActor)
	{
		if (InActor && InActor->IsPackageExternal())
		{
			if (UPackage* ActorPackage = InActor->GetPackage())
			{
				UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Actor '%s' deleted, package '%s' added to delete list."),
					*InActor->GetName(),
					*ActorPackage->GetName());

				InOutDeletedActorPackages.AddUnique(ActorPackage);
			}
		}
	});

	ON_SCOPE_EXIT
	{
		GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
	};

	TSet<TObjectKey<UPCGComponent>> GeneratedComponents;

	for (TWeakObjectPtr<UPCGComponent> ComponentWeakPtr : Components)
	{
		UPCGComponent* Component = ComponentWeakPtr.Get();
		if (!Component)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Lost a PCG component weak pointer, component will not be generated."));
			continue;
		}

		// Validate this before running the filter as the filtering can check the graph name etc.
		const UPCGGraph* Graph = Component->GetGraph();
		if (!Graph)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("PCG component on actor '%s' label '%s' has no graph assigned, skipping."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel());
			continue;
		}

		if (!ComponentFilter(Component))
		{
			continue;
		}
		
		// Last minute validations, done here just prior to generation (after component has passed all previous filters) to minimize spam.
		if (!Component->bActivated)
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("'Activated' toggle was set false on PCG component on actor '%s' label '%s' graph '%s'. Component skipped."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel(),
				*Component->GetGraph()->GetName());
			continue;
		}

		if (Component->IsManagedByRuntimeGenSystem())
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("PCG component generation trigger is set to run-time generation on actor '%s' label '%s' graph '%s'. Component skipped."),
				*Component->GetOwner()->GetName(),
				*Component->GetOwner()->GetActorNameOrLabel(),
				*Component->GetGraph()->GetName());
			continue;
		}

		if (bOneComponentAtATime)
		{
			PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(InWorld);
		}

		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Generate PCG component on actor '%s' label '%s', graph '%s'"),
			*Component->GetOwner()->GetName(),
			*Component->GetOwner()->GetActorNameOrLabel(),
			*Component->GetGraph()->GetName());

		PCGWorldPartitionBuilder::GenerateComponent(Component, InWorld);
		GeneratedComponents.Add(Component);

		if (bOneComponentAtATime)
		{
			WaitForComponentGeneration(Component);
		}
	}

	if (!bOneComponentAtATime)
	{
		for (TObjectKey<UPCGComponent>& GeneratedComponent : GeneratedComponents)
		{
			if (const UPCGComponent* Component = GeneratedComponent.ResolveObjectPtr())
			{
				WaitForComponentGeneration(Component);
			}
			else
			{
				UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("GeneratedComponent reference lost, abandoning."));
			}
		}
	}

	bOutGenerationErrors = DetectErrors.GetErrorOccurred();

	return !GeneratedComponents.IsEmpty();
}

void PCGWorldPartitionBuilder::GenerateComponent(UPCGComponent* InComponent, UWorld* InWorld)
{
	// Separate ensures for maximum debug information.
	if (!ensure(InComponent) || !ensure(InComponent->GetGraph()))
	{
		return;
	}

	ensure(InComponent->bActivated);
	ensure(!InComponent->IsManagedByRuntimeGenSystem());

	if (InComponent->GetSerializedEditingMode() == EPCGEditorDirtyMode::LoadAsPreview)
	{
		UE_LOG(LogPCGWorldPartitionBuilder, Display, TEXT("Setting PCG editing mode to Load As Preview on actor '%s' label '%s' graph '%s'."),
			*InComponent->GetOwner()->GetName(),
			*InComponent->GetOwner()->GetActorNameOrLabel(),
			*InComponent->GetGraph()->GetName());

		InComponent->SetEditingMode(EPCGEditorDirtyMode::LoadAsPreview, InComponent->GetSerializedEditingMode());
		InComponent->ChangeTransientState(EPCGEditorDirtyMode::LoadAsPreview);
	}

	// Force generate as components that are already generated may decline the request.
	const FPCGTaskId GenerateTask = InComponent->GenerateLocalGetTaskId(/*bForce=*/true);

	if (GenerateTask == InvalidPCGTaskId)
	{
		if (ensure(InComponent->GetOwner()))
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Scheduling generate task failed for PCG Component on actor '%s' label '%s' graph '%s'."),
				*InComponent->GetOwner()->GetName(),
				*InComponent->GetOwner()->GetActorNameOrLabel(),
				*InComponent->GetGraph()->GetName());
		}
		else
		{
			UE_LOG(LogPCGWorldPartitionBuilder, Warning, TEXT("Scheduling generate task failed for PCG Component with no owner. Component '%s' graph '%s'."),
				*InComponent->GetName(),
				*InComponent->GetGraph()->GetName());
		}
	}
}

void PCGWorldPartitionBuilder::WaitForAllAsyncEditorProcesses(UWorld* InWorld)
{
	InWorld->BlockTillLevelStreamingCompleted();

	// Quite a lot of activity can happen here..
	FWorldPartitionHelpers::FakeEngineTick(InWorld);

	// Make sure all actor changes are out of the way, as we are sensitive to these.
	bool bActorsStable = false;
	FDelegateHandle ActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda([&bActorsStable](AActor* InActor) { bActorsStable = false; });
	FDelegateHandle ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([&bActorsStable](AActor* InActor) { bActorsStable = false; });

	while (!bActorsStable)
	{
		bActorsStable = true;
		FWorldPartitionHelpers::FakeEngineTick(InWorld);
	}

	GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
	GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);

	// Finalize asset compilation before we potentially use them during generation. Example: static mesh collision depends on built SMs.
	// Done before every generation to ensure everything up until now is compiled/built.
	FAssetCompilingManager::Get().FinishAllCompilation();

	// This may execute pending construction scripts.
	FAssetCompilingManager::Get().ProcessAsyncTasks();
}

void PCGWorldPartitionBuilder::Build(const TArray<FString>& Args)
{
	if (UWorld* World = (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr))
	{
		IWorldPartitionEditorModule& WorldPartitionEditorModule = FModuleManager::LoadModuleChecked<IWorldPartitionEditorModule>("WorldPartitionEditor");
		IWorldPartitionEditorModule::FRunBuilderParams Params;
		Params.BuilderClass = UPCGWorldPartitionBuilder::StaticClass();
		Params.World = World;
		Params.OperationDescription = FText::FromString("Generating PCG Components...");
		
		Params.ExtraArgs = TEXT("-AllowCommandletRendering");
		for (const FString& Arg : Args)
		{
			Params.ExtraArgs += " ";
			Params.ExtraArgs += Arg;
		}

		WorldPartitionEditorModule.RunBuilder(Params);
	}
}
