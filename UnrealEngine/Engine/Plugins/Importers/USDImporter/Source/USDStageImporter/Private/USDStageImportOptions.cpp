// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageImportOptions.h"

#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"

#include "AnalyticsEventAttribute.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

UUsdStageImportOptions::UUsdStageImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bImportActors = true;
	bImportGeometry = true;
	bImportSkeletalAnimations = true;
	bImportLevelSequences = true;
	bImportMaterials = true;

	PurposesToImport = (int32) (EUsdPurpose::Default | EUsdPurpose::Proxy | EUsdPurpose::Render | EUsdPurpose::Guide);
	NaniteTriangleThreshold = INT32_MAX;
	IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >( TEXT("USDSchemas") );
	RenderContextToImport = UsdSchemasModule.GetRenderContextRegistry().GetUnrealRenderContext();
	MaterialPurpose = *UnrealIdentifiers::MaterialAllPurpose;
	bOverrideStageOptions = false;
	StageOptions.MetersPerUnit = 0.01f;
	StageOptions.UpAxis = EUsdUpAxis::ZAxis;

	ExistingActorPolicy = EReplaceActorPolicy::Replace;
	ExistingAssetPolicy = EReplaceAssetPolicy::Replace;

	bPrimPathFolderStructure = false;
	KindsToCollapse = ( int32 ) ( EUsdDefaultKind::Component | EUsdDefaultKind::Subcomponent );
	bMergeIdenticalMaterialSlots = true;
	bCollapseTopLevelPointInstancers = false;
	bInterpretLODs = true;
}

void UUsdStageImportOptions::EnableActorImport( bool bEnable )
{
	for ( TFieldIterator<FProperty> PropertyIterator( UUsdStageImportOptions::StaticClass() ); PropertyIterator; ++PropertyIterator )
	{
		FProperty* Property = *PropertyIterator;
		if ( Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED( UUsdStageImportOptions, bImportActors ) )
		{
			if ( bEnable )
			{
				Property->SetMetaData(TEXT("ToolTip"), TEXT("Whether to spawn imported actors into the current level"));
				Property->ClearPropertyFlags(CPF_EditConst);
			}
			else
			{
				bImportActors = false;
				Property->SetMetaData(TEXT("ToolTip"), TEXT("Actor import is disabled when importing via the Content Browser. Use File->\"Import into Level...\" to also import actors."));
				Property->SetPropertyFlags(CPF_EditConst);
			}
			break;
		}
	}
}

void UUsdStageImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		SaveConfig();
	}
}

void UsdUtils::AddAnalyticsAttributes(
	const UUsdStageImportOptions& Options,
	TArray< FAnalyticsEventAttribute >& InOutAttributes
)
{
	InOutAttributes.Emplace( TEXT( "ImportActors" ), LexToString( Options.bImportActors ) );
	InOutAttributes.Emplace( TEXT( "ImportGeometry" ), LexToString( Options.bImportGeometry ) );
	InOutAttributes.Emplace( TEXT( "ImportSkeletalAnimations" ), LexToString( Options.bImportSkeletalAnimations ) );
	InOutAttributes.Emplace( TEXT( "ImportLevelSequences" ), LexToString( Options.bImportLevelSequences ) );
	InOutAttributes.Emplace( TEXT( "ImportMaterials" ), LexToString( Options.bImportMaterials ) );
	if ( Options.PrimsToImport != TArray<FString>{ TEXT( "/" ) } )
	{
		InOutAttributes.Emplace( TEXT( "NumPrimsToImport" ), LexToString( Options.PrimsToImport.Num() ) );
	}
	InOutAttributes.Emplace( TEXT( "PurposesToImport" ), LexToString( Options.PurposesToImport ) );
	InOutAttributes.Emplace( TEXT( "NaniteTriangleThreshold" ), LexToString( Options.NaniteTriangleThreshold ) );
	InOutAttributes.Emplace( TEXT( "RenderContextToImport" ), Options.RenderContextToImport.ToString() );
	InOutAttributes.Emplace( TEXT( "MaterialPurpose" ), Options.MaterialPurpose.ToString() );
	InOutAttributes.Emplace( TEXT( "RootMotionHandling" ), LexToString( ( uint8 ) Options.RootMotionHandling ) );
	InOutAttributes.Emplace( TEXT( "OverrideStageOptions" ), Options.bOverrideStageOptions );
	if ( Options.bOverrideStageOptions )
	{
		UsdUtils::AddAnalyticsAttributes( Options.StageOptions, InOutAttributes );
	}
	InOutAttributes.Emplace( TEXT( "NumGroomInterpolationSettings" ), LexToString( Options.GroomInterpolationSettings.Num() ) );
	InOutAttributes.Emplace( TEXT( "ReplaceActorPolicy" ), LexToString( ( uint8 ) Options.ExistingActorPolicy ) );
	InOutAttributes.Emplace( TEXT( "ReplaceAssetPolicy" ), LexToString( ( uint8 ) Options.ExistingAssetPolicy ) );
	InOutAttributes.Emplace( TEXT( "PrimPathFolderStructure" ), LexToString( Options.bPrimPathFolderStructure ) );
	InOutAttributes.Emplace( TEXT( "KindsToCollapse" ), LexToString( Options.KindsToCollapse ) );
	InOutAttributes.Emplace( TEXT( "MergeIdenticalMaterialSlots" ), LexToString( Options.bMergeIdenticalMaterialSlots ) );
	InOutAttributes.Emplace( TEXT( "CollapseTopLevelPointInstancers" ), LexToString( Options.bCollapseTopLevelPointInstancers ) );
	InOutAttributes.Emplace( TEXT( "InterpretLODs" ), LexToString( Options.bInterpretLODs ) );
}