// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

#include "UnrealUSDWrapper.h"
#include "UsdWrappers/ForwardDeclarations.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdPrim;
PXR_NAMESPACE_CLOSE_SCOPE
#endif	  // #if USE_USD_SDK

namespace UE
{
	class FSdfPath;
	class FUsdAttribute;
	class FUsdPayloads;
	class FUsdReferences;
	class FUsdVariantSet;
	class FUsdVariantSets;
	class FUsdRelationship;
	

	namespace Internal
	{
		class FUsdPrimImpl;
	}

	/** Corresponds to pxr::SdfSpecifier, refer to the USD SDK documentation */
	enum class ESdfSpecifier
	{
		Def,	  // Defines a concrete prim
		Over,	  // Overrides an existing prim
		Class,	  // Defines an abstract prim
		Num		  // The number of specifiers
	};

	/**
	 * Minimal pxr::UsdPrim wrapper for Unreal that can be used from no-rtti modules.
	 */
	class UNREALUSDWRAPPER_API FUsdPrim
	{
	public:
		FUsdPrim();

		FUsdPrim(const FUsdPrim& Other);
		FUsdPrim(FUsdPrim&& Other);
		~FUsdPrim();

		FUsdPrim& operator=(const FUsdPrim& Other);
		FUsdPrim& operator=(FUsdPrim&& Other);

		bool operator==(const FUsdPrim& Other) const;
		bool operator!=(const FUsdPrim& Other) const;

		explicit operator bool() const;

		// Auto conversion from/to pxr::UsdPrim
	public:
#if USE_USD_SDK
		explicit FUsdPrim(const pxr::UsdPrim& InUsdPrim);
		explicit FUsdPrim(pxr::UsdPrim&& InUsdPrim);
		FUsdPrim& operator=(const pxr::UsdPrim& InUsdPrim);
		FUsdPrim& operator=(pxr::UsdPrim&& InUsdPrim);

		operator pxr::UsdPrim&();
		operator const pxr::UsdPrim&() const;
#endif	  // #if USE_USD_SDK

		  // Wrapped pxr::UsdPrim functions, refer to the USD SDK documentation
	public:
		bool SetSpecifier(ESdfSpecifier Specifier);

		bool IsActive() const;
		bool SetActive(bool bActive);

		bool IsValid() const;
		bool IsPseudoRoot() const;
		bool IsModel() const;
		bool IsGroup() const;

		TArray<FName> GetAppliedSchemas() const;

		bool IsA(FName SchemaIdentifier) const;
		bool HasAPI(FName SchemaIdentifier) const;
		bool HasAPI(FName SchemaIdentifier, FName InstanceName) const;

		UE_DEPRECATED(
			5.3,
			"Please use the one FName overload for single apply API schemas, or the two FName overload for multiple apply API schemas."
		)
		bool HasAPI(FName SchemaType, TOptional<FName> InstanceName) const;

		bool CanApplyAPI(FName SchemaIdentifier, FString* OutWhyNot = nullptr) const;
		bool CanApplyAPI(FName SchemaIdentifier, FName InstanceName, FString* OutWhyNot = nullptr) const;

		bool ApplyAPI(FName SchemaIdentifier) const;
		bool ApplyAPI(FName SchemaIdentifier, FName InstanceName) const;

		bool RemoveAPI(FName SchemaIdentifier) const;
		bool RemoveAPI(FName SchemaIdentifier, FName InstanceName) const;

		const FSdfPath GetPrimPath() const;
		FUsdStage GetStage() const;
		FUsdRelationship GetRelationship(const TCHAR* RelationshipName) const;

		FName GetName() const;

		FName GetTypeName() const;
		bool SetTypeName(FName TypeName) const;
		bool ClearTypeName() const;
		bool HasAuthoredTypeName() const;

		FUsdPrim GetParent() const;

		TArray<FUsdPrim> GetChildren() const;
		TArray<FUsdPrim> GetFilteredChildren(bool bTraverseInstanceProxies) const;

		FUsdVariantSets GetVariantSets() const;
		FUsdVariantSet GetVariantSet(const FString& VariantSetName) const;
		bool HasVariantSets() const;

		FUsdPayloads GetPayloads() const;
		bool HasAuthoredPayloads() const;
		UE_DEPRECATED(5.3, "Please use HasAuthoredPayloads() instead.")
		bool HasPayload() const;
		bool IsLoaded() const;
		void Load(EUsdLoadPolicy Policy = EUsdLoadPolicy::UsdLoadWithDescendants);
		void Unload();

		FUsdReferences GetReferences() const;
		bool HasAuthoredReferences() const;

		bool RemoveProperty(FName PropName) const;

		FUsdAttribute CreateAttribute(const TCHAR* AttrName, FName TypeName) const;
		TArray<FUsdAttribute> GetAttributes() const;
		FUsdAttribute GetAttribute(const TCHAR* AttrName) const;
		bool HasAttribute(const TCHAR* AttrName) const;

		bool IsInstanceable() const;
		bool SetInstanceable(bool bInstanceable) const;
		bool ClearInstanceable() const;
		bool HasAuthoredInstanceable() const;
		bool IsInstance() const;
		bool IsInstanceProxy() const;
		bool IsPrototype() const;
		bool IsInPrototype() const;
		FUsdPrim GetPrototype() const;
		FUsdPrim GetPrimInPrototype() const;
		TArray<FUsdPrim> GetInstances() const;
		static bool IsPrototypePath(const FSdfPath& Path);
		static bool IsPathInPrototype(const FSdfPath& Path);

	private:
		TUniquePtr<Internal::FUsdPrimImpl> Impl;
	};
}	 // namespace UE
