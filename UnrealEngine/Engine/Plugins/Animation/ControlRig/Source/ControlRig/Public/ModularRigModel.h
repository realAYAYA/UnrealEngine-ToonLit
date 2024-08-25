// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRig.h"
#include "ModularRigModel.generated.h"

class UModularRigController;

UENUM()
enum class EModularRigNotification : uint8
{
	ModuleAdded,

	ModuleRenamed,

	ModuleRemoved,

	ModuleReparented,

	ConnectionChanged,

	ModuleConfigValueChanged,

	ModuleShortNameChanged,

	InteractionBracketOpened, // A bracket has been opened
	
	InteractionBracketClosed, // A bracket has been opened
	
	InteractionBracketCanceled, // A bracket has been canceled

	/** MAX - invalid */
	Max UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleReference
{
	GENERATED_BODY()

	FRigModuleReference()
		: Name(NAME_None)
		, ShortName()
		, bShortNameBasedOnPath(true)
		, ParentPath(FString())
		, Class(nullptr)
	{}
	
	FRigModuleReference(const FName& InName, TSubclassOf<UControlRig> InClass, const FString& InParentPath)
		: Name(InName)
		, ShortName()
		, bShortNameBasedOnPath(true)
		, ParentPath(InParentPath)
		, Class(InClass)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString ShortName;

	UPROPERTY()
	bool bShortNameBasedOnPath;

	UPROPERTY()
	FString ParentPath;

	UPROPERTY()
	TSoftClassPtr<UControlRig> Class;

	UPROPERTY(meta = (DeprecatedProperty))
	TMap<FRigElementKey, FRigElementKey> Connections_DEPRECATED; // Connectors to Connection element

	UPROPERTY()
	TMap<FName, FString> ConfigValues;

	UPROPERTY()
	TMap<FName, FString> Bindings; // ExternalVariableName (current module) -> SourceExternalVariableNamespacedPath (root rig or other module)

	UPROPERTY(transient)
	FName PreviousName;

	UPROPERTY(transient)
	FString PreviousParentPath;

	TArray<FRigModuleReference*> CachedChildren;

	FString GetShortName() const;

	FString GetLongName() const
	{
		return GetPath();
	}

	FString GetPath() const;

	FString GetNamespace() const;

	bool HasParentModule() const { return !ParentPath.IsEmpty(); }

	bool IsRootModule() const { return !HasParentModule(); }
	
	friend bool operator==(const FRigModuleReference& A, const FRigModuleReference& B)
	{
		return A.ParentPath == B.ParentPath &&
			A.Name == B.Name;
	}

	const FRigConnectorElement* FindPrimaryConnector(const URigHierarchy* InHierarchy) const;
	TArray<const FRigConnectorElement*> FindConnectors(const URigHierarchy* InHierarchy) const;

	friend class UModularRigController;
};

USTRUCT(BlueprintType)
struct FModularRigSingleConnection
{
	GENERATED_BODY()

	FModularRigSingleConnection()
		: Connector(FRigElementKey()), Target(FRigElementKey()) {}
	
	FModularRigSingleConnection(const FRigElementKey& InConnector, const FRigElementKey& InTarget)
		: Connector(InConnector), Target(InTarget) {}

	UPROPERTY()
	FRigElementKey Connector;

	UPROPERTY()
	FRigElementKey Target;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FModularRigConnections
{
public:
	
	GENERATED_BODY()
	/** Connections sorted by creation order */

private:
	
	UPROPERTY()
	TArray<FModularRigSingleConnection> ConnectionList;

	/** Target key to connector array */
	TMap<FRigElementKey, TArray<FRigElementKey>> ReverseConnectionMap;

public:

	const TArray<FModularRigSingleConnection>& GetConnectionList() const { return ConnectionList; }

	bool IsEmpty() const { return ConnectionList.IsEmpty(); }
	int32 Num() const { return ConnectionList.Num(); }
	const FModularRigSingleConnection& operator[](int32 InIndex) const { return ConnectionList[InIndex]; }
	FModularRigSingleConnection& operator[](int32 InIndex) { return ConnectionList[InIndex]; }
	TArray<FModularRigSingleConnection>::RangedForIteratorType begin() { return ConnectionList.begin(); }
	TArray<FModularRigSingleConnection>::RangedForIteratorType end() { return ConnectionList.end(); }
	TArray<FModularRigSingleConnection>::RangedForConstIteratorType begin() const { return ConnectionList.begin(); }
	TArray<FModularRigSingleConnection>::RangedForConstIteratorType end() const { return ConnectionList.end(); }

	void UpdateFromConnectionList()
	{
		ReverseConnectionMap.Reset();
		for (const FModularRigSingleConnection& Connection : ConnectionList)
		{
			TArray<FRigElementKey>& Connectors = ReverseConnectionMap.FindOrAdd(Connection.Target);
			Connectors.AddUnique(Connection.Connector);
		}
	}

	void AddConnection(const FRigElementKey& Connector, const FRigElementKey& Target)
	{
		// Remove any existing connection
		RemoveConnection(Connector);

		ConnectionList.Add(FModularRigSingleConnection(Connector, Target));
		ReverseConnectionMap.FindOrAdd(Target).AddUnique(Connector);
	}

	void RemoveConnection(const FRigElementKey& Connector)
	{
		int32 ExistingIndex = FindConnectionIndex(Connector);
		if (ConnectionList.IsValidIndex(ExistingIndex))
		{
			if (TArray<FRigElementKey>* Connectors = ReverseConnectionMap.Find(ConnectionList[ExistingIndex].Target))
			{
				*Connectors = Connectors->FilterByPredicate([Connector](const FRigElementKey& TargetConnector)
				{
					return TargetConnector != Connector;
				});
				if (Connectors->IsEmpty())
				{
					ReverseConnectionMap.Remove(ConnectionList[ExistingIndex].Target);
				}
			}
			ConnectionList.RemoveAt(ExistingIndex);
		}
	}

	int32 FindConnectionIndex(const FRigElementKey& InConnectorKey) const
	{
		return ConnectionList.IndexOfByPredicate([InConnectorKey](const FModularRigSingleConnection& Connection)
		{
			return InConnectorKey == Connection.Connector;
		});
	}

	FRigElementKey FindTargetFromConnector(const FRigElementKey& InConnectorKey) const
	{
		int32 Index = FindConnectionIndex(InConnectorKey);
		if (ConnectionList.IsValidIndex(Index))
		{
			return ConnectionList[Index].Target;
		}
		static const FRigElementKey EmptyKey;
		return EmptyKey;
	}

	const TArray<FRigElementKey>& FindConnectorsFromTarget(const FRigElementKey& InTargetKey) const
	{
		if(const TArray<FRigElementKey>* Connectors = ReverseConnectionMap.Find(InTargetKey))
		{
			return *Connectors;
		}
		static const TArray<FRigElementKey> EmptyList;
		return EmptyList;
	}

	bool HasConnection(const FRigElementKey& InConnectorKey) const
	{
		return ConnectionList.IsValidIndex(FindConnectionIndex(InConnectorKey));
	}

	bool HasConnection(const FRigElementKey& InConnectorKey, const URigHierarchy* InHierarchy) const
	{
		if(HasConnection(InConnectorKey))
		{
			const FRigElementKey Target = FindTargetFromConnector(InConnectorKey);
			return InHierarchy->Contains(Target);
		}
		return false;
	}

	/** Gets the connection map for a single module, where the connectors are identified without its namespace*/
	TMap<FRigElementKey, FRigElementKey> GetModuleConnectionMap(const FString& InModulePath) const;
};

// A management struct containing all modules in the rig
USTRUCT(BlueprintType)
struct CONTROLRIG_API FModularRigModel
{
public:

	GENERATED_BODY()

	FModularRigModel(){}
	FModularRigModel(const FModularRigModel& Other)
	{
		Modules = Other.Modules;
		Connections = Other.Connections;
		UpdateCachedChildren();
		Connections.UpdateFromConnectionList();
	}
	
	FModularRigModel& operator=(const FModularRigModel& Other)
	{
		Modules = Other.Modules;
		Connections = Other.Connections;
		UpdateCachedChildren();
		Connections.UpdateFromConnectionList();
		return *this;
	}
	FModularRigModel(FModularRigModel&&) = delete;
	FModularRigModel& operator=(FModularRigModel&&) = delete;

	UPROPERTY()
	TArray<FRigModuleReference> Modules;
	TArray<FRigModuleReference*> RootModules;
	TArray<FRigModuleReference> DeletedModules;

	UPROPERTY()
	FModularRigConnections Connections;

	UPROPERTY(transient)
	TObjectPtr<UObject> Controller;

	void PatchModelsOnLoad();

	UModularRigController* GetController(bool bCreateIfNeeded = true);

	UObject* GetOuter() const { return OuterClientHost.IsValid() ? OuterClientHost.Get() : nullptr; }

	void SetOuterClientHost(UObject* InOuterClientHost);

	void UpdateCachedChildren();

	FRigModuleReference* FindModule(const FString& InPath);
	const FRigModuleReference* FindModule(const FString& InPath) const;

	FRigModuleReference* GetParentModule(const FString& InPath);
	const FRigModuleReference* GetParentModule(const FString& InPath) const;

	FRigModuleReference* GetParentModule(const FRigModuleReference* InChildModule);
	const FRigModuleReference* GetParentModule(const FRigModuleReference* InChildModule) const;

	FString GetParentPath(const FString& InPath) const;

	void ForEachModule(TFunction<bool(const FRigModuleReference*)> PerModule) const;

	TArray<FString> SortPaths(const TArray<FString>& InPaths) const;

	bool IsModuleParentedTo(const FString& InChildModulePath, const FString& InParentModulePath) const;
	
	bool IsModuleParentedTo(const FRigModuleReference* InChildModule, const FRigModuleReference* InParentModule) const;

private:
	TWeakObjectPtr<UObject> OuterClientHost;

	friend class UModularRigController;
	friend struct FRigModuleReference;
};