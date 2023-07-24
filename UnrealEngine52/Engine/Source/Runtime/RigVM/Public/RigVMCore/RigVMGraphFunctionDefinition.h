// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMByteCode.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "RigVMGraphFunctionDefinition.generated.h"

class IRigVMGraphFunctionHost;
struct FRigVMGraphFunctionData;

USTRUCT()
struct FRigVMFunctionCompilationPropertyDescription
{
	GENERATED_BODY()

	// The name of the property to create
	UPROPERTY()
	FName Name;

	// The complete CPP type to base a new property off of (for ex: 'TArray<TArray<FVector>>')
	UPROPERTY()
	FString CPPType;

	// The tail CPP Type object, for example the UScriptStruct for a struct
	UPROPERTY()
	TSoftObjectPtr<UObject> CPPTypeObject;

	// The default value to use for this property (for example: '(((X=1.000000, Y=2.000000, Z=3.000000)))')
	UPROPERTY()
	FString DefaultValue;

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationPropertyDescription& Description) 
	{
		uint32 Hash = GetTypeHash(Description.Name);
		Hash = HashCombine(Hash, GetTypeHash(Description.CPPType));
		Hash = HashCombine(Hash, GetTypeHash(Description.CPPTypeObject));
		Hash = HashCombine(Hash, GetTypeHash(Description.DefaultValue));
		return Hash;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationPropertyDescription& Data)
	{
		Ar << Data.Name;
		Ar << Data.CPPType;
		Ar << Data.CPPTypeObject;
		Ar << Data.DefaultValue;
		return Ar;
	}
};

USTRUCT()
struct FRigVMFunctionCompilationPropertyPath
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PropertyIndex = INDEX_NONE;

	UPROPERTY()
	FString HeadCPPType;

	UPROPERTY()
	FString SegmentPath;

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationPropertyPath& Path)
	{
		uint32 Hash = GetTypeHash(Path.PropertyIndex);
		Hash = HashCombine(Hash, GetTypeHash(Path.HeadCPPType));
		Hash = HashCombine(Hash, GetTypeHash(Path.SegmentPath));
		return Hash;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationPropertyPath& Data)
	{
		Ar << Data.PropertyIndex;
		Ar << Data.HeadCPPType;
		Ar << Data.SegmentPath;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMFunctionCompilationData
{
	GENERATED_BODY()

	FRigVMFunctionCompilationData() : Hash(0) {}

	UPROPERTY()
	FRigVMByteCode ByteCode;

	UPROPERTY()
	TArray<FName> FunctionNames;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> WorkPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> WorkPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> LiteralPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> LiteralPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> DebugPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> DebugPropertyPathDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyDescription> ExternalPropertyDescriptions;

	UPROPERTY()
	TArray<FRigVMFunctionCompilationPropertyPath> ExternalPropertyPathDescriptions;

	UPROPERTY()
	TMap<int32, FName> ExternalRegisterIndexToVariable;

	UPROPERTY()
	TMap<FString, FRigVMOperand> Operands;

	UPROPERTY()
	uint32 Hash;

	bool IsValid() const
	{
		return Hash != 0;
	}

	friend uint32 GetTypeHash(const FRigVMFunctionCompilationData& Data) 
	{
		uint32 DataHash = Data.ByteCode.GetByteCodeHash();
		for (const FName& Name : Data.FunctionNames)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Name));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.WorkPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.WorkPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.LiteralPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.LiteralPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.DebugPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.DebugPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}

		for (const FRigVMFunctionCompilationPropertyDescription& Description : Data.ExternalPropertyDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Description));
		}
		for (const FRigVMFunctionCompilationPropertyPath& Path : Data.ExternalPropertyPathDescriptions)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Path));
		}
		
		for (const TPair<int32,FName>& Pair : Data.ExternalRegisterIndexToVariable)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Key));
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Value));
		}

		for (const TPair<FString, FRigVMOperand>& Pair : Data.Operands)
		{
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Key));
			DataHash = HashCombine(DataHash, GetTypeHash(Pair.Value));
		}

		return DataHash;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMFunctionCompilationData& Data)
	{
		Ar << Data.ByteCode;
		Ar << Data.FunctionNames;
		Ar << Data.WorkPropertyDescriptions;
		Ar << Data.WorkPropertyPathDescriptions;
		Ar << Data.LiteralPropertyDescriptions;
		Ar << Data.LiteralPropertyPathDescriptions;
		Ar << Data.DebugPropertyDescriptions;
		Ar << Data.DebugPropertyPathDescriptions;
		Ar << Data.ExternalPropertyDescriptions;
		Ar << Data.ExternalPropertyPathDescriptions;
		Ar << Data.ExternalRegisterIndexToVariable;
		Ar << Data.Operands;
		Ar << Data.Hash;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMGraphFunctionArgument
{
	GENERATED_BODY()
	
	FRigVMGraphFunctionArgument()
	: Name(NAME_None)
	, DisplayName(NAME_None)
	, CPPType(NAME_None)
	, CPPTypeObject(nullptr)
	, bIsArray(false)
	, Direction(ERigVMPinDirection::Input)
	, bIsConst(false)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName DisplayName;

	UPROPERTY()
	FName CPPType;

	UPROPERTY()
	TSoftObjectPtr<UObject> CPPTypeObject;

	UPROPERTY()
	bool bIsArray;

	UPROPERTY()
	ERigVMPinDirection Direction;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	bool bIsConst;
	
	UPROPERTY()
	TMap<FString, FText> PathToTooltip;

	FRigVMExternalVariable GetExternalVariable() const;

	friend uint32 GetTypeHash(const FRigVMGraphFunctionArgument& Argument)
	{
		uint32 Hash = HashCombine(GetTypeHash(Argument.Name), GetTypeHash(Argument.DisplayName));
		Hash = HashCombine(Hash, GetTypeHash(Argument.CPPType));
		Hash = HashCombine(Hash, GetTypeHash(Argument.CPPTypeObject));
		Hash = HashCombine(Hash, GetTypeHash(Argument.bIsArray));
		Hash = HashCombine(Hash, GetTypeHash(Argument.Direction));
		Hash = HashCombine(Hash, GetTypeHash(Argument.DefaultValue));
		Hash = HashCombine(Hash, GetTypeHash(Argument.bIsConst));
		for (const TPair<FString, FText>& Pair : Argument.PathToTooltip)
		{
			Hash = HashCombine(Hash, GetTypeHash(Pair.Key));
			Hash = HashCombine(Hash, GetTypeHash(Pair.Value.ToString()));
		}
		return Hash;
	}

	bool operator==(const FRigVMGraphFunctionArgument& Other) const
	{
		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionArgument& Data)
	{
		Ar << Data.Name;
		Ar << Data.DisplayName;
		Ar << Data.CPPType;
		Ar << Data.CPPTypeObject;
		Ar << Data.bIsArray;
		Ar << Data.Direction;
		Ar << Data.DefaultValue;
		Ar << Data.bIsConst;
		Ar << Data.PathToTooltip;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMGraphFunctionIdentifier
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath LibraryNode;

	// A path to the IRigVMGraphFunctionHost that stores the function information, and compilation data (e.g. ControlRigBlueprintGeneratedClass)
	UPROPERTY()
	FSoftObjectPath HostObject;

	FRigVMGraphFunctionIdentifier()
		: LibraryNode(nullptr), HostObject(nullptr) {}
	
	FRigVMGraphFunctionIdentifier(FSoftObjectPath InHostObject, FSoftObjectPath InLibraryNode)
		: LibraryNode(InLibraryNode), HostObject(InHostObject) {}

	friend uint32 GetTypeHash(const FRigVMGraphFunctionIdentifier& Pointer)
	{
		return HashCombine(GetTypeHash(Pointer.LibraryNode), GetTypeHash(Pointer.HostObject));
	}

	bool operator==(const FRigVMGraphFunctionIdentifier& Other) const
	{
		return HostObject == Other.HostObject && LibraryNode == Other.LibraryNode;
	}

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionIdentifier& Data)
	{
		Ar << Data.LibraryNode;
		Ar << Data.HostObject;
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMGraphFunctionHeader
{
	GENERATED_BODY()

	FRigVMGraphFunctionHeader()
		: LibraryPointer(nullptr, nullptr)
		, Name(NAME_None)
	{}

	UPROPERTY()
	FRigVMGraphFunctionIdentifier LibraryPointer;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString NodeTitle;
	
	UPROPERTY()
	FLinearColor NodeColor = FLinearColor::White;

	UPROPERTY()
	FText Tooltip;
	
	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString Keywords;	

	UPROPERTY()
	TArray<FRigVMGraphFunctionArgument> Arguments;

	UPROPERTY()
	TMap<FRigVMGraphFunctionIdentifier, uint32> Dependencies;

	UPROPERTY()
	TArray<FRigVMExternalVariable> ExternalVariables;

	bool IsMutable() const;

	bool IsValid() const { return !LibraryPointer.HostObject.IsNull(); }

	FString GetHash() const
	{
		return FString::Printf(TEXT("%s:%s"), *LibraryPointer.HostObject.ToString(), *Name.ToString());
	}

	friend uint32 GetTypeHash(const FRigVMGraphFunctionHeader& Header)
	{
		return GetTypeHash(Header.LibraryPointer);
	}

	bool operator==(const FRigVMGraphFunctionHeader& Other) const
	{
		return LibraryPointer == Other.LibraryPointer;
	}

	IRigVMGraphFunctionHost* GetFunctionHost() const;

	FRigVMGraphFunctionData* GetFunctionData() const;

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionHeader& Data)
	{
		Ar << Data.LibraryPointer;
		Ar << Data.Name;
		Ar << Data.NodeTitle;
		Ar << Data.NodeColor;
		Ar << Data.Tooltip;
		Ar << Data.Category;
		Ar << Data.Keywords;
		Ar << Data.Arguments;
		Ar << Data.Dependencies;
		Ar << Data.ExternalVariables;
		return Ar;
	}

	void PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName);
};

USTRUCT(BlueprintType)
struct RIGVM_API FRigVMGraphFunctionData
{
	GENERATED_BODY()

	FRigVMGraphFunctionData(){}

	FRigVMGraphFunctionData(const FRigVMGraphFunctionHeader& InHeader)
		: Header(InHeader) {	}

	UPROPERTY()
	FRigVMGraphFunctionHeader Header;

	UPROPERTY()
	FRigVMFunctionCompilationData CompilationData;

	UPROPERTY()
	FString SerializedCollapsedNode;

	bool IsMutable() const;

	bool operator==(const FRigVMGraphFunctionData& Other) const
	{
		return Header == Other.Header;
	}

	void ClearCompilationData() { CompilationData = FRigVMFunctionCompilationData(); }

	friend FArchive& operator<<(FArchive& Ar, FRigVMGraphFunctionData& Data)
	{
		Ar << Data.Header;
		Ar << Data.CompilationData;

		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMSaveSerializedGraphInGraphFunctionData)
		{
			return Ar;
		}

		Ar << Data.SerializedCollapsedNode;
		return Ar;
	}

	void PostDuplicateHost(const FString& InOldPathName, const FString& InNewPathName);

	static FRigVMGraphFunctionData* FindFunctionData(const FRigVMGraphFunctionIdentifier& InIdentifier, bool* bOutIsPublic = nullptr);	
};
