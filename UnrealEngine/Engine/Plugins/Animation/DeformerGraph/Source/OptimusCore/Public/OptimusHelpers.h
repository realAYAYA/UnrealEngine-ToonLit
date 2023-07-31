// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/TransformCalculus.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/Class.h"

class FShaderParametersMetadata;
class FShaderParametersMetadataBuilder;
struct FShaderValueTypeHandle;

namespace Optimus
{
	/** Attempts to find an object, first within a specific package, if the dot prefix
	  * points to a known package, otherwise fall back to searching globally. */
	template<typename T>
	T* FindObjectInPackageOrGlobal(const FString& InObjectPath)
	{
		UPackage* Package = nullptr;
		FString PackageName;
		FString PackageObjectName = InObjectPath;
		if (InObjectPath.Split(TEXT("."), &PackageName, &PackageObjectName))
		{
			Package = FindPackage(nullptr, *PackageName);
		}

		T* FoundObject = FindObject<T>(Package, *PackageObjectName);

		// If not found in a specific, search everywhere.
		if (FoundObject == nullptr)
		{
			FoundObject = FindFirstObject<T>(*InObjectPath, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("Optimus::FindObjectInPackageOrGlobal"));
		}

		return FoundObject;
	}

	/** Given an object scope, ensure that the given name is unique within that scope.
	    If the name is already unique, it will be returned unchanged. */
	FName GetUniqueNameForScope(UObject *InScopeObj, FName InName);

	/** A small helper class to enable binary reads on an archive, since the 
		FObjectReader::Serialize(TArray<uint8>& InBytes) constructor is protected */
	class FBinaryObjectReader : public FObjectReader
	{
	public:
		FBinaryObjectReader(UObject* Obj, const TArray<uint8>& InBytes)
			// FIXME: The constructor is broken. It only needs a const ref.
		    : FObjectReader(const_cast<TArray<uint8>&>(InBytes))
		{
			FObjectReader::SetWantBinaryPropertySerialization(true);
			Obj->Serialize(*this);
		}
	};

	class FBinaryObjectWriter : public FObjectWriter
	{
	public:
		FBinaryObjectWriter(UObject* Obj, TArray<uint8>& OutBytes)
		    : FObjectWriter(OutBytes)
		{
			FObjectWriter::SetWantBinaryPropertySerialization(true);
			Obj->Serialize(*this);
		}
	};

	FName GetSanitizedNameForHlsl(FName InName);

	FORCEINLINE_DEBUGGABLE FMatrix44f ConvertFTransformToFMatrix44f(const FTransform& InTransform)
	{
		return TransformConverter<FMatrix44f>::Convert<FMatrix44d>(InTransform.ToMatrixWithScale());	
	};

	bool RenameObject(UObject* InObjectToRename, const TCHAR* InNewName, UObject* InNewOuter);

	/** Our generated classes are parented to the package, this is a utility function
		to collect them */
	TArray<UClass*> GetClassObjectsInPackage(UPackage* InPackage);

	/** Return the unique type name for registry and kernel code generation if the bInShouldGetUniqueNameForUserDefinedStruct = true
		Otherise, it returns the friendly name for user-facing shader text display*/
	OPTIMUSCORE_API FName GetTypeName(UScriptStruct* InStruct, bool bInShouldGetUniqueNameForUserDefinedStruct = true);
	
	/** Return the display name for the struct to be shown in type pickers*/
	OPTIMUSCORE_API FText GetTypeDisplayName(UScriptStruct* InStruct);

	/** Helper function to remove guids from member property names for user defined structs	*/
	OPTIMUSCORE_API FName GetMemberPropertyShaderName(UScriptStruct* InStruct, const FProperty* InMemberProperty);
}
