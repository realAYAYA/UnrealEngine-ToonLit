// Copyright Epic Games, Inc. All Rights Reserved.

#include "UsdWrappers/UsdPrim.h"

#include "USDMemory.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdStage.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/sdf/schema.h"
	#include "pxr/usd/usd/schemaRegistry.h"
	#include "pxr/usd/usd/attribute.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/schemaBase.h"
#include "USDIncludesEnd.h"

#endif // #if USE_USD_SDK

namespace UE
{
	namespace Internal
	{
		class FUsdPrimImpl
		{
		public:
			FUsdPrimImpl() = default;

#if USE_USD_SDK
			explicit FUsdPrimImpl( const pxr::UsdPrim& InUsdPrim )
				: PxrUsdPrim( InUsdPrim )
			{
			}

			explicit FUsdPrimImpl( pxr::UsdPrim&& InUsdPrim )
				: PxrUsdPrim( MoveTemp( InUsdPrim ) )
			{
			}

			TUsdStore< pxr::UsdPrim > PxrUsdPrim;
#endif // #if USE_USD_SDK
		};
	}

	FUsdPrim::FUsdPrim()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >();
	}

	FUsdPrim::FUsdPrim( const FUsdPrim& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >( Other.Impl->PxrUsdPrim.Get() );
#endif // #if USE_USD_SDK
	}

	FUsdPrim::FUsdPrim( FUsdPrim&& Other ) = default;

	FUsdPrim::~FUsdPrim()
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl.Reset();
	}

	FUsdPrim& FUsdPrim::operator=( const FUsdPrim& Other )
	{
#if USE_USD_SDK
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >(  Other.Impl->PxrUsdPrim.Get() );
#endif // #if USE_USD_SDK
		return *this;
	}

	FUsdPrim& FUsdPrim::operator=( FUsdPrim&& Other )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MoveTemp(  Other.Impl );

		return *this;
	}

	FUsdPrim::operator bool() const
	{
#if USE_USD_SDK
		return (bool)Impl->PxrUsdPrim.Get();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::operator==( const FUsdPrim& Other ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get() == Other.Impl->PxrUsdPrim.Get();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::operator!=( const FUsdPrim& Other ) const
	{
		return !( *this == Other );
	}

#if USE_USD_SDK
	FUsdPrim::FUsdPrim( const pxr::UsdPrim& InUsdPrim )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl =  MakeUnique< Internal::FUsdPrimImpl >( InUsdPrim );
	}

	FUsdPrim::FUsdPrim( pxr::UsdPrim&& InUsdPrim )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >( MoveTemp( InUsdPrim ) );
	}

	FUsdPrim& FUsdPrim::operator=( const pxr::UsdPrim& InUsdPrim )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >( InUsdPrim );
		return *this;
	}

	FUsdPrim& FUsdPrim::operator=( pxr::UsdPrim&& InUsdPrim )
	{
		FScopedUnrealAllocs UnrealAllocs;
		Impl = MakeUnique< Internal::FUsdPrimImpl >( MoveTemp( InUsdPrim ) );
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
#endif // #if USE_USD_SDK

	bool FUsdPrim::SetSpecifier( ESdfSpecifier Specifier )
	{
#if USE_USD_SDK
		static_assert( ( int32 ) ESdfSpecifier::Def == ( int32 ) pxr::SdfSpecifierDef, "ESdfSpecifier enum doesn't match USD!" );
		static_assert( ( int32 ) ESdfSpecifier::Over == ( int32 ) pxr::SdfSpecifierOver, "ESdfSpecifier enum doesn't match USD!" );
		static_assert( ( int32 ) ESdfSpecifier::Class == ( int32 ) pxr::SdfSpecifierClass, "ESdfSpecifier enum doesn't match USD!" );
		static_assert( ( int32 ) ESdfSpecifier::Num == ( int32 ) pxr::SdfNumSpecifiers, "ESdfSpecifier enum doesn't match USD!" );

		return Impl->PxrUsdPrim.Get().SetSpecifier( static_cast< pxr::SdfSpecifier > ( Specifier ) );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsActive() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsActive();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::SetActive( bool bActive )
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().SetActive( bActive );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsValid() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsValid();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsPseudoRoot() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsPseudoRoot();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsModel() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsModel();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsGroup() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsGroup();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	TArray<FName> FUsdPrim::GetAppliedSchemas() const
	{
		TArray<FName> AppliedSchemas;

#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		std::vector<pxr::TfToken> UsdAppliedSchemas = Impl->PxrUsdPrim.Get().GetAppliedSchemas();
		AppliedSchemas.Reserve(UsdAppliedSchemas.size());

		for ( const pxr::TfToken& UsdSchema : UsdAppliedSchemas )
		{
			AppliedSchemas.Add( ANSI_TO_TCHAR( UsdSchema.GetString().c_str() ) );
		}
#endif // #if USE_USD_SDK

		return AppliedSchemas;
	}

	bool FUsdPrim::IsA( FName SchemaType ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		pxr::TfType Type = pxr::UsdSchemaRegistry::GetTypeFromName( pxr::TfToken( TCHAR_TO_ANSI( *SchemaType.ToString() ) ) );
		if ( Type.IsUnknown() )
		{
			return false;
		}

		return Impl->PxrUsdPrim.Get().IsA( Type );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAPI( FName SchemaType, TOptional<FName> InstanceName ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs Allocs;

		pxr::TfType Type = pxr::UsdSchemaRegistry::GetTypeFromName( pxr::TfToken( TCHAR_TO_ANSI( *SchemaType.ToString() ) ) );
		if ( Type.IsUnknown() )
		{
			return false;
		}

		pxr::TfToken UsdInstanceName = InstanceName.IsSet() ? pxr::TfToken( TCHAR_TO_ANSI( *InstanceName.GetValue().ToString() ) ) : pxr::TfToken();

		return Impl->PxrUsdPrim.Get().HasAPI( Type, UsdInstanceName );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	const FSdfPath FUsdPrim::GetPrimPath() const
	{
#if USE_USD_SDK
		return FSdfPath( Impl->PxrUsdPrim.Get().GetPrimPath() );
#else
		return FSdfPath();
#endif // #if USE_USD_SDK
	}

	FUsdStage FUsdPrim::GetStage() const
	{
#if USE_USD_SDK
		return FUsdStage( Impl->PxrUsdPrim.Get().GetStage() );
#else
		return FUsdStage();
#endif // #if USE_USD_SDK
	}

	FName FUsdPrim::GetName() const
	{
#if USE_USD_SDK
		return FName( ANSI_TO_TCHAR( Impl->PxrUsdPrim.Get().GetName().GetString().c_str() ) );
#else
		return FName();
#endif // #if USE_USD_SDK
	}

	FName FUsdPrim::GetTypeName() const
	{
#if USE_USD_SDK
		return FName( ANSI_TO_TCHAR( Impl->PxrUsdPrim.Get().GetTypeName().GetString().c_str() ) );
#else
		return FName();
#endif // #if USE_USD_SDK
	}

	FUsdPrim FUsdPrim::GetParent() const
	{
#if USE_USD_SDK
		return FUsdPrim( Impl->PxrUsdPrim.Get().GetParent() );
#else
		return FUsdPrim();
#endif // #if USE_USD_SDK
	}

	TArray< FUsdPrim > FUsdPrim::GetChildren() const
	{
		TArray< FUsdPrim > Children;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrimSiblingRange PrimChildren = Impl->PxrUsdPrim.Get().GetChildren();

		for ( const pxr::UsdPrim& Child : PrimChildren )
		{
			Children.Emplace( Child );
		}
#endif // #if USE_USD_SDK

		return Children;
	}

	TArray< FUsdPrim > FUsdPrim::GetFilteredChildren( bool bTraverseInstanceProxies ) const
	{
		TArray< FUsdPrim > Children;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::Usd_PrimFlagsPredicate Predicate = pxr::UsdPrimDefaultPredicate;

		if ( bTraverseInstanceProxies )
		{
			Predicate = pxr::UsdTraverseInstanceProxies( Predicate ) ;
		}

		pxr::UsdPrimSiblingRange PrimChildren = Impl->PxrUsdPrim.Get().GetFilteredChildren( Predicate );

		for ( const pxr::UsdPrim& Child : PrimChildren )
		{
			Children.Emplace( Child );
		}
#endif // #if USE_USD_SDK

		return Children;
	}

	bool FUsdPrim::HasVariantSets() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasVariantSets();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAuthoredReferences() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAuthoredReferences();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::HasPayload() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasPayload();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::IsLoaded() const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().IsLoaded();
#else
		return false;
#endif // #if USE_USD_SDK
	}

	void FUsdPrim::Load()
	{
#if USE_USD_SDK
		Impl->PxrUsdPrim.Get().Load();
#endif // #if USE_USD_SDK
	}

	void FUsdPrim::Unload()
	{
#if USE_USD_SDK
		Impl->PxrUsdPrim.Get().Unload();
#endif // #if USE_USD_SDK
	}

	TArray< FUsdAttribute > FUsdPrim::GetAttributes() const
	{
		TArray< FUsdAttribute > Attributes;

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		for ( const pxr::UsdAttribute& Attribute : Impl->PxrUsdPrim.Get().GetAttributes() )
		{
			Attributes.Emplace( Attribute );
		}
#endif // #if USE_USD_SDK

		return Attributes;
	}

	FUsdAttribute FUsdPrim::GetAttribute(const TCHAR* AttrName) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		return FUsdAttribute( Impl->PxrUsdPrim.Get().GetAttribute( pxr::TfToken( TCHAR_TO_ANSI( AttrName ) ) ) );
#else
		return FUsdAttribute{};
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::HasAttribute( const TCHAR* AttrName ) const
	{
#if USE_USD_SDK
		return Impl->PxrUsdPrim.Get().HasAttribute( pxr::TfToken( TCHAR_TO_ANSI( AttrName ) ) );
#else
		return false;
#endif // #if USE_USD_SDK
	}

	FUsdAttribute FUsdPrim::CreateAttribute( const TCHAR* AttrName, FName TypeName ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		return FUsdAttribute(
			Impl->PxrUsdPrim.Get().CreateAttribute(
				pxr::TfToken( TCHAR_TO_ANSI( AttrName ) ),
				pxr::SdfSchema::GetInstance().FindType( TCHAR_TO_ANSI( *TypeName.ToString() ) )
			)
		);
#else
		return FUsdAttribute{};
#endif // #if USE_USD_SDK
	}

	bool FUsdPrim::RemoveProperty( FName PropName ) const
	{
#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;
		return Impl->PxrUsdPrim.Get().RemoveProperty( pxr::TfToken( TCHAR_TO_ANSI( *PropName.ToString() ) ) );
#else
		return false;
#endif // #if USE_USD_SDK
	}
}
