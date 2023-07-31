// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDLayerUtils.h"

#include "UnrealUSDWrapper.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfChangeBlock.h"
#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/SWidget.h"

#if WITH_EDITOR
	#include "DesktopPlatformModule.h"
#endif // WITH_EDITOR

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/pcp/layerStack.h"
	#include "pxr/usd/sdf/attributeSpec.h"
	#include "pxr/usd/sdf/fileFormat.h"
	#include "pxr/usd/sdf/layer.h"
	#include "pxr/usd/sdf/layerUtils.h"
	#include "pxr/usd/sdf/textFileFormat.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/modelAPI.h"
	#include "pxr/usd/usd/primCompositionQuery.h"
	#include "pxr/usd/usd/stage.h"
	#include "pxr/usd/usdGeom/xform.h"
#include "USDIncludesEnd.h"

#define LOCTEXT_NAMESPACE "USDLayerUtils"

namespace UE::UsdLayerUtilsImpl::Private
{
	/**
		* Adapted from flattenUtils.cpp::_FixAssetPaths, except that we only handle actual AssetPaths here as layer/prim paths
		* will be remapped via Layer.UpdateCompositionAssetDependency().
		* Returns whether anything was remapped
		*/
	bool FixAssetPaths( const pxr::SdfLayerHandle& SourceLayer, pxr::VtValue* Value )
	{
		if ( Value->IsHolding<pxr::SdfAssetPath>() )
		{
			pxr::SdfAssetPath AssetPath;
			Value->Swap( AssetPath );
			AssetPath = pxr::SdfAssetPath( SourceLayer->ComputeAbsolutePath( AssetPath.GetAssetPath() ) );
			Value->Swap( AssetPath );
			return true;
		}
		else if ( Value->IsHolding<pxr::VtArray<pxr::SdfAssetPath>>() )
		{
			pxr::VtArray<pxr::SdfAssetPath> PathArray;
			Value->Swap( PathArray );
			for ( pxr::SdfAssetPath& AssetPath : PathArray )
			{
				AssetPath = pxr::SdfAssetPath( SourceLayer->ComputeAbsolutePath( AssetPath.GetAssetPath() ) );
			}
			Value->Swap( PathArray );
			return true;
		}

		return false;
	}
}

FText UsdUtils::ToText( ECanInsertSublayerResult Result )
{
	switch ( Result )
	{
	default:
	case ECanInsertSublayerResult::Success:
		return FText::GetEmpty();
		break;
	case ECanInsertSublayerResult::ErrorSubLayerNotFound:
		return LOCTEXT( "CanAddSubLayerNotFound", "SubLayer not found!" );
		break;
	case ECanInsertSublayerResult::ErrorSubLayerInvalid:
		return LOCTEXT( "CanAddSubLayerInvalid", "SubLayer is invalid!" );
		break;
	case ECanInsertSublayerResult::ErrorSubLayerIsParentLayer:
		return LOCTEXT( "CanAddSubLayerIsParent", "SubLayer is the same as the parent layer!" );
		break;
	case ECanInsertSublayerResult::ErrorCycleDetected:
		return LOCTEXT( "CanAddSubLayerCycle", "Cycles detected!" );
		break;
	}
}

UsdUtils::ECanInsertSublayerResult UsdUtils::CanInsertSubLayer(
	const pxr::SdfLayerRefPtr& ParentLayer,
	const TCHAR* SubLayerIdentifier
)
{
	if ( !SubLayerIdentifier )
	{
		return ECanInsertSublayerResult::ErrorSubLayerNotFound;
	}

	FScopedUsdAllocs Allocs;

	pxr::SdfLayerRefPtr SubLayer = pxr::SdfLayer::FindOrOpen( UnrealToUsd::ConvertString( SubLayerIdentifier ).Get() );
	if ( !SubLayer )
	{
		return ECanInsertSublayerResult::ErrorSubLayerNotFound;
	}

	if ( SubLayer == ParentLayer )
	{
		return ECanInsertSublayerResult::ErrorSubLayerIsParentLayer;
	}

	// We can't climb through ancestors of ParentLayer, so we have to open sublayer and see if parentlayer is a
	// descendant of *it* in order to detect cycles
	TFunction< ECanInsertSublayerResult( const pxr::SdfLayerRefPtr& ) > CanAddSubLayerRecursive;
	CanAddSubLayerRecursive = [ParentLayer, &CanAddSubLayerRecursive](
		const pxr::SdfLayerRefPtr& CurrentParent
	) -> ECanInsertSublayerResult
	{
		for ( const std::string& SubLayerPath : CurrentParent->GetSubLayerPaths() )
		{
			// This may seem expensive, but keep in mind the main use case for this (at least for now) is for checking
			// during layer drag and drop, where all of these layers are actually already open anyway
			pxr::SdfLayerRefPtr ChildSubLayer = pxr::SdfLayer::FindOrOpenRelativeToLayer(
				CurrentParent,
				SubLayerPath
			);

			if ( !ChildSubLayer )
			{
				return ECanInsertSublayerResult::ErrorSubLayerInvalid;
			}

			ECanInsertSublayerResult RecursiveResult = ChildSubLayer == ParentLayer
				 ? ECanInsertSublayerResult::ErrorCycleDetected
				 : CanAddSubLayerRecursive( ChildSubLayer );

			if ( RecursiveResult != ECanInsertSublayerResult::Success )
			{
				return RecursiveResult;
			}
		}

		return ECanInsertSublayerResult::Success;
	};

	return CanAddSubLayerRecursive( SubLayer );
}

bool UsdUtils::InsertSubLayer( const pxr::SdfLayerRefPtr& ParentLayer, const TCHAR* SubLayerFile, int32 Index, double OffsetTimeCodes, double TimeCodesScale )
{
	if ( !ParentLayer )
	{
		return false;
	}

	FString RelativeSubLayerPath = SubLayerFile;
	MakePathRelativeToLayer( UE::FSdfLayer{ ParentLayer }, RelativeSubLayerPath );

	// If the relative path is just the same as the clean name (e.g. Layer.usda wants to add Layer.usda as a sublayer) then
	// just stop here as that is always an error
	FString ParentLayerPath = UsdToUnreal::ConvertString( ParentLayer->GetRealPath() );
	if ( FPaths::GetCleanFilename( ParentLayerPath ) == RelativeSubLayerPath )
	{
		UE_LOG( LogUsd, Warning, TEXT( "Tried to add layer '%s' as a sublayer of itself!" ), *ParentLayerPath );
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	ParentLayer->InsertSubLayerPath( UnrealToUsd::ConvertString( *RelativeSubLayerPath ).Get(), Index );

	if ( !FMath::IsNearlyZero( OffsetTimeCodes ) || !FMath::IsNearlyEqual( TimeCodesScale, 1.0 ) )
	{
		if ( Index == -1 )
		{
			Index = ParentLayer->GetNumSubLayerPaths() - 1;
		}

		ParentLayer->SetSubLayerOffset( pxr::SdfLayerOffset{ OffsetTimeCodes, TimeCodesScale }, Index );
	}

	return true;
}

#if WITH_EDITOR
TOptional< FString > UsdUtils::BrowseUsdFile( EBrowseFileMode Mode )
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if ( DesktopPlatform == nullptr )
	{
		return {};
	}

	TArray< FString > OutFiles;

	// When browsing files for the purposes of opening a stage or saving layers,
	// we offer the native USD file formats as options. Browsing files in order
	// to use them as the targets of composition arcs (e.g. sublayers,
	// references, payloads, etc.) also offers any plugin file formats that are
	// registered.
	TArray< FString > SupportedExtensions = ( Mode == EBrowseFileMode::Composition ) ?
		UnrealUSDWrapper::GetAllSupportedFileFormats() :
		UnrealUSDWrapper::GetNativeFileFormats();
	if ( SupportedExtensions.Num() == 0 )
	{
		UE_LOG(LogUsd, Error, TEXT("No file extensions supported by the USD SDK!"));
		return {};
	}

	if ( Mode == EBrowseFileMode::Save )
	{
		// USD 21.08 doesn't yet support saving to USDZ, so instead of allowing this option and leading to an error we'll just hide it
		SupportedExtensions.Remove( TEXT( "usdz" ) );
	}

	FString JoinedExtensions = FString::Join( SupportedExtensions, TEXT( ";*." ) ); // Combine "usd" and "usda" into "usd; *.usda"
	FString FileTypes = FString::Printf( TEXT( "Universal Scene Description files (*.%s)|*.%s|" ), *JoinedExtensions, *JoinedExtensions );
	for ( const FString& SupportedExtension : SupportedExtensions )
	{
		// The '(*.%s)' on the actual name (before the '|') is not optional: We need the name part to be different for each format
		// or else the options will overwrite each other on the Mac
		FileTypes += FString::Printf( TEXT( "Universal Scene Description file (*.%s)|*.%s|" ), *SupportedExtension, *SupportedExtension );
	}

	switch ( Mode )
	{
		case EBrowseFileMode::Open :
		case EBrowseFileMode::Composition :
		{
			if ( !DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs( nullptr ),
				LOCTEXT( "ChooseFile", "Choose file").ToString(),
				TEXT(""),
				TEXT(""),
				*FileTypes,
				EFileDialogFlags::None,
				OutFiles
			) )
			{
				return {};
			}
			break;
		}
		case EBrowseFileMode::Save :
		{
			if ( !DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs( nullptr ),
				LOCTEXT( "ChooseFile", "Choose file").ToString(),
				TEXT(""),
				TEXT(""),
				*FileTypes,
				EFileDialogFlags::None,
				OutFiles
			) )
			{
				return {};
			}
			break;
		}
		default:
			break;
	}

	if ( OutFiles.Num() > 0 )
	{
		// Always make this an absolute path because it may try generating a relative path to the engine binary if it can
		return FPaths::ConvertRelativePathToFull( OutFiles[ 0 ] );
	}

	return {};
}

#endif // WITH_EDITOR

FString UsdUtils::MakePathRelativeToProjectDir( const FString& Path )
{
	FString PathConverted = FPaths::ConvertRelativePathToFull( Path );

	// Mirror behavior of RelativeToGameDir meta tag on the stage actor's RootLayer
	if ( FPaths::IsUnderDirectory( PathConverted, FPaths::ProjectDir() ) )
	{
		FPaths::MakePathRelativeTo( PathConverted, *FPaths::ProjectDir() );
	}

	return PathConverted;
}

TUsdStore< pxr::SdfLayerRefPtr > UsdUtils::CreateNewLayer( TUsdStore< pxr::UsdStageRefPtr > UsdStage, const TUsdStore<pxr::SdfLayerRefPtr>& ParentLayer, const TCHAR* LayerFilePath )
{
	FScopedUsdAllocs UsdAllocs;

	std::string UsdLayerFilePath = UnrealToUsd::ConvertString( *FPaths::ConvertRelativePathToFull( LayerFilePath ) ).Get();

	pxr::SdfLayerRefPtr LayerRef = pxr::SdfLayer::CreateNew( UsdLayerFilePath );

	if ( !LayerRef )
	{
		return {};
	}

	// New layer needs to be created and in the stage layer stack before we can edit it
	UsdUtils::InsertSubLayer( ParentLayer.Get(), LayerFilePath );

	UsdUtils::StartMonitoringErrors();
	pxr::UsdEditContext UsdEditContext( UsdStage.Get(), LayerRef );

	// Create default prim
	FString PrimPath = TEXT("/") + FPaths::GetBaseFilename( UsdToUnreal::ConvertString( LayerRef->GetDisplayName() ) );

	pxr::SdfPath UsdPrimPath = UnrealToUsd::ConvertPath( *PrimPath ).Get();
	pxr::UsdGeomXform DefaultPrim = pxr::UsdGeomXform::Define( UsdStage.Get(), UsdPrimPath );

	if ( DefaultPrim)
	{
		// Set default prim
		LayerRef->SetDefaultPrim( DefaultPrim.GetPrim().GetName() );
	}

	bool bHadErrors = UsdUtils::ShowErrorsAndStopMonitoring();

	if (bHadErrors)
	{
		return {};
	}

	return TUsdStore< pxr::SdfLayerRefPtr >( LayerRef );
}

UE::FSdfLayer UsdUtils::FindLayerForPrim( const pxr::UsdPrim& Prim )
{
	if ( !Prim )
	{
		return {};
	}

	FScopedUsdAllocs UsdAllocs;

	// Use this instead of UsdPrimCompositionQuery as that one can simply fail in some scenarios
	// (e.g. empty parent layer pointing at a sublayer with a prim, where it fails to provide the sublayer arc's layer)
	for ( const pxr::SdfPrimSpecHandle& Handle : Prim.GetPrimStack() )
	{
		if ( Handle )
		{
			if ( pxr::SdfLayerHandle Layer = Handle->GetLayer() )
			{
				return UE::FSdfLayer( Layer );
			}
		}
	}

	return UE::FSdfLayer( Prim.GetStage()->GetRootLayer() );
}

UE::FSdfLayer UsdUtils::FindLayerForAttribute( const pxr::UsdAttribute& Attribute, double TimeCode )
{
	if ( !Attribute )
	{
		return {};
	}

	FScopedUsdAllocs UsdAllocs;

	for ( const pxr::SdfPropertySpecHandle& PropertySpec : Attribute.GetPropertyStack( TimeCode ) )
	{
		if ( PropertySpec->HasDefaultValue() || PropertySpec->GetLayer()->GetNumTimeSamplesForPath( PropertySpec->GetPath() ) > 0 )
		{
			return UE::FSdfLayer( PropertySpec->GetLayer() );
		}
	}

	return {};
}

UE::FSdfLayer UsdUtils::FindLayerForAttributes( const TArray<UE::FUsdAttribute>& Attributes, double TimeCode, bool bIncludeSessionLayers )
{
	FScopedUsdAllocs UsdAllocs;

	TMap<FString, UE::FSdfLayer> IdentifierToLayers;
	IdentifierToLayers.Reserve( Attributes.Num() );

	pxr::UsdStageRefPtr Stage;
	for ( const UE::FUsdAttribute& Attribute : Attributes )
	{
		if ( Attribute )
		{
			if ( UE::FSdfLayer Layer = UsdUtils::FindLayerForAttribute( Attribute, TimeCode ) )
			{
				IdentifierToLayers.Add( Layer.GetIdentifier(), Layer );

				if ( !Stage )
				{
					Stage = pxr::UsdStageRefPtr{ Attribute.GetPrim().GetStage() };
				}
			}
		}
	}

	if ( !Stage || IdentifierToLayers.Num() == 0)
	{
		return {};
	}

	if ( IdentifierToLayers.Num() == 1 )
	{
		return IdentifierToLayers.CreateIterator().Value();
	}

	// Iterate through the layer stack in strong to weak order, and return the first of those layers
	// that is actually one of the attribute layers
	for ( const pxr::SdfLayerHandle& LayerHandle : Stage->GetLayerStack( bIncludeSessionLayers ) )
	{
		FString Identifier = UsdToUnreal::ConvertString( LayerHandle->GetIdentifier() );
		if ( UE::FSdfLayer* AttributeLayer = IdentifierToLayers.Find( Identifier ) )
		{
			return *AttributeLayer;
		}
	}

	return {};
}

UE::FSdfLayer UsdUtils::FindLayerForSubLayerPath( const UE::FSdfLayer& RootLayer, const FStringView& SubLayerPath )
{
	const FString RelativeLayerPath = UE::FSdfLayerUtils::SdfComputeAssetPathRelativeToLayer( RootLayer, SubLayerPath.GetData() );

	return UE::FSdfLayer::FindOrOpen( *RelativeLayerPath );
}

bool UsdUtils::SetRefOrPayloadLayerOffset( pxr::UsdPrim& Prim, const UE::FSdfLayerOffset& LayerOffset )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrimCompositionQuery PrimCompositionQuery( Prim );
	std::vector< pxr::UsdPrimCompositionQueryArc > CompositionArcs = PrimCompositionQuery.GetCompositionArcs();

	for ( const pxr::UsdPrimCompositionQueryArc& CompositionArc : CompositionArcs )
	{
		if ( CompositionArc.GetArcType() == pxr::PcpArcTypeReference )
		{
			pxr::SdfReferenceEditorProxy ReferenceEditor;
			pxr::SdfReference OldReference;

			if ( CompositionArc.GetIntroducingListEditor( &ReferenceEditor, &OldReference ) )
			{
				pxr::SdfReference NewReference = OldReference;
				NewReference.SetLayerOffset( pxr::SdfLayerOffset( LayerOffset.Offset, LayerOffset.Scale ) );

				ReferenceEditor.ReplaceItemEdits( OldReference, NewReference );

				return true;
			}
		}
		else if ( CompositionArc.GetArcType() == pxr::PcpArcTypePayload )
		{
			pxr::SdfPayloadEditorProxy PayloadEditor;
			pxr::SdfPayload OldPayload;

			if ( CompositionArc.GetIntroducingListEditor( &PayloadEditor, &OldPayload ) )
			{
				pxr::SdfPayload NewPayload = OldPayload;
				NewPayload.SetLayerOffset( pxr::SdfLayerOffset( LayerOffset.Offset, LayerOffset.Scale ) );

				PayloadEditor.ReplaceItemEdits( OldPayload, NewPayload );

				return true;
			}
		}
	}

	return false;
}

UE::FSdfLayerOffset UsdUtils::GetLayerToStageOffset( const pxr::UsdAttribute& Attribute )
{
	// Inspired by pxr::_GetLayerToStageOffset

	UE::FSdfLayer AttributeLayer = UsdUtils::FindLayerForAttribute( Attribute, pxr::UsdTimeCode::EarliestTime().GetValue() );

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdResolveInfo ResolveInfo = Attribute.GetResolveInfo( pxr::UsdTimeCode::EarliestTime() );
	pxr::PcpNodeRef Node = ResolveInfo.GetNode();
	if ( !Node )
	{
		return UE::FSdfLayerOffset();
	}

	const pxr::PcpMapExpression& MapToRoot = Node.GetMapToRoot();
	if ( MapToRoot.IsNull() )
	{
		return UE::FSdfLayerOffset();
	}

	pxr::SdfLayerOffset NodeToRootNodeOffset = MapToRoot.GetTimeOffset();

	pxr::SdfLayerOffset LocalOffset = NodeToRootNodeOffset;

	if ( const pxr::SdfLayerOffset* LayerToRootLayerOffset = ResolveInfo.GetNode().GetLayerStack()->GetLayerOffsetForLayer( pxr::SdfLayerRefPtr( AttributeLayer ) ) )
	{
		LocalOffset = LocalOffset * (*LayerToRootLayerOffset);
	}

	return UE::FSdfLayerOffset( LocalOffset.GetOffset(), LocalOffset.GetScale() );
}

UE::FSdfLayerOffset UsdUtils::GetPrimToStageOffset( const UE::FUsdPrim& Prim )
{
	// In most cases all we care about is an offset from the prim's layer to the stage, but it is also possible for a prim
	// to directly reference another layer with an offset and scale as well, and this function will pick up on that. Example:
	//
	// def SkelRoot "Model" (
	//	  prepend references = @sublayer.usda@ ( offset = 15; scale = 2.0 )
	// )
	// {
	// }
	//
	// Otherwise, this function really has the same effect as GetLayerToStageOffset, but we need to use an actual prim to be able
	// to get USD to combine layer offsets and scales for us (via UsdPrimCompositionQuery).

	FScopedUsdAllocs Allocs;

	UE::FSdfLayer StrongestLayerForPrim = UsdUtils::FindLayerForPrim( Prim );

	pxr::UsdPrim UsdPrim{ Prim };

	pxr::UsdPrimCompositionQuery PrimCompositionQuery( UsdPrim );
	pxr::UsdPrimCompositionQuery::Filter Filter;
	Filter.hasSpecsFilter = pxr::UsdPrimCompositionQuery::HasSpecsFilter::HasSpecs;
	PrimCompositionQuery.SetFilter( Filter );

	for ( const pxr::UsdPrimCompositionQueryArc& CompositionArc : PrimCompositionQuery.GetCompositionArcs() )
	{
		if ( pxr::PcpNodeRef Node = CompositionArc.GetTargetNode() )
		{
			pxr::SdfLayerOffset Offset;

			// This part of the offset will handle direct prim references
			const pxr::PcpMapExpression& MapToRoot = Node.GetMapToRoot();
			if ( !MapToRoot.IsNull() )
			{
				Offset = MapToRoot.GetTimeOffset();
			}

			if ( const pxr::SdfLayerOffset* LayerOffset = Node.GetLayerStack()->GetLayerOffsetForLayer( pxr::SdfLayerRefPtr{ StrongestLayerForPrim } ) )
			{
				Offset = Offset * (*LayerOffset);
			}

			return UE::FSdfLayerOffset{ Offset.GetOffset(), Offset.GetScale() };
		}
	}

	return UE::FSdfLayerOffset{};
}

void UsdUtils::AddTimeCodeRangeToLayer( const pxr::SdfLayerRefPtr& Layer, double StartTimeCode, double EndTimeCode )
{
	FScopedUsdAllocs UsdAllocs;

	if ( !Layer )
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Warning, LOCTEXT( "AddTimeCode_InvalidLayer", "Trying to set timecodes on an invalid layer." ) );
		return;
	}

	// The HasTimeCode check is needed or else we can't author anything with a StartTimeCode lower than the default of 0
	if ( StartTimeCode < Layer->GetStartTimeCode() || !Layer->HasStartTimeCode() )
	{
		Layer->SetStartTimeCode( StartTimeCode );
	}

	if ( EndTimeCode > Layer->GetEndTimeCode() || !Layer->HasEndTimeCode() )
	{
		Layer->SetEndTimeCode( StartTimeCode );
	}
}

void UsdUtils::MakePathRelativeToLayer( const UE::FSdfLayer& Layer, FString& Path )
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	if ( pxr::SdfLayerRefPtr UsdLayer = (pxr::SdfLayerRefPtr)Layer )
	{
		std::string RepositoryPath = UsdLayer->GetRepositoryPath().empty() ? UsdLayer->GetRealPath() : UsdLayer->GetRepositoryPath();
		FString LayerAbsolutePath = UsdToUnreal::ConvertString( RepositoryPath );
		if ( !LayerAbsolutePath.IsEmpty() )
		{
			FPaths::MakePathRelativeTo( Path, *LayerAbsolutePath );
		}
	}
#endif // #if USE_USD_SDK
}

UE::FSdfLayer UsdUtils::GetUEPersistentStateSublayer( const UE::FUsdStage& Stage, bool bCreateIfNeeded )
{
	UE::FSdfLayer StateLayer;
	if ( !Stage )
	{
		return StateLayer;
	}

	FScopedUsdAllocs Allocs;

	UE::FSdfChangeBlock ChangeBlock;

	FString PathPart;
	FString FilenamePart;
	FString ExtensionPart;
	FPaths::Split( Stage.GetRootLayer().GetRealPath(), PathPart, FilenamePart, ExtensionPart );

	FString ExpectedStateLayerPath = FPaths::Combine( PathPart, FString::Printf( TEXT( "%s-UE-persistent-state.%s" ), *FilenamePart, *ExtensionPart ) );
	FPaths::NormalizeFilename( ExpectedStateLayerPath );

	StateLayer = UE::FSdfLayer::FindOrOpen( *ExpectedStateLayerPath );

	if ( !StateLayer && bCreateIfNeeded )
	{
		StateLayer = pxr::SdfLayer::New(
			pxr::SdfFileFormat::FindById( pxr::SdfTextFileFormatTokens->Id ),
			UnrealToUsd::ConvertString( *ExpectedStateLayerPath ).Get()
		);
	}

	// Add the layer as a sublayer of the session layer, in the right location
	// Always check this because we need to do this even if we just loaded an existing state layer from disk
	if ( StateLayer )
	{
		UE::FSdfLayer SessionLayer = Stage.GetSessionLayer();

		// For consistency we always add the UEPersistentState sublayer as the weakest sublayer of the stage's session layer
		// Note that we intentionally only guarantee the UEPersistentLayer is weaker than the UESessionLayer when inserting,
		// so that the user may reorder these if they want, for whatever reason
		bool bNeedsToBeAdded = true;
		for ( const FString& Path : SessionLayer.GetSubLayerPaths() )
		{
			if ( FPaths::IsSamePath( Path, ExpectedStateLayerPath ) )
			{
				bNeedsToBeAdded = false;
				break;
			}
		}

		if ( bNeedsToBeAdded )
		{
			// Always add it at the back, so it's weaker than the session layer
			InsertSubLayer( static_cast< pxr::SdfLayerRefPtr& >( SessionLayer ), *ExpectedStateLayerPath );
		}
	}

	return StateLayer;
}

UE::FSdfLayer UsdUtils::GetUESessionStateSublayer( const UE::FUsdStage& Stage, bool bCreateIfNeeded )
{
	UE::FSdfLayer StateLayer;
	if ( !Stage )
	{
		return StateLayer;
	}

	FScopedUsdAllocs Allocs;

	const pxr::UsdStageRefPtr UsdStage{ Stage };
	pxr::SdfLayerRefPtr UsdSessionLayer = UsdStage->GetSessionLayer();

	FString PathPart;
	FString FilenamePart;
	FString ExtensionPart;
	FPaths::Split( Stage.GetRootLayer().GetRealPath(), PathPart, FilenamePart, ExtensionPart );

	FString ExpectedStateLayerDisplayName = FString::Printf( TEXT( "%s-UE-session-state.%s" ), *FilenamePart, *ExtensionPart );
	FPaths::NormalizeFilename( ExpectedStateLayerDisplayName );

	std::string UsdExpectedStateLayerDisplayName = UnrealToUsd::ConvertString( *ExpectedStateLayerDisplayName ).Get();

	// Check if we already have an existing utils layer in this stage
	std::string ExistingUESessionStateIdentifier;
	{
		std::unordered_set<std::string> SessionLayerSubLayerIdentifiers;
		for ( const std::string& SubLayerIdentifier : UsdSessionLayer->GetSubLayerPaths() )
		{
			SessionLayerSubLayerIdentifiers.insert( SubLayerIdentifier );
		}
		if ( SessionLayerSubLayerIdentifiers.size() > 0 )
		{
			const bool bIncludeSessionLayers = true;
			for ( const pxr::SdfLayerHandle& Layer : UsdStage->GetLayerStack( bIncludeSessionLayers ) )
			{
				// All session layers always come before the root layer
				if ( Layer == UsdStage->GetRootLayer() )
				{
					break;
				}

				const std::string& Identifier = Layer->GetIdentifier();
				if ( Layer->IsAnonymous() && Layer->GetDisplayName() == UsdExpectedStateLayerDisplayName && SessionLayerSubLayerIdentifiers.count( Identifier ) > 0 )
				{
					ExistingUESessionStateIdentifier = Identifier;
					break;
				}
			}
		}
	}

	if ( ExistingUESessionStateIdentifier.size() > 0 )
	{
		StateLayer = UE::FSdfLayer::FindOrOpen( *UsdToUnreal::ConvertString( ExistingUESessionStateIdentifier ) );
	}

	// We only need to add as sublayer when creating the StateLayer layers, because they are always transient and never saved/loaded from disk
	// so if it exists already, it was created right here, where we add it as a sublayer
	if ( !StateLayer && bCreateIfNeeded )
	{
		pxr::SdfLayerRefPtr UsdStateLayer = pxr::SdfLayer::CreateAnonymous( UsdExpectedStateLayerDisplayName );
		UsdSessionLayer->InsertSubLayerPath( UsdStateLayer->GetIdentifier(), 0 ); // Always add it at the front, so it's stronger than the persistent layer

		StateLayer = UsdStateLayer;
	}

	return StateLayer;
}

UE::FSdfLayer UsdUtils::FindLayerForIdentifier( const TCHAR* Identifier, const UE::FUsdStage& Stage )
{
	FScopedUsdAllocs UsdAllocs;

	std::string IdentifierStr = UnrealToUsd::ConvertString( Identifier ).Get();
	if ( pxr::SdfLayer::IsAnonymousLayerIdentifier( IdentifierStr ) )
	{
		std::string DisplayName = pxr::SdfLayer::GetDisplayNameFromIdentifier( IdentifierStr );

		if ( pxr::UsdStageRefPtr UsdStage = static_cast< pxr::UsdStageRefPtr >( Stage ) )
		{
			const bool bIncludeSessionLayers = true;
			for ( const pxr::SdfLayerHandle& Layer : UsdStage->GetLayerStack( bIncludeSessionLayers ) )
			{
				if ( Layer->GetDisplayName() == DisplayName )
				{
					return UE::FSdfLayer{ Layer };
				}
			}
		}
	}
	else
	{
		if ( pxr::SdfLayerRefPtr Layer = pxr::SdfLayer::FindOrOpen( IdentifierStr ) )
		{
			return UE::FSdfLayer{ Layer };
		}
	}

	return UE::FSdfLayer{};
}

bool UsdUtils::IsSessionLayerWithinStage( const pxr::SdfLayerRefPtr& Layer, const pxr::UsdStageRefPtr& Stage )
{
	if ( !Layer || !Stage )
	{
		return false;
	}

	pxr::SdfLayerRefPtr RootLayer = Stage->GetRootLayer();

	const bool bIncludeSessionLayers = true;
	for ( const pxr::SdfLayerHandle& ExistingLayer : Stage->GetLayerStack( bIncludeSessionLayers ) )
	{
		// All session layers come before the root layer within the layer stack
		// Break before we compare with Layer because if Layer is the actual stage's RootLayer we want to return false
		if ( ExistingLayer == RootLayer )
		{
			break;
		}

		if ( ExistingLayer == Layer )
		{
			return true;
		}
	}

	return false;
}

void UsdUtils::ConvertAssetRelativePathsToAbsolute( UE::FSdfLayer& LayerToConvert, const UE::FSdfLayer& AnchorLayer )
{
	FScopedUsdAllocs Allocs;

	pxr::SdfLayerRefPtr UsdLayer{ LayerToConvert };

	UsdLayer->Traverse(
		pxr::SdfPath::AbsoluteRootPath(),
		[ &UsdLayer, &AnchorLayer ]( const pxr::SdfPath& Path )
		{
			pxr::SdfSpecType SpecType = UsdLayer->GetSpecType( Path );
			if ( SpecType != pxr::SdfSpecTypeAttribute )
			{
				return;
			}

			pxr::VtValue LayerValue;
			if ( !UsdLayer->HasField( Path, pxr::SdfFieldKeys->Default, &LayerValue ) )
			{
				return;
			}

			if ( UE::UsdLayerUtilsImpl::Private::FixAssetPaths( pxr::SdfLayerRefPtr{ AnchorLayer }, &LayerValue ) )
			{
				UsdLayer->SetField( Path, pxr::SdfFieldKeys->Default, LayerValue );
			}
		}
	);
}

int32 UsdUtils::GetSdfLayerNumFrames( const pxr::SdfLayerRefPtr& Layer )
{
	if ( !Layer )
	{
		return 0;
	}

	// USD time code range is inclusive on both ends
	return FMath::Abs( FMath::CeilToInt32( Layer->GetEndTimeCode() ) - FMath::FloorToInt32( Layer->GetStartTimeCode() ) + 1 );
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
