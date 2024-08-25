// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMGraphFunctionHost.generated.h"

struct FRigVMGraphFunctionStore;

UINTERFACE()
class RIGVM_API URigVMGraphFunctionHost : public UInterface
{
	GENERATED_BODY()
};

// Interface that allows an object to host a rig VM graph function Host.
class RIGVM_API IRigVMGraphFunctionHost
{
	GENERATED_BODY()

public:
	
	// IRigVMClientHost interface
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() = 0;
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const = 0;
};

// A management struct containing graph functions
USTRUCT()
struct RIGVM_API FRigVMGraphFunctionStore
{
public:

	GENERATED_BODY()

	/** Exposed public functions on this rig */
	UPROPERTY()
	TArray<FRigVMGraphFunctionData> PublicFunctions;

	UPROPERTY(Transient)
	TArray<FRigVMGraphFunctionData> PrivateFunctions;

	const FRigVMGraphFunctionData* FindFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic = nullptr) const;

	FRigVMGraphFunctionData* FindFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic = nullptr);

	FRigVMGraphFunctionData* FindFunctionByName(const FName& Name, bool *bOutIsPublic = nullptr);

	bool ContainsFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer) const;

	bool IsFunctionPublic(const FRigVMGraphFunctionIdentifier& InLibraryPointer) const;

	FRigVMGraphFunctionData* AddFunction(const FRigVMGraphFunctionHeader& FunctionHeader, bool bIsPublic);

	bool RemoveFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bIsPublic = nullptr);

	bool MarkFunctionAsPublic(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool bIsPublic);

	FRigVMGraphFunctionData* UpdateFunctionInterface(const FRigVMGraphFunctionHeader& Header);

	bool UpdateDependencies(const FRigVMGraphFunctionIdentifier& InLibraryPointer, TMap<FRigVMGraphFunctionIdentifier, uint32>& Dependencies);

	bool UpdateExternalVariables(const FRigVMGraphFunctionIdentifier& InLibraryPointer, TArray<FRigVMExternalVariable> ExternalVariables);

	bool UpdateFunctionCompilationData(const FRigVMGraphFunctionIdentifier& InLibraryPointer, const FRigVMFunctionCompilationData& CompilationData);

	bool RemoveFunctionCompilationData(const FRigVMGraphFunctionIdentifier& InLibraryPointer);

	bool RemoveAllCompilationData();

	void PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName);

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionStore& Host)
	{
		Ar << Host.PublicFunctions;
		
		// This is only added to make sure SoftObjectPaths can be gathered and fixed up in the case of asset rename
		// It should not affect data on disk
		if (Ar.IsObjectReferenceCollector())
		{
			Ar << Host.PrivateFunctions;
		}

		return Ar;
	}

	void PostLoad()
	{
		for(FRigVMGraphFunctionData& Function: PublicFunctions)
		{
			Function.PatchSharedArgumentOperandsIfRequired();
		}
	}

private:

	const FRigVMGraphFunctionData* FindFunctionImpl(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic = nullptr) const;
};

