// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/UsdPrim.h"

#include "USDMemory.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPayloads.h"
#include "UsdWrappers/UsdReferences.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdRelationship.h"
#include "UsdWrappers/UsdVariantSets.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/relationship.h"
#include "USDIncludesEnd.h"
#endif	  // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FUsdPrimImpl
		{
		public:
			FUsdPrimImpl() = default;

#if USE_USD_SDK

#if ENABLE_USD_DEBUG_PATH
			FSdfPath DebugPath;
#endif
			explicit FUsdPrimImpl(const pxr::UsdPrim& InUsdPrim)
				: PxrUsdPrim(InUsdPrim)
			{
#if ENABLE_USD_DEBUG_PATH
				DebugPath = PxrUsdPrim.Get().GetPrimPath();
#endif
			}

			explicit FUsdPrimImpl(pxr::UsdPrim&& InUsdPrim)
				: PxrUsdPrim(MoveTemp(InUsdPrim))
			{
#if ENABLE_USD_DEBUG_PATH
				DebugPath = PxrUsdPrim.Get().GetPrimPath();
#endif
			}

			TUsdStore<pxr::UsdPrim> PxrUsdPrim;
#endif	  // #if USE_USD_SDK
		};
	}	  // namespace Internal

	FUsdPrim::FUsdPrim()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>();
	}

	FUsdPrim::FUsdPrim(const FUsdPrim& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(Other.Impl->PxrUsdPrim.Get());
#endif	  // #if USE_USD_SDK
	}

	FUsdPrim::FUsdPrim(FUsdPrim&& Other) = default;

	FUsdPrim::~FUsdPrim()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	FUsdPrim& FUsdPrim::operator=(const FUsdPrim& Other)
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(Other.Impl->PxrUsdPrim.Get());
#endif	  // #if USE_USD_SDK
		return *this;
	}

	FUsdPrim& FUsdPrim::operator=(FUsdPrim&& Other)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MoveTemp(Other.Impl);

		return *this;
	}

	FUsdPrim::operator bool() const
	{
#if USE_USD_SDK
		return (bool)Impl->PxrUsdPrim.Get();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::operator==(const FUsdPrim& Other) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get() == Other.Impl->PxrUsdPrim.Get();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::operator!=(const FUsdPrim& Other) const
	{
		return !(*this == Other);
	}

#if USE_USD_SDK
	FUsdPrim::FUsdPrim(const pxr::UsdPrim& InUsdPrim)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(InUsdPrim);
	}

	FUsdPrim::FUsdPrim(pxr::UsdPrim&& InUsdPrim)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(MoveTemp(InUsdPrim));
	}

	FUsdPrim& FUsdPrim::operator=(const pxr::UsdPrim& InUsdPrim)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(InUsdPrim);
		return *this;
	}

	FUsdPrim& FUsdPrim::operator=(pxr::UsdPrim&& InUsdPrim)
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique<Internal::FUsdPrimImpl>(MoveTemp(InUsdPrim));
		return *this;
	}

	FUsdPrim::operator pxr::UsdPrim&()
	{
		return Impl->PxrUsdPrim.Get();
	}

	FUsdPrim::operator const pxr::UsdPrim&() const
	{
		return Impl->PxrUsdPrim.Get();
	}
#endif	  // #if USE_USD_SDK

	bool FUsdPrim::SetSpecifier(ESdfSpecifier Specifier)
	{
#if USE_USD_SDK
		static_assert((int32)ESdfSpecifier::Def == (int32)pxr::SdfSpecifierDef, "ESdfSpecifier enum doesn't match USD!");
		static_assert((int32)ESdfSpecifier::Over == (int32)pxr::SdfSpecifierOver, "ESdfSpecifier enum doesn't match USD!");
		static_assert((int32)ESdfSpecifier::Class == (int32)pxr::SdfSpecifierClass, "ESdfSpecifier enum doesn't match USD!");
		static_assert((int32)ESdfSpecifier::Num == (int32)pxr::SdfNumSpecifiers, "ESdfSpecifier enum doesn't match USD!");

		return Impl->PxrUsdPrim.Get().SetSpecifier(static_cast<pxr::SdfSpecifier>(Specifier));
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsActive() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsActive();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::SetActive(bool bActive)
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().SetActive(bActive);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsValid() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsValid();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsPseudoRoot() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsPseudoRoot();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsModel() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsModel();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsGroup() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsGroup();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	TArray<FName> FUsdPrim::GetAppliedSchemas() const
	{
		TArray<FName> AppliedSchemas;

#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		std::vector<pxr::TfToken> UsdAppliedSchemas = Impl->PxrUsdPrim.Get().GetAppliedSchemas();
		AppliedSchemas.Reserve(UsdAppliedSchemas.size());

		for (const pxr::TfToken& UsdSchema : UsdAppliedSchemas)
		{
			AppliedSchemas.Add(ANSI_TO_TCHAR(UsdSchema.GetString().c_str()));
		}
#endif	  // #if USE_USD_SDK

		return AppliedSchemas;
	}

	bool FUsdPrim::IsA(FName SchemaIdentifier) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));

		return Impl->PxrUsdPrim.Get().IsA(UsdSchemaIdentifier);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAPI(FName SchemaIdentifier) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));

		return Impl->PxrUsdPrim.Get().HasAPI(UsdSchemaIdentifier);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAPI(FName SchemaIdentifier, FName InstanceName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));
		const pxr::TfToken UsdInstanceName(TCHAR_TO_ANSI(*InstanceName.ToString()));

		return Impl->PxrUsdPrim.Get().HasAPI(UsdSchemaIdentifier, UsdInstanceName);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	// Deprecated in 5.3
	bool FUsdPrim::HasAPI(FName SchemaType, TOptional<FName> InstanceName) const
	{
		if (InstanceName.IsSet())
		{
			return HasAPI(SchemaType, InstanceName.GetValue());
		}

		return HasAPI(SchemaType);
	}

	bool FUsdPrim::CanApplyAPI(FName SchemaIdentifier, FString* OutWhyNot) const
	{
		bool bResult = false;

#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));

		std::string WhyNot;
		bResult = Impl->PxrUsdPrim.Get().CanApplyAPI(UsdSchemaIdentifier, OutWhyNot != nullptr ? &WhyNot : nullptr);

		if (!bResult && OutWhyNot != nullptr)
		{
			*OutWhyNot = ANSI_TO_TCHAR(WhyNot.c_str());
		}
#endif	  // #if USE_USD_SDK

		return bResult;
	}

	bool FUsdPrim::CanApplyAPI(FName SchemaIdentifier, FName InstanceName, FString* OutWhyNot) const
	{
		bool bResult = false;

#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));
		const pxr::TfToken UsdInstanceName(TCHAR_TO_ANSI(*InstanceName.ToString()));

		std::string WhyNot;
		bResult = Impl->PxrUsdPrim.Get().CanApplyAPI(UsdSchemaIdentifier, UsdInstanceName, OutWhyNot != nullptr ? &WhyNot : nullptr);

		if (!bResult && OutWhyNot != nullptr)
		{
			*OutWhyNot = ANSI_TO_TCHAR(WhyNot.c_str());
		}
#endif	  // #if USE_USD_SDK

		return bResult;
	}

	bool FUsdPrim::ApplyAPI(FName SchemaIdentifier) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));

		return Impl->PxrUsdPrim.Get().ApplyAPI(UsdSchemaIdentifier);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::ApplyAPI(FName SchemaIdentifier, FName InstanceName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));
		const pxr::TfToken UsdInstanceName(TCHAR_TO_ANSI(*InstanceName.ToString()));

		return Impl->PxrUsdPrim.Get().ApplyAPI(UsdSchemaIdentifier, UsdInstanceName);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::RemoveAPI(FName SchemaIdentifier) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));

		return Impl->PxrUsdPrim.Get().RemoveAPI(UsdSchemaIdentifier);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::RemoveAPI(FName SchemaIdentifier, FName InstanceName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		const pxr::TfToken UsdSchemaIdentifier(TCHAR_TO_ANSI(*SchemaIdentifier.ToString()));
		const pxr::TfToken UsdInstanceName(TCHAR_TO_ANSI(*InstanceName.ToString()));

		return Impl->PxrUsdPrim.Get().RemoveAPI(UsdSchemaIdentifier, UsdInstanceName);
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	const FSdfPath FUsdPrim::GetPrimPath() const
	{
#if USE_USD_SDK
		return FSdfPath(Impl->PxrUsdPrim.Get().GetPrimPath());
#else
		return FSdfPath();
#endif	  // #if USE_USD_SDK
	}

	FUsdStage FUsdPrim::GetStage() const
	{
#if USE_USD_SDK
		return FUsdStage(Impl->PxrUsdPrim.Get().GetStage());
#else
		return FUsdStage();
#endif	  // #if USE_USD_SDK
	}

	FUsdRelationship FUsdPrim::GetRelationship(const TCHAR* RelationshipName) const
	{
#if USE_USD_SDK
		return FUsdRelationship(Impl->PxrUsdPrim.Get().GetRelationship(pxr::TfToken(TCHAR_TO_ANSI(RelationshipName))));
#else
		return FUsdRelationship();
#endif	  // #if USE_USD_SDK
	}

	FName FUsdPrim::GetName() const
	{
#if USE_USD_SDK
		return FName(ANSI_TO_TCHAR(Impl->PxrUsdPrim.Get().GetName().GetString().c_str()));
#else
		return FName();
#endif	  // #if USE_USD_SDK
	}

	FName FUsdPrim::GetTypeName() const
	{
#if USE_USD_SDK
		return FName(ANSI_TO_TCHAR(Impl->PxrUsdPrim.Get().GetTypeName().GetString().c_str()));
#else
		return FName();
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::SetTypeName(FName TypeName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;
		return Impl->PxrUsdPrim.Get().SetTypeName(pxr::TfToken(TCHAR_TO_ANSI(*TypeName.ToString())));
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::ClearTypeName() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().ClearTypeName();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAuthoredTypeName() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAuthoredTypeName();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FUsdPrim FUsdPrim::GetParent() const
	{
#if USE_USD_SDK
		return FUsdPrim(Impl->PxrUsdPrim.Get().GetParent());
#else
		return FUsdPrim();
#endif	  // #if USE_USD_SDK
	}

	TArray<FUsdPrim> FUsdPrim::GetChildren() const
	{
		TArray<FUsdPrim> Children;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrimSiblingRange PrimChildren = Impl->PxrUsdPrim.Get().GetChildren();

		for (const pxr::UsdPrim& Child : PrimChildren)
		{
			Children.Emplace(Child);
		}
#endif	  // #if USE_USD_SDK

		return Children;
	}

	TArray<FUsdPrim> FUsdPrim::GetFilteredChildren(bool bTraverseInstanceProxies) const
	{
		TArray<FUsdPrim> Children;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::Usd_PrimFlagsPredicate Predicate = pxr::UsdPrimDefaultPredicate;

		if (bTraverseInstanceProxies)
		{
			Predicate = pxr::UsdTraverseInstanceProxies(Predicate);
		}

		pxr::UsdPrimSiblingRange PrimChildren = Impl->PxrUsdPrim.Get().GetFilteredChildren(Predicate);

		for (const pxr::UsdPrim& Child : PrimChildren)
		{
			Children.Emplace(Child);
		}
#endif	  // #if USE_USD_SDK

		return Children;
	}

	FUsdVariantSets FUsdPrim::GetVariantSets() const
	{
#if USE_USD_SDK
		return FUsdVariantSets(Impl->PxrUsdPrim.Get());
#else
		return FUsdVariantSets{};
#endif	  // #if USE_USD_SDK
	}

	FUsdVariantSet FUsdPrim::GetVariantSet(const FString& VariantSetName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		const std::string UsdVariantSetName(TCHAR_TO_ANSI(*VariantSetName));

		return FUsdVariantSet(Impl->PxrUsdPrim.Get(), UsdVariantSetName);
#else
		return FUsdVariantSet{};
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasVariantSets() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasVariantSets();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FUsdPayloads FUsdPrim::GetPayloads() const
	{
#if USE_USD_SDK
		return FUsdPayloads(Impl->PxrUsdPrim.Get());
#else
		return FUsdPayloads{};
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAuthoredPayloads() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAuthoredPayloads();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	// Deprecated in 5.3
	bool FUsdPrim::HasPayload() const
	{
		return HasAuthoredPayloads();
	}

	bool FUsdPrim::IsLoaded() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsLoaded();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	void FUsdPrim::Load(EUsdLoadPolicy Policy)
	{
#if USE_USD_SDK
		static_assert((int32)EUsdLoadPolicy::UsdLoadWithDescendants == (int32)pxr::UsdLoadWithDescendants, "EUsdLoadPolicy enum doesn't match USD!");
		static_assert(
			(int32)EUsdLoadPolicy::UsdLoadWithoutDescendants == (int32)pxr::UsdLoadWithoutDescendants,
			"EUsdLoadPolicy enum doesn't match USD!"
		);

		Impl->PxrUsdPrim.Get().Load(static_cast<pxr::UsdLoadPolicy>(Policy));
#endif	  // #if USE_USD_SDK
	}

	void FUsdPrim::Unload()
	{
#if USE_USD_SDK
		Impl->PxrUsdPrim.Get().Unload();
#endif	  // #if USE_USD_SDK
	}

	TArray<FUsdAttribute> FUsdPrim::GetAttributes() const
	{
		TArray<FUsdAttribute> Attributes;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		for (const pxr::UsdAttribute& Attribute : Impl->PxrUsdPrim.Get().GetAttributes())
		{
			Attributes.Emplace(Attribute);
		}
#endif	  // #if USE_USD_SDK

		return Attributes;
	}

	FUsdReferences FUsdPrim::GetReferences() const
	{
#if USE_USD_SDK
		return FUsdReferences(Impl->PxrUsdPrim.Get());
#else
		return FUsdReferences{};
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAuthoredReferences() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAuthoredReferences();
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FUsdAttribute FUsdPrim::GetAttribute(const TCHAR* AttrName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		return FUsdAttribute(Impl->PxrUsdPrim.Get().GetAttribute(pxr::TfToken(TCHAR_TO_ANSI(AttrName))));
#else
		return FUsdAttribute{};
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAttribute(const TCHAR* AttrName) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAttribute(pxr::TfToken(TCHAR_TO_ANSI(AttrName)));
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	FUsdAttribute FUsdPrim::CreateAttribute(const TCHAR* AttrName, FName TypeName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		return FUsdAttribute(Impl->PxrUsdPrim.Get().CreateAttribute(
			pxr::TfToken(TCHAR_TO_ANSI(AttrName)),
			pxr::SdfSchema::GetInstance().FindType(TCHAR_TO_ANSI(*TypeName.ToString()))
		));
#else
		return FUsdAttribute{};
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::RemoveProperty(FName PropName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;
		return Impl->PxrUsdPrim.Get().RemoveProperty(pxr::TfToken(TCHAR_TO_ANSI(*PropName.ToString())));
#else
		return false;
#endif	  // #if USE_USD_SDK
	}

	bool FUsdPrim::IsInstanceable() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().IsInstanceable();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::SetInstanceable(bool bInstanceable) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().SetInstanceable(bInstanceable);
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::ClearInstanceable() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().ClearInstanceable();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::HasAuthoredInstanceable() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().HasAuthoredInstanceable();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::IsInstance() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().IsInstance();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::IsInstanceProxy() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().IsInstanceProxy();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::IsPrototype() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().IsPrototype();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::IsInPrototype() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return Impl->PxrUsdPrim.Get().IsInPrototype();
#else
		return false;
#endif	  // USE_USD_SDK
	}

	FUsdPrim FUsdPrim::GetPrototype() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return UE::FUsdPrim{Impl->PxrUsdPrim.Get().GetPrototype()};
#else
		return {};
#endif	  // USE_USD_SDK
	}

	FUsdPrim FUsdPrim::GetPrimInPrototype() const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return UE::FUsdPrim{Impl->PxrUsdPrim.Get().GetPrimInPrototype()};
#else
		return {};
#endif	  // USE_USD_SDK
	}

	TArray<FUsdPrim> FUsdPrim::GetInstances() const
	{
		TArray<FUsdPrim> Result;

#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		std::vector<pxr::UsdPrim> Instances = Impl->PxrUsdPrim.Get().GetInstances();
		Result.Reserve(Instances.size());

		for (const pxr::UsdPrim& Instance : Instances)
		{
			Result.Emplace(Instance);
		}
#endif	  // USE_USD_SDK

		return Result;
	}

	bool FUsdPrim::IsPrototypePath(const FSdfPath& Path)
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return pxr::UsdPrim::IsPrototypePath(Path);
#else
		return false;
#endif	  // USE_USD_SDK
	}

	bool FUsdPrim::IsPathInPrototype(const FSdfPath& Path)
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;
		return pxr::UsdPrim::IsPathInPrototype(Path);
#else
		return false;
#endif	  // USE_USD_SDK
	}

}	 // namespace UE
