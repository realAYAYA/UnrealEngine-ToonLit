// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMGraphFunctionHost.h"

const FRigVMGraphFunctionData* FRigVMGraphFunctionStore::FindFunctionImpl(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic) const
{
	const FRigVMGraphFunctionData* Info = PublicFunctions.FindByPredicate([InLibraryPointer](const FRigVMGraphFunctionData& Info)
	{
		return Info.Header.LibraryPointer == InLibraryPointer;
	});
	if (Info)
	{
		if (bOutIsPublic)
		{
			(*bOutIsPublic) = true; 
		}
		return Info;
	}

	Info = PrivateFunctions.FindByPredicate([InLibraryPointer](const FRigVMGraphFunctionData& Info)
	{
		return Info.Header.LibraryPointer == InLibraryPointer;
	});
	if (Info)
	{
		if (bOutIsPublic)
		{
			(*bOutIsPublic) = false; 
		}
		return Info;
	}
	return nullptr;
}

const FRigVMGraphFunctionData* FRigVMGraphFunctionStore::FindFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic) const
{
	return FindFunctionImpl(InLibraryPointer, bOutIsPublic);
}

FRigVMGraphFunctionData* FRigVMGraphFunctionStore::FindFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bOutIsPublic)
{
	return const_cast<FRigVMGraphFunctionData*>(FindFunctionImpl(InLibraryPointer, bOutIsPublic));
}

FRigVMGraphFunctionData* FRigVMGraphFunctionStore::FindFunctionByName(const FName& Name, bool* bOutIsPublic)
{
	FRigVMGraphFunctionData* Info = PublicFunctions.FindByPredicate([Name](const FRigVMGraphFunctionData& Info)
	{
		return Info.Header.Name == Name;
	});
	if (Info)
	{
		if (bOutIsPublic)
		{
			(*bOutIsPublic) = true; 
		}
		return Info;
	}

	Info = PrivateFunctions.FindByPredicate([Name](const FRigVMGraphFunctionData& Info)
	{
		return Info.Header.Name == Name;
	});
	if (Info)
	{
		if (bOutIsPublic)
		{
			(*bOutIsPublic) = false; 
		}
		return Info;
	}
	return nullptr;
}

bool FRigVMGraphFunctionStore::ContainsFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer) const
{
	if(FindFunction(InLibraryPointer))
	{
		return true;
	}
	return false;
}

bool FRigVMGraphFunctionStore::IsFunctionPublic(const FRigVMGraphFunctionIdentifier& InLibraryPointer) const
{
	bool bIsPublic;
	if (FindFunction(InLibraryPointer, &bIsPublic))
	{
		return bIsPublic;
	}
	return false;
}

FRigVMGraphFunctionData* FRigVMGraphFunctionStore::AddFunction(const FRigVMGraphFunctionHeader& FunctionHeader, bool bIsPublic)
{
	// Fail if the function already exists
	if (ContainsFunction(FunctionHeader.LibraryPointer))
	{
		return nullptr;
	}

	FRigVMGraphFunctionData NewInfo(FunctionHeader);
	if(bIsPublic)
	{
		int32 Index = PublicFunctions.Add(NewInfo);
		return &PublicFunctions[Index];
	}
	else
	{
		int32 Index =  PrivateFunctions.Add(NewInfo);
		return &PrivateFunctions[Index];
	}

	return nullptr;
}

bool FRigVMGraphFunctionStore::RemoveFunction(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool* bIsPublic)
{
	int32 NumRemoved = PublicFunctions.RemoveAll([InLibraryPointer](const FRigVMGraphFunctionData& Info)
	{
		return Info.Header.LibraryPointer == InLibraryPointer;
	});
	if (NumRemoved > 0 && bIsPublic)
	{
		*bIsPublic = true;
	}
		
	if (NumRemoved == 0)
	{
		NumRemoved = PrivateFunctions.RemoveAll([InLibraryPointer](const FRigVMGraphFunctionData& Info)
		{
		   return Info.Header.LibraryPointer == InLibraryPointer;
		});
		if (NumRemoved > 0 && bIsPublic)
		{
			*bIsPublic = false;
		}
	}

	return NumRemoved > 0;
}

bool FRigVMGraphFunctionStore::MarkFunctionAsPublic(const FRigVMGraphFunctionIdentifier& InLibraryPointer, bool bIsPublic)
{
	TArray<FRigVMGraphFunctionData>* OldContainer = nullptr;
	TArray<FRigVMGraphFunctionData>* NewContainer = nullptr;
	if(bIsPublic)
	{
		OldContainer = &PrivateFunctions;
		NewContainer = &PublicFunctions;
	}
	else
	{
		OldContainer = &PublicFunctions;
		NewContainer = &PrivateFunctions;
	}
	
	int32 Index = OldContainer->IndexOfByPredicate([InLibraryPointer](const FRigVMGraphFunctionData& FunctionData)
		{
			return FunctionData.Header.LibraryPointer == InLibraryPointer;
		});
	if(Index == INDEX_NONE)
	{
		return false;
	}
	NewContainer->Add(OldContainer->operator[](Index));
	OldContainer->RemoveAt(Index);
	return true;
}

FRigVMGraphFunctionData* FRigVMGraphFunctionStore::UpdateFunctionInterface(const FRigVMGraphFunctionHeader& Header)
{
	bool bIsPublic;

	if (const FRigVMGraphFunctionData* OldData = FindFunction(Header.LibraryPointer))
	{
		TMap<FRigVMGraphFunctionIdentifier, uint32> Dependencies = OldData->Header.Dependencies;
		TArray<FRigVMExternalVariable> ExternalVariables = OldData->Header.ExternalVariables;
		if (RemoveFunction(Header.LibraryPointer, &bIsPublic))
		{
			if (FRigVMGraphFunctionData* NewData = AddFunction(Header, bIsPublic))
			{
				NewData->Header.Dependencies = Dependencies;
				NewData->Header.ExternalVariables = ExternalVariables;
				return NewData;
			}
		}
	}
	return nullptr;
}

bool FRigVMGraphFunctionStore::UpdateDependencies(const FRigVMGraphFunctionIdentifier& InLibraryPointer, TMap<FRigVMGraphFunctionIdentifier, uint32>& Dependencies)
{
	if (FRigVMGraphFunctionData* Data = FindFunction(InLibraryPointer))
	{
		FRigVMGraphFunctionHeader& Header = Data->Header;
		
		// Check if they are the same
		if (Header.Dependencies.Num() == Dependencies.Num())
		{
			bool bFoundDifference = false;
			for (const TPair<FRigVMGraphFunctionIdentifier, uint32>& Pair : Dependencies)
			{
				if (uint32* Hash = Header.Dependencies.Find(Pair.Key))
				{
					if (*Hash != Pair.Value)
					{
						bFoundDifference = true;
						break;
					}
				}
				else
				{
					bFoundDifference = true;
					break;
				}
			}
			if (!bFoundDifference)
			{
				return false;
			}
		}
		
		Data->Header.Dependencies = Dependencies;		
		return true;
	}
	return false;
}

bool FRigVMGraphFunctionStore::UpdateExternalVariables(const FRigVMGraphFunctionIdentifier& InLibraryPointer, TArray<FRigVMExternalVariable> ExternalVariables)
{
	if (FRigVMGraphFunctionData* Data = FindFunction(InLibraryPointer))
	{
		FRigVMGraphFunctionHeader& Header = Data->Header;

		// Check if they are the same
		if (Header.ExternalVariables.Num() == ExternalVariables.Num())
		{
			bool bFoundDifference = false;
			for (const FRigVMExternalVariable& Variable : ExternalVariables)
			{
				if (!Header.ExternalVariables.ContainsByPredicate([Variable](const FRigVMExternalVariable& ExternalVariable)
				{
					return Variable.Name == ExternalVariable.Name &&
							Variable.TypeName == ExternalVariable.TypeName &&
							Variable.TypeObject == ExternalVariable.TypeObject &&
							Variable.bIsArray == ExternalVariable.bIsArray &&
							Variable.bIsPublic == ExternalVariable.bIsPublic &&
							Variable.bIsReadOnly == ExternalVariable.bIsReadOnly &&
							Variable.Size == ExternalVariable.Size;
				}))
				{
					bFoundDifference = true;
					break;
				}
			}
			if (!bFoundDifference)
			{
				return false;
			}
		}
		
		Header.ExternalVariables = ExternalVariables;
		return true;
	}
	return false;
}

bool FRigVMGraphFunctionStore::UpdateFunctionCompilationData(const FRigVMGraphFunctionIdentifier& InLibraryPointer, const FRigVMFunctionCompilationData& CompilationData)
{
	FRigVMGraphFunctionData* Info = FindFunction(InLibraryPointer);
	if (Info)
	{
		if (Info->CompilationData.Hash == CompilationData.Hash)
		{
			return false;
		}
		
		Info->CompilationData = CompilationData;
		return true;
	}

	return false;
}

bool FRigVMGraphFunctionStore::RemoveFunctionCompilationData(const FRigVMGraphFunctionIdentifier& InLibraryPointer)
{
	FRigVMGraphFunctionData* FunctionData = FindFunction(InLibraryPointer);
	if (FunctionData)
	{
		FunctionData->ClearCompilationData();
		return true;
	}
	return false;
}

bool FRigVMGraphFunctionStore::RemoveAllCompilationData()
{
	for (FRigVMGraphFunctionData& Data : PublicFunctions)
	{
		Data.ClearCompilationData();
	}

	for (FRigVMGraphFunctionData& Data : PrivateFunctions)
	{
		Data.ClearCompilationData();
	}

	return true;
}

void FRigVMGraphFunctionStore::PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName)
{
	for (FRigVMGraphFunctionData& Data : PublicFunctions)
	{
		Data.Header.PostDuplicateHost(InOldPathName, InNewPathName);
	}

	for (FRigVMGraphFunctionData& Data : PrivateFunctions)
	{
		Data.Header.PostDuplicateHost(InOldPathName, InNewPathName);
	}
}
