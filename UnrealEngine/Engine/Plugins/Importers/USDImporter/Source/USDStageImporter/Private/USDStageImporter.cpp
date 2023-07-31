// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageImporter.h"

#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDLog.h"
#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"
#include "USDStageImportContext.h"
#include "USDStageImportOptions.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdTyped.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogs/DlgPickPath.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "EngineUtils.h"
#include "GeometryCache.h"
#include "GroomAsset.h"
#include "GroomCache.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "LevelSequence.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "USDStageImporter"

namespace UsdStageImporterImpl
{
	void OpenStage( FUsdStageImportContext& ImportContext, bool bNeedsMasking )
	{
		const FString FilePath = !ImportContext.FilePath.IsEmpty()
			? IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead( *ImportContext.FilePath )
			: FString{};

		if ( FilePath.IsEmpty() && !ImportContext.Stage )
		{
			// There's nothing we can do here to create a stage
			return;
		}

		UsdUtils::StartMonitoringErrors();

		UE::FUsdStage Stage;
		if ( bNeedsMasking )
		{
			// If we're masking we'll make a unique stage for us.
			// Also, within USD the OpenMasked methods do not consult the stage cache anyway
			ImportContext.bReadFromStageCache = false;
			ImportContext.bStageWasOriginallyOpenInCache = false;

			// Here even though we won't use the stage cache we don't want to force reload the stage's layers either:
			// The whole point is that we're just opening another masked "view" into the same composed (potentially
			// modified) layers
			const bool bForceReloadLayersFromDisk = false;

			// We don't have a file path to reopen, so just reopen the existing layers
			if ( FilePath.IsEmpty() )
			{
				Stage = UnrealUSDWrapper::OpenMaskedStage(
					ImportContext.Stage.GetRootLayer(),
					ImportContext.Stage.GetSessionLayer(),
					EUsdInitialLoadSet::LoadAll,
					ImportContext.ImportOptions->PrimsToImport,
					bForceReloadLayersFromDisk
				);
			}
			else
			{
				Stage = UnrealUSDWrapper::OpenMaskedStage(
					*FilePath,
					EUsdInitialLoadSet::LoadAll,
					ImportContext.ImportOptions->PrimsToImport,
					bForceReloadLayersFromDisk
				);
			}
		}
		else
		{
			for ( const UE::FUsdStage& OpenedStage : UnrealUSDWrapper::GetAllStagesFromCache() )
			{
				FString RootPath = OpenedStage.GetRootLayer().GetRealPath();
				FPaths::NormalizeFilename( RootPath );
				if ( ImportContext.FilePath == RootPath )
				{
					ImportContext.bStageWasOriginallyOpenInCache = true;
					break;
				}
			}

			const bool bForceReloadLayersFromDisk = !ImportContext.bReadFromStageCache;
			Stage = UnrealUSDWrapper::OpenStage(
				*FilePath,
				EUsdInitialLoadSet::LoadAll,
				ImportContext.bReadFromStageCache,
				bForceReloadLayersFromDisk
			);
		}

		TArray<FString> ErrorStrings = UsdUtils::GetErrorsAndStopMonitoring();
		FString Error = FString::Join(ErrorStrings, TEXT("\n"));

		if (Error.IsEmpty())
		{
			ImportContext.Stage = Stage;
		}
		else
		{
			ImportContext.Stage = UE::FUsdStage();
			FUsdLogManager::LogMessage( EMessageSeverity::Error, FText::Format( LOCTEXT( "CouldNotImportUSDFile", "Could not import USD file {0}\n {1}" ), FText::FromString( FilePath ), FText::FromString( Error ) ) );
		}
	}

	FString FindValidPackagePath(const FString& InPackagePath)
	{
		int32 Suffix = 0;
		FString SearchPackagePath = InPackagePath;
		UPackage* ExistingPackage = nullptr;

		do
		{
			// Look for the package in memory
			ExistingPackage = FindPackage(nullptr, *SearchPackagePath);

			// Look for the package on disk
			if (!ExistingPackage && FPackageName::DoesPackageExist(SearchPackagePath))
			{
				ExistingPackage = LoadPackage(nullptr, *SearchPackagePath, LOAD_None);
			}

			SearchPackagePath = InPackagePath + TEXT("_") + LexToString(Suffix++);
		}
		while(ExistingPackage != nullptr);

		// Undo the last SearchPackagePath update, returning the path that worked (vacant Package path)
		return Suffix == 1 ? InPackagePath : InPackagePath + TEXT("_") + LexToString(Suffix - 1);
	}

	void SetupSceneActor(FUsdStageImportContext& ImportContext)
	{
		if ( !ImportContext.ImportOptions->bImportActors )
		{
			return;
		}

		ULevel* Level = ImportContext.World->GetCurrentLevel();
		if(!Level)
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = ImportContext.ImportObjectFlags & ~RF_Standalone;
		SpawnParameters.OverrideLevel = Level;

		// We always spawn another scene actor regardless of collision or whether the level already has one,
		// so that we can fully build our hierarchy separately before resolving collisions according to ExistingActorPolicy
		AActor* Actor = ImportContext.World->SpawnActor(AActor::StaticClass(), nullptr, SpawnParameters);
		Actor->SetActorLabel(ObjectTools::SanitizeObjectName(ImportContext.ObjectName));

		USceneComponent* RootComponent = Actor->GetRootComponent();
		if (!RootComponent)
		{
			RootComponent = NewObject<USceneComponent>(Actor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
			RootComponent->Mobility = EComponentMobility::Static;
			RootComponent->bVisualizeComponent = false;

			Actor->SetRootComponent(RootComponent);
			Actor->AddInstanceComponent(RootComponent);
		}

		if (RootComponent && !RootComponent->IsRegistered())
		{
			RootComponent->RegisterComponent();
		}

		if ( ImportContext.TargetSceneActorAttachParent )
		{
			RootComponent->AttachToComponent( ImportContext.TargetSceneActorAttachParent, FAttachmentTransformRules::KeepRelativeTransform );
		}

		Actor->SetActorTransform( ImportContext.TargetSceneActorTargetTransform );

		ImportContext.SceneActor = Actor;
	}

	AActor* GetExistingSceneActor(FUsdStageImportContext& ImportContext)
	{
		// We always reuse the existing scene actor for a scene, regardless of ReplacePolicy
		FString TargetActorLabel = ObjectTools::SanitizeObjectName(ImportContext.ObjectName);
		for (TActorIterator<AActor> ActorItr(ImportContext.World); ActorItr; ++ActorItr)
		{
			AActor* ThisActor = *ActorItr;

			// Found a top level actor with the same label
			if ( !ThisActor->HasAnyFlags(RF_Transient) &&
				 ThisActor->GetAttachParentActor() == nullptr &&
				 ThisActor->GetActorLabel() == TargetActorLabel &&
				 ThisActor != ImportContext.SceneActor)
			{
				return ThisActor;
			}
		}

		return nullptr;
	}

	void SetupStageForImport( FUsdStageImportContext& ImportContext )
	{
#if USE_USD_SDK
		if ( ImportContext.ImportOptions->bOverrideStageOptions )
		{
			ImportContext.OriginalMetersPerUnit = UsdUtils::GetUsdStageMetersPerUnit( ImportContext.Stage );
			ImportContext.OriginalUpAxis = UsdUtils::GetUsdStageUpAxisAsEnum( ImportContext.Stage );

			UsdUtils::SetUsdStageMetersPerUnit( ImportContext.Stage, ImportContext.ImportOptions->StageOptions.MetersPerUnit );
			UsdUtils::SetUsdStageUpAxis( ImportContext.Stage, ImportContext.ImportOptions->StageOptions.UpAxis );
		}
#endif // #if USE_USD_SDK
	}

	void CreateAssetsForPrims(const TArray<UE::FUsdPrim>& Prims, FUsdSchemaTranslationContext& TranslationContext, const FText& ProgressMessage )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( CreateAssetsForPrims );

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked<IUsdSchemasModule>(TEXT("USDSchemas"));

		FScopedSlowTask SlowTask( Prims.Num(), ProgressMessage );

		for (const UE::FUsdPrim& Prim : Prims)
		{
			if (TSharedPtr<FUsdSchemaTranslator> SchemaTranslator = UsdSchemasModule.GetTranslatorRegistry().CreateTranslatorForSchema(TranslationContext.AsShared(), UE::FUsdTyped(Prim)))
			{
				SchemaTranslator->CreateAssets();
			}

			SlowTask.EnterProgressFrame();
		}

		TranslationContext.CompleteTasks();
	}

	void CacheCollapsingState( FUsdSchemaTranslationContext& TranslationContext )
	{
		if ( !TranslationContext.InfoCache.IsValid() )
		{
			TranslationContext.InfoCache = MakeShared<FUsdInfoCache>();
		}

		if ( TranslationContext.InfoCache->IsEmpty() )
		{
			TranslationContext.InfoCache->RebuildCacheForSubtree( TranslationContext.Stage.GetPseudoRoot(), TranslationContext );
		}
	}

	void ImportMaterials(FUsdStageImportContext& ImportContext, FUsdSchemaTranslationContext& TranslationContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( ImportMaterials );

		if (!ImportContext.ImportOptions->bImportMaterials)
		{
			return;
		}

		TArray< UE::FUsdPrim > MaterialPrims = UsdUtils::GetAllPrimsOfType( ImportContext.Stage.GetPseudoRoot(), TEXT("UsdShadeMaterial") );

		CreateAssetsForPrims(MaterialPrims, TranslationContext, LOCTEXT("CreateMaterials", "Creating materials") );
	}

	void ImportMeshes(FUsdStageImportContext& ImportContext, FUsdSchemaTranslationContext& TranslationContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( ImportMeshes );

#if USE_USD_SDK
		if (!ImportContext.ImportOptions->bImportGeometry)
		{
			return;
		}

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked<IUsdSchemasModule>(TEXT("USDSchemas"));

		auto PruneCollapsedMeshes = [&UsdSchemasModule, &TranslationContext](const UE::FUsdPrim& UsdPrim) -> bool
		{
			if (TSharedPtr< FUsdSchemaTranslator > SchemaTranslator = UsdSchemasModule.GetTranslatorRegistry().CreateTranslatorForSchema(TranslationContext.AsShared(), UE::FUsdTyped(UsdPrim)))
			{
				return SchemaTranslator->CollapsesChildren(ECollapsingType::Assets);
			}

			return false;
		};

		TArray< UE::FUsdPrim > MeshPrims = UsdUtils::GetAllPrimsOfType( ImportContext.Stage.GetPseudoRoot(), TEXT("UsdGeomXformable"), PruneCollapsedMeshes );
		CreateAssetsForPrims(MeshPrims, TranslationContext, LOCTEXT( "CreateMeshes", "Creating meshes" ) );
#endif // #if USE_USD_SDK
	}

	void ImportAnimation(FUsdStageImportContext& ImportContext, UE::FUsdPrim& Prim, bool bAnimatedVisibility, USceneComponent* SceneComponent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( ImportAnimation );

		if ( !ImportContext.ImportOptions->bImportLevelSequences )
		{
			return;
		}

		UUsdPrimTwin* UsdPrimTwin = NewObject< UUsdPrimTwin >();
		UsdPrimTwin->PrimPath = Prim.GetPrimPath().GetString();
		UsdPrimTwin->SceneComponent = SceneComponent;

		ImportContext.LevelSequenceHelper.AddPrim( *UsdPrimTwin, bAnimatedVisibility );
	}

	void ImportActor(FUsdStageImportContext& ImportContext, UE::FUsdPrim& Prim, bool bForceVisibilityAnimationTracks, FUsdSchemaTranslationContext& TranslationContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( ImportActor );

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >(TEXT("USDSchemas"));

		FScopedSlowTask SlowTask( 3, LOCTEXT("ImportActor", "Importing Actor") );

		bool bExpandChilren = true;
		USceneComponent* Component = nullptr;

		// Spawn components and/or actors for this prim
		if (TSharedPtr<FUsdSchemaTranslator> SchemaTranslator = UsdSchemasModule.GetTranslatorRegistry().CreateTranslatorForSchema(TranslationContext.AsShared(),UE::FUsdTyped(Prim)))
		{
			Component = SchemaTranslator->CreateComponents();

			bExpandChilren = !SchemaTranslator->CollapsesChildren(ECollapsingType::Components);
		}

		// In USD if a parent prim has animated visibility, it will affect the entire subtree. In UE this doesn't
		// happen by default, so if our visibility is animated (or if we're forced to animate visibility from a parent prim),
		// then we must ensure that we generate visibility tracks for our child prims that don't collapse either, so that
		// they get the chance to bake their "computed visibilities" and e.g. be hidden whenever a parent prim goes invisible
#if USE_USD_SDK
		const bool bAnimatedVisibility = bForceVisibilityAnimationTracks || UsdUtils::HasAnimatedVisibility( Prim );
#else
		const bool bAnimatedVisibility = bForceVisibilityAnimationTracks;
#endif

		SlowTask.EnterProgressFrame();
		// Recurse to children
		if (bExpandChilren)
		{
			USceneComponent* ContextParentComponent = Component ? Component : TranslationContext.ParentComponent;
			TGuardValue<USceneComponent*> ParentComponentGuard(TranslationContext.ParentComponent, ContextParentComponent);

			const bool bTraverseInstanceProxies = true;
			for (UE::FUsdPrim Child : Prim.GetFilteredChildren(bTraverseInstanceProxies))
			{
				ImportActor(ImportContext, Child, bAnimatedVisibility, TranslationContext);
			}
		}
		SlowTask.EnterProgressFrame();
		if ( Component )
		{
			// LightComponents specifically need this to setup static lighting
			Component->PostEditChange();

			if ( !Component->IsRegistered() )
			{
				Component->RegisterComponent();
			}

#if USE_USD_SDK
			if ( bAnimatedVisibility || UsdUtils::IsAnimated( Prim ) )
			{
				ImportAnimation(ImportContext, Prim, bAnimatedVisibility, Component);
			}
#endif // USE_USD_SDK
		}
		SlowTask.EnterProgressFrame();
	}

	void ImportActors(FUsdStageImportContext& ImportContext, FUsdSchemaTranslationContext& TranslationContext)
	{
		if (!ImportContext.ImportOptions->bImportActors)
		{
			return;
		}

		const bool bForceVisibilityAnimationTracks = false;
		UE::FUsdPrim RootPrim = ImportContext.Stage.GetPseudoRoot();
		ImportActor(ImportContext, RootPrim, bForceVisibilityAnimationTracks, TranslationContext);
	}

	// Assets coming out of USDSchemas module have default names, so here we do our best to provide them with
	// names based on the source prims. This is likely a temporary solution, as it may be interesting to do this in the
	// USDSchemas module itself
	FString GetUserFriendlyName(UObject* Asset, TSet<FString>& UniqueAssetNames)
	{
		if (!Asset)
		{
			return {};
		}

		FString AssetPrefix;

		FString AssetPath = Asset->GetName();
		if (UUsdAssetImportData* AssetImportData = UsdUtils::GetAssetImportData(Asset))
		{
			AssetPath = AssetImportData->PrimPath;
		}

		FString AssetName = FPaths::GetBaseFilename( AssetPath );

		if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
		{
			AssetPrefix = TEXT("SM_");

			if (UUsdAssetImportData* AssetImportData = Cast<UUsdAssetImportData>(Mesh->AssetImportData))
			{
				// If we have multiple LODs here we must have parsed the LOD variant set pattern. If our prims were named with the LOD
				// pattern, go from e.g. '/Root/MyMesh/LOD0' to '/Root/MyMesh', or else every single LOD mesh will be named "SM_LOD0_X".
				// We'll actually check though because if the user set a custom name for their prim other than LOD0 then we'll keep that
				if ( Mesh->GetNumLODs() > 1 )
				{
					FString PrimName = FPaths::GetBaseFilename( AssetPath );
					if ( PrimName.RemoveFromStart( TEXT( "LOD" ), ESearchCase::CaseSensitive ) )
					{
						if ( PrimName.IsNumeric() )
						{
							AssetPath = FPaths::GetPath( AssetPath );
							AssetName = FPaths::GetBaseFilename( AssetPath );
						}
					}
				}
			}
		}
		else if (USkeletalMesh* SkMesh = Cast<USkeletalMesh>(Asset))
		{
			AssetPrefix = TEXT("SK_");
		}
		else if (USkeleton* Skeleton = Cast<USkeleton>(Asset))
		{
			// Skeletons don't have asset import data, so we can't store the prim that originated them, but we do
			// name the actual assets after the prims, so unlike for the other asset types we want to use the actual
			// asset name here
			AssetName = Asset->GetFName().GetPlainNameString();
			AssetPrefix = TEXT("SKEL_");
		}
		else if ( UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>( Asset ) )
		{
			// See comments above on the case for USkeleton
			FString TempName = Asset->GetFName().GetPlainNameString();

			// The asset is named after the SkelRoot prim. If we're importing back a scene that was originally exported,
			// we should clean up these prefixes or else we may end up with something like "PHYS_SK_PrimName"
			TempName.RemoveFromStart( TEXT( "PHYS_" ), ESearchCase::CaseSensitive );
			TempName.RemoveFromStart( TEXT( "SK_" ), ESearchCase::CaseSensitive );
			if ( !TempName.IsEmpty() )
			{
				AssetName = TempName;
			}

			AssetPrefix = TEXT( "PHYS_" );
		}
		else if ( UAnimSequence* AnimSequence = Cast<UAnimSequence>( Asset ) )
		{
			AssetPrefix = TEXT( "AS_" );
		}
		else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
		{
			if ( Material->IsA<UMaterialInstance>() )
			{
				AssetPrefix = TEXT( "MI_" );
			}
			else
			{
				AssetPrefix = TEXT( "M_" );
			}

			if (UUsdAssetImportData* AssetImportData = Cast<UUsdAssetImportData>(Material->AssetImportData))
			{
				// The only materials with no prim path are our auto-generated displayColor materials
				AssetPath = AssetImportData->PrimPath.IsEmpty()? TEXT("DisplayColor") : AssetImportData->PrimPath;
				AssetName = FPaths::GetBaseFilename( AssetPath );
			}
		}
		else if (UTexture* Texture = Cast<UTexture>(Asset))
		{
			AssetPrefix = TEXT("T_");
		}
		else if ( ULevelSequence* LevelSequence = Cast<ULevelSequence>( Asset ) )
		{
			AssetPrefix = TEXT( "LS_" );
		}
		else if ( UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>( Asset ) )
		{
			FString TempName = AssetName;

			// The asset is named after the SkelRoot prim. If we're importing back a scene that was originally exported,
			// we should clean up these prefixes or else we may end up with something like "ABP_SK_PrimName"
			TempName.RemoveFromStart( TEXT( "ABP_" ), ESearchCase::CaseSensitive );
			TempName.RemoveFromStart( TEXT( "SK_" ), ESearchCase::CaseSensitive );
			if ( !TempName.IsEmpty() )
			{
				AssetName = TempName;
			}

			AssetPrefix = TEXT( "ABP_" );
		}

		if ( !AssetName.StartsWith( AssetPrefix ) )
		{
			AssetName = AssetPrefix + AssetName;
		}

		// We don't care if our assets overwrite something in the final destination package (that conflict will be
		// handled according to EReplaceAssetPolicy). But we do want these assets to have unique names amongst themselves
		// or else they will overwrite each other when publishing
		AssetName = UsdUtils::GetUniqueName( ObjectTools::SanitizeObjectName( AssetName ), UniqueAssetNames );
		UniqueAssetNames.Add(AssetName);

		return AssetName;
	}

	void UpdateAssetImportData( UObject* Asset, const FString& MainFilePath, UUsdStageImportOptions* ImportOptions )
	{
		if ( !Asset )
		{
			return;
		}

		UUsdAssetImportData* ImportData = UsdUtils::GetAssetImportData( Asset );
		if ( !ImportData )
		{
			return;
		}

		// Don't force update as textures will already come with this preset to their actual texture path
		if ( ImportData->SourceData.SourceFiles.Num() == 0 )
		{
			ImportData->UpdateFilenameOnly( MainFilePath );
		}

		ImportData->ImportOptions = ImportOptions;
	}

	void UpdateAssetImportData(const TSet<UObject*>& UsedAssetsAndDependencies, const FString& MainFilePath, UUsdStageImportOptions* ImportOptions)
	{
		for ( UObject* Asset : UsedAssetsAndDependencies )
		{
			UpdateAssetImportData( Asset, MainFilePath, ImportOptions );
		}
	}

	// Moves Asset from its folder to the package at DestFullContentPath and sets up its flags.
	// Depending on ReplacePolicy it may replace the existing actor (if it finds one) or just abort
	UObject* PublishAsset(FUsdStageImportContext& ImportContext, UObject* Asset, const FString& DestFullPackagePath, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap )
	{
		if (!Asset)
		{
			return nullptr;
		}

		EReplaceAssetPolicy ReplacePolicy = ImportContext.ImportOptions->ExistingAssetPolicy;
		FString TargetPackagePath = UPackageTools::SanitizePackageName(DestFullPackagePath);
		FString TargetAssetName = FPaths::GetBaseFilename( TargetPackagePath );
		UObject* ExistingAsset = nullptr;
		UPackage* ExistingPackage = nullptr;

		if ( ReplacePolicy == EReplaceAssetPolicy::Append )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" );
			AssetToolsModule.Get().CreateUniqueAssetName( TargetPackagePath, TEXT(""), TargetPackagePath, TargetAssetName );
		}
		else
		{
			// See if we have an existing asset/package
			ExistingPackage = FindPackage( nullptr, *TargetPackagePath );
			if ( !ExistingPackage && FPackageName::DoesPackageExist( TargetPackagePath ) )
			{
				ExistingPackage = LoadPackage( nullptr, *TargetPackagePath, LOAD_None );
			}
			if ( ExistingPackage )
			{
				FSoftObjectPath ObjectPath( TargetPackagePath );
				ExistingAsset = static_cast< UObject* >( FindObjectWithOuter( ExistingPackage, Asset->GetClass() ) );
				if ( !ExistingAsset )
				{
					ExistingAsset = ObjectPath.TryLoad();
				}
			}

			// If we're ignoring assets that conflict, just abort now
			if ( ExistingAsset != nullptr && ExistingAsset != Asset && ReplacePolicy == EReplaceAssetPolicy::Ignore )
			{
				// Redirect any users of our new transient asset to the old, existing asset
				ObjectsToRemap.Add( Asset, ExistingAsset );
				SoftObjectsToRemap.Add( Asset, ExistingAsset );
				return nullptr;
			}
		}

		// Close editors opened on existing asset if applicable
		bool bAssetWasOpen = false;
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (ExistingAsset && AssetEditorSubsystem->FindEditorForAsset(ExistingAsset, false) != nullptr)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(ExistingAsset);
			bAssetWasOpen = true;
		}

		UPackage* Package = ExistingPackage ? ExistingPackage : CreatePackage(*TargetPackagePath);
		if (!Package)
		{
			FUsdLogManager::LogMessage( EMessageSeverity::Error, FText::Format( LOCTEXT( "PublishFailure", "Failed to get destination package at '{0}' for imported asset '{1}'!" ), FText::FromString( TargetPackagePath ), FText::FromName( Asset->GetFName() ) ) );
			return nullptr;
		}
		Package->FullyLoad();

		FSoftObjectPath OldPath = Asset;

		// Strategy copied from FDatasmithImporterImpl::PublicizeAsset
		// Replace existing asset (reimport or conflict) with new asset
		UObject* MovedAsset = ExistingAsset;
		if (ExistingAsset != nullptr && ExistingAsset != Asset && ReplacePolicy == EReplaceAssetPolicy::Replace)
		{
			// We must do this here or else on the next engine tick the USkeletalMeshComponent::ParallelAnimationEvaluation
			// will try to use the old anim instance, which will try using the old Skeleton's SmartNameContainer and crash.
			// We do not need to recreate the anim instance afterwards as InitAnim is called on component registration anyway.
			for ( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
			{
				if ( It->GetSkeletalMeshAsset() == ExistingAsset)
				{
					It->ClearAnimScriptInstance();
				}
			}

			MovedAsset = DuplicateObject<UObject>( Asset, Package, *TargetAssetName );

			// If our DuplicateObject didn't stomp the old asset because TargetAssetName != ExistingAsset->GetName(),
			// mark the existing asset as garbage so that we collect it when we run GC after the import
			if ( ExistingAsset->GetName() != TargetAssetName )
			{
				ExistingAsset->MarkAsGarbage();
				ImportContext.bNeedsGarbageCollection = true;
			}

			if (UStaticMesh* DestinationMesh = Cast< UStaticMesh >(MovedAsset))
			{
				// This is done during the mesh build process but we need to redo it after the DuplicateObject since the links are now valid
				for (TObjectIterator< UStaticMeshComponent > It; It; ++It)
				{
					if (It->GetStaticMesh() == DestinationMesh)
					{
						It->FixupOverrideColorsIfNecessary(true);
						It->InvalidateLightingCache();
					}
				}
			}
		}
		else
		{
			// We can't dirty the package here. Read the comment around MarkPackageDirty, below
			Asset->Rename(*TargetAssetName, Package, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty );
			MovedAsset = Asset;
		}

		SoftObjectsToRemap.Add( OldPath, MovedAsset );
		if (MovedAsset != Asset)
		{
			ObjectsToRemap.Add(Asset, MovedAsset);
		}

		// Important as some assets (e.g. material instances) are created with no flags
		MovedAsset->SetFlags(ImportContext.ImportObjectFlags | EObjectFlags::RF_Public | EObjectFlags::RF_Standalone );
		MovedAsset->ClearFlags(EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient | EObjectFlags::RF_NonPIEDuplicateTransient);

		// Some subobjects like UStaticMesh::HiResSourceModel->StaticMeshDescriptionBulkData can't be left transient, or else they won't serialize their data.
		// We probably never want to make them public or standalone if they aren't already though
		TArray<UObject*> Subobjects;
		MovedAsset->GetDefaultSubobjects( Subobjects );
		for ( UObject* Subobject : Subobjects )
		{
			Subobject->ClearFlags( EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient | EObjectFlags::RF_NonPIEDuplicateTransient );
		}

		// We need to make sure that "dirtying the final package" is not added to the transaction, because if we undo this transaction
		// the assets should remain on their final destination, so we still want the packages to remain marked as dirty (as they're really not on the disk yet).
		// If we didn't suppress, the package would become transactional by this call. When undoing, the assets would still remain on the final package,
		// but the "dirtying" would be undone, so the engine would think the assets weren't dirty (i.e. were already saved), which is not true
		{
			TGuardValue< ITransaction* > SuppressTransaction{ GUndo, nullptr };
			Package->MarkPackageDirty();
		}

		// Reopen asset editor if we were editing the asset
		if (bAssetWasOpen)
		{
			AssetEditorSubsystem->OpenEditorForAsset(MovedAsset);
		}

		ImportContext.ImportedAsset = MovedAsset;

		return MovedAsset;
	}

	// Move imported assets from transient folder to their final package, updating AssetCache to point to the moved assets
	void PublishAssets(FUsdStageImportContext& ImportContext, const TSet<UObject*>& AssetsToPublish, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap )
	{
		TSet<FString> UniqueAssetNames;

		for ( UObject* Asset : AssetsToPublish )
		{
			if ( !Asset )
			{
				continue;
			}

			if ( Asset->IsA( UGeometryCache::StaticClass() ) )
			{
				UE_LOG( LogUsd, Warning, TEXT( "Ignoring asset '%s': Importing GeometryCaches assets from USD is not supported at this time" ), *Asset->GetName() );
				continue;
			}

			FString AssetTypeFolder;
			if ( ImportContext.ImportOptions->bPrimPathFolderStructure )
			{
				if ( UUsdAssetImportData* ImportData = UsdUtils::GetAssetImportData( Asset ) )
				{
					// For skeletal stuff, the primpaths point to the SkelRoot, so it is useful to place the assets in there,
					// as we'll always have at least the skeletal mesh and the skeleton
					if ( Asset->IsA( USkeletalMesh::StaticClass() ) ||
						 Asset->IsA( USkeleton::StaticClass() ) ||
						 Asset->IsA( UPhysicsAsset::StaticClass() ) ||
						 Asset->IsA( UAnimSequence::StaticClass() ) ||
						 Asset->IsA( UAnimBlueprint::StaticClass() )
					)
					{
						AssetTypeFolder = ImportData->PrimPath;
					}
					else
					{
						AssetTypeFolder = FPaths::GetPath( ImportData->PrimPath );
					}
				}
			}
			else
			{
				if (Asset->IsA(UMaterialInterface::StaticClass()))
				{
					AssetTypeFolder = TEXT("Materials");
				}
				else if (Asset->IsA(UStaticMesh::StaticClass()))
				{
					AssetTypeFolder = TEXT("StaticMeshes");
				}
				else if (Asset->IsA(UGeometryCache::StaticClass()))
				{
					AssetTypeFolder = TEXT("GeometryCaches");
				}
				else if (Asset->IsA(UTexture::StaticClass()))
				{
					AssetTypeFolder = TEXT("Textures");
				}
				else if (
					Asset->IsA( USkeletalMesh::StaticClass() ) ||
					Asset->IsA( USkeleton::StaticClass() ) ||
					Asset->IsA( UPhysicsAsset::StaticClass() ) ||
					Asset->IsA( UAnimSequence::StaticClass() ) ||
					Asset->IsA( UAnimBlueprint::StaticClass() )
				)
				{
					AssetTypeFolder = TEXT("SkeletalMeshes");
				}
				else if ( Asset->IsA( ULevelSequence::StaticClass() ) )
				{
					AssetTypeFolder = TEXT("LevelSequences");
				}
			}

			FString TargetAssetName = GetUserFriendlyName(Asset, UniqueAssetNames);
			FString DestPackagePath = FPaths::Combine(ImportContext.PackagePath, AssetTypeFolder, TargetAssetName);
			PublishAsset(ImportContext, Asset, DestPackagePath, ObjectsToRemap, SoftObjectsToRemap);
		}
	}

	void ResolveComponentConflict(USceneComponent* NewRoot, USceneComponent* ExistingRoot, EReplaceActorPolicy ReplacePolicy, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap )
	{
		if (!NewRoot || !ExistingRoot || ReplacePolicy == EReplaceActorPolicy::Append)
		{
			return;
		}

		ObjectsToRemap.Add(ExistingRoot, NewRoot);
		SoftObjectsToRemap.Add(ExistingRoot, NewRoot);

		TArray<USceneComponent*> ExistingComponents = ExistingRoot->GetAttachChildren();
		TArray<USceneComponent*> NewComponents = NewRoot->GetAttachChildren();

		AActor* NewActor = NewRoot->GetOwner();
		AActor* ExistingActor = ExistingRoot->GetOwner();

		const auto CatalogByName = [](AActor* Owner, const TArray<USceneComponent*>& Components, TMap<FString, USceneComponent*>& Map)
		{
			for (USceneComponent* Component : Components)
			{
				if (Component->GetOwner() == Owner)
				{
					Map.Add(Component->GetName(), Component);
				}
			}
		};

		TMap<FString, USceneComponent*> ExistingComponentsByName;
		TMap<FString, USceneComponent*> NewComponentsByName;
		CatalogByName(ExistingActor, ExistingComponents, ExistingComponentsByName);
		CatalogByName(NewActor, NewComponents, NewComponentsByName);

		// Handle conflict between new and existing hierarchies
		for (const TPair<FString, USceneComponent*>& NewPair : NewComponentsByName)
		{
			const FString& Name = NewPair.Key;
			USceneComponent* NewComponent = NewPair.Value;

			if (USceneComponent** FoundExistingComponent = ExistingComponentsByName.Find(Name))
			{
				bool bRecurse = false;

				switch (ReplacePolicy)
				{
				case EReplaceActorPolicy::UpdateTransform:
					(*FoundExistingComponent)->SetRelativeTransform(NewComponent->GetRelativeTransform());
					(*FoundExistingComponent)->AttachToComponent(NewRoot, FAttachmentTransformRules::KeepRelativeTransform);
					bRecurse = true;
					break;
				case EReplaceActorPolicy::Ignore:
					// Note how we're iterating the new hierarchy here, so "ignore" means "keep the existing one"
					NewComponent->DestroyComponent(false);
					(*FoundExistingComponent)->AttachToComponent(NewRoot, FAttachmentTransformRules::KeepRelativeTransform);
					bRecurse = false;
					break;
				case EReplaceActorPolicy::Replace:
				default:
					// Keep NewChild completely, but recurse to replace components and children
					bRecurse = true;
					break;
				}

				if (bRecurse)
				{
					ResolveComponentConflict(NewComponent, *FoundExistingComponent, ReplacePolicy, ObjectsToRemap, SoftObjectsToRemap);
				}
			}
		}

		// Move child components from the existing hierarchy that don't conflict with anything in the new hierarchy,
		// as the new hierarchy is the one that will remain. Do these later so that we don't recurse into them
		for (const TPair<FString, USceneComponent*>& ExistingPair : ExistingComponentsByName)
		{
			const FString& Name = ExistingPair.Key;
			USceneComponent* ExistingComponent = ExistingPair.Value;

			USceneComponent** FoundNewComponent = NewComponentsByName.Find(Name);
			if (!FoundNewComponent)
			{
				ExistingComponent->AttachToComponent(NewRoot, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}

	void RecursiveDestroyActor(AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}

		const bool bResetArray = false;
		TArray<AActor*> Children;
		Actor->GetAttachedActors(Children, bResetArray);

		for (AActor* Child : Children)
		{
			RecursiveDestroyActor(Child);
		}

		Actor->GetWorld()->DestroyActor(Actor);
	}

	void ResolveActorConflict(AActor* NewActor, AActor* ExistingActor, EReplaceActorPolicy ReplacePolicy, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap)
	{
		if (!NewActor || !ExistingActor || ReplacePolicy == EReplaceActorPolicy::Append)
		{
			return;
		}

		ObjectsToRemap.Add(ExistingActor, NewActor);
		SoftObjectsToRemap.Add( ExistingActor, NewActor );

		// Collect new and existing actors by label
		const bool bResetArray = false;
		TArray<AActor*> ExistingChildren;
		TArray<AActor*> NewChildren;
		ExistingActor->GetAttachedActors(ExistingChildren, bResetArray);
		NewActor->GetAttachedActors(NewChildren, bResetArray);
		const auto CatalogByLabel = [](const TArray<AActor*>& Actors, TMap<FString, AActor*>& Map)
		{
			for (AActor* Actor : Actors)
			{
				Map.Add(Actor->GetActorLabel(), Actor);
			}
		};
		TMap<FString, AActor*> ExistingChildrenByLabel;
		TMap<FString, AActor*> NewChildrenByLabel;
		CatalogByLabel(ExistingChildren, ExistingChildrenByLabel);
		CatalogByLabel(NewChildren, NewChildrenByLabel);

		// Handle conflicts between new and existing actor hierarchies
		for (const TPair<FString, AActor*>& NewPair : NewChildrenByLabel)
		{
			const FString& Label = NewPair.Key;
			AActor* NewChild = NewPair.Value;

			// There's a conflict
			if (AActor** ExistingChild = ExistingChildrenByLabel.Find(Label))
			{
				bool bRecurse = false;

				switch (ReplacePolicy)
				{
				case EReplaceActorPolicy::UpdateTransform:
					(*ExistingChild)->GetRootComponent()->SetRelativeTransform(NewChild->GetRootComponent()->GetRelativeTransform());
					GEditor->ParentActors( NewActor, *ExistingChild, NAME_None );
					bRecurse = true;
					break;
				case EReplaceActorPolicy::Ignore:
					// Note how we're iterating the new hierarchy here, so "ignore" means "keep the existing one"
					RecursiveDestroyActor(NewChild);
					GEditor->ParentActors(NewActor, *ExistingChild, NAME_None);
					bRecurse = false;
					break;
				case EReplaceActorPolicy::Replace:
				default:
					// Keep NewChild, but recurse to replace components and children
					bRecurse = true;
					break;
				}

				if (bRecurse)
				{
					ResolveActorConflict(NewChild, *ExistingChild, ReplacePolicy, ObjectsToRemap, SoftObjectsToRemap);
				}
			}
		}

		// Handle component hierarchy collisions
		USceneComponent* ExistingRoot = ExistingActor->GetRootComponent();
		USceneComponent* NewRoot = NewActor->GetRootComponent();
		ResolveComponentConflict(NewRoot, ExistingRoot, ReplacePolicy, ObjectsToRemap, SoftObjectsToRemap);

		// Move child actors over from existing hierarchy that don't conflict with anything in new hierarchy
		// Do these later so that we don't recurse into them
		for (const TPair<FString, AActor*>& ExistingPair : ExistingChildrenByLabel)
		{
			const FString& Label = ExistingPair.Key;
			AActor* ExistingChild = ExistingPair.Value;

			AActor** NewChild = NewChildrenByLabel.Find(Label);
			if (NewChild == nullptr)
			{
				GEditor->ParentActors(NewActor, ExistingChild, NAME_None);
			}
		}
	}

	void ResolveActorConflicts(FUsdStageImportContext& ImportContext, AActor* ExistingSceneActor, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap)
	{
		if ( !ImportContext.ImportOptions->bImportActors )
		{
			return;
		}

		if (!ImportContext.SceneActor)
		{
			FUsdLogManager::LogMessage( EMessageSeverity::Error, LOCTEXT( "NoSceneActor", "Failed to publish actors as there was no scene actor available!" ) );
			return;
		}

		EReplaceActorPolicy ReplacePolicy = ImportContext.ImportOptions->ExistingActorPolicy;

		// No conflicts, nothing to replace or redirect (even with Append replace mode we don't want to redirect references to the existing items)
		if (!ExistingSceneActor || ReplacePolicy == EReplaceActorPolicy::Append)
		{
			return;
		}

		ResolveActorConflict(ImportContext.SceneActor, ExistingSceneActor, ReplacePolicy, ObjectsToRemap, SoftObjectsToRemap);
	}

	// If we just reimported a static mesh, we use this to remap the material references to the existing materials, as any
	// materials we just reimported will be discarded
	void CopyOriginalMaterialAssignment(FUsdStageImportContext& ImportContext, UObject* ExistingAsset, UObject* NewAsset)
	{
		UStaticMesh* ExistingMesh = Cast<UStaticMesh>(ExistingAsset);
		UStaticMesh* NewMesh = Cast<UStaticMesh>(NewAsset);

		if (ExistingAsset && NewMesh)
		{
			int32 NumExistingMaterials = ExistingMesh->GetStaticMaterials().Num();
			int32 NumNewMaterials = NewMesh->GetStaticMaterials().Num();

			for (int32 NewMaterialIndex = 0; NewMaterialIndex < NumNewMaterials; ++NewMaterialIndex)
			{
				UMaterialInterface* ExistingMaterial = ExistingMesh->GetMaterial(NewMaterialIndex);

				// Can't use SetMaterial as it starts a scoped transaction that would hold on to our transient assets...
				NewMesh->GetStaticMaterials()[NewMaterialIndex].MaterialInterface = ExistingMaterial;
			}

			// Clear out any other assignments we may have
			for (int32 Index = NumNewMaterials; Index < NumExistingMaterials; ++Index)
			{
				NewMesh->GetStaticMaterials()[Index].MaterialInterface = nullptr;
			}

			return;
		}

		USkeletalMesh* ExistingSkeletalMesh = Cast<USkeletalMesh>(ExistingAsset);
		USkeletalMesh* NewSkeletalMesh = Cast<USkeletalMesh>(NewAsset);
		if (ExistingSkeletalMesh && NewSkeletalMesh)
		{
			NewSkeletalMesh->SetMaterials(ExistingSkeletalMesh->GetMaterials());
			return;
		}
	}

	void CopySkeletonAssignment( FUsdStageImportContext& ImportContext, UObject* ExistingAsset, UObject* NewAsset )
	{
		USkeletalMesh* ExistingSkeletalMesh = Cast<USkeletalMesh>( ExistingAsset );
		USkeletalMesh* NewSkeletalMesh = Cast<USkeletalMesh>( NewAsset );
		if ( ExistingSkeletalMesh && NewSkeletalMesh )
		{
			// Never assign a transient skeleton
			if ( ExistingSkeletalMesh->GetSkeleton() && ExistingSkeletalMesh->GetSkeleton()->GetOutermost() == GetTransientPackage() )
			{
				return;
			}

			// Assign even if ExistingSkeletalMesh has nullptr skeleton because we must be able to cleanup the
			// abandoned Skeleton in the transient package
			NewSkeletalMesh->SetSkeleton( ExistingSkeletalMesh->GetSkeleton() );
			NewSkeletalMesh->SetPhysicsAsset( ExistingSkeletalMesh->GetPhysicsAsset() );
		}

		UAnimSequence* ExistingAnimSequence = Cast<UAnimSequence>( ExistingAsset );
		UAnimSequence* NewAnimSequence = Cast<UAnimSequence>( NewAsset );
		if ( ExistingAnimSequence && NewAnimSequence )
		{
			// Never assign a transient skeleton
			USkeleton* ExistingSkeleton = ExistingAnimSequence->GetSkeleton();
			if ( ExistingSkeleton && ExistingSkeleton->GetOutermost() == GetTransientPackage() )
			{
				return;
			}

			NewAnimSequence->SetSkeleton( ExistingSkeleton );
		}
	}

	// Adapted from FDatasmithImporterImpl::FixReferencesForObject
	void RemapReferences(FUsdStageImportContext& ImportContext, const TSet<UObject*>& PublishedObjects, const TMap< UObject*, UObject* >& ObjectsToRemap)
	{
		if (ObjectsToRemap.Num() == 0)
		{
			return;
		}

		// Remap references held by assets that were moved directly to the destination package, and won't be in ObjectsToRemap
		TSet<UObject*> Referencers = PublishedObjects;
		if ( AActor* SceneActor = ImportContext.SceneActor )
		{
			// Remap references to spawned actors
			Referencers.Add( ImportContext.SceneActor->GetWorld()->GetCurrentLevel() );
		}
		for ( const TPair<UObject*, UObject*>& Pair : ObjectsToRemap )
		{
			// Remap internal references between the remapped objects
			Referencers.Add( Pair.Value );
		}

		// Fix references between actors and assets (e.g. mesh in final package referencing material in transient package)
		// Note we don't care if transient assets reference each other, as we'll delete them all at once anyway
		for ( UObject* Referencer : Referencers )
		{
			if ( !Referencer || Referencer->GetOutermost() == GetTransientPackage() )
			{
				continue;
			}

			constexpr EArchiveReplaceObjectFlags ReplaceFlags = (EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
			FArchiveReplaceObjectRef< UObject > ArchiveReplaceObjectRefInner(Referencer, ObjectsToRemap, ReplaceFlags);
		}


	}

	void CallAssetsPostEditChange(const TSet<UObject*>& PublishedObjects)
	{
		for (UObject* PublishedObject : PublishedObjects)
		{
			if (UMaterialInstance* Material = Cast<UMaterialInstance>(PublishedObject))
			{
				// After we remapped all references to the persistent assets we need to call PostEditChange on
				// all materials. This because material instance proxies hold direct references to the UTextures that
				// are used by the material, and those references will be left pointing at the transient assets in
				// case we had to use DuplicateObject to "publish" them to their final locations. Calling
				// PostEditChange rebuilds those proxies from the UMaterialInstance's parameters, which *have* been
				// remapped by our RemapReferences and RemapSoftReferences functions.
				Material->PostEditChange();
			}
		}
	}

	void Cleanup(AActor* NewSceneActor, AActor* ExistingSceneActor, EReplaceActorPolicy ReplacePolicy)
	{
		if ( !NewSceneActor )
		{
			return;
		}

		// By this point all of our actors and components are moved to the new hierarchy, and all references
		// are remapped. So let's clear the replaced existing actors and components
		if (ExistingSceneActor && ExistingSceneActor != NewSceneActor && ReplacePolicy == EReplaceActorPolicy::Replace)
		{
			RecursiveDestroyActor(ExistingSceneActor);
		}
	}

	void CloseStageIfNeeded(FUsdStageImportContext& ImportContext)
	{
#if USE_USD_SDK
		// Remove our imported stage from the stage cache if it wasn't in there to begin with
		if (!ImportContext.bStageWasOriginallyOpenInCache && ImportContext.bReadFromStageCache)
		{
			UnrealUSDWrapper::EraseStageFromCache(ImportContext.Stage);
		}

		if ( ImportContext.ImportOptions->bOverrideStageOptions )
		{
			UsdUtils::SetUsdStageMetersPerUnit( ImportContext.Stage, ImportContext.OriginalMetersPerUnit );
			UsdUtils::SetUsdStageUpAxis( ImportContext.Stage, ImportContext.OriginalUpAxis );
		}

		// Always discard the context's reference to the stage because it may be a persistent import context (like
		// the non-static data member of UUsdStageImportFactory
		ImportContext.Stage = UE::FUsdStage();
		ImportContext.LevelSequenceHelper.Init( UE::FUsdStage() );
#endif // #if USE_USD_SDK
	}

	/**
	 * FUsdAssetCache can track which assets are requested/added to itself during translation, but it may miss some dependencies
	 * that are only retrieved/added themselves when the original asset is first parsed. This function recursively collects all of those.
	 * Example: An UMaterialInstance is already in the cache, so when translating we just retrieve the existing asset --> The textures that it's using won't be retrieved or marked as "Used"
	 * Example: An USkeletalMesh is already in the cache, so in the same way we would miss its USkeleton, materials and textures of those materials
	 */
	void CollectUsedAssetDependencies( FUsdStageImportContext& ImportContext, TSet<UObject*>& OutAssetsAndDependencies )
	{
		const int32 ReserveSize = OutAssetsAndDependencies.Num() + ( ImportContext.AssetCache ? ImportContext.AssetCache->GetActiveAssets().Num() : 0 );

		// We will only emit the level sequences if we have data in the main one.
		// Keep subsequences even if they have no data as the main sequence/other sequences may reference them
		if ( ImportContext.ImportOptions->bImportLevelSequences && ImportContext.LevelSequenceHelper.HasData() )
		{
			TArray<ULevelSequence*> SubSequences = ImportContext.LevelSequenceHelper.GetSubSequences();
			ULevelSequence* MainSequence = ImportContext.LevelSequenceHelper.GetMainLevelSequence();

			OutAssetsAndDependencies.Reserve( ReserveSize + SubSequences.Num() + 1 );
			OutAssetsAndDependencies.Add( MainSequence );
			for ( ULevelSequence* SubSequence : SubSequences )
			{
				OutAssetsAndDependencies.Add( SubSequence );
			}
		}
		else
		{
			OutAssetsAndDependencies.Reserve( ReserveSize );
		}

		if ( ImportContext.AssetCache )
		{
			const TSet<UObject*>& InPrimaryAssets = ImportContext.AssetCache->GetActiveAssets();
			TArray<UObject*> AssetQueue = InPrimaryAssets.Array();

			for ( int32 AssetIndex = 0; AssetIndex < AssetQueue.Num(); ++AssetIndex )
			{
				UObject* Asset = AssetQueue[ AssetIndex ];

				// Only add it as a dependency if it's owned by the asset cache, but still traverse it because
				// we may be in some strange situation where the material shouldn't be in this list, but one of its used textures should
				if ( ImportContext.AssetCache->IsAssetOwnedByCache( Asset ) )
				{
					OutAssetsAndDependencies.Add( Asset );
				}

				if ( UMaterial* Material = Cast<UMaterial>( Asset ) )
				{
					TArray<UTexture*> UsedTextures;
					const bool bAllQualityLevels = true;
					const bool bAllFeatureLevels = true;
					Material->GetUsedTextures( UsedTextures, EMaterialQualityLevel::High, bAllQualityLevels, ERHIFeatureLevel::SM5, bAllFeatureLevels );

					OutAssetsAndDependencies.Reserve( OutAssetsAndDependencies.Num() + UsedTextures.Num() );
					for ( UTexture* UsedTexture : UsedTextures )
					{
						if ( ImportContext.AssetCache->IsAssetOwnedByCache( UsedTexture ) )
						{
							OutAssetsAndDependencies.Add( UsedTexture );
						}
					}
				}
				else if ( UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( Asset ) )
				{
					OutAssetsAndDependencies.Reserve( OutAssetsAndDependencies.Num() + MaterialInstance->TextureParameterValues.Num() );
					for ( const FTextureParameterValue& TextureValue : MaterialInstance->TextureParameterValues )
					{
						if ( UTexture* Texture = TextureValue.ParameterValue )
						{
							if ( ImportContext.AssetCache->IsAssetOwnedByCache( Texture ) )
							{
								OutAssetsAndDependencies.Add( Texture );
							}
						}
					}
				}
				else if ( USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>( Asset ) )
				{
					if ( USkeleton* Skeleton = SkeletalMesh->GetSkeleton() )
					{
						if ( ImportContext.AssetCache->IsAssetOwnedByCache( Skeleton ) )
						{
							OutAssetsAndDependencies.Add( Skeleton );
						}
					}

					if ( UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset() )
					{
						if ( ImportContext.AssetCache->IsAssetOwnedByCache( PhysicsAsset ) )
						{
							OutAssetsAndDependencies.Add( PhysicsAsset );
						}
					}

					AssetQueue.Reserve( AssetQueue.Num() + SkeletalMesh->GetMaterials().Num() );
					for ( const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials() )
					{
						if ( UMaterialInterface* UsedMaterial = SkeletalMaterial.MaterialInterface )
						{
							AssetQueue.Add( UsedMaterial );
						}
					}
				}
				else if ( UStaticMesh* StaticMesh = Cast<UStaticMesh>( Asset ) )
				{
					AssetQueue.Reserve( AssetQueue.Num() + StaticMesh->GetStaticMaterials().Num() );
					for ( const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials() )
					{
						if ( UMaterialInterface* UsedMaterial = StaticMaterial.MaterialInterface )
						{
							AssetQueue.Add( UsedMaterial );
						}
					}
				}
				else if ( UGeometryCache* GeometryCache = Cast<UGeometryCache>( Asset ) )
				{
					for ( UMaterialInterface* UsedMaterial : GeometryCache->Materials )
					{
						if ( UsedMaterial )
						{
							AssetQueue.Add( UsedMaterial );
						}
					}
				}
				else if ( UAnimSequence* AnimSequence = Cast<UAnimSequence>( Asset ) )
				{
					if ( USkeletalMesh* Mesh = AnimSequence->GetPreviewMesh() )
					{
						AssetQueue.Add( Mesh );
					}

					if ( USkeleton* Skeleton = AnimSequence->GetSkeleton() )
					{
						if ( ImportContext.AssetCache->IsAssetOwnedByCache( Skeleton ) )
						{
							OutAssetsAndDependencies.Add( Skeleton );
						}
					}
				}
				else if ( UTexture* Texture = Cast<UTexture>( Asset ) )
				{
					// Do nothing. Textures have no additional dependencies
				}
				else if ( USkeleton* Skeleton = Cast<USkeleton>( Asset ) )
				{
					// Do nothing. Skeletons have no additional dependencies
				}
				else if ( UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>( Asset ) )
				{
					// Do nothing. PhysicsAssets have no additional dependencies
				}
				else if ( UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>( Asset ) )
				{
					// Do nothing. AnimBlueprints have no additional dependencies
				}
				else
				{
					UE_LOG( LogUsd, Warning, TEXT( "Unknown asset '%s' encountered when collecting used assets before USD import." ), Asset ? *Asset->GetName() : TEXT( "nullptr" ) );
				}
			}
		}
	}

	/**
	 * Remaps asset's soft object pointers to point to the post-publish paths of their target assets.
	 * It's important to run this *after* RemapReferences, as we will sometimes rely on those references to find our target assets.
	 */
	void RemapSoftReferences( const FUsdStageImportContext& ImportContext, const TSet<UObject*>& UsedAssetsAndDependencies, const TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap )
	{
		TSet<UPackage*> Packages;
		for ( UObject* Object : UsedAssetsAndDependencies )
		{
			if ( Object )
			{
				Packages.Add( Object->GetOutermost() );
			}
		}

		if ( AActor* SceneActor = ImportContext.SceneActor )
		{
			Packages.Add( ImportContext.SceneActor->GetWorld()->GetOutermost() );
		}

		// In case one our used assets was left on the transient package.
		// We don't care about anything that was left on the transient package, and doing this may actually cause some reference counting issues
		// if we try deleting those assets afterwards
		Packages.Remove( GetTransientPackage() );

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>( "AssetTools" ).Get();
		AssetTools.RenameReferencingSoftObjectPaths( Packages.Array(), SoftObjectsToRemap );
	}

	void GetPublishedAssetsAndDependencies(
		const TSet<UObject*>& UsedAssetsAndDependencies,
		const TMap< UObject*, UObject* >& ObjectsToRemap,
		const TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap,
		TSet<UObject*>& OutPublishedAssetsAndDependencies
	)
	{
		OutPublishedAssetsAndDependencies.Empty(UsedAssetsAndDependencies.Num());

		for (UObject* Asset : UsedAssetsAndDependencies)
		{
			if (!Asset)
			{
				continue;
			}

			UObject* RemappedAsset = ObjectsToRemap.FindRef(Asset);

			// The asset itself was renamed onto its final package, no remapping needed
			if (!RemappedAsset && Asset->GetOutermost() != GetTransientPackage())
			{
				OutPublishedAssetsAndDependencies.Add(Asset);
				continue;
			}

			// If we have remapped it, ObjectsToRemap and SoftObjectsToRemap should have agreed on where to
			const FSoftObjectPath* RemappedSoftAsset = SoftObjectsToRemap.Find(FSoftObjectPath{Asset});
			if (!RemappedAsset || !RemappedSoftAsset || RemappedAsset != RemappedSoftAsset->TryLoad())
			{
				UE_LOG(LogUsd, Warning, TEXT("Failed to publish or remap asset '%s'!"), *Asset->GetPathName());
				continue;
			}

			OutPublishedAssetsAndDependencies.Add(RemappedAsset);
		}
	}

	/** After we remapped everything, notify the AssetRegistry that we created some new assets */
	void NotifyAssetRegistry( const TSet<UObject*>& UsedAssetsAndDependencies )
	{
		for ( UObject* Object : UsedAssetsAndDependencies )
		{
			// If it's still on the transient package it means we abandoned this one (maybe we had asset replace policy ignore and hit a conflict)
			if ( Object->GetOutermost() != GetTransientPackage() )
			{
				FAssetRegistryModule::AssetCreated( Object );
			}
		}
	}

	void SendAnalytics(
		FUsdStageImportContext& ImportContext,
		UObject* Asset,
		const FString& Operation,
		const TSet<UObject*>& ImportedAssets,
		double ElapsedSeconds
	)
	{
#if USE_USD_SDK
		if ( FEngineAnalytics::IsAvailable() )
		{
			TArray<FAnalyticsEventAttribute> EventAttributes;

			FString EventName = Operation;
			if ( Asset )
			{
				FString ClassName = Asset->GetClass()->GetName();

				// e.g. "Reimport.StaticMesh"
				EventName = FString::Printf( TEXT( "%s.%s" ), *EventName, *ClassName );
				EventAttributes.Emplace( TEXT( "AssetType" ), ClassName );
			}

			if ( ImportContext.ImportOptions )
			{
				UsdUtils::AddAnalyticsAttributes( *ImportContext.ImportOptions, EventAttributes );
			}

			int32 NumStaticMeshes = 0;
			int32 NumSkeletalMeshes = 0;
			int32 NumMaterials = 0;
			int32 NumAnimSequences = 0;
			int32 NumLevelSequences = 0;
			int32 NumTextures = 0;
			int32 NumGeometryCaches = 0;
			int32 NumGroomAssets = 0;
			int32 NumGroomCaches = 0;
			for ( UObject* ImportedAsset : ImportedAssets )
			{
				if ( !ImportedAsset )
				{
					continue;
				}

				if ( ImportedAsset->IsA<UStaticMesh>() )
				{
					++NumStaticMeshes;
				}
				else if ( ImportedAsset->IsA<USkeletalMesh>() )
				{
					++NumSkeletalMeshes;
				}
				else if ( ImportedAsset->IsA<UMaterialInterface>() )
				{
					++NumMaterials;
				}
				else if ( ImportedAsset->IsA<UAnimSequence>() )
				{
					++NumAnimSequences;
				}
				else if ( ImportedAsset->IsA<ULevelSequence>() )
				{
					++NumLevelSequences;
				}
				else if ( ImportedAsset->IsA<UTexture>() )
				{
					++NumTextures;
				}
				else if ( ImportedAsset->IsA<UGeometryCache>() )
				{
					++NumGeometryCaches;
				}
				else if ( ImportedAsset->IsA<UGroomAsset>() )
				{
					++NumGroomAssets;
				}
				else if ( ImportedAsset->IsA<UGroomCache>() )
				{
					++NumGroomCaches;
				}
			}
			EventAttributes.Emplace( TEXT( "NumStaticMeshes" ), LexToString( NumStaticMeshes ) );
			EventAttributes.Emplace( TEXT( "NumSkeletalMeshes" ), LexToString( NumSkeletalMeshes ) );
			EventAttributes.Emplace( TEXT( "NumMaterials" ), LexToString( NumMaterials ) );
			EventAttributes.Emplace( TEXT( "NumAnimSequences" ), LexToString( NumAnimSequences ) );
			EventAttributes.Emplace( TEXT( "NumLevelSequences" ), LexToString( NumLevelSequences ) );
			EventAttributes.Emplace( TEXT( "NumTextures" ), LexToString( NumTextures ) );
			EventAttributes.Emplace( TEXT( "NumGeometryCaches" ), LexToString( NumGeometryCaches ) );
			EventAttributes.Emplace( TEXT( "NumGroomAssets" ), LexToString( NumGroomAssets ) );
			EventAttributes.Emplace( TEXT( "NumGroomCaches" ), LexToString( NumGroomCaches ) );

			FString RootLayerIdentifier = ImportContext.FilePath;
			if ( ImportContext.Stage )
			{
				if ( RootLayerIdentifier.IsEmpty() )
				{
					RootLayerIdentifier = ImportContext.Stage.GetRootLayer().GetIdentifier();
				}
			}

			IUsdClassesModule::SendAnalytics(
				MoveTemp( EventAttributes ),
				EventName,
				ImportContext.bIsAutomated,
				ElapsedSeconds,
				UsdUtils::GetUsdStageNumFrames( ImportContext.Stage ),
				FPaths::GetExtension( RootLayerIdentifier )
			);
		}
#endif // USE_USD_SDK
	}

	// Removes from AssetsToImport assets that are unwanted according to our import options, and adds entries to
	// ObjectsToRemap and SoftObjectsToRemap that remaps them to nullptr.
	// This function is needed because it's not enough to e.g. just prevent new meshes from being imported from
	// UsdStageImporterImpl::ImportMeshes, because we may want to reuse meshes we already got from the asset cache.
	// Additionally, we'll want to remap even our components away from pointing to these assets
	void PruneUnwantedAssets( FUsdStageImportContext& ImportContext, TSet<UObject*>& AssetsToImport, TMap<UObject*, UObject*>& ObjectsToRemap, TMap<FSoftObjectPath, FSoftObjectPath>& SoftObjectsToRemap )
	{
		const bool bImportSkeletalAnimations = ImportContext.ImportOptions->bImportGeometry && ImportContext.ImportOptions->bImportSkeletalAnimations;

		for ( TSet<UObject*>::TIterator It( AssetsToImport ); It; ++It )
		{
			UObject* Asset = *It;

			if ( !Asset )
			{
				It.RemoveCurrent();
				continue;
			}

			if (
				( !ImportContext.ImportOptions->bImportGeometry && (
					Asset->IsA<UStaticMesh>() ||
					Asset->IsA<USkeletalMesh>() ||
					Asset->IsA<USkeleton>() ||
					Asset->IsA<UPhysicsAsset>() ||
					Asset->IsA<UGeometryCache>()
				) ) ||
				( !bImportSkeletalAnimations && ( Asset->IsA<UAnimSequence>() ) ) ||
				( !ImportContext.ImportOptions->bImportLevelSequences && ( Asset->IsA<ULevelSequence>() ) ) ||
				( !ImportContext.ImportOptions->bImportMaterials && ( Asset->IsA<UMaterialInterface>() || Asset->IsA<UTexture>() ) )
			)
			{
				ObjectsToRemap.Add( Asset, nullptr );
				SoftObjectsToRemap.Add( Asset, nullptr );
				It.RemoveCurrent();
			}
		}
	}

	// We need to recreate the render state for some mesh component types in case we changed the materials that are assigned to them.
	// Also, skeletal mesh components need to be manually ticked, or else they may be showing an animated state of an animation that
	// we chose not to import, and wouldn't update otherwise until manually ticked by the user (or after save/reload), which may look
	// like a bug
	void RefreshComponents( AActor* RootSceneActor )
	{
		if ( !RootSceneActor )
		{
			return;
		}

		TArray<USceneComponent*> Components;
		const bool bIncludeAllDescendants = true;
		RootSceneActor->GetRootComponent()->GetChildrenComponents( bIncludeAllDescendants, Components );

		for ( USceneComponent* Component : Components )
		{
			if ( USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>( Component ) )
			{
				if ( SkeletalMeshComponent->AnimationData.AnimToPlay == nullptr )
				{
					SkeletalMeshComponent->TickAnimation( 0.f, false );
					SkeletalMeshComponent->RefreshBoneTransforms();
					SkeletalMeshComponent->RefreshFollowerComponents();
					SkeletalMeshComponent->UpdateComponentToWorld();
					SkeletalMeshComponent->FinalizeBoneTransform();
					SkeletalMeshComponent->MarkRenderTransformDirty();
					SkeletalMeshComponent->MarkRenderDynamicDataDirty();
				}

				// It does need us to manually set this to dirty regardless or else it won't update in case we changed material
				// assignments
				SkeletalMeshComponent->MarkRenderStateDirty();
			}
		}
	}
}

void UUsdStageImporter::ImportFromFile(FUsdStageImportContext& ImportContext)
{
#if USE_USD_SDK
	if (!ImportContext.World)
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, LOCTEXT( "NoWorldError", "Failed to import USD Stage because the target UWorld is invalid!" ) );
		return;
	}

	double StartTime = FPlatformTime::Cycles64();

	if ( ImportContext.ImportOptions->PrimsToImport.Num() == 0 )
	{
		return;
	}

	const bool bNeedsMasking =
		ImportContext.ImportOptions->PrimsToImport != TArray<FString>{ UE::FSdfPath::AbsoluteRootPath().GetString() };

	if ( !ImportContext.Stage || bNeedsMasking )
	{
		UsdStageImporterImpl::OpenStage( ImportContext, bNeedsMasking );
	}

	if ( !ImportContext.Stage )
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, LOCTEXT( "NoStageError", "Failed to open the USD Stage!" ) );
		return;
	}

	UsdStageImporterImpl::SetupSceneActor( ImportContext );
	if ( !ImportContext.SceneActor && ImportContext.ImportOptions->bImportActors )
	{
		return;
	}

	FUsdDelegates::OnPreUsdImport.Broadcast( ImportContext.FilePath );

	AActor* ExistingSceneActor = UsdStageImporterImpl::GetExistingSceneActor( ImportContext );

	UsdStageImporterImpl::SetupStageForImport( ImportContext );

	TMap<FSoftObjectPath, FSoftObjectPath> SoftObjectsToRemap;
	TMap<UObject*, UObject*> ObjectsToRemap;
	TSet<UObject*> UsedAssetsAndDependencies;
	TSet<UObject*> PublishedAssetsAndDependencies;
	UsdUtils::FBlendShapeMap BlendShapesByPath;

	// Ensure a valid asset cache
	if ( !ImportContext.AssetCache )
	{
		ImportContext.AssetCache = NewObject<UUsdAssetCache>();
	}
	ImportContext.AssetCache->MarkAssetsAsStale();
	ImportContext.LevelSequenceHelper.SetAssetCache( ImportContext.AssetCache );
	ImportContext.LevelSequenceHelper.Init( ImportContext.Stage );  // Must happen after the context gets an AssetCache!

	// Shotgun approach to recreate all render states because we may want to reimport/delete/reassign a material/static/skeletalmesh while it is currently being drawn
	FGlobalComponentRecreateRenderStateContext RecreateRenderStateContext;

	TSharedRef<FUsdSchemaTranslationContext> TranslationContext = MakeShared<FUsdSchemaTranslationContext>( ImportContext.Stage, *ImportContext.AssetCache );
	TranslationContext->bIsImporting = true;
	TranslationContext->Level = ImportContext.World->GetCurrentLevel();
	TranslationContext->ObjectFlags = ImportContext.ImportObjectFlags;
	TranslationContext->Time = static_cast< float >( UsdUtils::GetDefaultTimeCode() );
	TranslationContext->PurposesToLoad = ( EUsdPurpose ) ImportContext.ImportOptions->PurposesToImport;
	TranslationContext->NaniteTriangleThreshold = ImportContext.ImportOptions->NaniteTriangleThreshold;
	TranslationContext->RenderContext = ImportContext.ImportOptions->RenderContextToImport;
	TranslationContext->MaterialPurpose = ImportContext.ImportOptions->MaterialPurpose;
	TranslationContext->RootMotionHandling = ImportContext.ImportOptions->RootMotionHandling;
	TranslationContext->ParentComponent = ImportContext.SceneActor ? ImportContext.SceneActor->GetRootComponent() : nullptr;
	TranslationContext->KindsToCollapse = ( EUsdDefaultKind ) ImportContext.ImportOptions->KindsToCollapse;
	TranslationContext->bMergeIdenticalMaterialSlots = ImportContext.ImportOptions->bMergeIdenticalMaterialSlots;
	TranslationContext->bCollapseTopLevelPointInstancers = ImportContext.ImportOptions->bCollapseTopLevelPointInstancers;
	TranslationContext->bAllowInterpretingLODs = ImportContext.ImportOptions->bInterpretLODs;
	TranslationContext->bAllowParsingSkeletalAnimations = ImportContext.ImportOptions->bImportGeometry && ImportContext.ImportOptions->bImportSkeletalAnimations;
	TranslationContext->MaterialToPrimvarToUVIndex = &ImportContext.MaterialToPrimvarToUVIndex;
	TranslationContext->InfoCache = ImportContext.InfoCache;
	TranslationContext->BlendShapesByPath = &BlendShapesByPath;
	TranslationContext->GroomInterpolationSettings = ImportContext.ImportOptions->GroomInterpolationSettings;
	{
		UsdStageImporterImpl::CacheCollapsingState( TranslationContext.Get() );
		UsdStageImporterImpl::ImportMaterials( ImportContext, TranslationContext.Get() );
		UsdStageImporterImpl::ImportMeshes( ImportContext, TranslationContext.Get() );
		UsdStageImporterImpl::ImportActors( ImportContext, TranslationContext.Get() );
	}
	TranslationContext->CompleteTasks();

	UsdStageImporterImpl::CollectUsedAssetDependencies( ImportContext, UsedAssetsAndDependencies );
	UsdStageImporterImpl::PruneUnwantedAssets( ImportContext, UsedAssetsAndDependencies, ObjectsToRemap, SoftObjectsToRemap );
	UsdStageImporterImpl::UpdateAssetImportData( UsedAssetsAndDependencies, ImportContext.FilePath, ImportContext.ImportOptions );
	UsdStageImporterImpl::PublishAssets( ImportContext, UsedAssetsAndDependencies, ObjectsToRemap, SoftObjectsToRemap );
	UsdStageImporterImpl::ResolveActorConflicts( ImportContext, ExistingSceneActor, ObjectsToRemap, SoftObjectsToRemap );
	UsdStageImporterImpl::RemapReferences( ImportContext, UsedAssetsAndDependencies, ObjectsToRemap );
	UsdStageImporterImpl::RemapSoftReferences( ImportContext, UsedAssetsAndDependencies, SoftObjectsToRemap );
	UsdStageImporterImpl::GetPublishedAssetsAndDependencies(UsedAssetsAndDependencies, ObjectsToRemap, SoftObjectsToRemap, PublishedAssetsAndDependencies);
	UsdStageImporterImpl::CallAssetsPostEditChange(PublishedAssetsAndDependencies);
	UsdStageImporterImpl::Cleanup( ImportContext.SceneActor, ExistingSceneActor, ImportContext.ImportOptions->ExistingActorPolicy );
	UsdStageImporterImpl::NotifyAssetRegistry(PublishedAssetsAndDependencies);
	UsdStageImporterImpl::RefreshComponents( ImportContext.SceneActor );

	FUsdDelegates::OnPostUsdImport.Broadcast( ImportContext.FilePath );

	// Analytics
	{
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		UsdStageImporterImpl::SendAnalytics( ImportContext, nullptr, TEXT("Import"), PublishedAssetsAndDependencies, ElapsedSeconds);
	}

	UsdStageImporterImpl::CloseStageIfNeeded( ImportContext );

	if ( ImportContext.bNeedsGarbageCollection )
	{
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}
#endif // #if USE_USD_SDK
}

bool UUsdStageImporter::ReimportSingleAsset(FUsdStageImportContext& ImportContext, UObject* OriginalAsset, UUsdAssetImportData* OriginalImportData, UObject*& OutReimportedAsset)
{
	OutReimportedAsset = nullptr;
	bool bSuccess = false;

#if USE_USD_SDK
	double StartTime = FPlatformTime::Cycles64();

	// TODO: Maybe change this whole reimporting approach to just taking advantage of a population mask instead?
	const bool bNeedsMasking =
		ImportContext.ImportOptions->PrimsToImport != TArray<FString>{ UE::FSdfPath::AbsoluteRootPath().GetString() };

	if ( !ImportContext.Stage || bNeedsMasking )
	{
		UsdStageImporterImpl::OpenStage( ImportContext, bNeedsMasking );
	}

	if ( !ImportContext.Stage )
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, LOCTEXT( "NoStageError", "Failed to open the USD Stage!" ) );
		return bSuccess;
	}

	FUsdDelegates::OnPreUsdImport.Broadcast(ImportContext.FilePath);

	// We still need the scene actor to remap all other users of the mesh to the new reimported one. It's not critical if we fail though,
	// the goal is to just reimport the asset
	UsdStageImporterImpl::SetupSceneActor(ImportContext);

	UsdStageImporterImpl::SetupStageForImport( ImportContext );

	TMap<FSoftObjectPath, FSoftObjectPath> SoftObjectsToRemap;
	TMap<UObject*, UObject*> ObjectsToRemap;
	UsdUtils::FBlendShapeMap BlendShapesByPath;

	// Ensure a valid asset cache
	if ( !ImportContext.AssetCache )
	{
		ImportContext.AssetCache = NewObject<UUsdAssetCache>();
	}
	ImportContext.AssetCache->MarkAssetsAsStale();

	// Shotgun approach to recreate all render states because we may want to reimport/delete/reassign a material/static/skeletalmesh while it is currently being drawn
	FGlobalComponentRecreateRenderStateContext RecreateRenderStateContext;

	TSharedRef<FUsdSchemaTranslationContext> TranslationContext = MakeShared<FUsdSchemaTranslationContext>( ImportContext.Stage, *ImportContext.AssetCache );
	TranslationContext->bIsImporting = true;
	TranslationContext->Level = ImportContext.World->GetCurrentLevel();
	TranslationContext->ObjectFlags = ImportContext.ImportObjectFlags;
	TranslationContext->Time = static_cast< float >( UsdUtils::GetDefaultTimeCode() );
	TranslationContext->PurposesToLoad = ( EUsdPurpose ) ImportContext.ImportOptions->PurposesToImport;
	TranslationContext->NaniteTriangleThreshold = ImportContext.ImportOptions->NaniteTriangleThreshold;
	TranslationContext->RenderContext = ImportContext.ImportOptions->RenderContextToImport;
	TranslationContext->MaterialPurpose = ImportContext.ImportOptions->MaterialPurpose;
	TranslationContext->RootMotionHandling = ImportContext.ImportOptions->RootMotionHandling;
	TranslationContext->KindsToCollapse = ( EUsdDefaultKind ) ImportContext.ImportOptions->KindsToCollapse;
	TranslationContext->bMergeIdenticalMaterialSlots = ImportContext.ImportOptions->bMergeIdenticalMaterialSlots;
	TranslationContext->bCollapseTopLevelPointInstancers = ImportContext.ImportOptions->bCollapseTopLevelPointInstancers;
	TranslationContext->bAllowInterpretingLODs = ImportContext.ImportOptions->bInterpretLODs;
	TranslationContext->bAllowParsingSkeletalAnimations = ImportContext.ImportOptions->bImportGeometry && ImportContext.ImportOptions->bImportSkeletalAnimations;
	TranslationContext->MaterialToPrimvarToUVIndex = &ImportContext.MaterialToPrimvarToUVIndex;
	TranslationContext->InfoCache = ImportContext.InfoCache;
	TranslationContext->BlendShapesByPath = &BlendShapesByPath;
	TranslationContext->GroomInterpolationSettings = ImportContext.ImportOptions->GroomInterpolationSettings;
	{
		UsdStageImporterImpl::CacheCollapsingState( TranslationContext.Get() );

		UE::FUsdPrim TargetPrim = ImportContext.Stage.GetPrimAtPath( UE::FSdfPath( *OriginalImportData->PrimPath ) );
		if ( TargetPrim )
		{
			UsdStageImporterImpl::CreateAssetsForPrims({TargetPrim}, TranslationContext.Get(), LOCTEXT("CreateAssets", "Creating assets"));
		}
	}
	TranslationContext->CompleteTasks();

	// Look for our reimported asset in the assets cache as we may have multiple assets with the same prim path
	UObject* ReimportedObject = nullptr;
	for ( UObject* Asset : ImportContext.AssetCache->GetActiveAssets() )
	{
		UUsdAssetImportData* NewAssetImportData = UsdUtils::GetAssetImportData( Asset );

		if ( Asset &&
			 NewAssetImportData &&
			 Asset->GetClass() == OriginalAsset->GetClass() &&
			 NewAssetImportData->PrimPath.Equals( OriginalImportData->PrimPath, ESearchCase::CaseSensitive ) )
		{
			ReimportedObject = Asset;
			break;
		}
	}

	if ( ReimportedObject )
	{
		UsdStageImporterImpl::UpdateAssetImportData( ReimportedObject, ImportContext.FilePath, ImportContext.ImportOptions);

		// Assign things from the original assets before we publish the reimported asset, overwriting it
		UsdStageImporterImpl::CopyOriginalMaterialAssignment(ImportContext, OriginalAsset, ReimportedObject );
		UsdStageImporterImpl::CopySkeletonAssignment(ImportContext, OriginalAsset, ReimportedObject );

		// Just publish the one asset we wanted to reimport. Note that we may have other assets here too, but we'll ignore those e.g. a displayColor material or a skeleton
		OutReimportedAsset = UsdStageImporterImpl::PublishAsset(ImportContext, ReimportedObject, OriginalAsset->GetOutermost()->GetPathName(), ObjectsToRemap, SoftObjectsToRemap);

		TSet<UObject*> UsedAssetsAndDependencies = ImportContext.AssetCache->GetActiveAssets();
		TSet<UObject*> PublishedAssetsAndDependencies;
		UsdStageImporterImpl::RemapReferences(ImportContext, UsedAssetsAndDependencies, ObjectsToRemap);
		UsdStageImporterImpl::RemapSoftReferences(ImportContext, UsedAssetsAndDependencies, SoftObjectsToRemap);
		UsdStageImporterImpl::GetPublishedAssetsAndDependencies(UsedAssetsAndDependencies, ObjectsToRemap, SoftObjectsToRemap, PublishedAssetsAndDependencies);
		UsdStageImporterImpl::CallAssetsPostEditChange(PublishedAssetsAndDependencies);

		bSuccess = OutReimportedAsset != nullptr && ImportContext.AssetCache->GetActiveAssets().Contains( ReimportedObject );
	}

	UsdStageImporterImpl::Cleanup( ImportContext.SceneActor, nullptr, ImportContext.ImportOptions->ExistingActorPolicy );
	UsdStageImporterImpl::NotifyAssetRegistry( { ReimportedObject } );
	UsdStageImporterImpl::RefreshComponents( ImportContext.SceneActor );

	FUsdDelegates::OnPostUsdImport.Broadcast(ImportContext.FilePath);

	// Analytics
	{
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		UsdStageImporterImpl::SendAnalytics( ImportContext, ReimportedObject, TEXT( "Reimport" ), { ReimportedObject }, ElapsedSeconds );
	}

	UsdStageImporterImpl::CloseStageIfNeeded( ImportContext );

	if ( ImportContext.bNeedsGarbageCollection )
	{
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
	}

#endif // #if USE_USD_SDK
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE

