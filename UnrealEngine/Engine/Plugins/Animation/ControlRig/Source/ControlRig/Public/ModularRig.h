// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ControlRig.h"
#include "ModularRigModel.h"
#include "ModularRig.generated.h"

struct FRigModuleInstance;

struct CONTROLRIG_API FModuleInstanceHandle
{
public:

	FModuleInstanceHandle()
		: ModularRig(nullptr)
		, Path()
	{}

	FModuleInstanceHandle(const UModularRig* InModularRig, const FString& InPath);
	FModuleInstanceHandle(const UModularRig* InModularRig, const FRigModuleInstance* InElement);

	bool IsValid() const { return Get() != nullptr; }
	operator bool() const { return IsValid(); }
	
	const UModularRig* GetModularRig() const
	{
		if (ModularRig.IsValid())
		{
			return ModularRig.Get();
		}
		return nullptr;
	}
	const FString& GetPath() const { return Path; }

	const FRigModuleInstance* Get() const;

private:

	mutable TSoftObjectPtr<UModularRig> ModularRig;
	FString Path;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleInstance 
{
	GENERATED_USTRUCT_BODY()

public:
	
	FRigModuleInstance()
	: Name(NAME_None)
	, RigPtr(nullptr)
	, ParentPath(FString())
	{
	}

	UPROPERTY()
	FName Name;

private:

	UPROPERTY(transient)
	mutable TObjectPtr<UControlRig> RigPtr;

public:
	
	UPROPERTY()
	FString ParentPath;

	UPROPERTY()
	TMap<FName, FRigVMExternalVariable> VariableBindings;

	TArray<FRigModuleInstance*> CachedChildren;
	mutable const FRigConnectorElement* PrimaryConnector = nullptr;

	FString GetShortName() const;
	FString GetLongName() const
	{
		return GetPath();
	}
	FString GetPath() const;
	FString GetNamespace() const;
	UControlRig* GetRig() const;
	void SetRig(UControlRig* InRig);
	bool ContainsRig(const UControlRig* InRig) const;
	const FRigModuleReference* GetModuleReference() const;
	const FRigModuleInstance* GetParentModule() const;
	const FRigModuleInstance* GetRootModule() const;
	const FRigConnectorElement* FindPrimaryConnector() const;
	TArray<const FRigConnectorElement*> FindConnectors() const;
	bool IsRootModule() const;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigModuleExecutionElement
{
	GENERATED_USTRUCT_BODY()

	FRigModuleExecutionElement()
		: ModulePath(FString())
		, ModuleInstance(nullptr)
		, EventName(NAME_None)
		, bExecuted(false)
	{}

	FRigModuleExecutionElement(FRigModuleInstance* InModule, FName InEvent)
		: ModulePath(InModule->GetPath())
		, ModuleInstance(InModule)
		, EventName(InEvent)
		, bExecuted(false)
	{}

	UPROPERTY()
	FString ModulePath;
	FRigModuleInstance* ModuleInstance;

	UPROPERTY()
	FName EventName;

	UPROPERTY()
	bool bExecuted;
};

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UModularRig : public UControlRig
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FRigModuleInstance> Modules;
	TArray<FRigModuleInstance*> RootModules;

	mutable TArray<FName> SupportedEvents;

public:

	virtual void PostInitProperties() override;

	// BEGIN ControlRig
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void InitializeVMs(bool bRequestInit = true) override;
	virtual bool InitializeVMs(const FName& InEventName) override;
	virtual void InitializeVMsFromCDO() override { URigVMHost::InitializeFromCDO(); }
	virtual void InitializeFromCDO() override;
	virtual void RequestInitVMs() override { URigVMHost::RequestInit(); }
	virtual bool Execute_Internal(const FName& InEventName) override;
	virtual void Evaluate_AnyThread() override;
	virtual FRigElementKeyRedirector& GetElementKeyRedirector() override { return ElementKeyRedirector; }
	virtual FRigElementKeyRedirector GetElementKeyRedirector() const override { return ElementKeyRedirector; }
	virtual bool SupportsEvent(const FName& InEventName) const override;
	virtual const TArray<FName>& GetSupportedEvents() const override;
	// END ControlRig

	UPROPERTY()
	FModularRigSettings ModularRigSettings;

	// Returns the settings of the modular rig
	const FModularRigSettings& GetModularRigSettings() const;

	UPROPERTY()
	FModularRigModel ModularRigModel;

	UPROPERTY()
	TArray<FRigModuleExecutionElement> ExecutionQueue;
	int32 ExecutionQueueFront = 0;
	void ExecuteQueue();
	void ResetExecutionQueue();

	// BEGIN UObject
	virtual void BeginDestroy() override;
	// END UObject
	
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	void ResetModules(bool bDestroyModuleRigs = true);

	const FModularRigModel& GetModularRigModel() const;
	void UpdateModuleHierarchyFromCDO();
	void UpdateCachedChildren();
	void UpdateSupportedEvents() const;

	const FRigModuleInstance* FindModule(const FString& InPath) const;
	const FRigModuleInstance* FindModule(const UControlRig* InModuleInstance) const;
	const FRigModuleInstance* FindModule(const FRigBaseElement* InElement) const;
	const FRigModuleInstance* FindModule(const FRigElementKey& InElementKey) const;
	FString GetParentPath(const FString& InPath) const;
	FString GetShortestDisplayPathForElement(const FRigElementKey& InElementKey, bool bAlwaysShowNameSpace) const;

	void ForEachModule(TFunctionRef<bool(FRigModuleInstance*)> PerModuleFunction);
	void ForEachModule(TFunctionRef<bool(const FRigModuleInstance*)> PerModuleFunction) const;

	void ExecuteConnectorEvent(const FRigElementKey& InConnector, const FRigModuleInstance* InModuleInstance, const FRigElementKeyRedirector* InRedirector, TArray<FRigElementResolveResult>& InOutCandidates);

	/**
	 * Returns a handle to an existing module
	 * @param InPath The path of the module to retrieve a handle for.
	 * @return The retrieved handle (may be invalid)
	 */
	FModuleInstanceHandle GetHandle(const FString& InPath) const
	{
		if(FindModule(InPath))
		{
			return FModuleInstanceHandle((UModularRig*)this, InPath);
		}
		return FModuleInstanceHandle();
	}

	static const FString NamespaceSeparator;

protected:

	virtual void RunPostConstructionEvent() override;

private:

	/** Adds a module to the rig*/
	FRigModuleInstance* AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, const FRigModuleInstance* InParent, const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues);

	/** Updates the module's variable bindings */
	bool SetModuleVariableBindings(const FString& InModulePath, const TMap<FName, FString>& InVariableBindings);

	/** Destroys / discards a module instance rig */
	static void DiscardModuleRig(UControlRig* InControlRig);

	TMap<FString, UControlRig*> PreviousModuleRigs;

	void ResetShortestDisplayPathCache() const;
	void RecomputeShortestDisplayPathCache() const;
	mutable TMap<FRigElementKey, TTuple<FString, FString>> ElementKeyToShortestDisplayPath; 

	friend struct FRigModuleInstance;
};
