// Copyright Epic Games, Inc. All Rights Reserved.

#include "NativeClassHierarchy.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SourceCodeNavigation.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"

DEFINE_LOG_CATEGORY_STATIC(LogNativeClassHierarchy, Log, All);

TSharedRef<FNativeClassHierarchyNode> FNativeClassHierarchyNode::MakeFolderEntry(FName InEntryName, FString InEntryPath)
{
	TSharedRef<FNativeClassHierarchyNode> NewEntry = MakeShareable(new FNativeClassHierarchyNode());
	NewEntry->Type = ENativeClassHierarchyNodeType::Folder;
	NewEntry->Class = nullptr;
	NewEntry->EntryName = InEntryName;
	NewEntry->EntryPath = MoveTemp(InEntryPath);
	return NewEntry;
}

TSharedRef<FNativeClassHierarchyNode> FNativeClassHierarchyNode::MakeClassEntry(UClass* InClass, FName InClassModuleName, FString InClassModuleRelativePath, FString InEntryPath)
{
	TSharedRef<FNativeClassHierarchyNode> NewEntry = MakeShareable(new FNativeClassHierarchyNode());
	NewEntry->Type = ENativeClassHierarchyNodeType::Class;
	NewEntry->Class = InClass;
	NewEntry->ClassModuleName = MoveTemp(InClassModuleName);
	NewEntry->ClassModuleRelativePath = MoveTemp(InClassModuleRelativePath);
	NewEntry->EntryName = InClass->GetFName();
	NewEntry->EntryPath = MoveTemp(InEntryPath);
	return NewEntry;
}

void FNativeClassHierarchyNode::AddChild(TSharedRef<FNativeClassHierarchyNode> ChildEntry)
{
	check(Type == ENativeClassHierarchyNodeType::Folder);
	Children.Add(FNativeClassHierarchyNodeKey(ChildEntry->EntryName, ChildEntry->Type), MoveTemp(ChildEntry));
}

FNativeClassHierarchy::FNativeClassHierarchy()
{
	PopulateHierarchy();

	// Register to be notified of module changes
	FModuleManager::Get().OnModulesChanged().AddRaw(this, &FNativeClassHierarchy::OnModulesChanged);

	// Register to be notified of reloads
	FCoreUObjectDelegates::ReloadCompleteDelegate.AddRaw(this, &FNativeClassHierarchy::OnReloadComplete);
}

FNativeClassHierarchy::~FNativeClassHierarchy()
{
	FModuleManager::Get().OnModulesChanged().RemoveAll(this);

	FCoreUObjectDelegates::ReloadCompleteDelegate.RemoveAll(this);
}

TSharedPtr<const FNativeClassHierarchyNode> FNativeClassHierarchy::FindNode(const FName InClassPath, const ENativeClassHierarchyNodeType InType) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>> NodesFound;
	GatherMatchingNodesForPaths(TArrayView<const FName>(&InClassPath, 1), NodesFound, InType);

	for(const auto& NodeFound : NodesFound)
	{
		if(NodeFound->Type == InType)
		{
			return NodeFound;
		}
	}

	return nullptr;
}

bool FNativeClassHierarchy::RootNodePassesFilter(const FName InRootName, const TSharedPtr<const FNativeClassHierarchyNode>& InRootNode, const bool bIncludeEngineClasses, const bool bIncludePluginClasses) const
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	return (InRootName == GameRootNodeName) ||								// Always include game classes
			(InRootName == EngineRootNodeName && bIncludeEngineClasses) ||  	// Only include engine classes if we were asked for them
			(bIncludePluginClasses && InRootNode->LoadedFrom == EPluginLoadedFrom::Project) || // Only include Game plugin classes if we were asked for them
			(bIncludePluginClasses && bIncludeEngineClasses && InRootNode->LoadedFrom == EPluginLoadedFrom::Engine); //  Only include engine plugin classes if we were asked for them
}

bool FNativeClassHierarchy::RootNodePassesFilter(const FName InRootName, const bool bIncludeEngineClasses, const bool bIncludePluginClasses) const
{
	if (const TSharedPtr<FNativeClassHierarchyNode>* Found = RootNodes.Find(InRootName))
	{
		return RootNodePassesFilter(InRootName, *Found, bIncludeEngineClasses, bIncludePluginClasses);
	}

	return false;
}

bool FNativeClassHierarchy::HasClasses(const FName InClassPath, const bool bRecursive) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>> NodesToSearch;
	GatherMatchingNodesForPaths(TArrayView<const FName>(&InClassPath, 1), NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		if(HasClassesRecursive(NodeToSearch, bRecursive))
		{
			return true;
		}
	}

	return false;
}

bool FNativeClassHierarchy::HasFolders(const FName InClassPath, const bool bRecursive) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>> NodesToSearch;
	GatherMatchingNodesForPaths(TArrayView<const FName>(&InClassPath, 1), NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		if(HasFoldersRecursive(NodeToSearch, bRecursive))
		{
			return true;
		}
	}

	return false;
}

void FNativeClassHierarchy::GetMatchingClasses(const FNativeClassHierarchyFilter& Filter, TArray<UClass*>& OutClasses) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>> NodesToSearch;
	GatherMatchingNodesForPaths(Filter.ClassPaths, NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		GetClassesRecursive(NodeToSearch, OutClasses, Filter.bRecursivePaths);
	}
}

void FNativeClassHierarchy::GetMatchingFolders(const FNativeClassHierarchyFilter& Filter, TArray<FString>& OutFolders) const
{
	TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>> NodesToSearch;
	GatherMatchingNodesForPaths(Filter.ClassPaths, NodesToSearch);

	for(const auto& NodeToSearch : NodesToSearch)
	{
		GetFoldersRecursive(NodeToSearch, OutFolders, Filter.bRecursivePaths);
	}
}

void FNativeClassHierarchy::GetClassRoots(TArray<FName>& OutClassRoots, const bool bIncludeEngineClasses, const bool bIncludePluginClasses) const
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	for(const auto& RootNode : RootNodes)
	{
		bool bRootNodePassesFilter =
			(RootNode.Key == GameRootNodeName) ||								// Always include game classes
			(RootNode.Key == EngineRootNodeName && bIncludeEngineClasses) ||  	// Only include engine classes if we were asked for them
			(bIncludePluginClasses && RootNode.Value->LoadedFrom == EPluginLoadedFrom::Project) || // Only include Game plugin classes if we were asked for them
			(bIncludePluginClasses && bIncludeEngineClasses && RootNode.Value->LoadedFrom == EPluginLoadedFrom::Engine); //  Only include engine plugin classes if we were asked for them

		if(bRootNodePassesFilter)
		{
			OutClassRoots.Add(*RootNode.Value->EntryPath);
		}
	}
}

void FNativeClassHierarchy::GetClassFolders(TArray<FName>& OutClassRoots, TArray<FString>& OutClassFolders, const bool bIncludeEngineClasses, const bool bIncludePluginClasses) const
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	for(const auto& RootNode : RootNodes)
	{
		bool bRootNodePassesFilter =
			(RootNode.Key == GameRootNodeName) ||								// Always include game classes
			(RootNode.Key == EngineRootNodeName && bIncludeEngineClasses) ||  	// Only include engine classes if we were asked for them
			(bIncludePluginClasses && RootNode.Value->LoadedFrom == EPluginLoadedFrom::Project) || // Only include Game plugin classes if we were asked for them
			(bIncludePluginClasses && bIncludeEngineClasses && RootNode.Value->LoadedFrom == EPluginLoadedFrom::Engine); //  Only include engine plugin classes if we were asked for them

		if(bRootNodePassesFilter)
		{
			OutClassRoots.Add(*RootNode.Value->EntryPath);
			GetFoldersRecursive(RootNode.Value.ToSharedRef(), OutClassFolders);
		}
	}
}

bool FNativeClassHierarchy::HasClassesRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Class)
		{
			return true;
		}

		if(bRecurse && HasClassesRecursive(ChildNode.Value.ToSharedRef()))
		{
			return true;
		}
	}

	return false;
}

bool FNativeClassHierarchy::HasFoldersRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Folder)
		{
			return true;
		}

		if(bRecurse && HasFoldersRecursive(ChildNode.Value.ToSharedRef()))
		{
			return true;
		}
	}

	return false;
}

void FNativeClassHierarchy::GetClassesRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<UClass*>& OutClasses, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Class)
		{
			OutClasses.Add(ChildNode.Value->Class);
		}

		if(bRecurse)
		{
			GetClassesRecursive(ChildNode.Value.ToSharedRef(), OutClasses);
		}
	}
}

void FNativeClassHierarchy::GetFoldersRecursive(const TSharedRef<FNativeClassHierarchyNode>& HierarchyNode, TArray<FString>& OutFolders, const bool bRecurse)
{
	for(const auto& ChildNode : HierarchyNode->Children)
	{
		if(ChildNode.Value->Type == ENativeClassHierarchyNodeType::Folder)
		{
			OutFolders.Add(ChildNode.Value->EntryPath);
		}

		if(bRecurse)
		{
			GetFoldersRecursive(ChildNode.Value.ToSharedRef(), OutFolders);
		}
	}
}

void FNativeClassHierarchy::GatherMatchingNodesForPaths(const TArrayView<const FName>& InClassPaths, TArray<TSharedRef<FNativeClassHierarchyNode>, TInlineAllocator<4>>& OutMatchingNodes, const ENativeClassHierarchyNodeType InType) const
{
	if(InClassPaths.Num() == 0)
	{
		// No paths means search all roots
		OutMatchingNodes.Reserve(RootNodes.Num());
		for(const auto& RootNode : RootNodes)
		{
			OutMatchingNodes.Add(RootNode.Value.ToSharedRef());
		}
	}
	else
	{
		for(const FName& ClassPath : InClassPaths)
		{
			TSharedPtr<FNativeClassHierarchyNode> CurrentNode;

			TArray<FString> ClassPathParts;
			ClassPath.ToString().ParseIntoArray(ClassPathParts, TEXT("/"), true);
			for(const FString& ClassPathPart : ClassPathParts)
			{
				// Try and find the node associated with this part of the path...
				const FName ClassPathPartName = *ClassPathPart;

				if (InType == ENativeClassHierarchyNodeType::Class && ClassPathPart == ClassPathParts.Last())
				{

					CurrentNode = (CurrentNode.IsValid()) ? CurrentNode->Children.FindRef(FNativeClassHierarchyNodeKey(ClassPathPartName, ENativeClassHierarchyNodeType::Class)) : RootNodes.FindRef(ClassPathPartName);
				}
				else
				{
					CurrentNode = (CurrentNode.IsValid()) ? CurrentNode->Children.FindRef(FNativeClassHierarchyNodeKey(ClassPathPartName, ENativeClassHierarchyNodeType::Folder)) : RootNodes.FindRef(ClassPathPartName);
				}

				// ... bail out if we didn't find a valid node
				if(!CurrentNode.IsValid())
				{
					break;
				}
			}

			if(CurrentNode.IsValid())
			{
				OutMatchingNodes.Add(CurrentNode.ToSharedRef());
			}
		}
	}
}

void FNativeClassHierarchy::PopulateHierarchy()
{
	FAddClassMetrics AddClassMetrics;

	RootNodes.Empty();

	TSet<FName> GameModules = GetGameModules();
	
	for(TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* const CurrentClass = *ClassIt;
		AddClass(CurrentClass, GameModules, AddClassMetrics);
	}

	UE_LOG(LogNativeClassHierarchy, Verbose, TEXT("Native class hierarchy populated in %0.4f seconds. Added %d classes and %d folders."), FPlatformTime::Seconds() - AddClassMetrics.StartTime, AddClassMetrics.NumClassesAdded, AddClassMetrics.NumFoldersAdded);

	ClassHierarchyUpdatedDelegate.Broadcast();
}

void FNativeClassHierarchy::AddClassesForModule(const FName& InModuleName)
{
	FAddClassMetrics AddClassMetrics;

	// Find the class package for this module
	UPackage* const ClassPackage = FindPackage(nullptr, *(FString("/Script/") + InModuleName.ToString()));
	if(!ClassPackage)
	{
		return;
	}

	TSet<FName> GameModules = GetGameModules();
	
	TArray<UObject*> PackageObjects;
	GetObjectsWithOuter(ClassPackage, PackageObjects, false);
	for(UObject* Object : PackageObjects)
	{
		UClass* const CurrentClass = Cast<UClass>(Object);
		if(CurrentClass)
		{
			AddClass(CurrentClass, GameModules, AddClassMetrics);
		}
	}

	UE_LOG(LogNativeClassHierarchy, Verbose, TEXT("Native class hierarchy updated for '%s' in %0.4f seconds. Added %d classes and %d folders."), *InModuleName.ToString(), FPlatformTime::Seconds() - AddClassMetrics.StartTime, AddClassMetrics.NumClassesAdded, AddClassMetrics.NumFoldersAdded);

	ClassHierarchyUpdatedDelegate.Broadcast();
}

void FNativeClassHierarchy::RemoveClassesForModule(const FName& InModuleName)
{
	// Modules always exist directly under a root
	for(const auto& RootNode : RootNodes)
	{
		TSharedPtr<FNativeClassHierarchyNode> ModuleNode = RootNode.Value->Children.FindRef(FNativeClassHierarchyNodeKey(InModuleName, ENativeClassHierarchyNodeType::Folder));
		if(ModuleNode.IsValid())
		{
			// Remove this module from its root
			RootNode.Value->Children.Remove(FNativeClassHierarchyNodeKey(InModuleName, ENativeClassHierarchyNodeType::Folder));

			// If this module was the only child of this root, then we need to remove the root as well
			if(RootNode.Value->Children.Num() == 0)
			{
				RootNodes.Remove(RootNode.Key);
			}

			ClassHierarchyUpdatedDelegate.Broadcast();

			// We've found the module - break
			break;
		}
	}
}

void FNativeClassHierarchy::AddClass(UClass* InClass, const TSet<FName>& InGameModules, FAddClassMetrics& AddClassMetrics)
{
	// Ignore deprecated and temporary classes
	if(InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || FKismetEditorUtilities::IsClassABlueprintSkeleton(InClass))
	{
		return;
	}

	const FName ClassModuleName = GetClassModuleName(InClass);
	if(ClassModuleName.IsNone())
	{
		return;
	}

	static const FName ModuleRelativePathMetaDataKey = "ModuleRelativePath";
	const FString& ClassModuleRelativeIncludePath = InClass->GetMetaData(ModuleRelativePathMetaDataKey);
	if(ClassModuleRelativeIncludePath.IsEmpty())
	{
		return;
	}

	// Work out which root this class should go under
	EPluginLoadedFrom WhereLoadedFrom;
	const FName RootNodeName = GetClassPathRootForModule(ClassModuleName, InGameModules, WhereLoadedFrom);

	// Work out the final path to this class within the hierarchy (which isn't the same as the path on disk)
	const FString ClassModuleRelativePath = ClassModuleRelativeIncludePath.Left(ClassModuleRelativeIncludePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
	const FString ClassHierarchyPath = ClassModuleName.ToString() + TEXT("/") + ClassModuleRelativePath;

	// Ensure we've added a valid root node
	TSharedPtr<FNativeClassHierarchyNode>& RootNode = RootNodes.FindOrAdd(RootNodeName);
	if(!RootNode.IsValid())
	{
		RootNode = FNativeClassHierarchyNode::MakeFolderEntry(RootNodeName, TEXT("/") + RootNodeName.ToString());
		RootNode->LoadedFrom = WhereLoadedFrom;
		++AddClassMetrics.NumFoldersAdded;
	}

	// Split the class path and ensure we have nodes for each part
	TArray<FString> HierarchyPathParts;
	ClassHierarchyPath.ParseIntoArray(HierarchyPathParts, TEXT("/"), true);
	TSharedPtr<FNativeClassHierarchyNode> CurrentNode = RootNode;
	for(const FString& HierarchyPathPart : HierarchyPathParts)
	{
		const FName HierarchyPathPartName = *HierarchyPathPart;
		TSharedPtr<FNativeClassHierarchyNode>& ChildNode = CurrentNode->Children.FindOrAdd(FNativeClassHierarchyNodeKey(HierarchyPathPartName, ENativeClassHierarchyNodeType::Folder));
		if(!ChildNode.IsValid())
		{
			ChildNode = FNativeClassHierarchyNode::MakeFolderEntry(HierarchyPathPartName, CurrentNode->EntryPath + TEXT("/") + HierarchyPathPart);
			++AddClassMetrics.NumFoldersAdded;
		}
		CurrentNode = ChildNode;
	}

	// Now add the final entry for the class
	CurrentNode->AddChild(FNativeClassHierarchyNode::MakeClassEntry(InClass, ClassModuleName, ClassModuleRelativePath, CurrentNode->EntryPath + TEXT("/") + InClass->GetName()));
	++AddClassMetrics.NumClassesAdded;
}

void FNativeClassHierarchy::AddFolder(const FString& InClassPath)
{
	bool bHasAddedFolder = false;

	// Split the class path and ensure we have nodes for each part
	TArray<FString> ClassPathParts;
	InClassPath.ParseIntoArray(ClassPathParts, TEXT("/"), true);
	TSharedPtr<FNativeClassHierarchyNode> CurrentNode;
	for(const FString& ClassPathPart : ClassPathParts)
	{
		const FName ClassPathPartName = *ClassPathPart;
		TSharedPtr<FNativeClassHierarchyNode>& ChildNode = (CurrentNode.IsValid()) ? CurrentNode->Children.FindOrAdd(FNativeClassHierarchyNodeKey(ClassPathPartName, ENativeClassHierarchyNodeType::Folder)) : RootNodes.FindOrAdd(ClassPathPartName);
		if(!ChildNode.IsValid())
		{
			ChildNode = FNativeClassHierarchyNode::MakeFolderEntry(ClassPathPartName, CurrentNode->EntryPath + TEXT("/") + ClassPathPart);
			bHasAddedFolder = true;
		}
		CurrentNode = ChildNode;
	}

	if(bHasAddedFolder)
	{
		ClassHierarchyUpdatedDelegate.Broadcast();
	}
}

bool FNativeClassHierarchy::GetFileSystemPath(const FString& InClassPath, FString& OutFileSystemPath) const
{
	// Split the class path into its component parts
	TArray<FString> ClassPathParts;
	InClassPath.ParseIntoArray(ClassPathParts, TEXT("/"), true);
	
	// We need to have at least two sections (a root, and a module name) to be able to resolve a file system path
	if(ClassPathParts.Num() < 2)
	{
		return false;
	}

	// Is this path using a known root?
	TSharedPtr<FNativeClassHierarchyNode> RootNode = RootNodes.FindRef(*ClassPathParts[0]);
	if(!RootNode.IsValid())
	{
		return false;
	}

	// Is this path using a known module within that root?
	TSharedPtr<FNativeClassHierarchyNode> ModuleNode = RootNode->Children.FindRef(FNativeClassHierarchyNodeKey(*ClassPathParts[1], ENativeClassHierarchyNodeType::Folder));
	if(!ModuleNode.IsValid())
	{
		return false;
	}

	// Get the base file path to the module, and then append any remaining parts of the class path (as the remaining parts mirror the file system)
	if(FSourceCodeNavigation::FindModulePath(ClassPathParts[1], OutFileSystemPath))
	{
		for(int32 PathPartIndex = 2; PathPartIndex < ClassPathParts.Num(); ++PathPartIndex)
		{
			OutFileSystemPath /= ClassPathParts[PathPartIndex];
		}
		return true;
	}

	return false;
}

bool FNativeClassHierarchy::GetClassPath(const UClass* InClass, FString& OutClassPath, FNativeClassHierarchyGetClassPathCache& InCache, const bool bIncludeClassName) const
{
	const FName ClassModuleName = GetClassModuleName(InClass);
	if(ClassModuleName.IsNone())
	{
		return false;
	}

	static const FName ModuleRelativePathMetaDataKey = "ModuleRelativePath";
	const FString& ClassModuleRelativeIncludePath = InClass->GetMetaData(ModuleRelativePathMetaDataKey);
	if(ClassModuleRelativeIncludePath.IsEmpty())
	{
		return false;
	}

	if (InCache.GameModules.Num() == 0)
	{
		InCache.GameModules = GetGameModules();
	}

	// Work out which root this class should go under
	EPluginLoadedFrom WhereLoadedFrom;
	const FName RootNodeName = GetClassPathRootForModule(ClassModuleName, InCache.GameModules, WhereLoadedFrom);

	// Work out the final path to this class within the hierarchy (which isn't the same as the path on disk)
	const FString ClassModuleRelativePath = ClassModuleRelativeIncludePath.Left(ClassModuleRelativeIncludePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
	OutClassPath = FString(TEXT("/")) + RootNodeName.ToString() + TEXT("/") + ClassModuleName.ToString();

	if(!ClassModuleRelativePath.IsEmpty())
	{
		OutClassPath += TEXT("/") + ClassModuleRelativePath;
	}

	if(bIncludeClassName)
	{
		OutClassPath += TEXT("/") + InClass->GetName();
	}

	return true;
}

void FNativeClassHierarchy::OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{
	switch(InModuleChangeReason)
	{
	case EModuleChangeReason::ModuleLoaded:
		AddClassesForModule(InModuleName);
		break;

	case EModuleChangeReason::ModuleUnloaded:
		RemoveClassesForModule(InModuleName);
		break;

	default:
		break;
	}
}

void FNativeClassHierarchy::OnReloadComplete(EReloadCompleteReason Reason)
{
	PopulateHierarchy();
}

FName FNativeClassHierarchy::GetClassModuleName(const UClass* InClass)
{
	UPackage* const ClassPackage = InClass->GetOuterUPackage();

	if(ClassPackage)
	{
		return FPackageName::GetShortFName(ClassPackage->GetFName());
	}

	return NAME_None;
}

FName FNativeClassHierarchy::GetClassPathRootForModule(const FName& InModuleName, const TSet<FName>& InGameModules, EPluginLoadedFrom& OutWhereLoadedFrom)
{
	static const FName EngineRootNodeName = "Classes_Engine";
	static const FName GameRootNodeName = "Classes_Game";

	// Work out which root this class should go under (anything that isn't a game or plugin module goes under engine)
	FName RootNodeName = EngineRootNodeName;
	OutWhereLoadedFrom = EPluginLoadedFrom::Engine;

	if(InGameModules.Contains(InModuleName))
	{
		RootNodeName = GameRootNodeName;
		OutWhereLoadedFrom = EPluginLoadedFrom::Project;
	}
	else
	{
		TSharedPtr<IPlugin> ModulePlugin = IPluginManager::Get().GetModuleOwnerPlugin(InModuleName);
		if (ModulePlugin.IsValid())
		{
			RootNodeName = FName(*(TEXT("Classes_") + ModulePlugin->GetName()));
			OutWhereLoadedFrom = ModulePlugin->GetLoadedFrom();
		}
	}

	return RootNodeName;
}

TSet<FName> FNativeClassHierarchy::GetGameModules()
{
	TSet<FName> GameModules;

	if (FApp::HasProjectName())
	{
		if (const FProjectDescriptor* const CurrentProject = IProjectManager::Get().GetCurrentProject())
		{
			for (const FModuleDescriptor& Module : CurrentProject->Modules)
			{
				GameModules.Add(Module.Name);
			}
		}
	}

	return GameModules;
}
