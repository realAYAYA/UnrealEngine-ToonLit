// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDIntegrationsViewModel.h"

#include "USDIntegrationUtils.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfAttributeSpec.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

void FUsdIntegrationsViewModel::UpdateAttributes( const UE::FUsdStageWeak& InUsdStage, const TCHAR* InPrimPath )
{
#if USE_USD_SDK
	UsdStage = InUsdStage;
	PrimPath = InPrimPath;

	Attributes.Reset();

	if ( !UsdStage || PrimPath.IsEmpty() || UE::FSdfPath{ InPrimPath }.IsAbsoluteRootPath() )
	{
		return;
	}

	TArray<FString> AttributeNames;

	TFunction<void(bool, bool)> AddLiveLinkAttributes = [&AttributeNames]( bool bHasLiveLink, bool bIsSkelRoot )
	{
		if ( bHasLiveLink )
		{
			if ( bIsSkelRoot )
			{
				AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealAnimBlueprintPath ) );
			}

			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealLiveLinkSubjectName ) );
			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealLiveLinkEnabled ) );
		}
	};

	TFunction<void( bool, bool )> AddControlRigAttributes = [&AttributeNames]( bool bHasControlRig, bool bIsSkelRoot )
	{
		if ( bHasControlRig && bIsSkelRoot )
		{
			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealControlRigPath ) );
			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealUseFKControlRig ) );
			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealControlRigReduceKeys ) );
			AttributeNames.Add( *UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealControlRigReductionTolerance ) );
		}
	};

    if ( UE::FUsdPrim Prim = InUsdStage.GetPrimAtPath( UE::FSdfPath{ InPrimPath } ) )
    {
        if ( !Prim.IsPseudoRoot() )
        {
            const bool bHasLiveLink = UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::LiveLinkAPI );
            const bool bHasControlRig = UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::ControlRigAPI );
            const bool bIsSkelRoot = Prim.IsA( TEXT( "SkelRoot" ) );

            AddLiveLinkAttributes( bHasLiveLink, bIsSkelRoot );
            AddControlRigAttributes( bHasControlRig, bIsSkelRoot );

            for ( const FString& AttributeName : AttributeNames )
            {
                if ( UE::FUsdAttribute Attr = Prim.GetAttribute( *AttributeName ) )
                {
					Attributes.Add( MakeShared<UE::FUsdAttribute>( Attr ) );
                }
            }
        }
    }
#endif // USE_USD_SDK
}

