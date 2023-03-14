// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools.h"
#include "Factories/Factory.h"
#include "Factories/BlueprintFactory.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Exporters/Exporter.h"
#include "Editor/EditorEngine.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Editor.h"
#include "EditorDirectories.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "AssetToolsLog.h"
#include "AssetToolsModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "IClassTypeActions.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "AssetTypeActions/AssetTypeActions_Actor.h"
#include "AssetTypeActions/AssetTypeActions_ActorFolder.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "AssetTypeActions/AssetTypeActions_BlueprintGeneratedClass.h"
#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInterface.h"
#include "AssetTypeActions/AssetTypeActions_SkeletalMesh.h"
#include "AssetTypeActions/AssetTypeActions_FbxSceneImportData.h"
#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget.h"
#include "AssetTypeActions/AssetTypeActions_VectorField.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "AssetTypeActions/AssetTypeActions_AnimBlueprint.h"
#include "AssetTypeActions/AssetTypeActions_AnimBlueprintInterface.h"
#include "AssetTypeActions/AssetTypeActions_AnimBlueprintGeneratedClass.h"
#include "AssetTypeActions/AssetTypeActions_AnimBoneCompressionSettings.h"
#include "AssetTypeActions/AssetTypeActions_AnimComposite.h"
#include "AssetTypeActions/AssetTypeActions_AnimStreamable.h"
#include "AssetTypeActions/AssetTypeActions_AnimCurveCompressionSettings.h"
#include "AssetTypeActions/AssetTypeActions_AnimMontage.h"
#include "AssetTypeActions/AssetTypeActions_AnimSequence.h"
#include "AssetTypeActions/AssetTypeActions_BlendSpace.h"
#include "AssetTypeActions/AssetTypeActions_AimOffset.h"
#include "AssetTypeActions/AssetTypeActions_BlendSpace1D.h"
#include "AssetTypeActions/AssetTypeActions_AimOffset1D.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget2D.h"
#include "AssetTypeActions/AssetTypeActions_CanvasRenderTarget2D.h"
#include "AssetTypeActions/AssetTypeActions_CurveFloat.h"
#include "AssetTypeActions/AssetTypeActions_CurveTable.h"
#include "AssetTypeActions/AssetTypeActions_CompositeCurveTable.h"
#include "AssetTypeActions/AssetTypeActions_CurveVector.h"
#include "AssetTypeActions/AssetTypeActions_CurveLinearColor.h"
#include "AssetTypeActions/AssetTypeActions_CurveLinearColorAtlas.h"
#include "AssetTypeActions/AssetTypeActions_DataAsset.h"
#include "AssetTypeActions/AssetTypeActions_DataLayer.h"
#include "AssetTypeActions/AssetTypeActions_DataTable.h"
#include "AssetTypeActions/AssetTypeActions_CompositeDataTable.h"
#include "AssetTypeActions/AssetTypeActions_Enum.h"
#include "AssetTypeActions/AssetTypeActions_Class.h"
#include "AssetTypeActions/AssetTypeActions_Struct.h"
#include "AssetTypeActions/AssetTypeActions_Font.h"
#include "AssetTypeActions/AssetTypeActions_FontFace.h"
#include "AssetTypeActions/AssetTypeActions_ForceFeedbackAttenuation.h"
#include "AssetTypeActions/AssetTypeActions_ForceFeedbackEffect.h"
#include "AssetTypeActions/AssetTypeActions_HapticFeedback.h"
#include "AssetTypeActions/AssetTypeActions_HLODProxy.h"
#include "AssetTypeActions/AssetTypeActions_SubsurfaceProfile.h"
#include "AssetTypeActions/AssetTypeActions_ActorFoliageSettings.h"
#include "AssetTypeActions/AssetTypeActions_InstancedFoliageSettings.h"
#include "AssetTypeActions/AssetTypeActions_LandscapeLayer.h"
#include "AssetTypeActions/AssetTypeActions_LandscapeGrassType.h"
#include "AssetTypeActions/AssetTypeActions_LightWeightInstance.h"
#include "AssetTypeActions/AssetTypeActions_Material.h"
#include "AssetTypeActions/AssetTypeActions_MaterialFunction.h"
#include "AssetTypeActions/AssetTypeActions_MaterialFunctionInstance.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInstanceConstant.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInstanceDynamic.h"
#include "AssetTypeActions/AssetTypeActions_MaterialParameterCollection.h"
#include "AssetTypeActions/AssetTypeActions_MirrorDataTable.h"
#include "AssetTypeActions/AssetTypeActions_ObjectLibrary.h"
#include "AssetTypeActions/AssetTypeActions_ParticleSystem.h"
#include "AssetTypeActions/AssetTypeActions_PhysicalMaterial.h"
#include "AssetTypeActions/AssetTypeActions_PhysicalMaterialMask.h"
#include "AssetTypeActions/AssetTypeActions_PhysicsAsset.h"
#include "AssetTypeActions/AssetTypeActions_PoseAsset.h"
#include "AssetTypeActions/AssetTypeActions_PreviewMeshCollection.h"
#include "AssetTypeActions/AssetTypeActions_ProceduralFoliageSpawner.h"
#include "AssetTypeActions/AssetTypeActions_Redirector.h"
#include "AssetTypeActions/AssetTypeActions_Rig.h"
#include "AssetTypeActions/AssetTypeActions_Skeleton.h"
#include "AssetTypeActions/AssetTypeActions_SlateBrush.h"
#include "AssetTypeActions/AssetTypeActions_SlateWidgetStyle.h"
#include "AssetTypeActions/AssetTypeActions_StaticMesh.h"
#include "AssetTypeActions/AssetTypeActions_SubUVAnimation.h"
#include "AssetTypeActions/AssetTypeActions_Texture2D.h"
#include "AssetTypeActions/AssetTypeActions_Texture2DArray.h"
#include "AssetTypeActions/AssetTypeActions_TextureCube.h"
#include "AssetTypeActions/AssetTypeActions_TextureCubeArray.h"
#include "AssetTypeActions/AssetTypeActions_VolumeTexture.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTarget2DArray.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTargetCube.h"
#include "AssetTypeActions/AssetTypeActions_TextureRenderTargetVolume.h"
#include "AssetTypeActions/AssetTypeActions_TextureLightProfile.h"
#include "AssetTypeActions/AssetTypeActions_TouchInterface.h"
#include "AssetTypeActions/AssetTypeActions_VectorFieldAnimated.h"
#include "AssetTypeActions/AssetTypeActions_VectorFieldStatic.h"
#include "AssetTypeActions/AssetTypeActions_World.h"
#include "WorldPartition/WorldPartition.h"
#include "SDiscoveringAssetsDialog.h"
#include "AssetFixUpRedirectors.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SPackageReportDialog.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Logging/MessageLog.h"
#include "UnrealExporter.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AutomatedAssetImportData.h"
#include "AssetImportTask.h"
#include "Dialogs/DlgPickPath.h"
#include "Misc/FeedbackContext.h"
#include "BusyCursor.h"
#include "AssetExportTask.h"
#include "Containers/UnrealString.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "AdvancedCopyCustomization.h"
#include "SAdvancedCopyReportDialog.h"
#include "AssetToolsSettings.h"
#include "AssetVtConversion.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/NamePermissionList.h"
#include "InterchangeManager.h"
#include "InterchangeProjectSettings.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "UObject/SavePackage.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/ContentBrowserSettings.h"
#include "Algo/Count.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "UObject/NameTypes.h"
#include "ExternalPackageHelper.h"
#include "UObject/LinkerInstancingContext.h"
#include "PackageMigrationContext.h"

#if WITH_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "Settings/EditorExperimentalSettings.h"
#include "ObjectTools.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetTools)

#define LOCTEXT_NAMESPACE "AssetTools"

namespace UE::AssetTools::Private
{
	bool bUseNewPackageMigration = true;
	static FAutoConsoleVariableRef CVarUseNewPackageMigration(
		TEXT("AssetTools.UseNewPackageMigration"),
		bUseNewPackageMigration,
		TEXT("When set, The package migration will use the new implementation made for 5.1.")
	);

	// use a struct as a namespace to allow easier friend declarations
	struct FPackageMigrationImpl
	{
	public:
		struct FPackageMigrationImplContext
		{
			FPackageMigrationImplContext()
				: SlowTask(100, LOCTEXT("MigratePackages", "Migrating Packages"))
			{
				InstancingContext.SetSoftObjectPathRemappingEnabled(true);
				InstancingContext.AddTag(ULevel::DontLoadExternalObjectsTag);

				const bool bCanCancel = true;
				SlowTask.MakeDialog(bCanCancel);
			}

			bool bWasCanceled = false;
			FLinkerInstancingContext InstancingContext;
			FScopedSlowTask SlowTask;
			FMigrationOptions Options;
		};


		static FString GetMountPointRootPath(bool bIsDestinationAProject, const FString& DestinationFolder)
		{
			FString MountPointRootPath;
			if (bIsDestinationAProject)
			{
				MountPointRootPath = TEXT("/Game/");
			}
			else
			{
				TArray<FString> PluginFiles;
				IFileManager::Get().FindFiles(PluginFiles, *(DestinationFolder + TEXT("../")), TEXT("uplugin"));

				if (PluginFiles.Num() == 1)
				{
					FStringView PluginNameView = FPathViews::GetBaseFilename(PluginFiles[0]);
					MountPointRootPath.Reserve(PluginNameView.Len() + 2);
					MountPointRootPath.AppendChar(TEXT('/'));
					MountPointRootPath.Append(PluginNameView);
					MountPointRootPath.AppendChar(TEXT('/'));
				}
				else
				{
					UE_LOG(LogAssetTools
						, Error
						, TEXT("{%s} does not appear to be a game Content folder. Migrated content only work if placed in a Content folder. Aborting the Migration.")
						, *DestinationFolder
					);
				}
			}

			return MountPointRootPath;
		}

		static UObject* FindAssetInPackage(UPackage* Package)
		{
			// Inspired from the save code (UEditorEngine::Save)
			UObject* Asset = nullptr;
			if (Package->HasAnyPackageFlags(PKG_ContainsMap))
			{
				Asset = UWorld::FindWorldInPackage(Package);
			}
			
			if (!Asset)
			{
				// Otherwise find the main asset of the package
				Asset = Package->FindAssetInPackage();
			}

			return Asset;
		};

		static bool CanSavePackageToFile(const FString& DestinationFile, const FStringView PackageName, FPackageMigrationContext* OptionalPackageMigrationContext)
		{
			// Don't try to migrate the package if the destination is read only
			if (IFileManager::Get().IsReadOnly(*DestinationFile))
			{
				if (OptionalPackageMigrationContext)
				{
					OptionalPackageMigrationContext->AddErrorMigrationMessage(FText::Format(LOCTEXT("MigratePackages_SaveFailedReadOnly", "Couldn't migrate package ({0}) because the destination file is read only. Destination File ({1})")
						, FText::FromStringView(PackageName)
						, FText::FromString(DestinationFile)
						));
				}

				return false;
			}

			return true;
		}

		// Prepare the MigrationPackagesData in the migration context and log any info about the package that will be renamed
		static void SetupPublicAssetPackagesMigrationData(const TSharedPtr<TArray<ReportPackageData>>& PackageDataToMigrate
			, TArray<TPair<const ReportPackageData*, const FAssetData>>& ExternalPackageDatas
			, TMap<const FStringView, int32>& ExistingPackageNameToMigrationDataIndex
			, FPackageMigrationContext& PackageMigrationContext
			, FPackageMigrationImplContext& PackageMigrationImplContext
			, FMessageLog& MigrateLog)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FAssetData> AssetsData;

			// Used to avoid a name collision between the instanced packages
			TSet<FString> UsedPackagesName;
			UsedPackagesName.Reserve(PackageDataToMigrate->Num());
			IFileManager& FileManager = IFileManager::Get();


			ExternalPackageDatas.Reserve(PackageDataToMigrate->Num());
			ExistingPackageNameToMigrationDataIndex.Reserve(PackageDataToMigrate->Num());

			TArray<FText> RenamedPackagesInfo;


			// Handle prompt response and automation
			EAppReturnType::Type LastResponse = EAppReturnType::Yes;
			if (!PackageMigrationImplContext.Options.bPrompt)
			{
				switch (PackageMigrationImplContext.Options.AssetConflict)
				{
				case EAssetMigrationConflict::Overwrite:
					LastResponse = EAppReturnType::YesAll;
					break;
				case EAssetMigrationConflict::Skip:
					LastResponse = EAppReturnType::NoAll;
					break;
				case EAssetMigrationConflict::Cancel:
					PackageMigrationImplContext.bWasCanceled = true;
					break;
				}
			}

			// Process the normal package here (no external object or actors).
			for (const ReportPackageData& PackageData : *(PackageDataToMigrate.Get()))
			{
				if (PackageMigrationImplContext.SlowTask.ShouldCancel() || PackageMigrationImplContext.bWasCanceled)
				{
					PackageMigrationImplContext.bWasCanceled = true;
					break;
				}

				if (!PackageData.bShouldMigratePackage)
				{
					continue;
				}

				AssetsData.Reset();
				const bool bIncludeOndiskAssetOnly = true;
				// We want all the asset including those not supported for the current platform.
				const bool bSkipARFilteredAssets = false;
				AssetRegistry.GetAssetsByPackageName(*PackageData.Name, AssetsData, bIncludeOndiskAssetOnly, bSkipARFilteredAssets);

				// Detect if the package contains some external object or actors
				{
					bool bIsExternalPackage = false;
					for (FAssetData& AssetData : AssetsData)
					{
						if (!AssetData.GetOptionalOuterPathName().IsNone())
						{
							bIsExternalPackage = true;
							ExternalPackageDatas.Emplace(&PackageData, MoveTemp(AssetData));
							break;
						}
					}

					if (bIsExternalPackage)
					{
						// Skip this asset since it is a external package. They are done in another pass
						continue;
					}
				}


				const FPackageMigrationContext::FScopedMountPoint& MountPoint = PackageMigrationContext.GetDestinationMountPoint();

				FString NewPackageName = MountPoint.GetNewPackageNameForMigration(PackageData.Name);

				// Is there a simpler way to get the package extension?
				FString ExistingFilename;
				FPackageName::DoesPackageExist(PackageData.Name, &ExistingFilename);

				const bool bIncludeDot = true;
				FString NewPackageFilename = MountPoint.GetMigratedPackageFilename(NewPackageName, FPathViews::GetExtension(ExistingFilename, bIncludeDot));
				uint32 NewPackageNameHash = GetTypeHash(NewPackageName);

				// Check we can use the new name for the asset
				const bool bIsNameChangeCausedByOtherMigrationPackage = UsedPackagesName.ContainsByHash(NewPackageNameHash, NewPackageName);
				const bool bIsNameChangeCausedByExistingAssetInDestination = FileManager.FileExists(*NewPackageFilename);

				bool bShouldMigrate = true;

				// Ask the user what to do if an asset already exist in the destination
				if (bIsNameChangeCausedByExistingAssetInDestination)
				{
					if (!CanSavePackageToFile(NewPackageFilename, PackageData.Name, &PackageMigrationContext))
					{
						bShouldMigrate = false;
					}
					else
					{ 
						// Handle name collision in the destination project. Post 5.1 should expose the possibility to rename the migrated package
						EAppReturnType::Type Response;
						if (FApp::IsUnattended() || !PackageMigrationImplContext.Options.bPrompt || LastResponse == EAppReturnType::YesAll || LastResponse == EAppReturnType::NoAll)
						{
							Response = LastResponse;
						}
						else
						{
							const FText Message = FText::Format(LOCTEXT("MigratePackages_AlreadyExists", "An asset already exists at location {0} would you like to overwrite it?"), FText::FromString(NewPackageFilename));
							Response = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAllCancel, Message);
							LastResponse = Response;
						}

						if (Response == EAppReturnType::Cancel)
						{
							// The user chose to cancel mid-operation. Break out.
							PackageMigrationImplContext.bWasCanceled = true;
							break;
						}

						if (Response == EAppReturnType::No || Response == EAppReturnType::NoAll)
						{
							bShouldMigrate = false;
						}
					}
				}

				// Avoid a name collision
				if (bIsNameChangeCausedByOtherMigrationPackage)
				{
					// Name collision with another package to migrate (simply add _number to package name)
					int32 Index = 1;
					FString ModifiedNewPackageName;
					// 4 for "_num"
					ModifiedNewPackageName.Reserve(NewPackageName.Len() + 3);
					uint32 ModifiedNewPackageNameHash;

					FString ModifiedNewPackageFilename;
					ModifiedNewPackageFilename.Reserve(NewPackageFilename.Len() + 3);
					FStringView FileExtension = FPathViews::GetExtension(NewPackageFilename, bIncludeDot);
					FStringView NewPackageFilenameWithoutExtension(*NewPackageFilename, NewPackageFilename.Len() - FileExtension.Len());

					do
					{
						++Index;
						ModifiedNewPackageName.Reset();
						ModifiedNewPackageName.Append(NewPackageName);
						ModifiedNewPackageName.AppendChar('_');
						ModifiedNewPackageName.AppendInt(Index);
						ModifiedNewPackageNameHash = GetTypeHash(ModifiedNewPackageName);

						ModifiedNewPackageFilename.Reset();
						ModifiedNewPackageFilename.Append(NewPackageFilenameWithoutExtension);
						ModifiedNewPackageFilename.AppendChar('_');
						ModifiedNewPackageFilename.AppendInt(Index);
						ModifiedNewPackageFilename.Append(FileExtension);
					} while (UsedPackagesName.ContainsByHash(ModifiedNewPackageNameHash, ModifiedNewPackageName)
						|| FileManager.FileExists(*ModifiedNewPackageFilename)
						);

					if (bIsNameChangeCausedByOtherMigrationPackage)
					{
						RenamedPackagesInfo.Add(
							FText::Format(LOCTEXT("MigratePackages_PackageNameChangeByOtherPackage", "({0}) will be named ({1}) because another migrated package is already using that name.")
								, FText::FromString(PackageData.Name)
								, FText::FromString(ModifiedNewPackageName)
							)
						);
					}
					else if (bIsNameChangeCausedByExistingAssetInDestination)
					{
						RenamedPackagesInfo.Add(
							FText::Format(LOCTEXT("MigratePackages_PackageNameChangeByExistingPackageInDestination", "({0}) will be named ({1}) because an file in the destination project is already using that name.")
								, FText::FromString(PackageData.Name)
								, FText::FromString(ModifiedNewPackageName)
							)
						);
					}

					NewPackageNameHash = ModifiedNewPackageNameHash;
					NewPackageName = MoveTemp(ModifiedNewPackageName);
					NewPackageFilename = MoveTemp(ModifiedNewPackageFilename);
				}

				UsedPackagesName.AddByHash(NewPackageNameHash, NewPackageName);

				int32 MigrationPackageDataIndex = PackageMigrationContext.MigrationPackagesData.Emplace(NewPackageName, PackageData.Name, NewPackageFilename);
				FPackageMigrationContext::FMigrationPackageData& MigrationData = PackageMigrationContext.MigrationPackagesData[MigrationPackageDataIndex];

				// Post 5.1 look into detecting which file can be migrated without being loaded
				MigrationData.bNeedInstancedLoad = bShouldMigrate;
				MigrationData.bNeedToBeSaveMigrated = bShouldMigrate;

				ExistingPackageNameToMigrationDataIndex.Add(PackageData.Name, MigrationPackageDataIndex);

				// Check if there is a existing package that need to be moved out the way
				if (UPackage* InTheWayPacakge = FindObjectFast<UPackage>(nullptr, *NewPackageName))
				{
					PackageMigrationContext.MoveInTheWayPackage(InTheWayPacakge);
				}

				PackageMigrationImplContext.InstancingContext.AddPackageMapping(*PackageData.Name, *NewPackageName);
			}

			if (!RenamedPackagesInfo.IsEmpty())
			{
				MigrateLog.NewPage(LOCTEXT("MigratePackages_PackageNameChange", "Modified Packages Name For Migration"));
				for (const FText& Message : RenamedPackagesInfo)
				{
					MigrateLog.Info(Message);
				}
			}
		}

		static void SetupExternalAssetPackagesMigrationData(const TArray<TPair<const ReportPackageData*, const FAssetData>>& ExternalPackageDatas, TMap<const FStringView, int32>& ExistingPackageNameToMigrationDataIndex, FPackageMigrationContext& PackageMigrationContext, FPackageMigrationImplContext& MigrationImplContext)
		{
			auto GenerateObjectPath = [](const FStringView PackageName, const FStringView AssetName, const FStringView SubPath) -> FString
			{
				FString InstancedPath;

				int32 DelimitersCount = 2;
				if (SubPath.IsEmpty())
				{
					--DelimitersCount;
				}

				InstancedPath.Reserve(PackageName.Len() + AssetName.Len() + SubPath.Len() + DelimitersCount);
				InstancedPath.Append(PackageName);
				InstancedPath.AppendChar(TEXT('.'));
				InstancedPath.Append(AssetName);

				if (!SubPath.IsEmpty())
				{
					InstancedPath.AppendChar(SUBOBJECT_DELIMITER_CHAR);
					InstancedPath.Append(SubPath);
				}

				return InstancedPath;
			};

			// Handle the external packages
			for (const TPair<const ReportPackageData*, const FAssetData>& PackageData : ExternalPackageDatas)
			{
				const FString OuterPath = PackageData.Value.GetOptionalOuterPathName().ToString();

				const FStringView OuterPackageName = FPathViews::GetBaseFilenameWithPath(OuterPath);

				const int32* PtrToMigrationDataIndex = ExistingPackageNameToMigrationDataIndex.Find(OuterPackageName);

				if (!PtrToMigrationDataIndex)
				{
					PackageMigrationContext.AddErrorMigrationMessage(FText::Format(
						LOCTEXT("MigratePackages_ExternalPackageNotExported", "({0}) won't be migrated because the outer of its content ({1}) will not be migrated")
						, FText::FromString(PackageData.Key->Name)
						, FText::FromStringView(OuterPackageName)
					)
					);
					continue;
				}

				FPackageMigrationContext::FMigrationPackageData& OuterMigrationData = PackageMigrationContext.MigrationPackagesData[*PtrToMigrationDataIndex];

				const FSoftObjectPath ExistingObjectPath = PackageData.Value.GetSoftObjectPath();
				FString NewObjectPath = GenerateObjectPath(OuterMigrationData.GetInstancedPackageName(), FPathViews::GetBaseFilename(OuterMigrationData.GetInstancedPackageName()), ExistingObjectPath.GetSubPathString());

				FString NewPackageName;

				if (FWorldPartitionActorDescUtils::IsValidActorDescriptorFromAssetData(PackageData.Value))
				{
					// If the external package is an actor.
					EActorPackagingScheme Scheme = ULevel::GetActorPackagingSchemeFromActorPackageName(PackageData.Key->Name);
					NewPackageName = ULevel::GetActorPackageName(ULevel::GetExternalActorsPath(OuterMigrationData.GetInstancedPackageName()), Scheme, NewObjectPath);
				}
				else
				{
					NewPackageName = FExternalPackageHelper::GetExternalPackageName(OuterMigrationData.GetInstancedPackageName(), NewObjectPath);
				}


				FString NewPackageFilename = PackageMigrationContext.GetDestinationMountPoint().GetMigratedPackageFilename(NewPackageName, FPackageName::GetAssetPackageExtension());

				// Add the external package to the list of migrated asset, even if the asset is not really migrated we still need it for the instance path
				uint32 MigrationPackageDataIndex = PackageMigrationContext.MigrationPackagesData.Emplace(NewPackageName, PackageData.Key->Name, NewPackageFilename);

				FPackageMigrationContext::FMigrationPackageData& MigrationData = PackageMigrationContext.MigrationPackagesData[MigrationPackageDataIndex];

				// Post 5.1 look into detecting which file can be migrated without being loaded
				MigrationData.bNeedInstancedLoad = OuterMigrationData.bNeedInstancedLoad && PackageData.Key->bShouldMigratePackage;
				MigrationData.bNeedToBeSaveMigrated = OuterMigrationData.bNeedToBeSaveMigrated && PackageData.Key->bShouldMigratePackage;

				// Check if there is a existing package that need to be moved out the way
				if (UPackage* InTheWayPacakge = FindObjectFast<UPackage>(nullptr, *NewPackageName))
				{
					PackageMigrationContext.MoveInTheWayPackage(InTheWayPacakge);
				}

				MigrationImplContext.InstancingContext.AddPackageMapping(*(PackageData.Key->Name), *NewPackageName);
			}
		}

		static void CreatePackagesAndSetupLinkers(FPackageMigrationContext& PackageMigrationContext, FPackageMigrationImplContext& MigrationImplContext, TArray<TWeakObjectPtr<UPackage>>& PackagesToClean)
		{
			FArchive* ReaderOverride = nullptr;

			// Create the package and their load linker. If an asset try to load a weak dependency the package will load from the right file because of the existing linker.
			for (FPackageMigrationContext::FMigrationPackageData& MigrationPackageData : PackageMigrationContext.MigrationPackagesData)
			{
				if (MigrationImplContext.SlowTask.ShouldCancel() || MigrationImplContext.bWasCanceled)
				{
					MigrationImplContext.bWasCanceled = true;
					break;
				}

				UPackage* MigrationPackage = CreatePackage(*MigrationPackageData.GetInstancedPackageName());

				/**
				 * Load_Verify tell the linker to not load the package but it will create the linker if the file exist and is valid.
				 * LOAD_NoVerify tell the linker to not check the import of the package (in the editor this will avoid loading the hard dependencies of the package)
				 * 
				 * This will the package with a pre-created linker that has the right instancing context. Ready to loaded by another call to load package that may not have all that info.
				 */
				MigrationPackageData.InstancedPackage = LoadPackage(MigrationPackage, *MigrationPackageData.GetOriginalPackageName(), LOAD_Verify | LOAD_NoVerify, ReaderOverride, &MigrationImplContext.InstancingContext);

				PackagesToClean.Add(MigrationPackage);
			}
		}

		static void LoadInstancedPackages(FPackageMigrationContext& PackageMigrationContext, FPackageMigrationImplContext& MigrationImplContext)
		{
			const int32 NumOfPackageToLoad = Algo::CountIf(PackageMigrationContext.MigrationPackagesData, [](const FPackageMigrationContext::FMigrationPackageData& MigrationPackageData)
				{
					return MigrationPackageData.bNeedInstancedLoad || MigrationPackageData.bNeedToBeSaveMigrated;
				});

			// We give 40% of the progress weight to the loading
			const float ProgressPerItemToload = 40.f / NumOfPackageToLoad;

			uint32 ProgressCount = 0;

			FArchive* ReaderOverride = nullptr;

			for (FPackageMigrationContext::FMigrationPackageData& MigrationPackageData : PackageMigrationContext.MigrationPackagesData)
			{
				if (MigrationImplContext.SlowTask.ShouldCancel() || MigrationImplContext.bWasCanceled)
				{
					MigrationImplContext.bWasCanceled = true;
					break;
				}

				if (MigrationPackageData.bNeedInstancedLoad || MigrationPackageData.bNeedToBeSaveMigrated)
				{
					MigrationImplContext.SlowTask.EnterProgressFrame(ProgressPerItemToload
						, FText::Format(LOCTEXT("MigratePackages_Load", "Migration: Loading packages {0}/{1}")
							, ProgressCount
							, NumOfPackageToLoad
						)
					);

					++ProgressCount;

					UPackage* LoadedPackage = LoadPackage(MigrationPackageData.GetInstancedPackage(), *MigrationPackageData.GetOriginalPackageName(), LOAD_None, ReaderOverride, &MigrationImplContext.InstancingContext);

					if (IsValid(LoadedPackage))
					{
						UObject* Asset = FPackageMigrationImpl::FindAssetInPackage(LoadedPackage);

						if (Asset && !Asset->HasAnyFlags(RF_Standalone))
						{
							// The external actor or object are sometimes not protected from the garbage collector. This is removed by the purge at the end of the migration
							Asset->AddToRoot();
						}
					}
					else
					{
						MigrationPackageData.bNeedToBeSaveMigrated = false;
						PackageMigrationContext.AddErrorMigrationMessage(FText::Format(LOCTEXT("MigratePackages_LoadFailed", "Couldn't migrate package ({0}) because the load of the asset failed"), FText::FromString(MigrationPackageData.GetOriginalPackageName())));
					}
				}
			}
		}

		static void SaveInstancedPackagesIntoDestination(FPackageMigrationContext& PackageMigrationContext, FPackageMigrationImplContext& MigrationImplContext)
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags |= SAVE_RehydratePayloads;

			// We should look into creating our own log to report the save erros to the user.
			SaveArgs.Error = GWarn;


			uint32 ProgressCount = 0;

			const int32 NumOfPackageToSave = Algo::CountIf(PackageMigrationContext.MigrationPackagesData, [](const FPackageMigrationContext::FMigrationPackageData& MigrationPackageData)
				{
					return MigrationPackageData.bNeedToBeSaveMigrated;
				});

			// We give 60% of the progress weight to the saving of the assets
			const float ProgressPerItemToSave = 60.f / NumOfPackageToSave;

			for (const FPackageMigrationContext::FMigrationPackageData& MigrationPackageData : PackageMigrationContext.MigrationPackagesData)
			{
				if (MigrationImplContext.SlowTask.ShouldCancel() || MigrationImplContext.bWasCanceled)
				{
					MigrationImplContext.bWasCanceled = true;
					break;
				}
				if (MigrationPackageData.bNeedToBeSaveMigrated)
				{

					MigrationImplContext.SlowTask.EnterProgressFrame(ProgressPerItemToSave
						, FText::Format(LOCTEXT("MigratePackages_Save", "Saving the packages into the destination {0}/{1}")
							, ProgressCount
							, NumOfPackageToSave
						)
					);

					++ProgressCount;
					
					UObject* Asset = FPackageMigrationImpl::FindAssetInPackage(MigrationPackageData.GetInstancedPackage());
					if (IsValid(MigrationPackageData.GetInstancedPackage()) && Asset)
					{
						FSavePackageResultStruct SaveResult = GEditor->Save(MigrationPackageData.GetInstancedPackage(), Asset, *MigrationPackageData.GetDestinationFilename(), SaveArgs);

						if (SaveResult.IsSuccessful())
						{
							PackageMigrationContext.AddSucessfullMigrationMessage(FText::Format(LOCTEXT("MigratePackages_SaveSuccess", "Package ({0}) was migrated successfully migrated as ({1}) with the fellowing filename ({2})")\
								, FText::FromString(MigrationPackageData.GetOriginalPackageName())
								, FText::FromString(MigrationPackageData.GetInstancedPackageName())
								, FText::FromString(MigrationPackageData.GetDestinationFilename())
							));
						}
						else
						{
							PackageMigrationContext.AddErrorMigrationMessage(FText::Format(LOCTEXT("MigratePackages_SaveFailed", "Couldn't migrate package ({0}) because the asset save failed. Destination File ({1})")
								, FText::FromString(MigrationPackageData.GetOriginalPackageName())
								, FText::FromString(MigrationPackageData.GetDestinationFilename())
							));
						}
					}
					else if (!IsValid(MigrationPackageData.GetInstancedPackage()))
					{
						PackageMigrationContext.AddErrorMigrationMessage(FText::Format(LOCTEXT("MigratePackages_SaveFailedPackageInvalid", "Couldn't migrate package ({0}) because the asset package is invalid. Destination File ({1})")
							, FText::FromString(MigrationPackageData.GetOriginalPackageName())
							, FText::FromString(MigrationPackageData.GetDestinationFilename())
						));
					}
					else
					{
						PackageMigrationContext.AddErrorMigrationMessage(FText::Format(LOCTEXT("MigratePackages_SaveFailedNoAsset", "Couldn't migrate package ({0}) because the package didn't contains a an asset. Destination File ({1})")
							, FText::FromString(MigrationPackageData.GetOriginalPackageName())
							, FText::FromString(MigrationPackageData.GetDestinationFilename())
						));
					}
				}
			}
		}

		static void CleanInstancedPackages(const TArray<TWeakObjectPtr<UPackage>>& PackagesToClean)
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			TSharedRef<TFunction<void(UObject*)>> PurgeObject = MakeShared<TFunction<void(UObject*)>>();
			PurgeObject.Get() = [PurgeObject](UObject* Object)
			{
				if (IsValid(Object))
				{
					if (UPackage* ExternalPackage = Object->GetExternalPackage())
					{
						if (Object != ExternalPackage && IsValid(ExternalPackage))
						{
							(*PurgeObject)(ExternalPackage);
							ForEachObjectWithOuter(ExternalPackage, *PurgeObject);
						}
					}

					if (Object->IsRooted())
					{
						Object->RemoveFromRoot();
					}

					Object->ClearFlags(RF_Public | RF_Standalone);
					Object->MarkAsGarbage();
				}
			};

			// Turn off the components while unloading stuff
			FGlobalComponentReregisterContext ComponentContext;

			TArray<UObject*> ReferenceToNull;

			// We do the clean pass of the packages in two loop because the PurgeObject can affect the ability to get the main object from another package.
			for (const TWeakObjectPtr<UPackage>& WeakPackage : PackagesToClean)
			{
				if (UPackage* Package = WeakPackage.Get())
				{
					if (UObject* Asset = FPackageMigrationImpl::FindAssetInPackage(Package))
					{
						AssetRegistry.AssetDeleted(Asset);
						ReferenceToNull.Add(Asset);
					}
				}
			}

			for (const TWeakObjectPtr<UPackage>& WeakPackage : PackagesToClean)
			{
				if (UPackage* Package = WeakPackage.Get())
				{
					const ERenameFlags PkgRenameFlags = REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses;
					check(Package->Rename(*MakeUniqueObjectName(nullptr, UPackage::StaticClass(), *FString::Printf(TEXT("%s_DEADFROMMIGRATION"), *Package->GetName())).ToString(), nullptr, PkgRenameFlags));
					(*PurgeObject)(Package);
					ForEachObjectWithOuter(Package, *PurgeObject);
				}
			}

			// Removing assets from memory is complicated
			TArray<UObject*> UDStructToReplace;
			for (int32 Index = 0; Index < ReferenceToNull.Num(); )
			{
				if (UUserDefinedStruct* UDStruct = Cast<UUserDefinedStruct>(ReferenceToNull[Index]))
				{
					ReferenceToNull.RemoveAtSwap(Index);
					UDStructToReplace.Add(UDStruct);
				}
				else
				{
					Index++;
				}
			}

			if (UDStructToReplace.Num())
			{
				ObjectTools::ForceReplaceReferences(GetFallbackStruct(), UDStructToReplace);
			}

			ObjectTools::ForceReplaceReferences(nullptr, ReferenceToNull);

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		static void RestoreInTheWayPackages(FPackageMigrationContext& PackageMigrationContext)
		{
			PackageMigrationContext.TemporalyMovedPackages.Empty();

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		static void BuildAndNotifyLogWindow(FPackageMigrationContext& PackageMigrationContext, FMessageLog& MigrateLog, FPackageMigrationImplContext& MigrationImplContext)
		{
			FText NotificationMessage;

			if (!PackageMigrationContext.MigratedPackageMessages.IsEmpty())
			{
				MigrateLog.NewPage(LOCTEXT("MigratePackages_SuccessPageLabel", "Migrated Pacakges"));
				for (const FText& InfoMessage : PackageMigrationContext.MigratedPackageMessages)
				{
					MigrateLog.Info(InfoMessage);
				}
			}

			if (!PackageMigrationContext.WarningMessage.IsEmpty())
			{
				MigrateLog.NewPage(LOCTEXT("MigratePackages_WarningPageLabel", "Migration warnings"));
				for (const FText& WarningMessage : PackageMigrationContext.WarningMessage)
				{
					MigrateLog.Warning(WarningMessage);
				}
			}

			if (PackageMigrationContext.ErrorMessages.IsEmpty())
			{
				if (PackageMigrationContext.WarningMessage.IsEmpty())
				{
					NotificationMessage = LOCTEXT("MigratePackages_SuccessNotification", "Content migration completed.");
				}
				else
				{
					NotificationMessage = LOCTEXT("MigratePackages_SuccessNotificationWithWarning", "Content migration completed with some warnings.");
				}
			}
			else
			{
				if (PackageMigrationContext.MigratedPackageMessages.IsEmpty())
				{
					NotificationMessage = LOCTEXT("MigratePackages_FailureNotification", "Content migration failed.");
				}
				else
				{
					NotificationMessage = LOCTEXT("MigratePackages_PartialFailureNotification", "Content migration failed but some asset have been migrated sucessfuly.");
				}

				MigrateLog.NewPage(LOCTEXT("MigratePackages_ErrorsPageLabel", "Migration Errors"));
				for (const FText& ErrorMessage : PackageMigrationContext.ErrorMessages)
				{
					MigrateLog.Error(ErrorMessage);
				}
			}

			if (MigrationImplContext.bWasCanceled)
			{
				if (PackageMigrationContext.MigratedPackageMessages.IsEmpty())
				{
					NotificationMessage = LOCTEXT("MigratePackages_Canceled", "Content migration was canceled.");
				}
				else
				{
					NotificationMessage = LOCTEXT("MigratePackages_CanceledWithMigratedAssets", "Content migration was canceled but some package where already migrated.");
				}
			}

			MigrateLog.Notify(NotificationMessage, EMessageSeverity::Info);
		}
	};
}


IAssetTools& IAssetTools::Get()
{
	FAssetToolsModule& Module = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	return Module.Get();
}

TScriptInterface<IAssetTools> UAssetToolsHelpers::GetAssetTools()
{
	return &UAssetToolsImpl::Get();
}

/** UInterface constructor */
UAssetTools::UAssetTools(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAssetToolsImpl::UAssetToolsImpl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AssetRenameManager(MakeShareable(new FAssetRenameManager))
	, AssetFixUpRedirectors(MakeShareable(new FAssetFixUpRedirectors))
	, NextUserCategoryBit(EAssetTypeCategories::FirstUser)
	, AssetClassPermissionList_DEPRECATED(MakeShared<FNamePermissionList>())
	, FolderPermissionList(MakeShared<FPathPermissionList>())
	, WritableFolderPermissionList(MakeShared<FPathPermissionList>())
	, CreateAssetsAsExternallyReferenceable(true)
{
	TArray<FString> SupportedTypesArray;
	GConfig->GetArray(TEXT("AssetTools"), TEXT("SupportedAssetTypes"), SupportedTypesArray, GEditorIni);
	for (int32 i = 0; i < (int32)EAssetClassAction::AllAssetActions; ++i)
	{
		AssetClassPermissionList.Add(MakeShared<FPathPermissionList>(EPathPermissionListType::ClassPaths));
		for (FString& Type : SupportedTypesArray)
		{
			if (FPackageName::IsShortPackageName(Type))
			{
				FTopLevelAssetPath TypePath = UClass::TryConvertShortTypeNameToPathName<UStruct>(Type, ELogVerbosity::Warning, TEXT("AssetToolsImpl"));
				if (TypePath.IsNull())
				{
					UE_LOG(LogAssetTools, Warning, TEXT("Failed to convert short type name \"%s\" to path name. Please update SupportedAssetTypes entries in [AssetTools] ini section"), *Type);
				}
				else
				{
					Type = TypePath.ToString();
				}
			}
			AssetClassPermissionList[i]->AddAllowListItem("AssetToolsConfigFile", Type);
		}
		AssetClassPermissionList[i]->OnFilterChanged().AddUObject(this, &UAssetToolsImpl::AssetClassPermissionListChanged, (EAssetClassAction)i);
	}

	TArray<FString> DenyListedViewPath;
	GConfig->GetArray(TEXT("AssetTools"), TEXT("DenyListAssetPaths"), DenyListedViewPath, GEditorIni);
	for (const FString& Path : DenyListedViewPath)
	{
		FolderPermissionList->AddDenyListItem("AssetToolsConfigFile", Path);
	}

	GConfig->GetArray(TEXT("AssetTools"), TEXT("DenyListContentSubPaths"), SubContentDenyListPaths, GEditorIni);
	TArray<FString> ContentRoots;
	FPackageName::QueryRootContentPaths(ContentRoots);
	for (const FString& ContentRoot : ContentRoots)
	{
		AddSubContentDenyList(ContentRoot);
	}
	FPackageName::OnContentPathMounted().AddUObject(this, &UAssetToolsImpl::OnContentPathMounted);

	// Register the built-in advanced categories
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_0"), FAdvancedAssetCategory(EAssetTypeCategories::Animation, LOCTEXT("AnimationAssetCategory", "Animation")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_1"), FAdvancedAssetCategory(EAssetTypeCategories::Blueprint, LOCTEXT("BlueprintAssetCategory", "Blueprints")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_2"), FAdvancedAssetCategory(EAssetTypeCategories::Materials, LOCTEXT("MaterialAssetCategory", "Materials")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_3"), FAdvancedAssetCategory(EAssetTypeCategories::Sounds, LOCTEXT("SoundAssetCategory", "Sounds")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_4"), FAdvancedAssetCategory(EAssetTypeCategories::Physics, LOCTEXT("PhysicsAssetCategory", "Physics")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_5"), FAdvancedAssetCategory(EAssetTypeCategories::UI, LOCTEXT("UserInterfaceAssetCategory", "User Interface")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_6"), FAdvancedAssetCategory(EAssetTypeCategories::Misc, LOCTEXT("MiscellaneousAssetCategory", "Miscellaneous")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_7"), FAdvancedAssetCategory(EAssetTypeCategories::Gameplay, LOCTEXT("GameplayAssetCategory", "Gameplay")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_8"), FAdvancedAssetCategory(EAssetTypeCategories::Media, LOCTEXT("MediaAssetCategory", "Media")));
	AllocatedCategoryBits.Add(TEXT("_BuiltIn_9"), FAdvancedAssetCategory(EAssetTypeCategories::Textures, LOCTEXT("TextureAssetCategory", "Textures")));

	EAssetTypeCategories::Type FoliageCategoryBit = RegisterAdvancedAssetCategory(FName(TEXT("Foliage")), LOCTEXT("FoliageAssetCategory", "Foliage"));
	EAssetTypeCategories::Type InputCategoryBit = RegisterAdvancedAssetCategory(FName(TEXT("Input")), LOCTEXT("InputAssetsCategory", "Input"));

	// Register the built-in asset type actions
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Actor));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ActorFolder));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimationAsset));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimBlueprintInterface));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimBlueprint));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimBlueprintGeneratedClass));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimBoneCompressionSettings));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimComposite));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimStreamable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimCurveCompressionSettings));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimMontage));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AnimSequence));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AimOffset));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_AimOffset1D));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_BlendSpace));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_PoseAsset));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_BlendSpace1D));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Blueprint));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_BlueprintGeneratedClass));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CanvasRenderTarget2D));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Curve));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CurveFloat));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CurveTable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CompositeCurveTable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CurveVector));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CurveLinearColor));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CurveLinearColorAtlas));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_DataAsset));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_DataLayer));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_DataTable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_CompositeDataTable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Enum));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Class));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Struct));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SceneImportData));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Font));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_FontFace));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ForceFeedbackAttenuation(InputCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ForceFeedbackEffect(InputCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_HLODProxy));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SubsurfaceProfile));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_InstancedFoliageSettings(FoliageCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ActorFoliageSettings(FoliageCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_LandscapeLayer));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_LandscapeGrassType(FoliageCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_LightWeightInstance));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Material(EAssetTypeCategories::Type::None)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunction));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunctionLayer));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunctionLayerInstance));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunctionLayerBlend));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunctionLayerBlendInstance));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialFunctionInstance));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialInstanceConstant(EAssetTypeCategories::Type::None)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialInstanceDynamic));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialInterface));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MaterialParameterCollection));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_MirrorDataTable));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ObjectLibrary));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ParticleSystem));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SubUVAnimation));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_PhysicalMaterial));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_PhysicalMaterialMask));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_PhysicsAsset));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_PreviewMeshCollection));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ProceduralFoliageSpawner(FoliageCategoryBit)));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Redirector));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Rig));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SkeletalMesh));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Skeleton));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SlateBrush));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SlateWidgetStyle));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_StaticMesh));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Texture));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Texture2D));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureCube));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_Texture2DArray));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureCubeArray));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_VolumeTexture));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureRenderTarget));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureRenderTarget2D));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureRenderTarget2DArray));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureRenderTargetCube));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureRenderTargetVolume));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TextureLightProfile));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TouchInterface));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_VectorField));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_VectorFieldAnimated));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_VectorFieldStatic));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_World));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_HapticFeedbackEffectBuffer));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_HapticFeedbackEffectCurve));
	RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_HapticFeedbackEffectSoundWave));

	// Note: Please don't add any more actions here!  They belong in an editor-only module that is more tightly
	// coupled to your new system, and you should not create a dependency on your new system from AssetTools.
}

void UAssetToolsImpl::RegisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& NewActions)
{
	bool bSupported = false;
	if (const UClass* SupportedClass = NewActions->GetSupportedClass())
	{
		bSupported = GetAssetClassPathPermissionList(EAssetClassAction::CreateAsset)->PassesFilter(SupportedClass->GetClassPathName().ToString());
	}
	else
	{
		bSupported = !NewActions->GetFilterName().IsNone();
	}

	NewActions->SetSupported(bSupported);

	AssetTypeActionsList.Add(NewActions);
}

void UAssetToolsImpl::UnregisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& ActionsToRemove)
{
	AssetTypeActionsList.Remove(ActionsToRemove);
}

void UAssetToolsImpl::GetAssetTypeActionsList( TArray<TWeakPtr<IAssetTypeActions>>& OutAssetTypeActionsList ) const
{
	for (auto ActionsIt = AssetTypeActionsList.CreateConstIterator(); ActionsIt; ++ActionsIt)
	{
		OutAssetTypeActionsList.Add(*ActionsIt);
	}
}

TWeakPtr<IAssetTypeActions> UAssetToolsImpl::GetAssetTypeActionsForClass(const UClass* Class) const
{
	if (!Class)
	{
		return TSharedPtr<IAssetTypeActions>();
	}

	TSharedPtr<IAssetTypeActions> MostDerivedAssetTypeActions;

	for (int32 TypeActionsIdx = 0; TypeActionsIdx < AssetTypeActionsList.Num(); ++TypeActionsIdx)
	{
		TSharedRef<IAssetTypeActions> TypeActions = AssetTypeActionsList[TypeActionsIdx];
		UClass* SupportedClass = TypeActions->GetSupportedClass();

		if ( Class->IsChildOf(SupportedClass) )
		{
			if ( !MostDerivedAssetTypeActions.IsValid() || SupportedClass->IsChildOf( MostDerivedAssetTypeActions->GetSupportedClass() ) )
			{
				MostDerivedAssetTypeActions = TypeActions;
			}
		}
	}

	return MostDerivedAssetTypeActions;
}

TArray<TWeakPtr<IAssetTypeActions>> UAssetToolsImpl::GetAssetTypeActionsListForClass(const UClass* Class) const
{
	TArray<TWeakPtr<IAssetTypeActions>> ResultAssetTypeActionsList;

	for (int32 TypeActionsIdx = 0; TypeActionsIdx < AssetTypeActionsList.Num(); ++TypeActionsIdx)
	{
		TSharedRef<IAssetTypeActions> TypeActions = AssetTypeActionsList[TypeActionsIdx];
		UClass* SupportedClass = TypeActions->GetSupportedClass();

		if (Class->IsChildOf(SupportedClass))
		{
			ResultAssetTypeActionsList.Add(TypeActions);
		}
	}

	return ResultAssetTypeActionsList;
}

EAssetTypeCategories::Type UAssetToolsImpl::RegisterAdvancedAssetCategory(FName CategoryKey, FText CategoryDisplayName)
{
	EAssetTypeCategories::Type Result = FindAdvancedAssetCategory(CategoryKey);
	if (Result == EAssetTypeCategories::Misc)
	{
		if (NextUserCategoryBit != 0)
		{
			// Register the category
			Result = (EAssetTypeCategories::Type)NextUserCategoryBit;
			AllocatedCategoryBits.Add(CategoryKey, FAdvancedAssetCategory(Result, CategoryDisplayName));

			// Advance to the next bit, or store that we're out
			if (NextUserCategoryBit == EAssetTypeCategories::LastUser)
			{
				NextUserCategoryBit = 0;
			}
			else
			{
				NextUserCategoryBit = NextUserCategoryBit << 1;
			}
		}
		else
		{
			UE_LOG(LogAssetTools, Warning, TEXT("RegisterAssetTypeCategory(\"%s\", \"%s\") failed as all user bits have been exhausted (placing into the Misc category instead)"), *CategoryKey.ToString(), *CategoryDisplayName.ToString());
		}
	}

	return Result;
}

EAssetTypeCategories::Type UAssetToolsImpl::FindAdvancedAssetCategory(FName CategoryKey) const
{
	if (const FAdvancedAssetCategory* ExistingCategory = AllocatedCategoryBits.Find(CategoryKey))
	{
		return ExistingCategory->CategoryType;
	}
	else
	{
		return EAssetTypeCategories::Misc;
	}
}

void UAssetToolsImpl::GetAllAdvancedAssetCategories(TArray<FAdvancedAssetCategory>& OutCategoryList) const
{
	AllocatedCategoryBits.GenerateValueArray(OutCategoryList);
}

void UAssetToolsImpl::RegisterClassTypeActions(const TSharedRef<IClassTypeActions>& NewActions)
{
	ClassTypeActionsList.Add(NewActions);
}

void UAssetToolsImpl::UnregisterClassTypeActions(const TSharedRef<IClassTypeActions>& ActionsToRemove)
{
	ClassTypeActionsList.Remove(ActionsToRemove);
}

void UAssetToolsImpl::GetClassTypeActionsList( TArray<TWeakPtr<IClassTypeActions>>& OutClassTypeActionsList ) const
{
	for (auto ActionsIt = ClassTypeActionsList.CreateConstIterator(); ActionsIt; ++ActionsIt)
	{
		OutClassTypeActionsList.Add(*ActionsIt);
	}
}

TWeakPtr<IClassTypeActions> UAssetToolsImpl::GetClassTypeActionsForClass( UClass* Class ) const
{
	TSharedPtr<IClassTypeActions> MostDerivedClassTypeActions;

	for (int32 TypeActionsIdx = 0; TypeActionsIdx < ClassTypeActionsList.Num(); ++TypeActionsIdx)
	{
		TSharedRef<IClassTypeActions> TypeActions = ClassTypeActionsList[TypeActionsIdx];
		UClass* SupportedClass = TypeActions->GetSupportedClass();

		if ( Class->IsChildOf(SupportedClass) )
		{
			if ( !MostDerivedClassTypeActions.IsValid() || SupportedClass->IsChildOf( MostDerivedClassTypeActions->GetSupportedClass() ) )
			{
				MostDerivedClassTypeActions = TypeActions;
			}
		}
	}

	return MostDerivedClassTypeActions;
}

UObject* UAssetToolsImpl::CreateAsset(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext)
{
	FGCObjectScopeGuard DontGCFactory(Factory);

	// Verify the factory class
	if ( !ensure(AssetClass || Factory) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("MustSupplyClassOrFactory", "The new asset wasn't created due to a problem finding the appropriate factory or class for the new asset.") );
		return nullptr;
	}

	if ( AssetClass && Factory && !ensure(AssetClass->IsChildOf(Factory->GetSupportedClass())) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("InvalidFactory", "The new asset wasn't created because the supplied factory does not support the supplied class.") );
		return nullptr;
	}

	const FString PackageName = UPackageTools::SanitizePackageName(PackagePath + TEXT("/") + AssetName);

	// Make sure we can create the asset without conflicts
	if ( !CanCreateAsset(AssetName, PackageName, LOCTEXT("CreateANewObject", "Create a new object")) )
	{
		return nullptr;
	}

	UClass* ClassToUse = AssetClass ? AssetClass : (Factory ? Factory->GetSupportedClass() : nullptr);

	UPackage* Pkg = CreatePackage(*PackageName);
	UObject* NewObj = nullptr;
	EObjectFlags Flags = RF_Public|RF_Standalone|RF_Transactional;
	if ( Factory )
	{
		NewObj = Factory->FactoryCreateNew(ClassToUse, Pkg, FName( *AssetName ), Flags, nullptr, GWarn, CallingContext);
	}
	else if ( AssetClass )
	{
		NewObj = NewObject<UObject>(Pkg, ClassToUse, FName(*AssetName), Flags);
	}

	if( NewObj )
	{

		Pkg->SetIsExternallyReferenceable(CreateAssetsAsExternallyReferenceable);

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(NewObj);

		// analytics create record
		UAssetToolsImpl::OnNewCreateRecord(AssetClass, false);

		// Mark the package dirty...
		Pkg->MarkPackageDirty();
	}

	return NewObj;
}

UObject* UAssetToolsImpl::CreateAssetWithDialog(UClass* AssetClass, UFactory* Factory, FName CallingContext)
{
	if (Factory != nullptr)
	{
		// Determine the starting path. Try to use the most recently used directory
		FString AssetPath;

		const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
		if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath))
		{
			// No saved path, just use the game content root
			AssetPath = TEXT("/Game");
		}

		FString PackageName;
		FString AssetName;
		CreateUniqueAssetName(AssetPath / Factory->GetDefaultNewAssetName(), TEXT(""), PackageName, AssetName);

		return CreateAssetWithDialog(AssetName, AssetPath, AssetClass, Factory, CallingContext);
	}

	return nullptr;
}


UObject* UAssetToolsImpl::CreateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext, const bool bCallConfigureProperties)
{
	FGCObjectScopeGuard DontGCFactory(Factory);
	if(Factory)
	{
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
		SaveAssetDialogConfig.DefaultPath = PackagePath;
		SaveAssetDialogConfig.DefaultAssetName = AssetName;
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			bool bCreateAsset = true;
			if (bCallConfigureProperties)
			{
				FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(Factory);
				bCreateAsset = Factory->ConfigureProperties();
			}

			if (bCreateAsset)
			{
				const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
				const FString SavePackagePath = FPaths::GetPath(SavePackageName);
				const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);

				return CreateAsset(SaveAssetName, SavePackagePath, AssetClass, Factory, CallingContext);
			}
		}
	}

	return nullptr;
}

UObject* UAssetToolsImpl::DuplicateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject)
{
	return DuplicateAssetWithDialogAndTitle(AssetName, PackagePath, OriginalObject, LOCTEXT("DuplicateAssetDialogTitle", "Duplicate Asset As"));
}

UObject* UAssetToolsImpl::DuplicateAssetWithDialogAndTitle(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject, FText DialogTitle)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = DialogTitle;
	SaveAssetDialogConfig.DefaultPath = PackagePath;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (!SaveObjectPath.IsEmpty())
	{
		const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		const FString SavePackagePath = FPaths::GetPath(SavePackageName);
		const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);

		return PerformDuplicateAsset(SaveAssetName, SavePackagePath, OriginalObject, true);
	}

	return nullptr;
}

UObject* UAssetToolsImpl::DuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject)
{
	return PerformDuplicateAsset(AssetName, PackagePath, OriginalObject, false);
}

UObject* UAssetToolsImpl::PerformDuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject, bool bWithDialog)
{
	// Verify the source object
	if ( !OriginalObject )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("InvalidSourceObject", "The new asset wasn't created due to a problem finding the object to duplicate.") );
		return nullptr;
	}

	const FString PackageName = PackagePath / AssetName;

	// Make sure we can create the asset without conflicts
	if ( !CanCreateAsset(AssetName, PackageName, LOCTEXT("DuplicateAnObject", "Duplicate an object")) )
	{
		return nullptr;
	}

	ObjectTools::FPackageGroupName PGN;
	PGN.PackageName = PackageName;
	PGN.GroupName = TEXT("");
	PGN.ObjectName = AssetName;

	TSet<UPackage*> ObjectsUserRefusedToFullyLoad;
	bool bPromtToOverwrite = bWithDialog;
	UObject* NewObject = ObjectTools::DuplicateSingleObject(OriginalObject, PGN, ObjectsUserRefusedToFullyLoad, bPromtToOverwrite);
	if(NewObject != nullptr)
	{
		// Assets must have RF_Public and RF_Standalone
		const bool bIsAsset = NewObject->IsAsset();
		NewObject->SetFlags(RF_Public | RF_Standalone);

		if (!bIsAsset && NewObject->IsAsset())
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewObject);
		}

		if ( ISourceControlModule::Get().IsEnabled() )
		{
			// Save package here if SCC is enabled because the user can use SCC to revert a change
			TArray<UPackage*> OutermostPackagesToSave;
			OutermostPackagesToSave.Add(NewObject->GetOutermost());

			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			FEditorFileUtils::PromptForCheckoutAndSave(OutermostPackagesToSave, bCheckDirty, bPromptToSave);

			// now attempt to branch, we can do this now as we should have a file on disk
			SourceControlHelpers::CopyPackage(NewObject->GetOutermost(), OriginalObject->GetOutermost());
		}

		// analytics create record
		UAssetToolsImpl::OnNewCreateRecord(NewObject->GetClass(), true);
	}

	return NewObject;
}

void UAssetToolsImpl::SetCreateAssetsAsExternallyReferenceable(bool bValue)
{
	CreateAssetsAsExternallyReferenceable = bValue;
}

bool UAssetToolsImpl::GetCreateAssetsAsExternallyReferenceable()
{
	return CreateAssetsAsExternallyReferenceable;
}

void UAssetToolsImpl::GenerateAdvancedCopyDestinations(FAdvancedCopyParams& InParams, const TArray<FName>& InPackageNamesToCopy, const UAdvancedCopyCustomization* CopyCustomization, TMap<FString, FString>& OutPackagesAndDestinations) const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	FString DestinationFolder = InParams.GetDropLocationForAdvancedCopy();

	if (ensure(DesktopPlatform))
	{
		FPaths::NormalizeFilename(DestinationFolder);
	}
	else
	{
		// Not on a platform that supports desktop functionality
		return;
	}

	bool bGenerateRelativePaths = CopyCustomization->GetShouldGenerateRelativePaths();

	for (auto PackageNameIt = InPackageNamesToCopy.CreateConstIterator(); PackageNameIt; ++PackageNameIt)
	{
		FName PackageName = *PackageNameIt;

		const FString& PackageNameString = PackageName.ToString();
		FString SrcFilename;
		if (FPackageName::DoesPackageExist(PackageNameString, &SrcFilename))
		{
			bool bFileOKToCopy = true;

			FString DestFilename = DestinationFolder;

			FString SubFolder;
			if (SrcFilename.Split(TEXT("/Content/"), nullptr, &SubFolder))
			{
				DestFilename += *SubFolder;
			}
			else
			{
				// Couldn't find Content folder in source path
				bFileOKToCopy = false;
			}

			if (bFileOKToCopy)
			{
				FString Parent = FString();
				if (bGenerateRelativePaths)
				{

					FString RootFolder = CopyCustomization->GetPackageThatInitiatedCopy();
					if (RootFolder != PackageNameString)
					{
						FString BaseParent = FString();
						int32 MinLength = RootFolder.Len() < PackageNameString.Len() ? RootFolder.Len() : PackageNameString.Len();
						for (int Char = 0; Char < MinLength; Char++)
						{
							if (RootFolder[Char] == PackageNameString[Char])
							{
								BaseParent += RootFolder[Char];
							}
							else
							{
								break;
							}
						}

						// If we are in the root content folder, don't break down the folder string
						if (BaseParent == TEXT("/Game"))
						{
							Parent = BaseParent;
						}
						else
						{
							BaseParent.Split(TEXT("/"), &Parent, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
						}
					}
				}

				const FString DestinationPackageName = UAssetToolsImpl::GenerateAdvancedCopyDestinationPackageName(PackageNameString, Parent, DestinationFolder);
				OutPackagesAndDestinations.Add(PackageNameString, DestinationPackageName);
			}
		}
	}
}

FString UAssetToolsImpl::GenerateAdvancedCopyDestinationPackageName(const FString& SourcePackage, const FString& SourcePath, const FString& DestinationFolder)
{
	FString DestinationPackageName;

	const bool bIsRelativeOperation = SourcePath.Len() && DestinationFolder.Len() && SourcePackage.StartsWith(SourcePath);
	if (bIsRelativeOperation)
	{
		// Folder copy/move.

		// Collect the relative path then use it to determine the new location
		// For example, if SourcePath = /Game/MyPath and SourcePackage = /Game/MyPath/MySubPath/MyAsset
		//     /Game/MyPath/MySubPath/MyAsset -> /MySubPath/

		const int32 ShortPackageNameLen = FPackageName::GetShortName(SourcePackage).Len();
		const int32 RelativePathLen = SourcePackage.Len() - ShortPackageNameLen - SourcePath.Len();
		const FString RelativeDestPath = SourcePackage.Mid(SourcePath.Len(), RelativePathLen);

		DestinationPackageName = DestinationFolder + RelativeDestPath + FPackageName::GetShortName(SourcePackage);
	}
	else if (DestinationFolder.Len())
	{
		// Use the passed in default path
		// Normal path
		DestinationPackageName = DestinationFolder + "/" + FPackageName::GetShortName(SourcePackage);
	}
	else
	{
		// Use the path from the old package
		DestinationPackageName = SourcePackage;
	}

	return DestinationPackageName;
}

bool UAssetToolsImpl::FlattenAdvancedCopyDestinations(const TArray<TMap<FString, FString>> PackagesAndDestinations, TMap<FString, FString>& FlattenedPackagesAndDestinations) const
{
	FString CopyErrors;
	for (TMap<FString, FString> PackageAndDestinationMap : PackagesAndDestinations)
	{
		for (auto It = PackageAndDestinationMap.CreateConstIterator(); It; ++It)
		{
			const FString& PackageName = It.Key();
			const FString& DestFilename = It.Value();

			if (FlattenedPackagesAndDestinations.Contains(PackageName))
			{
				const FString* ExistingDestinationPtr = FlattenedPackagesAndDestinations.Find(PackageName);
				const FString ExistingDestination = *ExistingDestinationPtr;
				if (ExistingDestination != DestFilename)
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopy_DuplicateDestinations", "Advanced Copy failed because {0} was being duplicated in two locations, {1} and {2}."),
						FText::FromString(PackageName),
						FText::FromString(FPaths::GetPath(ExistingDestination)),
						FText::FromString(FPaths::GetPath(DestFilename))));
					return false;
				}
			}
			
			// File passed all error conditions above, add it to valid flattened list
			FlattenedPackagesAndDestinations.Add(PackageName, DestFilename);
		}
	}
	// All files passed all validation tests
	return true;
}

bool UAssetToolsImpl::ValidateFlattenedAdvancedCopyDestinations(const TMap<FString, FString>& FlattenedPackagesAndDestinations) const
{
	FString CopyErrors;

	for (auto It = FlattenedPackagesAndDestinations.CreateConstIterator(); It; ++It)
	{
		const FString& PackageName = It.Key();
		const FString& DestFilename = It.Value();

		// Check for source/destination collisions
		if (PackageName == FPaths::GetPath(DestFilename))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopy_DuplicatedSource", "Advanced Copy failed because {0} was being copied over itself."), FText::FromString(PackageName)));
			return false;
		}
		else if (FlattenedPackagesAndDestinations.Contains(FPaths::GetPath(DestFilename)))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopy_DestinationEqualsSource", "Advanced Copy failed because {0} was being copied over the source file {1}."),
				FText::FromString(PackageName),
				FText::FromString(FPaths::GetPath(DestFilename))));
			return false;
		}

		// Check for valid copy locations
		FString SrcFilename;
		if (!FPackageName::DoesPackageExist(PackageName, &SrcFilename))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopyPackages_PackageMissing", "{0} does not exist on disk."), FText::FromString(PackageName)));
			return false;
		}
		else if (SrcFilename.Contains(FPaths::EngineContentDir()))
		{
			const FString LeafName = SrcFilename.Replace(*FPaths::EngineContentDir(), TEXT("Engine/"));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopyPackages_EngineContent", "Unable to copy Engine asset {0}. Engine assets cannot be copied using Advanced Copy."),
				FText::FromString(LeafName)));
			return false;
		}
	}
	
	// All files passed all validation tests
	return true;
}

void UAssetToolsImpl::GetAllAdvancedCopySources(FName SelectedPackage, FAdvancedCopyParams& CopyParams, TArray<FName>& OutPackageNamesToCopy, TMap<FName, FName>& DependencyMap, const UAdvancedCopyCustomization* CopyCustomization) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString CurrentRoot = FString();
	TArray<FName> CurrentDependencies;
	if (!OutPackageNamesToCopy.Contains(SelectedPackage))
	{
		TArray<FAssetData> SourceAssetData;
		AssetRegistry.GetAssetsByPackageName(SelectedPackage, SourceAssetData);
		// Check if this is a folder before using the filter to exclude assets
		bool bIsFolder = SourceAssetData.Num() == 0;
		FARFilter ExclusionFilter = CopyCustomization->GetARFilter();
		AssetRegistry.UseFilterToExcludeAssets(SourceAssetData, ExclusionFilter);
		// If this is a valid asset
		if (SourceAssetData.Num() > 0 || bIsFolder)
		{
			CurrentDependencies.Add(SelectedPackage);
		}

		// If we should check for dependencies OR we are currently checking a folder
		// Folders should ALWAYS get checked for assets and subfolders
		if ((CopyParams.bShouldCheckForDependencies && SourceAssetData.Num() > 0) || bIsFolder)
		{
			RecursiveGetDependenciesAdvanced(SelectedPackage, CopyParams, CurrentDependencies, DependencyMap, CopyCustomization, SourceAssetData);
		}
		OutPackageNamesToCopy.Append(CurrentDependencies);
	}
}

bool UAssetToolsImpl::AdvancedCopyPackages(
	const TMap<FString, FString>& SourceAndDestPackages,
	const bool bForceAutosave,
	const bool bCopyOverAllDestinationOverlaps,
	FDuplicatedObjects* OutDuplicatedObjects,
	EMessageSeverity::Type NotificationSeverityFilter) const
{
	if (ValidateFlattenedAdvancedCopyDestinations(SourceAndDestPackages))
	{
		TArray<FString> SuccessfullyCopiedDestinationFiles;
		TArray<FName> SuccessfullyCopiedSourcePackages;
		TArray<UPackage*> SuccessfullyCopiedDestinationPackages;

		FDuplicatedObjects DuplicatedObjectsLocal;
		FDuplicatedObjects& DuplicatedObjectsForEachPackage = OutDuplicatedObjects ? *OutDuplicatedObjects : DuplicatedObjectsLocal;

		TSet<UObject*> ExistingObjectSet;
		TSet<UObject*> NewObjectSet;
		FString CopyErrors;

		SuccessfullyCopiedDestinationFiles.Reserve(SourceAndDestPackages.Num());
		SuccessfullyCopiedSourcePackages.Reserve(SourceAndDestPackages.Num());
		SuccessfullyCopiedDestinationPackages.Reserve(SourceAndDestPackages.Num());
		DuplicatedObjectsForEachPackage.Reserve(SourceAndDestPackages.Num());
		ExistingObjectSet.Reserve(SourceAndDestPackages.Num());
		NewObjectSet.Reserve(SourceAndDestPackages.Num());

		FScopedSlowTask LoopProgress(SourceAndDestPackages.Num(), LOCTEXT("AdvancedCopying", "Copying files and dependencies..."));
		LoopProgress.MakeDialog();

		for (const auto& Package : SourceAndDestPackages)
		{
			const FString& PackageName = Package.Key;
			const FString& DestFilename = Package.Value;
			FString SrcFilename;

			if (FPackageName::DoesPackageExist(PackageName, &SrcFilename))
			{
				LoopProgress.EnterProgressFrame();
				UPackage* Pkg = LoadPackage(nullptr, *PackageName, LOAD_None);
				if (Pkg)
				{
					FString Name = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(SrcFilename));
					UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), Pkg, *Name);
					if (ExistingObject)
					{
						TSet<UPackage*> ObjectsUserRefusedToFullyLoad;
						ObjectTools::FPackageGroupName PGN;
						PGN.GroupName = TEXT("");
						PGN.ObjectName = FPaths::GetBaseFilename(DestFilename);
						PGN.PackageName = DestFilename;
						const bool bShouldPromptForDestinationConflict = !bCopyOverAllDestinationOverlaps;
						TMap<TSoftObjectPtr<UObject>, TSoftObjectPtr<UObject>> DuplicatedObjects;

						// Temp fix for some codepaths that allows advanced copy of world packages. For partitioned worlds, this can only be supported for worlds with
						// streaming disabled and this code should be removed once the callers switch to the same codepath as editor save as.
						if (UWorld* World = Cast<UWorld>(ExistingObject))
						{
							if (UWorldPartition* WorldPartition = World->GetWorldPartition())
							{
								check(!WorldPartition->IsStreamingEnabled());
								if (!WorldPartition->IsInitialized())
								{
									WorldPartition->Initialize(World, FTransform::Identity);
								}
							}
						}

						if (UObject* NewObject = ObjectTools::DuplicateSingleObject(ExistingObject, PGN, ObjectsUserRefusedToFullyLoad, bShouldPromptForDestinationConflict, &DuplicatedObjects))
						{
							ExistingObjectSet.Add(ExistingObject);
							NewObjectSet.Add(NewObject);
							DuplicatedObjectsForEachPackage.Add(MoveTemp(DuplicatedObjects));
							SuccessfullyCopiedSourcePackages.Add(FName(*PackageName));
							SuccessfullyCopiedDestinationFiles.Add(DestFilename);
							SuccessfullyCopiedDestinationPackages.Add(NewObject->GetPackage());
						}
					}
				}
			}
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		TSet<UObject*> ObjectsAndSubObjectsToReplaceWithin;
		ObjectTools::GatherSubObjectsForReferenceReplacement(NewObjectSet, ExistingObjectSet, ObjectsAndSubObjectsToReplaceWithin);

		TArray<FName> Dependencies;
		for (FName SuccessfullyCopiedPackage : SuccessfullyCopiedSourcePackages)
		{
			Dependencies.Reset();
			AssetRegistryModule.Get().GetDependencies(SuccessfullyCopiedPackage, Dependencies);
			for (FName Dependency : Dependencies)
			{
				const int32 DependencyIndex = SuccessfullyCopiedSourcePackages.IndexOfByKey(Dependency);
				if (DependencyIndex != INDEX_NONE)
				{
					TMap<UObject*, TArray<UObject*, TInlineAllocator<1>>> Consolidations;
					Consolidations.Reserve(DuplicatedObjectsForEachPackage[DependencyIndex].Num());

					for (const TPair<TSoftObjectPtr<UObject>, TSoftObjectPtr<UObject>>& Duplication : DuplicatedObjectsForEachPackage[DependencyIndex])
					{
						UObject* SourceObject = Duplication.Key.Get();
						UObject* NewObject = Duplication.Value.Get();
						if (SourceObject && NewObject)
						{
							Consolidations.FindOrAdd(NewObject).Add(SourceObject);
						}
					}

					TArray<ObjectTools::FReplaceRequest> Requests;
					Requests.Reserve(Consolidations.Num());
					for (TPair<UObject*, TArray<UObject*, TInlineAllocator<1>>>& Consolidation : Consolidations)
					{
						Requests.Add(ObjectTools::FReplaceRequest{Consolidation.Key, Consolidation.Value});
					}

					ObjectTools::ConsolidateObjects(Requests, ObjectsAndSubObjectsToReplaceWithin, ExistingObjectSet, false);
				}
			}
		}

		ObjectTools::CompileBlueprintsAfterRefUpdate(NewObjectSet.Array());

		// The default value for save packages is true if SCC is enabled because the user can use SCC to revert a change
		// The save needs to happen before FMarkForAdd is called on the files.
		bool bSavePackages = ISourceControlModule::Get().IsEnabled() || bForceAutosave;
		if (bSavePackages)
		{
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			FEditorFileUtils::PromptForCheckoutAndSave(SuccessfullyCopiedDestinationPackages, bCheckDirty, bPromptToSave);
		}

		FString SourceControlErrors;

		if (SuccessfullyCopiedDestinationFiles.Num() > 0)
		{
			// attempt to add files to source control (this can quite easily fail, but if it works it is very useful)
			if (GetDefault<UEditorLoadingSavingSettings>()->bSCCAutoAddNewFiles)
			{
				if (ISourceControlModule::Get().IsEnabled())
				{
					ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
					if (SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlHelpers::PackageFilenames(SuccessfullyCopiedDestinationFiles)) == ECommandResult::Failed)
					{
						for (auto FileIt(SuccessfullyCopiedDestinationFiles.CreateConstIterator()); FileIt; FileIt++)
						{
							if (!SourceControlProvider.GetState(*FileIt, EStateCacheUsage::Use)->IsAdded())
							{
								SourceControlErrors += FText::Format(LOCTEXT("AdvancedCopyPackages_SourceControlError", "{0} could not be added to source control"), FText::FromString(*FileIt)).ToString();
								SourceControlErrors += LINE_TERMINATOR;
							}
						}
					}
				}
			}
		}

		FMessageLog AdvancedCopyLog("AssetTools");
		FText LogMessage = FText::FromString(TEXT("Advanced content copy completed successfully!"));
		EMessageSeverity::Type Severity = EMessageSeverity::Info;
		if (SourceControlErrors.Len() > 0)
		{
			Severity = EMessageSeverity::Error;

			AdvancedCopyLog.NewPage(LOCTEXT("AdvancedCopyPackages_SourceControlErrorsListPage", "Source Control Errors"));
			AdvancedCopyLog.Error(FText::FromString(*SourceControlErrors));

			FString ErrorMessage = LOCTEXT("AdvancedCopyPackages_SourceControlErrorsList", "Some files reported source control errors.").ToString();
			
			if (SuccessfullyCopiedSourcePackages.Num() > 0)
			{
				AdvancedCopyLog.NewPage(LOCTEXT("AdvancedCopyPackages_CopyErrorsSuccesslistPage", "Copied Successfully"));
				for (auto FileIt = SuccessfullyCopiedSourcePackages.CreateConstIterator(); FileIt; ++FileIt)
				{
					if (!FileIt->IsNone())
					{
						AdvancedCopyLog.Info(FText::FromName(*FileIt));
					}
				}

				ErrorMessage += LINE_TERMINATOR;
				ErrorMessage += LOCTEXT("AdvancedCopyPackages_CopyErrorsSuccesslist", "Some files were copied successfully.").ToString();
			}
			LogMessage = FText::FromString(ErrorMessage);
		}
		else
		{
			AdvancedCopyLog.NewPage(LOCTEXT("AdvancedCopyPackages_CompletePage", "Advanced content copy completed successfully!"));
			for (auto FileIt = SuccessfullyCopiedSourcePackages.CreateConstIterator(); FileIt; ++FileIt)
			{
				if (!FileIt->IsNone())
				{
					AdvancedCopyLog.Info(FText::FromName(*FileIt));
				}
			}
		}
		// @note Using the bForce param because the InSeverityFilter param is only checked against logs in the last page
		AdvancedCopyLog.Notify(LogMessage, /*InSeverityFilter=*/EMessageSeverity::Error, /*bForce=*/(Severity <= NotificationSeverityFilter));
		return true;
	}
	return false;
}

bool UAssetToolsImpl::AdvancedCopyPackages(const FAdvancedCopyParams& CopyParams, const TArray<TMap<FString, FString>> PackagesAndDestinations) const
{
	TMap<FString, FString> FlattenedDestinationMap;
	if (FlattenAdvancedCopyDestinations(PackagesAndDestinations, FlattenedDestinationMap))
	{
		return AdvancedCopyPackages(FlattenedDestinationMap, CopyParams.bShouldForceSave, CopyParams.bCopyOverAllDestinationOverlaps, /*OutDuplicatedObjects=*/nullptr, EMessageSeverity::Info);
	}
	return false;
}

bool UAssetToolsImpl::RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames)
{
	return AssetRenameManager->RenameAssets(AssetsAndNames);
}

EAssetRenameResult UAssetToolsImpl::RenameAssetsWithDialog(const TArray<FAssetRenameData>& AssetsAndNames, bool bAutoCheckout)
{
	return AssetRenameManager->RenameAssetsWithDialog(AssetsAndNames, bAutoCheckout);
}

void UAssetToolsImpl::FindSoftReferencesToObject(FSoftObjectPath TargetObject, TArray<UObject*>& ReferencingObjects)
{
	AssetRenameManager->FindSoftReferencesToObject(TargetObject, ReferencingObjects);
}

void UAssetToolsImpl::FindSoftReferencesToObjects(const TArray<FSoftObjectPath>& TargetObjects, TMap<FSoftObjectPath, TArray<UObject*>>& ReferencingObjects)
{
	AssetRenameManager->FindSoftReferencesToObjects(TargetObjects, ReferencingObjects);
}

void UAssetToolsImpl::RenameReferencingSoftObjectPaths(const TArray<UPackage *> PackagesToCheck, const TMap<FSoftObjectPath, FSoftObjectPath>& AssetRedirectorMap)
{
	AssetRenameManager->RenameReferencingSoftObjectPaths(PackagesToCheck, AssetRedirectorMap);
}

TArray<UObject*> UAssetToolsImpl::ImportAssetsWithDialog(const FString& DestinationPath)
{
	const bool bAllowAsyncImport = false;
	return ImportAssetsWithDialogImplementation(DestinationPath, bAllowAsyncImport);
}

void UAssetToolsImpl::ImportAssetsWithDialogAsync(const FString& DestinationPath)
{
	const bool bAllowAsyncImport = true;
	ImportAssetsWithDialogImplementation(DestinationPath, bAllowAsyncImport);
}

TArray<UObject*> UAssetToolsImpl::ImportAssetsAutomated(const UAutomatedAssetImportData* ImportData)
{
	check(ImportData);

	FAssetImportParams Params;

	Params.bAutomated = true;
	Params.bForceOverrideExisting = ImportData->bReplaceExisting;
	Params.bSyncToBrowser = false;
	Params.SpecifiedFactory = TStrongObjectPtr<UFactory>(ImportData->Factory);
	Params.ImportData = ImportData;

	return ImportAssetsInternal(ImportData->Filenames, ImportData->DestinationPath, nullptr, Params);
}

void UAssetToolsImpl::ImportAssetTasks(const TArray<UAssetImportTask*>& ImportTasks)
{
	FScopedSlowTask SlowTask(ImportTasks.Num(), LOCTEXT("ImportSlowTask", "Importing"));
	SlowTask.MakeDialog();

	FAssetImportParams Params;
	Params.bSyncToBrowser = false;

	TArray<FString> Filenames;
	Filenames.Add(TEXT(""));
	TArray<UPackage*> PackagesToSave;
	for (UAssetImportTask* ImportTask : ImportTasks)
	{
		if (!ImportTask)
		{
			UE_LOG(LogAssetTools, Warning, TEXT("ImportAssetTasks() supplied an empty task"));
			continue;
		}

		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Import_ImportingFile", "Importing \"{0}\"..."), FText::FromString(FPaths::GetBaseFilename(ImportTask->Filename))));

		Params.AssetImportTask = ImportTask;
		Params.bAllowAsyncImport = ImportTask->bAsync;
		Params.bForceOverrideExisting = ImportTask->bReplaceExisting;
		Params.bAutomated = ImportTask->bAutomated;
		Params.SpecifiedFactory = TStrongObjectPtr<UFactory>(ImportTask->Factory);
		Filenames[0] = ImportTask->Filename;

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ImportTask->Result = ImportAssetsInternal(Filenames, ImportTask->DestinationPath, nullptr, Params);

		PackagesToSave.Reset(ImportTask->Result.Num());
		for (UObject* Object : ImportTask->Result)
		{
			ImportTask->ImportedObjectPaths.Add(Object->GetPathName());
			if (ImportTask->bSave)
			{
				PackagesToSave.AddUnique(Object->GetOutermost());
			}
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// If this wasn't an async import through Interchange (and hence does not have valid AsyncResults),
		// save imported packages here if required.
		if (!ImportTask->AsyncResults.IsValid() && ImportTask->bSave)
		{
			UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
		}
	}
}

void UAssetToolsImpl::ExportAssets(const TArray<FString>& AssetsToExport, const FString& ExportPath)
{
	TArray<UObject*> AssetObjectsToExport;
	AssetObjectsToExport.Reserve(AssetsToExport.Num());

	for (const FString& AssetStr : AssetsToExport)
	{
		UObject* Asset = LoadObject<UObject>(nullptr, *AssetStr);
		if (Asset)
		{
			AssetObjectsToExport.Add(Asset);
		}
		else
		{
			UE_LOG(LogAssetTools, Error, TEXT("Could not load asset '%s' to export it"), *AssetStr);
		}
	}

	const bool bPromptIndividualFilenames = false;
	ExportAssetsInternal(AssetObjectsToExport, bPromptIndividualFilenames, ExportPath);
}

void UAssetToolsImpl::ExportAssets(const TArray<UObject*>& AssetsToExport, const FString& ExportPath) const
{
	const bool bPromptIndividualFilenames = false;
	ExportAssetsInternal(AssetsToExport, bPromptIndividualFilenames, ExportPath);
}

void UAssetToolsImpl::ExportAssetsWithDialog(const TArray<UObject*>& AssetsToExport, bool bPromptForIndividualFilenames)
{
	ExportAssetsInternal(AssetsToExport, bPromptForIndividualFilenames, TEXT(""));
}

void UAssetToolsImpl::ExportAssetsWithDialog(const TArray<FString>& AssetsToExport, bool bPromptForIndividualFilenames)
{
	TArray<UObject*> AssetObjectsToExport;
	AssetObjectsToExport.Reserve(AssetsToExport.Num());

	for (const FString& AssetStr : AssetsToExport)
	{
		UObject* Asset = LoadObject<UObject>(nullptr, *AssetStr);
		if (Asset)
		{
			AssetObjectsToExport.Add(Asset);
		}
		else
		{
			UE_LOG(LogAssetTools, Error, TEXT("Could not load asset '%s' to export it"), *AssetStr);
		}
	}

	ExportAssetsInternal(AssetObjectsToExport, bPromptForIndividualFilenames, TEXT(""));
}

void UAssetToolsImpl::ExpandDirectories(const TArray<FString>& Files, const FString& DestinationPath, TArray<TPair<FString, FString>>& FilesAndDestinations) const
{
	// Iterate through all files in the list, if any folders are found, recurse and expand them.
	for ( int32 FileIdx = 0; FileIdx < Files.Num(); ++FileIdx )
	{
		const FString& Filename = Files[FileIdx];

		// If the file being imported is a directory, just include all sub-files and skip the directory.
		if ( IFileManager::Get().DirectoryExists(*Filename) )
		{
			FString FolderName = FPaths::GetCleanFilename(Filename);

			// Get all files & folders in the folder.
			FString SearchPath = Filename / FString(TEXT("*"));
			TArray<FString> SubFiles;
			IFileManager::Get().FindFiles(SubFiles, *SearchPath, true, true);

			// FindFiles just returns file and directory names, so we need to tack on the root path to get the full path.
			TArray<FString> FullPathItems;
			for ( FString& SubFile : SubFiles )
			{
				FullPathItems.Add(Filename / SubFile);
			}

			// Expand any sub directories found.
			FString NewSubDestination = DestinationPath / FolderName;
			ExpandDirectories(FullPathItems, NewSubDestination, FilesAndDestinations);
		}
		else
		{
			// Add any files and their destination path.
			FilesAndDestinations.Emplace(Filename, DestinationPath);
		}
	}
}
TArray<UObject*> UAssetToolsImpl::ImportAssets(const TArray<FString>& Files, const FString& DestinationPath, UFactory* ChosenFactory, bool bSyncToBrowser /* = true */, TArray<TPair<FString, FString>>* FilesAndDestinations /* = nullptr */, bool bAllowAsyncImport /* = false */, bool bSceneImport /*= false*/) const
{
	const bool bForceOverrideExisting = false;

	FAssetImportParams Params;

	Params.bAutomated = false;
	Params.bForceOverrideExisting = false;
	Params.bSyncToBrowser = bSyncToBrowser;
	Params.SpecifiedFactory = TStrongObjectPtr<UFactory>(ChosenFactory);
	Params.bAllowAsyncImport = bAllowAsyncImport;
	Params.bSceneImport = bSceneImport;

	return ImportAssetsInternal(Files, DestinationPath, FilesAndDestinations, Params);
}

void UAssetToolsImpl::CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName)
{
	const FString SanitizedBasePackageName = UPackageTools::SanitizePackageName(InBasePackageName);

	const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName);
	const FString BaseAssetNameWithSuffix = FPackageName::GetLongPackageAssetName(SanitizedBasePackageName) + InSuffix;
	const FString SanitizedBaseAssetName = ObjectTools::SanitizeObjectName(BaseAssetNameWithSuffix);

	int32 IntSuffix = 0;
	bool bObjectExists = false;

	int32 CharIndex = SanitizedBaseAssetName.Len() - 1;
	while (CharIndex >= 0 && SanitizedBaseAssetName[CharIndex] >= TEXT('0') && SanitizedBaseAssetName[CharIndex] <= TEXT('9'))
	{
		--CharIndex;
	}
	FString TrailingInteger;
	FString TrimmedBaseAssetName = SanitizedBaseAssetName;
	if (SanitizedBaseAssetName.Len() > 0 && CharIndex == -1)
	{
		// This is the all numeric name, in this case we'd like to append _number, because just adding a number isn't great
		TrimmedBaseAssetName += TEXT("_");
		IntSuffix = 2;
	}
	if (CharIndex >= 0 && CharIndex < SanitizedBaseAssetName.Len() - 1)
	{
		TrailingInteger = SanitizedBaseAssetName.RightChop(CharIndex + 1);
		TrimmedBaseAssetName = SanitizedBaseAssetName.Left(CharIndex + 1);
		IntSuffix = FCString::Atoi(*TrailingInteger);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	do
	{
		bObjectExists = false;
		if ( IntSuffix < 1 )
		{
			OutAssetName = SanitizedBaseAssetName;
		}
		else
		{
			FString Suffix = FString::Printf(TEXT("%d"), IntSuffix);
			while (Suffix.Len() < TrailingInteger.Len())
			{
				Suffix = TEXT("0") + Suffix;
			}
			OutAssetName = FString::Printf(TEXT("%s%s"), *TrimmedBaseAssetName, *Suffix);
		}
	
		OutPackageName = PackagePath + TEXT("/") + OutAssetName;
		FString ObjectPath = OutPackageName + TEXT(".") + OutAssetName;

		// Use the asset registry if possible to find existing assets without loading them
		if ( !AssetRegistryModule.Get().IsLoadingAssets() )
		{
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
			if(AssetData.IsValid())
			{
				bObjectExists = true;
			}
		}
		else
		{
			bObjectExists = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_NoRedirects) != nullptr;
		}
		IntSuffix++;
	}
	while (bObjectExists != false);
}

bool UAssetToolsImpl::AssetUsesGenericThumbnail( const FAssetData& AssetData ) const
{
	if ( !AssetData.IsValid() )
	{
		// Invalid asset, assume it does not use a shared thumbnail
		return false;
	}

	if( AssetData.IsAssetLoaded() )
	{
		// Loaded asset, see if there is a rendering info for it
		UObject* Asset = AssetData.GetAsset();
		FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Asset );
		return !RenderInfo || !RenderInfo->Renderer;
	}

	if ( AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName() )
	{
		// Unloaded blueprint asset
		// It would be more correct here to find the rendering info for the generated class,
		// but instead we are simply seeing if there is a thumbnail saved on disk for this asset
		FString PackageFilename;
		if ( FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &PackageFilename) )
		{
			TSet<FName> ObjectFullNames;
			FThumbnailMap ThumbnailMap;

			FName ObjectFullName = FName(*AssetData.GetFullName());
			ObjectFullNames.Add(ObjectFullName);

			ThumbnailTools::LoadThumbnailsFromPackage(PackageFilename, ObjectFullNames, ThumbnailMap);

			FObjectThumbnail* ThumbnailPtr = ThumbnailMap.Find(ObjectFullName);
			if (ThumbnailPtr)
			{
				return (*ThumbnailPtr).IsEmpty();
			}

			return true;
		}
	}
	else
	{
		// Unloaded non-blueprint asset. See if the class has a rendering info.
		UClass* Class = FindObject<UClass>(AssetData.AssetClassPath);

		UObject* ClassCDO = nullptr;
		if (Class != nullptr)
		{
			ClassCDO = Class->GetDefaultObject();
		}

		// Get the rendering info for this object
		FThumbnailRenderingInfo* RenderInfo = nullptr;
		if (ClassCDO != nullptr)
		{
			RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( ClassCDO );
		}

		return !RenderInfo || !RenderInfo->Renderer;
	}

	return false;
}

void UAssetToolsImpl::DiffAgainstDepot( UObject* InObject, const FString& InPackagePath, const FString& InPackageName ) const
{
	check( InObject );

	// Make sure our history is up to date
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	SourceControlProvider.Execute(UpdateStatusOperation, SourceControlHelpers::PackageFilename(InPackagePath));

	// Get the SCC state
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(InPackagePath), EStateCacheUsage::Use);

	// If we have an asset and its in SCC..
	if( SourceControlState.IsValid() && InObject != nullptr && SourceControlState->IsSourceControlled() )
	{
		// Get the file name of package
		FString RelativeFileName;
		if(FPackageName::DoesPackageExist(InPackagePath, &RelativeFileName))
		{
			if(SourceControlState->GetHistorySize() > 0)
			{
				TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->GetHistoryItem(0);
				check(Revision.IsValid());

				// Get the head revision of this package from source control
				FString AbsoluteFileName = FPaths::ConvertRelativePathToFull(RelativeFileName);
				FString TempFileName;
				if(Revision->Get(TempFileName))
				{
					// Try and load that package
					UPackage* TempPackage = LoadPackage(nullptr, *TempFileName, LOAD_ForDiff|LOAD_DisableCompileOnLoad);
					if(TempPackage != nullptr)
					{
						// Grab the old asset from that old package
						UObject* OldObject = FindObject<UObject>(TempPackage, *InPackageName);

						// Recovery for package names that don't match
						if (OldObject == nullptr)
						{
							OldObject = TempPackage->FindAssetInPackage();
						}

						if(OldObject != nullptr)
						{
							/* Set the revision information*/
							FRevisionInfo OldRevision;
							OldRevision.Changelist = Revision->GetCheckInIdentifier();
							OldRevision.Date = Revision->GetDate();
							OldRevision.Revision = Revision->GetRevision();

							FRevisionInfo NewRevision; 
							NewRevision.Revision = TEXT("");
							DiffAssets(OldObject, InObject, OldRevision, NewRevision);
						}
					}
				}
			}
		} 
	}
}

void UAssetToolsImpl::DiffAssets(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	if(OldAsset == nullptr || NewAsset == nullptr)
	{
		UE_LOG(LogAssetTools, Warning, TEXT("DiffAssets: One of the supplied assets was nullptr."));
		return;
	}

	// Get class of both assets 
	UClass* OldClass = OldAsset->GetClass();
	UClass* NewClass = NewAsset->GetClass();
	// If same class..
	if(OldClass == NewClass)
	{
		// Get class-specific actions
		TWeakPtr<IAssetTypeActions> Actions = GetAssetTypeActionsForClass( NewClass );
		if(Actions.IsValid())
		{
			// And use that to perform the Diff
			Actions.Pin()->PerformAssetDiff(OldAsset, NewAsset, OldRevision, NewRevision);
		}
	}
	else
	{
		UE_LOG(LogAssetTools, Warning, TEXT("DiffAssets: Classes were not the same."));
	}
}

FString UAssetToolsImpl::DumpAssetToTempFile(UObject* Asset) const
{
	check(Asset);

	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export asset to archive
	UExporter::ExportToOutputDevice(&Context, Asset, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified|PPF_Copy|PPF_Delimited, false, Asset->GetOuter());

	// Used to generate unique file names during a run
	static int TempFileNum = 0;

	// Build name for temp text file
	FString RelTempFileName = FString::Printf(TEXT("%sText%s-%d.txt"), *FPaths::DiffDir(), *Asset->GetName(), TempFileNum++);
	FString AbsoluteTempFileName = FPaths::ConvertRelativePathToFull(RelTempFileName);

	// Save text into temp file
	if( !FFileHelper::SaveStringToFile( Archive, *AbsoluteTempFileName ) )
	{
		//UE_LOG(LogAssetTools, Warning, TEXT("DiffAssets: Could not write %s"), *AbsoluteTempFileName);
		return TEXT("");
	}
	else
	{
		return AbsoluteTempFileName;
	}
}

FString WrapArgument(const FString& Argument)
{
	// Wrap the passed in argument so it changes from Argument to "Argument"
	return FString::Printf(TEXT("%s%s%s"),	(Argument.StartsWith("\"")) ? TEXT(""): TEXT("\""),
											*Argument,
											(Argument.EndsWith("\"")) ? TEXT(""): TEXT("\""));
}

bool UAssetToolsImpl::CreateDiffProcess(const FString& DiffCommand,  const FString& OldTextFilename,  const FString& NewTextFilename, const FString& DiffArgs) const
{
	// Construct Arguments
	FString Arguments = FString::Printf( TEXT("%s %s %s"),*WrapArgument(OldTextFilename), *WrapArgument(NewTextFilename), *DiffArgs );

	bool bTryRunDiff = true;
	FString NewDiffCommand = DiffCommand;

	while (bTryRunDiff)
	{
		// Fire process
		if (FPlatformProcess::CreateProc(*NewDiffCommand, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr).IsValid())
		{
			return true;
		}
		else
		{
			const FText Message = FText::Format(NSLOCTEXT("AssetTools", "DiffFail", "The currently set diff tool '{0}' could not be run. Would you like to set a new diff tool?"), FText::FromString(DiffCommand));
			EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, Message);
			if (Response == EAppReturnType::No)
			{
				bTryRunDiff = false;
			}
			else
			{
				IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
				check(DesktopPlatform);

				const FText FileFilterType = NSLOCTEXT("AssetTools", "Executables", "Executables");
#if PLATFORM_WINDOWS
				const FString FileFilterText = FString::Printf(TEXT("%s (*.exe)|*.exe"), *FileFilterType.ToString());
#elif PLATFORM_MAC
				const FString FileFilterText = FString::Printf(TEXT("%s (*.app)|*.app"), *FileFilterType.ToString());
#else
				const FString FileFilterText = FString::Printf(TEXT("%s"), *FileFilterType.ToString());
#endif

				TArray<FString> OutFiles;
				if (DesktopPlatform->OpenFileDialog(
					nullptr,
					NSLOCTEXT("AssetTools", "ChooseDiffTool", "Choose Diff Tool").ToString(),
					TEXT(""),
					TEXT(""),
					FileFilterText,
					EFileDialogFlags::None,
					OutFiles))
				{
					UEditorLoadingSavingSettings& Settings = *GetMutableDefault<UEditorLoadingSavingSettings>();
					Settings.TextDiffToolPath.FilePath = OutFiles[0];
					Settings.SaveConfig();
					NewDiffCommand = OutFiles[0];
				}
			}
		}
	}

	return false;
}

void UAssetToolsImpl::MigratePackages(const TArray<FName>& PackageNamesToMigrate) const
{
	FMigrationOptions Options = FMigrationOptions();
	Options.bPrompt = true;
	MigratePackages(PackageNamesToMigrate, FString(), Options);
}

void UAssetToolsImpl::MigratePackages(const TArray<FName>& PackageNamesToMigrate, const FString& DestinationPath, const struct FMigrationOptions& Options) const
{
	// Packages must be saved for the migration to work
	const bool bPromptUserToSave = !FApp::IsUnattended() && Options.bPrompt;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			// Open a dialog asking the user to wait while assets are being discovered
			SDiscoveringAssetsDialog::OpenDiscoveringAssetsDialog(
				SDiscoveringAssetsDialog::FOnAssetsDiscovered::CreateUObject(this, &UAssetToolsImpl::PerformMigratePackages, PackageNamesToMigrate, DestinationPath, Options)
			);
		}
		else
		{
			// Assets are already discovered, perform the migration now
			PerformMigratePackages(PackageNamesToMigrate, DestinationPath, Options);
		}
	}
}

UE::AssetTools::FOnPackageMigration& UAssetToolsImpl::GetOnPackageMigration()
{
	return OnPackageMigration;
}

void UAssetToolsImpl::OnNewImportRecord(UClass* AssetType, const FString& FileExtension, bool bSucceeded, bool bWasCancelled, const FDateTime& StartTime)
{
	// Don't attempt to report usage stats if analytics isn't available
	if(AssetType != nullptr && FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attribs;
		Attribs.Add(FAnalyticsEventAttribute(TEXT("AssetType"), AssetType->GetName()));
		Attribs.Add(FAnalyticsEventAttribute(TEXT("FileExtension"), FileExtension));
		Attribs.Add(FAnalyticsEventAttribute(TEXT("Outcome"), bSucceeded ? TEXT("Success") : (bWasCancelled ? TEXT("Cancelled") : TEXT("Failed"))));
		FTimespan TimeTaken = FDateTime::UtcNow() - StartTime;
		Attribs.Add(FAnalyticsEventAttribute(TEXT("TimeTaken.Seconds"), (float)TimeTaken.GetTotalSeconds()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.ImportAsset"), Attribs);
	}
}

void UAssetToolsImpl::OnNewCreateRecord(UClass* AssetType, bool bDuplicated)
{
	// Don't attempt to report usage stats if analytics isn't available
	if(AssetType != nullptr && FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attribs;
		Attribs.Add(FAnalyticsEventAttribute(TEXT("AssetType"), AssetType->GetName()));
		Attribs.Add(FAnalyticsEventAttribute(TEXT("Duplicated"), bDuplicated? TEXT("Yes") : TEXT("No")));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.CreateAsset"), Attribs);
	}
}

TArray<UObject*> UAssetToolsImpl::ImportAssetsInternal(const TArray<FString>& Files, const FString& RootDestinationPath, TArray<TPair<FString, FString>> *FilesAndDestinationsPtr, const FAssetImportParams& Params) const
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || Params.bAutomated);

	UFactory* SpecifiedFactory = Params.SpecifiedFactory.Get();
	const bool bForceOverrideExisting = Params.bForceOverrideExisting;
	const bool bSyncToBrowser = Params.bSyncToBrowser;
	const bool bAutomatedImport = Params.bAutomated || GIsAutomationTesting;

	TArray<UObject*> ReturnObjects;
	TArray<FString> ValidFiles;
	ValidFiles.Reserve(Files.Num());
	for (int32 FileIndex = 0; FileIndex < Files.Num(); ++FileIndex)
	{
		if (!Files[FileIndex].IsEmpty())
		{
			FString InputFile = Files[FileIndex];
			FPaths::NormalizeDirectoryName(InputFile);
			ValidFiles.Add(InputFile);
		}
	}
	TMap< FString, TArray<UFactory*> > ExtensionToFactoriesMap;
	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

	// Use Interchange if it's enabled and we weren't asked to use a specific UFactory
	const bool bUseInterchangeFramework = [SpecifiedFactory, &InterchangeManager]()
	{
		return UInterchangeManager::IsInterchangeImportEnabled() && (SpecifiedFactory == nullptr);
	}();

	FScopedSlowTask SlowTask(ValidFiles.Num(), LOCTEXT("ImportSlowTask", "Importing"), !bUseInterchangeFramework);

	if (!bUseInterchangeFramework && ValidFiles.Num() > 1)
	{	
		//Always allow user to cancel the import task if they are importing multiple ValidFiles.
		//If we're importing a single file, then the factory policy will dictate if the import if cancelable.
		SlowTask.MakeDialog(true);
	}

	TArray<TPair<FString, FString>> FilesAndDestinations;
	if (FilesAndDestinationsPtr == nullptr)
	{
		ExpandDirectories(ValidFiles, RootDestinationPath, FilesAndDestinations);
	}
	else
	{
		FilesAndDestinations = (*FilesAndDestinationsPtr);
	}

	if(SpecifiedFactory == nullptr)
	{
		// First instantiate one factory for each file extension encountered that supports the extension
		// @todo import: gmp: show dialog in case of multiple matching factories
		for(TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			if(!(*ClassIt)->IsChildOf(UFactory::StaticClass()) || ((*ClassIt)->HasAnyClassFlags(CLASS_Abstract)) || (*ClassIt)->IsChildOf(USceneImportFactory::StaticClass()))
			{
				continue;
			}

			UFactory* Factory = Cast<UFactory>((*ClassIt)->GetDefaultObject());

			if(!Factory->bEditorImport)
			{
				continue;
			}

			TArray<FString> FactoryExtensions;
			Factory->GetSupportedFileExtensions(FactoryExtensions);

			for(const TPair<FString, FString>& FileDest : FilesAndDestinations)
			{
				const FString FileExtension = FPaths::GetExtension(FileDest.Key);

				// Case insensitive string compare with supported formats of this factory
				if(FactoryExtensions.Contains(FileExtension))
				{
					TArray<UFactory*>& ExistingFactories = ExtensionToFactoriesMap.FindOrAdd(FileExtension);

					// Do not remap extensions, just reuse the existing UFactory.
					// There may be multiple UFactories, so we will keep track of all of them
					bool bFactoryAlreadyInMap = false;
					for(auto FoundFactoryIt = ExistingFactories.CreateConstIterator(); FoundFactoryIt; ++FoundFactoryIt)
					{
						if((*FoundFactoryIt)->GetClass() == Factory->GetClass())
						{
							bFactoryAlreadyInMap = true;
							break;
						}
					}

					if(!bFactoryAlreadyInMap)
					{
						// We found a factory for this file, it can be imported!
						// Create a new factory of the same class and make sure it doesn't get GCed.
						// The object will be removed from the root set at the end of this function.
						UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), Factory->GetClass());
						if(NewFactory->ConfigureProperties())
						{
							NewFactory->AddToRoot();
							ExistingFactories.Add(NewFactory);
						}
					}
				}
			}
		}
	}
	else if(SpecifiedFactory->bEditorImport && !bAutomatedImport) 
	{

		TArray<FString> FactoryExtensions;
		SpecifiedFactory->GetSupportedFileExtensions(FactoryExtensions);

		for(auto FileIt = ValidFiles.CreateConstIterator(); FileIt; ++FileIt)
		{
			const FString FileExtension = FPaths::GetExtension(*FileIt);

			// Case insensitive string compare with supported formats of this factory
			if(!FactoryExtensions.Contains(FileExtension))
			{
				continue;
			}

			TArray<UFactory*>& ExistingFactories = ExtensionToFactoriesMap.FindOrAdd(FileExtension);

			// Do not remap extensions, just reuse the existing UFactory.
			// There may be multiple UFactories, so we will keep track of all of them
			bool bFactoryAlreadyInMap = false;
			for(auto FoundFactoryIt = ExistingFactories.CreateConstIterator(); FoundFactoryIt; ++FoundFactoryIt)
			{
				if((*FoundFactoryIt)->GetClass() == SpecifiedFactory->GetClass())
				{
					bFactoryAlreadyInMap = true;
					break;
				}
			}

			if(!bFactoryAlreadyInMap)
			{
				// We found a factory for this file, it can be imported!
				// Create a new factory of the same class and make sure it doesnt get GCed.
				// The object will be removed from the root set at the end of this function.
				UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), SpecifiedFactory->GetClass());
				if(NewFactory->ConfigureProperties())
				{
					NewFactory->AddToRoot();
					ExistingFactories.Add(NewFactory);
				}
			}
		}
	}

	// We need to sort the factories so that they get tested in priority order
	for(TPair<FString, TArray<UFactory*>>& ExtensionToFactories : ExtensionToFactoriesMap)
	{
		ExtensionToFactories.Value.Sort(&UFactory::SortFactoriesByPriority);
	}

	// Some flags to keep track of what the user decided when asked about overwriting or replacing
	bool bOverwriteAll = false;
	bool bReplaceAll = false;
	bool bDontOverwriteAny = false;
	bool bDontReplaceAny = false;
	if (bAutomatedImport)
	{
		bOverwriteAll = bReplaceAll = bForceOverrideExisting;
		bDontOverwriteAny = bDontReplaceAny = !bForceOverrideExisting;
	}

	TArray<UFactory*> UsedFactories;
	bool bImportWasCancelled = false;
	bool bOnlyInterchangeImport = bUseInterchangeFramework;
	if(bUseInterchangeFramework)
	{
		for (int32 FileIdx = 0; FileIdx < FilesAndDestinations.Num(); ++FileIdx)
		{
			// Filename will need to get sanitized before we create an asset out of them as they
			// can be created out of sources that contain spaces and other invalid characters. Filename cannot be sanitized
			// until other checks are done that rely on looking at the actual source file so sanitation is delayed.
			const FString& Filename = FilesAndDestinations[FileIdx].Key;
			{
				UE::Interchange::FScopedSourceData ScopedSourceData(Filename);

				if (!InterchangeManager.CanTranslateSourceData(ScopedSourceData.GetSourceData()))
				{
					bOnlyInterchangeImport = false;
					break;
				}
			}
		}

		if (!bOnlyInterchangeImport)
		{
			if (Files.Num() > 1)
			{	
				//Always allow user to cancel the import task if they are importing multiple files.
				//If we're importing a single file, then the factory policy will dictate if the import if cancelable.
				SlowTask.MakeDialog(true);
			}
		}
		else
		{
			//Complete the slow task
			SlowTask.CompletedWork = FilesAndDestinations.Num();
		}
	}

	struct FInterchangeImportStatus
	{
		explicit FInterchangeImportStatus(int32 NumFiles)
			: InterchangeResultsContainer(NewObject<UInterchangeResultsContainer>(GetTransientPackage())),
			  ImportCount(NumFiles)
		{}

		TStrongObjectPtr<UInterchangeResultsContainer> InterchangeResultsContainer;
		TArray<TWeakObjectPtr<UObject>> ImportedObjects;
		std::atomic<int32> ImportCount;
	};

	TSharedPtr<FInterchangeImportStatus, ESPMode::ThreadSafe> ImportStatus = MakeShared<FInterchangeImportStatus>(FilesAndDestinations.Num());

	// Now iterate over the input files and use the same factory object for each file with the same extension
	for(int32 FileIdx = 0; FileIdx < FilesAndDestinations.Num() && !bImportWasCancelled; ++FileIdx)
	{
		// Filename and DestinationPath will need to get santized before we create an asset out of them as they
		// can be created out of sources that contain spaces and other invalid characters. Filename cannot be sanitized
		// until other checks are done that rely on looking at the actual source file so sanitation is delayed.
		const FString& Filename = FilesAndDestinations[FileIdx].Key;

		FString DestinationPath;
		FString ErrorMsg;
		if (!FPackageName::TryConvertFilenameToLongPackageName(ObjectTools::SanitizeObjectPath(FilesAndDestinations[FileIdx].Value), DestinationPath, &ErrorMsg))
		{
			const FText Message = FText::Format(LOCTEXT("CannotConvertDestinationPath", "Can't import the file '{0}' because the destination path '{1}' cannot be converted to a package path."), FText::FromString(Filename), FText::FromString(DestinationPath));
			if (!bAutomatedImport)
			{
				FMessageDialog::Open(EAppMsgType::Ok, Message);
			}

			UE_LOG(LogAssetTools, Warning, TEXT("%s"), *ErrorMsg);
			UE_LOG(LogAssetTools, Warning, TEXT("%s"), *Message.ToString());

			continue;
		}

		if (bUseInterchangeFramework)
		{
			UE::Interchange::FScopedSourceData ScopedSourceData(Filename);

			if (InterchangeManager.CanTranslateSourceData(ScopedSourceData.GetSourceData()))
			{
				FImportAssetParameters ImportAssetParameters;
				ImportAssetParameters.bIsAutomated = bAutomatedImport;
				ImportAssetParameters.ReimportAsset = nullptr;


				TFunction<void(UE::Interchange::FImportResult&)> AppendImportResult =
					// Note: ImportStatus captured by value so that the lambda keeps the shared ptr alive
					[ImportStatus, ImportTask = Params.AssetImportTask](UE::Interchange::FImportResult& Result)
					{
						ImportStatus->InterchangeResultsContainer->Append(Result.GetResults());
						ImportStatus->ImportedObjects.Append(Result.GetImportedObjects());

						// If imported through an AssetImportTask object, and it's set to save after import, do this here.
						if (ImportTask && ImportTask->bSave)
						{
							TArray<UPackage*> PackagesToSave;
							for (UObject* Object : Result.GetImportedObjects())
							{
								PackagesToSave.Add(Object->GetOutermost());
							}

							constexpr bool bDirtyOnly = true;
							UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, bDirtyOnly);
						}
					};

				TFunction<void(UE::Interchange::FImportResult&)> AppendAndBroadcastImportResultIfNeeded =
					// Note: ImportStatus captured by value so that the lambda keeps the shared ptr alive
					[ImportStatus, AppendImportResult, bSyncToBrowser](UE::Interchange::FImportResult& Result)
					{
						AppendImportResult(Result);

						if (--ImportStatus->ImportCount == 0)
						{
							UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
							InterchangeManager.OnBatchImportComplete.Broadcast(ImportStatus->InterchangeResultsContainer);

							if (bSyncToBrowser)
							{
								// Only sync the content browser when the full import is done. Otherwise it can be annoying for the user.
								// UX suggestion : Maybe we could move this into the post import window as button ("Select Imported Asset(s)") so it would be even less disruptive for the users.
								TArray<UObject*> ImportedObjects;
								ImportedObjects.Reserve(ImportStatus->ImportedObjects.Num());
								for (const TWeakObjectPtr<UObject>& WeakObject : ImportStatus->ImportedObjects)
								{
									ImportedObjects.Add(WeakObject.Get());
								}

								UAssetToolsImpl::Get().SyncBrowserToAssets(ImportedObjects);
							}
						}
					};

				if (Params.bSceneImport || (Params.SpecifiedFactory && Params.SpecifiedFactory->GetClass()->IsChildOf(USceneImportFactory::StaticClass())))
				{
					TPair<UE::Interchange::FAssetImportResultRef, UE::Interchange::FSceneImportResultRef> InterchangeResults =
						InterchangeManager.ImportSceneAsync(DestinationPath, ScopedSourceData.GetSourceData(), ImportAssetParameters);

					// If we have an ImportTask, fill out the asynchronous results object here so the caller can see when the results are ready
					if ( Params.AssetImportTask)
					{
						 Params.AssetImportTask->AsyncResults = InterchangeResults.Get<0>();
					}

					InterchangeResults.Get<0>()->OnDone(AppendImportResult);;
					InterchangeResults.Get<1>()->OnDone(AppendAndBroadcastImportResultIfNeeded);

					if (!Params.bAllowAsyncImport)
					{
						InterchangeResults.Get<0>()->WaitUntilDone();
						InterchangeResults.Get<1>()->WaitUntilDone();
					}
				}
				else
				{
					UE::Interchange::FAssetImportResultRef InterchangeResult = (InterchangeManager.ImportAssetAsync(DestinationPath, ScopedSourceData.GetSourceData(), ImportAssetParameters));

					// If we have an ImportTask, fill out the asynchronous results object here so the caller can see when the results are ready
					if ( Params.AssetImportTask)
					{
						 Params.AssetImportTask->AsyncResults = InterchangeResult;
					}

					InterchangeResult->OnDone(AppendAndBroadcastImportResultIfNeeded);

					if (!Params.bAllowAsyncImport)
					{
						InterchangeResult->WaitUntilDone();
					}
				}

				if (!Params.bAllowAsyncImport)
				{
					ReturnObjects.Reserve(ImportStatus->ImportedObjects.Num());

					for (const TWeakObjectPtr<UObject>& ImportedObject : ImportStatus->ImportedObjects)
					{
						ReturnObjects.Add(ImportedObject.Get());
					}
				}

				//Import done, iterate the next file and destination
				
				//If we do not import only interchange file, update the progress for each interchange task
				if (!bOnlyInterchangeImport)
				{
					SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Import_ImportingFile", "Importing \"{0}\"..."), FText::FromString(FPaths::GetBaseFilename(Filename))));
				}
				continue;
			}
		}
		FString FileExtension = FPaths::GetExtension(Filename);
		const TArray<UFactory*>* FactoriesPtr = ExtensionToFactoriesMap.Find(FileExtension);
		UFactory* Factory = nullptr;
		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("Import_ImportingFile", "Importing \"{0}\"..."), FText::FromString(FPaths::GetBaseFilename(Filename))));

		// Assume that for automated import, the user knows exactly what factory to use if it exists
		if(bAutomatedImport && SpecifiedFactory && SpecifiedFactory->FactoryCanImport(Filename))
		{
			Factory = SpecifiedFactory;
		}
		else if(FactoriesPtr)
		{
			const TArray<UFactory*>& Factories = *FactoriesPtr;

			if(Factories.Num() > 0)
			{
				// Handle the potential of multiple factories being found
				//	Factories was previously sorted by ImportPriority
				//  and filtered by file extension
				//  but FactoryCanImport has not been checked yet

				for(auto FactoryIt = Factories.CreateConstIterator(); FactoryIt; ++FactoryIt)
				{
					UFactory* TestFactory = *FactoryIt;
					if(TestFactory->FactoryCanImport(Filename))
					{
						if ( Factory != nullptr )
						{
							// we found a second factory for this type
							if ( Factory->ImportPriority == TestFactory->ImportPriority )
							{
								UE_LOG(LogAssetTools, Warning, TEXT("Two factories registered with same priority : %s and %s"), *Factory->GetName(), *TestFactory->GetName() );
							}
							break;
						}

						Factory = TestFactory;
						//found one, continue so we can check for multiple importers 
						//break;
					}
				}

				if ( Factory == nullptr )
				{
					// no factories passed FactoryCanImport()
					//  this seems wrong, but it preserves old behavior
					// ??
					
					Factory = Factories[0];
					
					UE_LOG(LogAssetTools, Warning, TEXT("Factory did not pass FactoryCanImport, trying anyway : %s"), *Factory->GetName());
				}
			}
		}
		else
		{
			if(FEngineAnalytics::IsAvailable())
			{
				TArray<FAnalyticsEventAttribute> Attribs;
				Attribs.Add(FAnalyticsEventAttribute(TEXT("FileExtension"), FileExtension));

				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.ImportFailed"), Attribs);
			}

			const FText Message = FText::Format(LOCTEXT("ImportFailed_UnknownExtension", "Failed to import '{0}'. Unknown extension '{1}'."), FText::FromString(Filename), FText::FromString(FileExtension));
			FNotificationInfo Info(Message);
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			Info.bFireAndForget = true;
			Info.bUseSuccessFailIcons = true;
			FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);

			UE_LOG(LogAssetTools, Warning, TEXT("%s"), *Message.ToString());
		}

		if(Factory != nullptr)
		{
			if (FilesAndDestinations.Num() == 1)
			{
				SlowTask.MakeDialog(Factory->CanImportBeCanceled());
			}

			// Reset the 'Do you want to overwrite the existing object?' Yes to All / No to All prompt, to make sure the
			// user gets a chance to select something when the factory is first used during this import
			if (!UsedFactories.Contains(Factory))
			{
				Factory->ResetState();
				UsedFactories.AddUnique(Factory);
			}

			UClass* ImportAssetType = Factory->SupportedClass;
			bool bImportSucceeded = false;
			FDateTime ImportStartTime = FDateTime::UtcNow();

			FString Name;
			if (Params.AssetImportTask && !Params.AssetImportTask->DestinationName.IsEmpty())
			{
				Name = Params.AssetImportTask->DestinationName;
			}
			else
			{
				Name = FPaths::GetBaseFilename(Filename);
			}
			Name = ObjectTools::SanitizeObjectName(Name);

			FString PackageName = ObjectTools::SanitizeInvalidChars(FPaths::Combine(*DestinationPath, *Name), INVALID_LONGPACKAGE_CHARACTERS);

			// We can not create assets that share the name of a map file in the same location
			if(FEditorFileUtils::IsMapPackageAsset(PackageName))
			{
				const FText Message = FText::Format(LOCTEXT("AssetNameInUseByMap", "You can not create an asset named '{0}' because there is already a map file with this name in this folder."), FText::FromString(Name));
				if(!bAutomatedImport)
				{
					FMessageDialog::Open(EAppMsgType::Ok, Message);
				}
				UE_LOG(LogAssetTools, Warning, TEXT("%s"), *Message.ToString());
				OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
				continue;
			}

			UPackage* Pkg = CreatePackage( *PackageName);
			if(!ensure(Pkg))
			{
				// Failed to create the package to hold this asset for some reason
				OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
				continue;
			}

			// Make sure the destination package is loaded
			Pkg->FullyLoad();

			// Check for an existing object
			UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), Pkg, *Name);
			if(ExistingObject != nullptr)
			{
				// If the existing object is one of the imports we've just created we can't replace or overwrite it
				if(ReturnObjects.Contains(ExistingObject))
				{
					if(ImportAssetType == nullptr)
					{
						// The factory probably supports multiple types and cant be determined yet without asking the user or actually loading it
						// We just need to generate an unused name so object should do fine.
						ImportAssetType = UObject::StaticClass();
					}
					// generate a unique name for this import
					Name = MakeUniqueObjectName(Pkg, ImportAssetType, *Name).ToString();
				}
				else
				{
					// If the object is supported by the factory we are using, ask if we want to overwrite the asset
					// Otherwise, prompt to replace the object
					if(Factory->DoesSupportClass(ExistingObject->GetClass()))
					{
						// The factory can overwrite this object, ask if that is okay, unless "Yes To All" or "No To All" was already selected
						EAppReturnType::Type UserResponse;

						if(bForceOverrideExisting || bOverwriteAll || GIsAutomationTesting)
						{
							UserResponse = EAppReturnType::YesAll;
						}
						else if(bDontOverwriteAny)
						{
							UserResponse = EAppReturnType::NoAll;
						}
						else
						{
							UserResponse = FMessageDialog::Open(
								EAppMsgType::YesNoYesAllNoAll,
								FText::Format(LOCTEXT("ImportObjectAlreadyExists_SameClass", "Do you want to overwrite the existing asset?\n\nAn asset already exists at the import location: {0}"), FText::FromString(PackageName)));

							bOverwriteAll = UserResponse == EAppReturnType::YesAll;
							bDontOverwriteAny = UserResponse == EAppReturnType::NoAll;
						}

						const bool bWantOverwrite = UserResponse == EAppReturnType::Yes || UserResponse == EAppReturnType::YesAll;

						if(!bWantOverwrite)
						{
							// User chose not to replace the package
							bImportWasCancelled = true;
							OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
							continue;
						}
					}
					else if(!bAutomatedImport)
					{
						// The factory can't overwrite this asset, ask if we should delete the object then import the new one. Only do this if "Yes To All" or "No To All" was not already selected.
						EAppReturnType::Type UserResponse;

						if(bReplaceAll)
						{
							UserResponse = EAppReturnType::YesAll;
						}
						else if(bDontReplaceAny)
						{
							UserResponse = EAppReturnType::NoAll;
						}
						else
						{
							UserResponse = FMessageDialog::Open(
								EAppMsgType::YesNoYesAllNoAll,
								FText::Format(LOCTEXT("ImportObjectAlreadyExists_DifferentClass", "Do you want to replace the existing asset?\n\nAn asset already exists at the import location: {0}"), FText::FromString(PackageName)));

							bReplaceAll = UserResponse == EAppReturnType::YesAll;
							bDontReplaceAny = UserResponse == EAppReturnType::NoAll;
						}

						const bool bWantReplace = UserResponse == EAppReturnType::Yes || UserResponse == EAppReturnType::YesAll;

						if(bWantReplace)
						{
							// Delete the existing object
							int32 NumObjectsDeleted = 0;
							TArray< UObject* > ObjectsToDelete;
							ObjectsToDelete.Add(ExistingObject);

							// If the user forcefully deletes the package, all sorts of things could become invalidated,
							// the Pkg pointer might be killed even though it was added to the root.
							TWeakObjectPtr<UPackage> WeakPkg(Pkg);

							// Dont let the package get garbage collected (just in case we are deleting the last asset in the package)
							Pkg->AddToRoot();
							NumObjectsDeleted = ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

							// If the weak package ptr is still valid, it should then be safe to remove it from the root.
							if(WeakPkg.IsValid())
							{
								Pkg->RemoveFromRoot();
							}

							const FString QualifiedName = PackageName + TEXT(".") + Name;
							FText Reason;
							if(NumObjectsDeleted == 0 || !IsGloballyUniqueObjectName(*QualifiedName, &Reason))
							{
								// Original object couldn't be deleted
								const FText Message = FText::Format(LOCTEXT("ImportDeleteFailed", "Failed to delete '{0}'. The asset is referenced by other content."), FText::FromString(PackageName));
								FMessageDialog::Open(EAppMsgType::Ok, Message);
								UE_LOG(LogAssetTools, Warning, TEXT("%s"), *Message.ToString());
								OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
								continue;
							}
							else
							{
								// succeed, recreate package since it has been deleted
								Pkg = CreatePackage( *PackageName);
								Pkg->MarkAsFullyLoaded();
							}
						}
						else
						{
							// User chose not to replace the package
							bImportWasCancelled = true;
							OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
							continue;
						}
					}
				}
			}

			// Check for a package that was marked for delete in source control
			if(!CheckForDeletedPackage(Pkg))
			{
				OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
				continue;
			}

			Pkg->SetIsExternallyReferenceable(CreateAssetsAsExternallyReferenceable);

			Factory->SetAutomatedAssetImportData(Params.ImportData);
			Factory->SetAssetImportTask(Params.AssetImportTask);

			ImportAssetType = Factory->ResolveSupportedClass();
			UObject* Result = Factory->ImportObject(ImportAssetType, Pkg, FName(*Name), RF_Public | RF_Standalone | RF_Transactional, Filename, nullptr, bImportWasCancelled);

			Factory->SetAutomatedAssetImportData(nullptr);
			Factory->SetAssetImportTask(nullptr);

			// Do not report any error if the operation was canceled.
			if(!bImportWasCancelled)
			{
				if(Result)
				{
					ReturnObjects.Add(Result);

					// Notify the asset registry
					FAssetRegistryModule::AssetCreated(Result);
					GEditor->BroadcastObjectReimported(Result);

					for (UObject* AdditionalResult : Factory->GetAdditionalImportedObjects())
					{
						ReturnObjects.Add(AdditionalResult);
					}

					bImportSucceeded = true;
				}
				else
				{
					const FText Message = FText::Format(LOCTEXT("ImportFailed_Generic", "Failed to import '{0}'. Failed to create asset '{1}'.\nPlease see Output Log for details."), FText::FromString(Filename), FText::FromString(PackageName));
					if(!bAutomatedImport)
					{
						FMessageDialog::Open(EAppMsgType::Ok, Message);
					}
					UE_LOG(LogAssetTools, Warning, TEXT("%s"), *Message.ToString());
				}
			}

			// Refresh the supported class.  Some factories (e.g. FBX) only resolve their type after reading the file
			ImportAssetType = Factory->ResolveSupportedClass();
			OnNewImportRecord(ImportAssetType, FileExtension, bImportSucceeded, bImportWasCancelled, ImportStartTime);
		}
		else
		{
			// A factory or extension was not found. The extension warning is above. If a factory was not found, the user likely canceled a factory configuration dialog.
		}

		bImportWasCancelled |= SlowTask.ShouldCancel();
		if (bImportWasCancelled)
		{
			UE_LOG(LogAssetTools, Log, TEXT("The import task was canceled."));
		}
	}

	// Clean up and remove the factories we created from the root set
	for(TMap<FString, TArray<UFactory*>>::TConstIterator ExtensionIt = ExtensionToFactoriesMap.CreateConstIterator(); ExtensionIt; ++ExtensionIt)
	{
		for(TArray<UFactory*>::TConstIterator FactoryIt = ExtensionIt.Value().CreateConstIterator(); FactoryIt; ++FactoryIt)
		{
			(*FactoryIt)->CleanUp();
			(*FactoryIt)->RemoveFromRoot();
		}
	}

	// Sync content browser to the newly created assets
	if(ReturnObjects.Num() && (bSyncToBrowser != false))
	{
		UAssetToolsImpl::Get().SyncBrowserToAssets(ReturnObjects);
	}

	return ReturnObjects;
}

void UAssetToolsImpl::ExportAssetsInternal(const TArray<UObject*>& ObjectsToExport, bool bPromptIndividualFilenames, const FString& ExportPath) const
{
	FString LastExportPath = !ExportPath.IsEmpty() ? ExportPath : FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);

	if (ObjectsToExport.Num() == 0)
	{
		return;
	}

	FString SelectedExportPath;
	if (!bPromptIndividualFilenames)
	{
		if (ExportPath.IsEmpty())
		{
			// If not prompting individual files, prompt the user to select a target directory.
			IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
			if (DesktopPlatform)
			{
				FString FolderName;
				const FString Title = NSLOCTEXT("UnrealEd", "ChooseADirectory", "Choose A Directory").ToString();
				const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					Title,
					LastExportPath,
					FolderName
				);

				if (bFolderSelected)
				{
					SelectedExportPath = FolderName;
				}
			}
		}
		else
		{
			SelectedExportPath = ExportPath;
		}

		// Copy off the selected path for future export operations.
		LastExportPath = SelectedExportPath;
	}

	GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "Exporting", "Exporting"), true);

	// Create an array of all available exporters.
	TArray<UExporter*> Exporters;
	ObjectTools::AssembleListOfExporters(Exporters);

	//Array to control the batch mode and the show options for the exporters that will be use by the selected assets
	TArray<UExporter*> UsedExporters;

	// Export the objects.
	bool bAnyObjectMissingSourceData = false;
	for (int32 Index = 0; Index < ObjectsToExport.Num(); Index++)
	{
		GWarn->StatusUpdate(Index, ObjectsToExport.Num(), FText::Format(NSLOCTEXT("UnrealEd", "Exportingf", "Exporting ({0} of {1})"), FText::AsNumber(Index), FText::AsNumber(ObjectsToExport.Num())));

		UObject* ObjectToExport = ObjectsToExport[Index];
		if (!ObjectToExport)
		{
			continue;
		}

		if (ObjectToExport->GetOutermost()->HasAnyPackageFlags(PKG_DisallowExport))
		{
			continue;
		}

		// Find all the exporters that can export this type of object and construct an export file dialog.
		TArray<FString> AllFileTypes;
		TArray<FString> AllExtensions;
		TArray<FString> PreferredExtensions;

		// Iterate in reverse so the most relevant file formats are considered first.
		for (int32 ExporterIndex = Exporters.Num() - 1; ExporterIndex >= 0; --ExporterIndex)
		{
			UExporter* Exporter = Exporters[ExporterIndex];
			if (Exporter->SupportedClass)
			{
				const bool bObjectIsSupported = Exporter->SupportsObject(ObjectToExport);
				if (bObjectIsSupported)
				{
					// Get a string representing of the exportable types.
					check(Exporter->FormatExtension.Num() == Exporter->FormatDescription.Num());
					check(Exporter->FormatExtension.IsValidIndex(Exporter->PreferredFormatIndex));
					for (int32 FormatIndex = Exporter->FormatExtension.Num() - 1; FormatIndex >= 0; --FormatIndex)
					{
						const FString& FormatExtension = Exporter->FormatExtension[FormatIndex];
						const FString& FormatDescription = Exporter->FormatDescription[FormatIndex];

						if (FormatIndex == Exporter->PreferredFormatIndex)
						{
							PreferredExtensions.Add(FormatExtension);
						}
						AllFileTypes.Add(FString::Printf(TEXT("%s (*.%s)|*.%s"), *FormatDescription, *FormatExtension, *FormatExtension));
						AllExtensions.Add(FString::Printf(TEXT("*.%s"), *FormatExtension));
					}
				}
			}
		}

		// Skip this object if no exporter found for this resource type.
		if (PreferredExtensions.Num() == 0)
		{
			continue;
		}

		// If FBX is listed, make that the most preferred option
		//   also prioritize PNG and EXR ahead of other image formats
		// @todo provide a general purpose way of prioritizing export formats, don't hard-code FBX here
		const TCHAR * TopPriorityExtensions[] =
		{
			TEXT("FBX"),
			TEXT("PNG"),
			TEXT("EXR")
		};
		const int TopPriorityExtensionsCount = 3;

		for(int TopPriorityExtensionsIndex=0;TopPriorityExtensionsIndex<TopPriorityExtensionsCount;TopPriorityExtensionsIndex++)
		{
			FString ThisExtension = TopPriorityExtensions[TopPriorityExtensionsIndex];
			// FString.Find is case insensitive
			int32 ExtIndex = PreferredExtensions.Find(ThisExtension);
			if (ExtIndex != INDEX_NONE)
			{
				PreferredExtensions.RemoveAt(ExtIndex);
				PreferredExtensions.Insert(ThisExtension, 0);
				// only do for first one found:
				break;
			}
		}

		// if there are multiple exporters, we arbitrarily choose [0] to go first
		//	@todo sort by alpha or priority or something to make this order consistent
		FString FirstExtension = PreferredExtensions[0];

		// If TopPriorityExtension is listed, make that the first option here too, then compile them all into one string
		check(AllFileTypes.Num() == AllExtensions.Num())

		for (int ExtIndex = 1; ExtIndex < AllFileTypes.Num(); ++ExtIndex)
		{
			const FString FileType = AllFileTypes[ExtIndex];
			if (FileType.Contains(FirstExtension))
			{
				AllFileTypes.RemoveAt(ExtIndex);
				AllFileTypes.Insert(FileType, 0);

				const FString Extension = AllExtensions[ExtIndex];
				AllExtensions.RemoveAt(ExtIndex);
				AllExtensions.Insert(Extension, 0);
			}
		}

		FString FileTypes;
		FString Extensions;
		for (int ExtIndex = 0; ExtIndex < AllFileTypes.Num(); ++ExtIndex)
		{
			if (FileTypes.Len())
			{
				FileTypes += TEXT("|");
			}
			FileTypes += AllFileTypes[ExtIndex];

			if (Extensions.Len())
			{
				Extensions += TEXT(";");
			}
			Extensions += AllExtensions[ExtIndex];
		}
		FileTypes = FString::Printf(TEXT("%s|All Files (%s)|%s"), *FileTypes, *Extensions, *Extensions);

		FString SaveFileName;
		if (bPromptIndividualFilenames)
		{
			TArray<FString> SaveFilenames;
			IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
			bool bSave = false;
			if (DesktopPlatform)
			{
				bSave = DesktopPlatform->SaveFileDialog(
					FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
					FText::Format(NSLOCTEXT("UnrealEd", "Save_F", "Save: {0}"), FText::FromString(ObjectToExport->GetName())).ToString(),
					*LastExportPath,
					*ObjectToExport->GetName(),
					*FileTypes,
					EFileDialogFlags::None,
					SaveFilenames
				);
			}

			if (!bSave)
			{
				int32 NumObjectsLeftToExport = ObjectsToExport.Num() - Index - 1;
				if (NumObjectsLeftToExport > 0)
				{
					const FText ConfirmText = FText::Format(NSLOCTEXT("UnrealEd", "AssetTools_ExportObjects_CancelRemaining", "Would you like to cancel exporting the next {0} files as well?"), FText::AsNumber(NumObjectsLeftToExport));
					if (EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, ConfirmText))
					{
						break;
					}
				}
				continue;
			}
			SaveFileName = FString(SaveFilenames[0]);

			// Copy off the selected path for future export operations.
			LastExportPath = SaveFileName;
		}
		else
		{
			// Assemble a filename from the export directory and the object path.
			SaveFileName = SelectedExportPath;

			if (!FPackageName::IsShortPackageName(ObjectToExport->GetOutermost()->GetFName()))
			{
				// Determine the save file name from the long package name
				FString PackageName = ObjectToExport->GetOutermost()->GetName();
				if (PackageName.Left(1) == TEXT("/"))
				{
					// Trim the leading slash so the file manager doesn't get confused
					PackageName.MidInline(1, MAX_int32, false);
				}

				FPaths::NormalizeFilename(PackageName);
				SaveFileName /= PackageName;
			}
			else
			{
				// Assemble the path from the package name.
				SaveFileName /= ObjectToExport->GetOutermost()->GetName();
				SaveFileName /= ObjectToExport->GetName();
			}
			SaveFileName += FString::Printf(TEXT(".%s"), *FirstExtension);
			UE_LOG(LogAssetTools, Log, TEXT("Exporting \"%s\" to \"%s\""), *ObjectToExport->GetPathName(), *SaveFileName);
		}

		// Create the path, then make sure the target file is not read-only.
		const FString ObjectExportPath(FPaths::GetPath(SaveFileName));
		const bool bFileInSubdirectory = ObjectExportPath.Contains(TEXT("/"));
		if (bFileInSubdirectory && (!IFileManager::Get().MakeDirectory(*ObjectExportPath, true)))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_FailedToMakeDirectory", "Failed to make directory {0}"), FText::FromString(ObjectExportPath)));
		}
		else if (IFileManager::Get().IsReadOnly(*SaveFileName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntWriteToFile_F", "Couldn't write to file '{0}'. Maybe file is read-only?"), FText::FromString(SaveFileName)));
		}
		else
		{
			// We have a writeable file.  Now go through that list of exporters again and find the right exporter and use it.
			TArray<UExporter*>	ValidExporters;

			for (int32 ExporterIndex = 0; ExporterIndex < Exporters.Num(); ++ExporterIndex)
			{
				UExporter* Exporter = Exporters[ExporterIndex];
				if (Exporter->SupportsObject(ObjectToExport))
				{
					check(Exporter->FormatExtension.Num() == Exporter->FormatDescription.Num());
					for (int32 FormatIndex = 0; FormatIndex < Exporter->FormatExtension.Num(); ++FormatIndex)
					{
						const FString& FormatExtension = Exporter->FormatExtension[FormatIndex];
						if (FCString::Stricmp(*FormatExtension, *FPaths::GetExtension(SaveFileName)) == 0 ||
							FCString::Stricmp(*FormatExtension, TEXT("*")) == 0)
						{
							ValidExporters.Add(Exporter);
							break;
						}
					}
				}
			}

			// Handle the potential of multiple exporters being found
			UExporter* ExporterToUse = NULL;
			if (ValidExporters.Num() == 1)
			{
				ExporterToUse = ValidExporters[0];
			}
			else if (ValidExporters.Num() > 1)
			{
				// Set up the first one as default
				ExporterToUse = ValidExporters[0];

				// ...but search for a better match if available
				for (int32 ExporterIdx = 0; ExporterIdx < ValidExporters.Num(); ExporterIdx++)
				{
					if (ValidExporters[ExporterIdx]->GetClass()->GetFName() == ObjectToExport->GetExporterName())
					{
						ExporterToUse = ValidExporters[ExporterIdx];
						break;
					}
				}
			}

			// If an exporter was found, use it.
			if (ExporterToUse)
			{
				const FScopedBusyCursor BusyCursor;

				if (!UsedExporters.Contains(ExporterToUse))
				{
					ExporterToUse->SetBatchMode(ObjectsToExport.Num() > 1 && !bPromptIndividualFilenames);
					ExporterToUse->SetCancelBatch(false);
					ExporterToUse->SetShowExportOption(true);
					ExporterToUse->AddToRoot();
					UsedExporters.Add(ExporterToUse);
				}

				UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
				FGCObjectScopeGuard ExportTaskGuard(ExportTask);
				ExportTask->Object = ObjectToExport;
				ExportTask->Exporter = ExporterToUse;
				ExportTask->Filename = SaveFileName;
				ExportTask->bSelected = false;
				ExportTask->bReplaceIdentical = true;
				ExportTask->bPrompt = false;
				ExportTask->bUseFileArchive = ObjectToExport->IsA(UPackage::StaticClass());
				ExportTask->bWriteEmptyFiles = false;

				UExporter::RunAssetExportTask(ExportTask);

				if (ExporterToUse->GetBatchMode() && ExporterToUse->GetCancelBatch())
				{
					//Exit the export file loop when there is a cancel all
					break;
				}
			}
		}
	}

	//Set back the default value for the all used exporters
	for (UExporter* UsedExporter : UsedExporters)
	{
		UsedExporter->SetBatchMode(false);
		UsedExporter->SetCancelBatch(false);
		UsedExporter->SetShowExportOption(true);
		UsedExporter->RemoveFromRoot();
	}
	UsedExporters.Empty();

	if (bAnyObjectMissingSourceData)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Exporter_Error_SourceDataUnavailable", "No source data available for some objects.  See the log for details."));
	}

	GWarn->EndSlowTask();

	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, LastExportPath);
}

UAssetToolsImpl& UAssetToolsImpl::Get()
{
	FAssetToolsModule& Module = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	return static_cast<UAssetToolsImpl&>(Module.Get());
}

void UAssetToolsImpl::SyncBrowserToAssets(const TArray<UObject*>& AssetsToSync)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets( AssetsToSync, /*bAllowLockedBrowsers=*/true );
}

void UAssetToolsImpl::SyncBrowserToAssets(const TArray<FAssetData>& AssetsToSync)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets( AssetsToSync, /*bAllowLockedBrowsers=*/true );
}

bool UAssetToolsImpl::CheckForDeletedPackage(const UPackage* Package) const
{
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		if ( SourceControlProvider.IsAvailable() )
		{
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Package, EStateCacheUsage::ForceUpdate);
			if ( SourceControlState.IsValid() && SourceControlState->IsDeleted() )
			{
				// Creating an asset in a package that is marked for delete - revert the delete and check out the package
				if (!SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), Package))
				{
					// Failed to revert file which was marked for delete
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RevertDeletedFileFailed", "Failed to revert package which was marked for delete."));
					return false;
				}

				if (!SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), Package))
				{
					// Failed to check out file
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CheckOutFileFailed", "Failed to check out package"));
					return false;
				}
			}
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(LOCTEXT( "DeletingNoSCCConnection", "Could not check for deleted file. No connection to source control available!"));
			EditorErrors.Notify();
		}
	}

	return true;
}

bool UAssetToolsImpl::CanCreateAsset(const FString& AssetName, const FString& PackageName, const FText& OperationText) const
{
	// @todo: These 'reason' messages are not localized strings!
	FText Reason;
	if (!FName(*AssetName).IsValidObjectName( Reason )
		|| !FPackageName::IsValidLongPackageName( PackageName, /*bIncludeReadOnlyRoots=*/false, &Reason ) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, Reason );
		return false;
	}

	// We can not create assets that share the name of a map file in the same location
	if ( FEditorFileUtils::IsMapPackageAsset(PackageName) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("AssetNameInUseByMap", "You can not create an asset named '{0}' because there is already a map file with this name in this folder."), FText::FromString( AssetName ) ) );
		return false;
	}

	// Find (or create!) the desired package for this object
	UPackage* Pkg = CreatePackage(*PackageName);

	// Handle fully loading packages before creating new objects.
	TArray<UPackage*> TopLevelPackages;
	TopLevelPackages.Add( Pkg );
	if( !UPackageTools::HandleFullyLoadingPackages( TopLevelPackages, OperationText ) )
	{
		// User aborted.
		return false;
	}

	// We need to test again after fully loading.
	if (!FName(*AssetName).IsValidObjectName( Reason )
		||	!FPackageName::IsValidLongPackageName( PackageName, /*bIncludeReadOnlyRoots=*/false, &Reason ) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, Reason );
		return false;
	}

	// Check for an existing object
	UObject* ExistingObject = StaticFindObject( UObject::StaticClass(), Pkg, *AssetName );
	if( ExistingObject != nullptr )
	{
		// Object already exists in either the specified package or another package.  Check to see if the user wants
		// to replace the object.
		bool bWantReplace =
			EAppReturnType::Yes == FMessageDialog::Open(
				EAppMsgType::YesNo,
				EAppReturnType::No,
				FText::Format(
					NSLOCTEXT("UnrealEd", "ReplaceExistingObjectInPackage_F", "An object [{0}] of class [{1}] already exists in file [{2}].  Do you want to replace the existing object?  If you click 'Yes', the existing object will be deleted.  Otherwise, click 'No' and choose a unique name for your new object." ),
					FText::FromString(AssetName), FText::FromString(ExistingObject->GetClass()->GetName()), FText::FromString(PackageName) ) );

		if( bWantReplace )
		{
			// Replacing an object.  Here we go!
			// Delete the existing object
			bool bDeleteSucceeded = ObjectTools::DeleteSingleObject( ExistingObject );

			if (bDeleteSucceeded)
			{
				// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

				// Old package will be GC'ed... create a new one here
				Pkg = CreatePackage(*PackageName);
				Pkg->MarkAsFullyLoaded();
			}
			else
			{
				// Notify the user that the operation failed b/c the existing asset couldn't be deleted
				FMessageDialog::Open( EAppMsgType::Ok,	FText::Format( NSLOCTEXT("DlgNewGeneric", "ContentBrowser_CannotDeleteReferenced", "{0} wasn't created.\n\nThe asset is referenced by other content."), FText::FromString( AssetName ) ) );
			}

			if( !bDeleteSucceeded || !IsUniqueObjectName( *AssetName, Pkg, Reason ) )
			{
				// Original object couldn't be deleted
				return false;
			}
		}
		else
		{
			// User chose not to replace the object; they'll need to enter a new name
			return false;
		}
	}

	// Check for a package that was marked for delete in source control
	if ( !CheckForDeletedPackage(Pkg) )
	{
		return false;
	}

	return true;
}

void UAssetToolsImpl::PerformMigratePackages(TArray<FName> PackageNamesToMigrate, const FString DestinationPath, const FMigrationOptions Options) const
{
	// Form a full list of packages to move by including the dependencies of the supplied packages
	TSet<FName> AllPackageNamesToMove;
	TSet<FString> ExternalObjectsPaths;
	{
		FScopedSlowTask SlowTask( PackageNamesToMigrate.Num(), LOCTEXT( "MigratePackages_GatheringDependencies", "Gathering Dependencies..." ) );
		SlowTask.MakeDialog();

		for ( auto PackageIt = PackageNamesToMigrate.CreateConstIterator(); PackageIt; ++PackageIt )
		{
			SlowTask.EnterProgressFrame();

			if ( !AllPackageNamesToMove.Contains(*PackageIt) )
			{
				AllPackageNamesToMove.Add(*PackageIt);
				RecursiveGetDependencies(*PackageIt, AllPackageNamesToMove, ExternalObjectsPaths);
			}
		}
	}

	// Fetch the enabled plugins and their mount points
	TMap<FName, EPluginLoadedFrom> EnabledPluginToLoadedFrom;
	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPluginsWithContent();
	for (const TSharedRef<IPlugin>& EnabledPlugin : EnabledPlugins)
	{
		EnabledPluginToLoadedFrom.Add(FName(EnabledPlugin->GetMountedAssetPath()), EnabledPlugin->GetLoadedFrom());
	}

	// Find assets in non-Project Plugins
	TSet<FName> ShouldMigratePackage;
	bool bShouldShowEngineContent = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	{
		// This is the new list to prompt for migration
		TSet<FName> FilteredPackageNamesToMove;

		for (const FName& PackageName : AllPackageNamesToMove)
		{
			FName PackageMountPoint = FPackageName::GetPackageMountPoint(PackageName.ToString(), false);
			EPluginLoadedFrom* Found = EnabledPluginToLoadedFrom.Find(PackageMountPoint);

			bool bShouldMigratePackage = true;
			if (Found)
			 {
				// plugin content, decide if it's appropriate to migrate
				switch (*Found)
				{
				case EPluginLoadedFrom::Engine:
					if (!bShouldShowEngineContent)
					{
						continue;
					}
					bShouldMigratePackage = false;
					break;

				case EPluginLoadedFrom::Project:
					bShouldMigratePackage = true;
					break;
				 
				default:
					bShouldMigratePackage = false;
					break;
				 }
			 }
			 else
			 {
				// this is not plugin content
				if (PackageName.ToString().StartsWith(TEXT("/Engine")))
				{
					// Engine content
					if (!bShouldShowEngineContent)
					{
						continue;
					}
					bShouldMigratePackage = false;
				}
				else
				{
					// Game content
					bShouldMigratePackage = true;
				}
			}

			FilteredPackageNamesToMove.Add(PackageName);

			if (bShouldMigratePackage)
			{
				ShouldMigratePackage.Add(PackageName);
			}
		}

		AllPackageNamesToMove = FilteredPackageNamesToMove;
	}

	// Confirm that there is at least one package to move 
	if (AllPackageNamesToMove.Num() == 0)
	{
		if (!FApp::IsUnattended())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MigratePackages_NoFilesToMove", "No files were found to move"));
		}
		return;
	}

	TSharedPtr<TArray<ReportPackageData>> ReportPackages = MakeShareable(new TArray<ReportPackageData>);
	for (auto PackageIt = AllPackageNamesToMove.CreateConstIterator(); PackageIt; ++PackageIt)
	{
		bool bShouldMigratePackage = ShouldMigratePackage.Find(*PackageIt) != nullptr;
		ReportPackages.Get()->Add({ (*PackageIt).ToString(), bShouldMigratePackage });
	}
	// Prompt the user displaying all assets that are going to be migrated
	if(!FApp::IsUnattended() && Options.bPrompt)
	{
		const FText ReportMessage = LOCTEXT("MigratePackagesReportTitle", "The following assets will be migrated to another content folder.");
		SPackageReportDialog::FOnReportConfirmed OnReportConfirmed = SPackageReportDialog::FOnReportConfirmed::CreateUObject(this, &UAssetToolsImpl::MigratePackages_ReportConfirmed, ReportPackages, DestinationPath, Options);
		SPackageReportDialog::OpenPackageReportDialog(ReportMessage, *ReportPackages.Get(), OnReportConfirmed);
	}
	else
	{
		UAssetToolsImpl::MigratePackages_ReportConfirmed(ReportPackages, DestinationPath, Options);
	}
}

void UAssetToolsImpl::MigratePackages_ReportConfirmed(TSharedPtr<TArray<ReportPackageData>> PackageDataToMigrate, const FString DestinationPath, const FMigrationOptions Options) const
{
	FString DestinationFolder;
	if (FApp::IsUnattended() || !DestinationPath.IsEmpty())
	{
		if(DestinationPath.IsEmpty())
		{
			UE_LOG(LogAssetTools, Error, TEXT("Migration Destination path cannot be empty."));
			return;
		}

		DestinationFolder = DestinationPath;
		FPaths::NormalizeDirectoryName(DestinationFolder);
		DestinationFolder += TEXT("/");
	}
	else
	{
		// Choose a destination folder
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (ensure(DesktopPlatform))
		{
			const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

			const FString Title = LOCTEXT("MigrateToFolderTitle", "Choose a destination Content folder").ToString();
			bool bFolderAccepted = false;
			while (!bFolderAccepted)
			{
				const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
					ParentWindowWindowHandle,
					Title,
					FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT),
					DestinationFolder
				);

				if (!bFolderSelected)
				{
					// User canceled, return
					return;
				}

				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, DestinationFolder);
				FPaths::NormalizeFilename(DestinationFolder);
				if (!DestinationFolder.EndsWith(TEXT("/")))
				{
					DestinationFolder += TEXT("/");
				}

				// Verify that it is a content folder
				if (DestinationFolder.EndsWith(TEXT("/Content/")))
				{
					bFolderAccepted = true;
				}
				else
				{
					// The user chose a non-content folder. Let them know they cannot do that.
					const FText Message = FText::Format(LOCTEXT("MigratePackages_NonContentFolder", "{0} does not appear to be a game Content folder. Migrated content only work if placed in a Content folder. Select a Content folder."), FText::FromString(DestinationFolder));
					EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::OkCancel, Message);
					if (Response == EAppReturnType::Cancel)
					{
						return;
					}
				}
			}
		}
		else
		{
			// Not on a platform that supports desktop functionality
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoDesktopPlatform", "Error: This platform does not support a file dialog."));
			return;
		}
	}

	// Build a list of packages to handle
	TSet<FName> AllPackageNamesToMove;
	for (auto PackageDataIt = PackageDataToMigrate->CreateConstIterator(); PackageDataIt; ++PackageDataIt)
	{
		if (PackageDataIt->bShouldMigratePackage)
		{
			AllPackageNamesToMove.Add(FName(PackageDataIt->Name));
		}
	}

	FMessageLog MigrateLog("AssetTools");

	// Determine if the destination is a project content folder or a plugin content folder
	TArray<FString> ProjectFiles;
	IFileManager::Get().FindFiles(ProjectFiles, *(DestinationFolder + TEXT("../")), TEXT("uproject"));
	bool bIsDestinationAProject = !ProjectFiles.IsEmpty();

	if (UE::AssetTools::Private::bUseNewPackageMigration)
	{
		using namespace UE::AssetTools::Private;

		FString MountPointRootPath = FPackageMigrationImpl::GetMountPointRootPath(bIsDestinationAProject, DestinationFolder);

		if (!MountPointRootPath.IsEmpty())
		{

			FPackageMigrationImpl::FPackageMigrationImplContext MigrationImplContext;
			MigrationImplContext.Options = Options;

			// 1) Create the package migration state. It will create the mount point also if needed
			UE::AssetTools::FPackageMigrationContext PackageMigrationContext(UE::AssetTools::FPackageMigrationContext::FScopedMountPoint(MoveTemp(MountPointRootPath), MoveTemp(DestinationFolder)));

			PackageMigrationContext.CurrentStep =  UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::BeginMigration;
			OnPackageMigration.Broadcast(PackageMigrationContext);

			// 2) Set up the instancing context so that the packages are load in the destination mount point and setup the packages migration data
			{
				PackageMigrationContext.MigrationPackagesData.Reserve(PackageDataToMigrate->Num());

				TArray<TPair<const ReportPackageData*, const FAssetData>> ExternalPackageDatas;
				TMap<const FStringView, int32> ExistingPackageNameToMigrationDataIndex;

				FPackageMigrationImpl::SetupPublicAssetPackagesMigrationData(PackageDataToMigrate
					, ExternalPackageDatas
					, ExistingPackageNameToMigrationDataIndex
					, PackageMigrationContext
					, MigrationImplContext
					, MigrateLog);

				PackageMigrationContext.CurrentStep =  UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::PostAssetMigrationPackageDataCreated;
				OnPackageMigration.Broadcast(PackageMigrationContext);


				FPackageMigrationImpl::SetupExternalAssetPackagesMigrationData(ExternalPackageDatas
					, ExistingPackageNameToMigrationDataIndex
					, PackageMigrationContext
					, MigrationImplContext);

				PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::PostExternalMigrationPackageDataCreated;
				OnPackageMigration.Broadcast(PackageMigrationContext);

			}

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::InTheWayPackagesMoved;
			OnPackageMigration.Broadcast(PackageMigrationContext);


			// 3) Instanced load
			TArray<TWeakObjectPtr<UPackage>> PackagesToClean;
			PackagesToClean.Reserve(PackageMigrationContext.MigrationPackagesData.Num());
			FPackageMigrationImpl::CreatePackagesAndSetupLinkers(PackageMigrationContext, MigrationImplContext, PackagesToClean);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::InstancedPackagesCreated;
			OnPackageMigration.Broadcast(PackageMigrationContext);

			FPackageMigrationImpl::LoadInstancedPackages(PackageMigrationContext, MigrationImplContext);

			// Collect the garbage created by the load here and it also help detecting potential issues in migration (which can cause crashes).
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::InstancedPackagesLoaded;
			OnPackageMigration.Broadcast(PackageMigrationContext);


			// 4) Saves
			FPackageMigrationImpl::SaveInstancedPackagesIntoDestination(PackageMigrationContext, MigrationImplContext);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::InstancedPackagesSaved;
			OnPackageMigration.Broadcast(PackageMigrationContext);


			// 5) Remove the packages created for the migration
			FPackageMigrationImpl::CleanInstancedPackages(PackagesToClean);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::PostCleaningInstancedPackages;
			OnPackageMigration.Broadcast(PackageMigrationContext);

			FPackageMigrationImpl::RestoreInTheWayPackages(PackageMigrationContext);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::InstancedPackagesSaved;
			OnPackageMigration.Broadcast(PackageMigrationContext);

			FPackageMigrationImpl::BuildAndNotifyLogWindow(PackageMigrationContext, MigrateLog, MigrationImplContext);

			PackageMigrationContext.CurrentStep = UE::AssetTools::FPackageMigrationContext::EPackageMigrationStep::EndMigration;
			OnPackageMigration.Broadcast(PackageMigrationContext);
		}
	}
	else
	{
		bool bAbort = false;

		// Associate each Content folder in the target Plugin hierarchy to a content root string in UFS
		TMap<FName, FString> DestContentRootsToFolders;

		// Assets in /Game always map directly to the destination
		DestContentRootsToFolders.Add(FName(TEXT("/Game")), DestinationFolder);

		// If our destination is a project, it could have plugins...
		if (bIsDestinationAProject)
		{
			// Find all "Content" folders under the destination ../Plugins directory
			TArray<FString> ContentFolders;
			IFileManager::Get().FindFilesRecursive(ContentFolders, *(DestinationFolder + TEXT("../Plugins/")), TEXT("Content"), false, true);

			for (const FString& Folder : ContentFolders)
			{
				// Parse the parent folder of .../Content from "Folder"
				FString Path, Content, Root;
				bool bSplitContent = Folder.Split(TEXT("/"), &Path, &Content, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				bool bSplitParentPath = Path.Split(TEXT("/"), nullptr, &Root, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (!bSplitContent || !bSplitParentPath)
				{
					MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_NoMountPointFolder", "Unable to determine mount point for folder {0}"), FText::FromString(Folder)));
					bAbort = true;
					continue;
				}

				// Determine this folder name to be a content root in the destination
				FString DestContentRoot = FString(TEXT("/")) + Root;
				FString DestContentFolder = FPaths::ConvertRelativePathToFull(Folder + TEXT("/"));
				DestContentRootsToFolders.Add(FName(DestContentRoot), DestContentFolder);
			}
		}

		if (bAbort)
		{
			MigrateLog.Notify();
			return;
		}

		// Check if the root of any of the packages to migrate have no destination
		TArray<FName> LostPackages;
		TSet<FName> LostPackageRoots;
		for (const FName& PackageName : AllPackageNamesToMove)
		{
			// Acquire the mount point for this package
			FString Folder = TEXT("/") + FPackageName::GetPackageMountPoint(PackageName.ToString()).ToString();

			// If this is /Game package, it doesn't need special handling and is simply bound for the destination, continue
			if (Folder == TEXT("/Game"))
			{
				continue;
			}

			// Resolve the disk folder of this package's mount point so we compare directory names directly
			//  We do this FileSystem to FileSystem compare instead of mount point (UFS) to content root (FileSystem)
			//  The mount point name likely comes from the FriendlyName in the uplugin, or the uplugin basename
			//  What's important here is that we succeed in finding _the same plugin_ between a copy/pasted project
			if (!FPackageName::TryConvertLongPackageNameToFilename(Folder, Folder))
			{
				MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_NoContentFolder", "Unable to determine content folder for asset {0}"), FText::FromString(PackageName.ToString())));
				bAbort = true;
				continue;
			}
			Folder.RemoveFromEnd(TEXT("/"));

			// Parse the parent folder of .../Content from "Folder"
			FString Path, Content, Root;
			bool bSplitContent = Folder.Split(TEXT("/"), &Path, &Content, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			bool bSplitParentPath = Path.Split(TEXT("/"), nullptr, &Root, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (!bSplitContent || !bSplitParentPath)
			{
				MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_NoMountPointPackage", "Unable to determine mount point for package {0}"), FText::FromString(PackageName.ToString())));
				bAbort = true;
				continue;
			}

			// Check to see if the content root exists in the destination, otherwise it's "Lost"
			FString SrcContentRoot = FString(TEXT("/")) + Root;
			FName SrcContentRootName(SrcContentRoot);
			if (!DestContentRootsToFolders.Find(SrcContentRootName))
			{
				LostPackages.Add(PackageName);
				LostPackageRoots.Add(SrcContentRootName);
			}
		}

		if (bAbort)
		{
			MigrateLog.Notify();
			return;
		}

		// If some packages don't have a matching content root in the destination, prompt for desired behavior
		if (!LostPackages.IsEmpty())
		{
			if (!FApp::IsUnattended() && Options.bPrompt && Options.OrphanFolder.IsEmpty())
			{
				FString LostPackageRootsString;
				for (const FName& PackageRoot : LostPackageRoots)
				{
					LostPackageRootsString += FString(TEXT("\n\t")) + PackageRoot.ToString();
				}

				// Prompt to consolidate to a migration folder
				FText Prompt = FText::Format(LOCTEXT("MigratePackages_ConsolidateToTemp", "Some selected assets don't have a corresponding content root in the destination.{0}\n\nWould you like to save a copy of all selected assets into a folder with consolidated references? If you select No then assets in the above roots will not be migrated."), FText::FromString(LostPackageRootsString));
				switch (FMessageDialog::Open(EAppMsgType::YesNoCancel, Prompt))
				{
				case EAppReturnType::Yes:
					// No op
					break;
				case EAppReturnType::No:
					LostPackages.Reset();
					break;
				case EAppReturnType::Cancel:
					return;
				}
			}
			else if (Options.OrphanFolder.IsEmpty())
			{
				LostPackages.Reset();
			}
		}

		// This will be used to tidy up the temp folder after we copy the migrated assets
		FString SrcDiskFolderFilename;

		// Fixing up references requires resaving packages to a temporary location
		if (!LostPackages.IsEmpty())
		{
			// Query the user for a folder to migrate assets into. This folder will exist temporary in this project, and is the destination for migration in the target project
			FString FolderName = Options.OrphanFolder.IsEmpty() ? TEXT("Migrated") : Options.OrphanFolder;
			if (!FApp::IsUnattended() && Options.bPrompt)
			{
				bool bIsOkButtonEnabled = true;
				TSharedRef<SEditableTextBox> EditableTextBox = SNew(SEditableTextBox)
					.Text(FText::FromString(FolderName))
					.OnVerifyTextChanged_Lambda([&bIsOkButtonEnabled](const FText& InNewText, FText& OutErrorMessage) -> bool
						{
							if (InNewText.ToString().Contains(TEXT("/")))
							{
								OutErrorMessage = LOCTEXT("Migrated_CannotContainSlashes", "Cannot use a slash in a folder name.");

								// Disable Ok if the string is invalid
								bIsOkButtonEnabled = false;
								return false;
							}

							// Enable Ok if the string is valid
							bIsOkButtonEnabled = true;
							return true;
						})
					.OnTextCommitted_Lambda([&FolderName](const FText& NewValue, ETextCommit::Type)
						{
							// Set the result if they modified the text
							FolderName = NewValue.ToString();
						});

				// Set the result if they just click Ok
				SGenericDialogWidget::FArguments FolderDialogArguments;
				FolderDialogArguments.OnOkPressed_Lambda([&EditableTextBox, &FolderName]()
					{
						FolderName = EditableTextBox->GetText().ToString();
					});

				// Present the Dialog
				SGenericDialogWidget::OpenDialog(LOCTEXT("MigratePackages_FolderName", "Folder for Migrated Assets"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MigratePackages_SpecifyConsolidateFolder", "Please specify a new folder name to consolidate the assets into."))
					]
					+ SVerticalBox::Slot()
					.Padding(5.0f)
					[
						SNew(SSpacer)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5.0f)
					[
						EditableTextBox
					],
					FolderDialogArguments, true);

				// Sanity the user input
				if (FolderName.IsEmpty())
				{
					return;
				}
			}
			// Remove forbidden characters
			FolderName = FolderName.Replace(TEXT("/"), TEXT(""));

			// Verify that we don't have any assets that exist where we want to perform our consolidation
			//  We could do the same check at the destination, but the package copy code will happily overwrite,
			//  and it seems reasonable someone would want to Migrate several times if snapshotting changes from another project.
			//  So for now we don't necessitate that the destination content folder be missing as well.
			FString SrcUfsFolderName = FolderName;
			SrcUfsFolderName.InsertAt(0, TEXT("/Game/"));
			SrcDiskFolderFilename = FPackageName::LongPackageNameToFilename(SrcUfsFolderName, TEXT(""));
			if (IFileManager::Get().DirectoryExists(*SrcDiskFolderFilename))
			{
				const FText Message = FText::Format(LOCTEXT("MigratePackages_InvalidMigrateFolder", "{0} exists on disk in the source project, and cannot be used to consolidate assets."), FText::FromString(SrcDiskFolderFilename));
				if (!FApp::IsUnattended() && Options.bPrompt)
				{
					EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::Ok, Message);
				}
				else
				{
					UE_LOG(LogAssetTools, Error, TEXT("%s"), *Message.ToString());
				}
				return;
			}

			// Resolve the packages to migrate to assets
			TArray<UObject*> SrcObjects;
			{
				FScopedSlowTask SlowTask(AllPackageNamesToMove.Num(), LOCTEXT("MigratePackages_Loading", "Loading Packages..."));
				SlowTask.MakeDialog();
				for (const FName& SrcPackage : AllPackageNamesToMove)
				{
					SlowTask.EnterProgressFrame();
					UPackage* LoadedPackage = UPackageTools::LoadPackage(SrcPackage.ToString());
					if (!LoadedPackage)
					{
						MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_FailedToLoadPackage", "Failed to load package {0}"), FText::FromString(SrcPackage.ToString())));
						bAbort = true;
						continue;
					}

					UObject* Object = LoadedPackage->FindAssetInPackage();
					if (Object)
					{
						SrcObjects.Add(Object);
					}
					else
					{
						MigrateLog.Warning(FText::Format(LOCTEXT("MigratePackages_PackageHasNoAsset", "Package {0} has no asset in it"), FText::FromString(SrcPackage.ToString())));
					}

					UBlueprint* Blueprint = Cast<UBlueprint>(Object);
					if (Blueprint && (!Blueprint->SkeletonGeneratedClass || !Blueprint->GeneratedClass))
					{
						MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_InvalidBluepriint", "Blueprint is invalid in package {0}"), FText::FromString(SrcPackage.ToString())));
						bAbort = true;
						continue;
					}
				}
			}

			if (bAbort)
			{
				MigrateLog.Notify();
				return;
			}

			// To handle complex references and assets in different Plugins, we must first duplicate to temp packages
			TArray<UObject*> TempObjects;
			TMap<UObject*, UObject*> ReplacementMap;
			{
				FScopedSlowTask SlowTask(SrcObjects.Num(), LOCTEXT("MigratePackages_Duplicate", "Duplicating Assets..."));
				SlowTask.MakeDialog();

				TSet<UPackage*> PackagesUserRefusedToFullyLoad;
				for (int i=0; i<SrcObjects.Num(); ++i)
				{
					SlowTask.EnterProgressFrame();

					UObject* Object = SrcObjects[i];
					FString PackageName = Object->GetPackage()->GetName();

					FString Path, Root;
					PackageName.RemoveFromStart(TEXT("/"));
					bool bSplitRoot = PackageName.Split(TEXT("/"), &Root, &Path);
					if (!bSplitRoot)
					{
						MigrateLog.Error(FText::Format(LOCTEXT("MigratePackages_NoMountPointPackage", "Unable to determine mount point for package {0}"), FText::FromString(PackageName)));
						continue;
					}

					ObjectTools::FPackageGroupName PackageGroupName;
					PackageGroupName.ObjectName = Object->GetName();
					PackageGroupName.PackageName = SrcUfsFolderName + TEXT("/") + Path;
					FString GroupName = Object->GetFullGroupName(/*bStartWithOuter =*/true);
					if (GroupName != TEXT("None"))
					{
						PackageGroupName.GroupName = GroupName;
					}

					UObject* NewObject = ObjectTools::DuplicateSingleObject(Object, PackageGroupName, PackagesUserRefusedToFullyLoad);
					if (NewObject)
					{
						TempObjects.Add(NewObject);
						ReplacementMap.Add(SrcObjects[i], TempObjects[i]);
					}
				}
			}

			// Update references between TempObjects (to reference each other)
			{
				FScopedSlowTask SlowTask(TempObjects.Num(), LOCTEXT("MigratePackages_ReplaceReferences", "Replacing References..."));
				SlowTask.MakeDialog();

				for (int i=0; i<TempObjects.Num(); ++i)
				{
					SlowTask.EnterProgressFrame();
					UObject* TempObject = TempObjects[i];

					FArchiveReplaceObjectRef<UObject> ReplaceAr(TempObject, ReplacementMap, EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
				}
			}

			// Save fixed up packages to the migrated folder, and update the set of files to copy to be those migrated packages
			{
				FScopedSlowTask SlowTask(TempObjects.Num(), LOCTEXT("MigratePackages_ReplaceReferences", "Replacing References..."));
				SlowTask.MakeDialog();

				TSet<FName> NewPackageNamesToMove;
				for (int i=0; i<TempObjects.Num(); ++i)
				{
					SlowTask.EnterProgressFrame();
					UObject* TempObject = TempObjects[i];

					// Calculate the file path to the new, migrated package
					FString const TempPackageName = TempObject->GetPackage()->GetName();
					FString const TempPackageFilename = FPackageName::LongPackageNameToFilename(TempPackageName, FPackageName::GetAssetPackageExtension());

					// Save it
					FSavePackageArgs SaveArgs;
					GEditor->Save(TempObject->GetPackage(), /*InAsset=*/nullptr, *TempPackageFilename, SaveArgs);

					NewPackageNamesToMove.Add(FName(TempPackageName));
				}

				AllPackageNamesToMove = NewPackageNamesToMove;
			}
		}

		bool bUserCanceled = false;

		EAppReturnType::Type LastResponse = EAppReturnType::Yes;
		if (!Options.bPrompt)
		{
			switch (Options.AssetConflict)
			{
				case EAssetMigrationConflict::Overwrite:
					LastResponse = EAppReturnType::YesAll;
					break;
				case EAssetMigrationConflict::Skip:
					LastResponse = EAppReturnType::NoAll;
					break;
				case EAssetMigrationConflict::Cancel:
					LastResponse = EAppReturnType::Cancel;
					break;
			}
		}
		TArray<FString> SuccessfullyCopiedFiles;
		TArray<FString> SuccessfullyCopiedPackages;
		FString CopyErrors;

		{
			FScopedSlowTask SlowTask(AllPackageNamesToMove.Num(), LOCTEXT("MigratePackages_CopyingFiles", "Copying Files..."));
			SlowTask.MakeDialog();

			for ( const FName& PackageNameToMove : AllPackageNamesToMove )
			{
				SlowTask.EnterProgressFrame();

				const FString& PackageName = PackageNameToMove.ToString();
				FString SrcFilename;
			
				if (!FPackageName::DoesPackageExist(PackageName, &SrcFilename))
				{
					const FText ErrorMessage = FText::Format(LOCTEXT("MigratePackages_PackageMissing", "{0} does not exist on disk."), FText::FromString(PackageName));
					UE_LOG(LogAssetTools, Warning, TEXT("%s"), *ErrorMessage.ToString());
					CopyErrors += ErrorMessage.ToString() + LINE_TERMINATOR;
				}
				else if (SrcFilename.Contains(FPaths::EngineContentDir()))
				{
					const FString LeafName = SrcFilename.Replace(*FPaths::EngineContentDir(), TEXT("Engine/"));
					CopyErrors += FText::Format(LOCTEXT("MigratePackages_EngineContent", "Unable to migrate Engine asset {0}. Engine assets cannot be migrated."), FText::FromString(LeafName)).ToString() + LINE_TERMINATOR;
				}
				else
				{
					bool bFileOKToCopy = true;

					FString Path = PackageNameToMove.ToString();
					FString PackageRoot;
					Path.RemoveFromStart(TEXT("/"));
					Path.Split("/", &PackageRoot, &Path, ESearchCase::IgnoreCase, ESearchDir::FromStart);
					PackageRoot = TEXT("/") + PackageRoot;

					FString DestFilename;
					TMap<FName, FString>::ValueType* DestRootFolder = DestContentRootsToFolders.Find(FName(PackageRoot));
					if (ensure(DestRootFolder))
					{
						DestFilename = *DestRootFolder;
					}
					else
					{
						continue;
					}

					FString SubFolder;
					if ( SrcFilename.Split( TEXT("/Content/"), nullptr, &SubFolder ) )
					{
						DestFilename += *SubFolder;

						if ( IFileManager::Get().FileSize(*DestFilename) > 0 )
						{
							// The destination file already exists! Ask the user what to do.
							EAppReturnType::Type Response;
							if (FApp::IsUnattended() || !Options.bPrompt || LastResponse == EAppReturnType::YesAll || LastResponse == EAppReturnType::NoAll)
							{
								Response = LastResponse;
							}
							else
							{
								const FText Message = FText::Format( LOCTEXT("MigratePackages_AlreadyExists", "An asset already exists at location {0} would you like to overwrite it?"), FText::FromString(DestFilename) );
								Response = FMessageDialog::Open( EAppMsgType::YesNoYesAllNoAllCancel, Message );
								LastResponse = Response;
							}

							if (Response == EAppReturnType::Cancel)
							{
								// The user chose to cancel mid-operation. Break out.
								bUserCanceled = true;
								break;
							}

							const bool bWantOverwrite = Response == EAppReturnType::Yes || Response == EAppReturnType::YesAll;
							if( !bWantOverwrite )
							{
								// User chose not to replace the package
								bFileOKToCopy = false;
							}
						}
					}
					else
					{
						// Couldn't find Content folder in source path
						bFileOKToCopy = false;
					}

					if ( bFileOKToCopy )
					{
						if ( IFileManager::Get().Copy(*DestFilename, *SrcFilename) == COPY_OK )
						{
							SuccessfullyCopiedPackages.Add(PackageName);
							SuccessfullyCopiedFiles.Add(DestFilename);
						}
						else
						{
							UE_LOG(LogAssetTools, Warning, TEXT("Failed to copy %s to %s while migrating assets"), *SrcFilename, *DestFilename);
							CopyErrors += SrcFilename + LINE_TERMINATOR;
						}
					}
				}
			}
		}

		// If we are consolidating lost packages, we are copying temporary packages, so clean them up.
		if (!LostPackages.IsEmpty())
		{
			FScopedSlowTask SlowTask(AllPackageNamesToMove.Num(), LOCTEXT("MigratePackages_CleaningUp", "Cleaning Up..."));
			SlowTask.MakeDialog();

			TArray<UObject*> ObjectsToDelete;
			for (const FName& PackageNameToMove : AllPackageNamesToMove)
			{
				SlowTask.EnterProgressFrame();

				UPackage* Package = UPackageTools::LoadPackage(PackageNameToMove.ToString());
				if (Package)
				{
					ObjectsToDelete.Add(Package);
				}
			}

			ObjectTools::ForceDeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

			if (!IFileManager::Get().DeleteDirectory(*SrcDiskFolderFilename, /*RequireExists =*/ false, /*Tree =*/ true))
			{
				UE_LOG(LogAssetTools, Warning, TEXT("Failed to delete temporary directory %s while migrating assets"), *SrcDiskFolderFilename);
				CopyErrors += SrcDiskFolderFilename + LINE_TERMINATOR;
			}
		}

		FString SourceControlErrors;

		if ( !bUserCanceled && SuccessfullyCopiedFiles.Num() > 0 )
		{
			// attempt to add files to source control (this can quite easily fail, but if it works it is very useful)
			if(GetDefault<UEditorLoadingSavingSettings>()->bSCCAutoAddNewFiles)
			{
				if(ISourceControlModule::Get().IsEnabled())
				{
					ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
					if(SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SuccessfullyCopiedFiles) == ECommandResult::Failed)
					{
						FScopedSlowTask SlowTask(SuccessfullyCopiedFiles.Num(), LOCTEXT("MigratePackages_AddToSourceControl", "Adding To Source Control..."));
						SlowTask.MakeDialog();

						for(auto FileIt(SuccessfullyCopiedFiles.CreateConstIterator()); FileIt; FileIt++)
						{
							SlowTask.EnterProgressFrame();
							if(!SourceControlProvider.GetState(*FileIt, EStateCacheUsage::Use)->IsAdded())
							{
								SourceControlErrors += FText::Format(LOCTEXT("MigratePackages_SourceControlError", "{0} could not be added to source control"), FText::FromString(*FileIt)).ToString();
								SourceControlErrors += LINE_TERMINATOR;
							}
						}
					}
				}
			}
		}

		FText LogMessage = FText::FromString(TEXT("Content migration completed successfully!"));
		EMessageSeverity::Type Severity = EMessageSeverity::Info;
		if ( CopyErrors.Len() > 0 || SourceControlErrors.Len() > 0 )
		{
			FString ErrorMessage;
			Severity = EMessageSeverity::Error;
			if( CopyErrors.Len() > 0 )
			{
				MigrateLog.NewPage( LOCTEXT("MigratePackages_CopyErrorsPage", "Copy Errors") );
				MigrateLog.Error(FText::FromString(*CopyErrors));
				ErrorMessage += FText::Format(LOCTEXT( "MigratePackages_CopyErrors", "Copied {0} files. Some content could not be copied."), FText::AsNumber(SuccessfullyCopiedPackages.Num())).ToString();
			}
			if( SourceControlErrors.Len() > 0 )
			{
				MigrateLog.NewPage( LOCTEXT("MigratePackages_SourceControlErrorsListPage", "Source Control Errors") );
				MigrateLog.Error(FText::FromString(*SourceControlErrors));
				ErrorMessage += LINE_TERMINATOR;
				ErrorMessage += LOCTEXT( "MigratePackages_SourceControlErrorsList", "Some files reported source control errors.").ToString();
			}
			if ( SuccessfullyCopiedPackages.Num() > 0 )
			{
				MigrateLog.NewPage( LOCTEXT("MigratePackages_CopyErrorsSuccesslistPage", "Copied Successfully") );
				MigrateLog.Info(FText::FromString(*SourceControlErrors));
				ErrorMessage += LINE_TERMINATOR;
				ErrorMessage += LOCTEXT( "MigratePackages_CopyErrorsSuccesslist", "Some files were copied successfully.").ToString();
				for ( auto FileIt = SuccessfullyCopiedPackages.CreateConstIterator(); FileIt; ++FileIt )
				{
					if(FileIt->Len()>0)
					{
						MigrateLog.Info(FText::FromString(*FileIt));
					}
				}
			}
			LogMessage = FText::FromString(ErrorMessage);
		}
		else if (bUserCanceled)
		{
			LogMessage = LOCTEXT("MigratePackages_CanceledPage", "Content migration was canceled.");
		}
		else if ( !bUserCanceled )
		{
			MigrateLog.NewPage( LOCTEXT("MigratePackages_CompletePage", "Content migration completed successfully!") );
			for ( auto FileIt = SuccessfullyCopiedPackages.CreateConstIterator(); FileIt; ++FileIt )
			{
				if(FileIt->Len()>0)
				{
					MigrateLog.Info(FText::FromString(*FileIt));
				}
			}
		}
		MigrateLog.Notify(LogMessage, Severity, true);
	}
}

void UAssetToolsImpl::RecursiveGetDependencies(const FName& PackageName, TSet<FName>& AllDependencies, TSet<FString>& OutExternalObjectsPaths) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FName> Dependencies;
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetDependencies(PackageName, Dependencies);
	
	for ( auto DependsIt = Dependencies.CreateConstIterator(); DependsIt; ++DependsIt )
	{
		FString DependencyName = (*DependsIt).ToString();

		const bool bIsScriptPackage = DependencyName.StartsWith(TEXT("/Script"));

		// The asset registry can give some reference to some deleted assets. We don't want to migrate these.
		const bool bAssetExist = AssetRegistry.GetAssetPackageDataCopy(*DependsIt).IsSet();

		if (!bIsScriptPackage && bAssetExist)
		{
			if (!AllDependencies.Contains(*DependsIt))
			{
				AllDependencies.Add(*DependsIt);
				RecursiveGetDependencies(*DependsIt, AllDependencies, OutExternalObjectsPaths);
			}
		}
	}

	// Handle Specific External Objects use case (only used for the Migrate path for now)
	// todo: revisit how to handle those in a more generic way. Should the FWorldPartitionAssetDependencyGatherer handle the external objects reference also?
	TArray<FAssetData> Assets;

	// The migration only work on the saved version of the assets so no need to scan the for the in memory only assets. This also greatly improve the performance of the migration when a lot assets are loaded in the editor.
	const bool bOnlyIncludeOnDiskAssets = true;
	if (AssetRegistryModule.Get().GetAssetsByPackageName(PackageName, Assets, bOnlyIncludeOnDiskAssets))
	{
		for (const FAssetData& AssetData : Assets)
		{
			if (AssetData.GetClass() && AssetData.GetClass()->IsChildOf<UWorld>())
			{
				TArray<FString> ExternalObjectsPaths = ULevel::GetExternalObjectsPaths(PackageName.ToString());
				for (const FString& ExternalObjectsPath : ExternalObjectsPaths)
				{
					if (!ExternalObjectsPath.IsEmpty() && !OutExternalObjectsPaths.Contains(ExternalObjectsPath))
					{
						OutExternalObjectsPaths.Add(ExternalObjectsPath);
						AssetRegistryModule.Get().ScanPathsSynchronous({ ExternalObjectsPath }, /*bForceRescan*/true, /*bIgnoreDenyListScanFilters*/true);

						TArray<FAssetData> ExternalObjectAssets;
						AssetRegistryModule.Get().GetAssetsByPath(FName(*ExternalObjectsPath), ExternalObjectAssets, /*bRecursive*/true, bOnlyIncludeOnDiskAssets);

						for (const FAssetData& ExternalObjectAsset : ExternalObjectAssets)
						{
							AllDependencies.Add(ExternalObjectAsset.PackageName);
							RecursiveGetDependencies(ExternalObjectAsset.PackageName, AllDependencies, OutExternalObjectsPaths);
						}
					}
				}
			}
		}
	}
}

void UAssetToolsImpl::RecursiveGetDependenciesAdvanced(const FName& PackageName, FAdvancedCopyParams& CopyParams, TArray<FName>& AllDependencies, TMap<FName, FName>& DependencyMap, const UAdvancedCopyCustomization* CopyCustomization, TArray<FAssetData>& OptionalAssetData) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FName> Dependencies;
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	// We found an asset
	if (OptionalAssetData.Num() > 0)
	{
		AssetRegistryModule.Get().GetDependencies(PackageName, Dependencies);
		for (const FName& Dep : Dependencies)
		{
			if (!AllDependencies.Contains(Dep) && FPackageName::IsValidLongPackageName(Dep.ToString(), false))
			{
				TArray<FAssetData> DependencyAssetData;
				AssetRegistry.GetAssetsByPackageName(Dep, DependencyAssetData, true);
				FARFilter ExclusionFilter = CopyCustomization->GetARFilter();
				AssetRegistry.UseFilterToExcludeAssets(DependencyAssetData, ExclusionFilter);
				if (DependencyAssetData.Num() > 0)
				{
					AllDependencies.Add(Dep);
					DependencyMap.Add(Dep, PackageName);
					RecursiveGetDependenciesAdvanced(Dep, CopyParams, AllDependencies, DependencyMap, CopyCustomization, DependencyAssetData);
				}
			}
		}
	}
	else
	{
		TArray<FAssetData> PathAssetData;
		// We found a folder containing assets
		if (AssetRegistry.HasAssets(PackageName) && AssetRegistry.GetAssetsByPath(PackageName, PathAssetData))
		{

			FARFilter ExclusionFilter = CopyCustomization->GetARFilter();
			AssetRegistry.UseFilterToExcludeAssets(PathAssetData, ExclusionFilter);
			for(const FAssetData& Asset : PathAssetData)
			{
				AllDependencies.Add(*Asset.GetPackage()->GetName());
				// If we should check the assets we found for dependencies
				if (CopyParams.bShouldCheckForDependencies)
				{
					RecursiveGetDependenciesAdvanced(FName(*Asset.GetPackage()->GetName()), CopyParams, AllDependencies, DependencyMap, CopyCustomization, PathAssetData);
				}
			}
		}
		
		// Always get subpaths
		{
			TArray<FString> SubPaths;
			AssetRegistry.GetSubPaths(PackageName.ToString(), SubPaths, false);
			for (const FString& SubPath : SubPaths)
			{
				TArray<FAssetData> EmptyArray;
				RecursiveGetDependenciesAdvanced(FName(*SubPath), CopyParams, AllDependencies, DependencyMap, CopyCustomization, EmptyArray);
			}

		}
	}
}

void UAssetToolsImpl::FixupReferencers(const TArray<UObjectRedirector*>& Objects, bool bCheckoutDialogPrompt, ERedirectFixupMode FixupMode) const
{
	AssetFixUpRedirectors->FixupReferencers(Objects, bCheckoutDialogPrompt, FixupMode);
}

bool UAssetToolsImpl::IsFixupReferencersInProgress() const
{
	return AssetFixUpRedirectors->IsFixupReferencersInProgress();
}

void UAssetToolsImpl::OpenEditorForAssets(const TArray<UObject*>& Assets)
{
#if WITH_EDITOR
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(Assets);
#endif
}

void UAssetToolsImpl::ConvertVirtualTextures(const TArray<UTexture2D *>& Textures, bool bConvertBackToNonVirtual, const TArray<UMaterial *>* RelatedMaterials /* = nullptr */) const
{
	FVTConversionWorker VirtualTextureConversionWorker(bConvertBackToNonVirtual);
	VirtualTextureConversionWorker.UserTextures = Textures;
	//We want all given texture to be added, so we put a minimum texture size of 0
	VirtualTextureConversionWorker.FilterList(0);
	if (RelatedMaterials)
	{
		VirtualTextureConversionWorker.Materials.Append(*RelatedMaterials);
	}

	VirtualTextureConversionWorker.DoConvert();
}

void UAssetToolsImpl::BeginAdvancedCopyPackages(const TArray<FName>& InputNamesToCopy, const FString& TargetPath) const
{
	// Packages must be saved for the migration to work
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
		if (AssetRegistry.IsLoadingAssets())
		{
			// Open a dialog asking the user to wait while assets are being discovered
			SDiscoveringAssetsDialog::OpenDiscoveringAssetsDialog(
				SDiscoveringAssetsDialog::FOnAssetsDiscovered::CreateUObject(this, &UAssetToolsImpl::PerformAdvancedCopyPackages, InputNamesToCopy, TargetPath)
			);
		}
		else
		{
			// Assets are already discovered, perform the migration now
			PerformAdvancedCopyPackages(InputNamesToCopy, TargetPath);
		}
	}
}

TArray<FName> UAssetToolsImpl::ExpandAssetsAndFoldersToJustAssets(TArray<FName> SelectedAssetAndFolderNames) const
{
	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();

	TSet<FName> ExpandedAssets;
	for (auto NameIt = SelectedAssetAndFolderNames.CreateIterator(); NameIt; ++NameIt)
	{
		FName OriginalName = *NameIt;
		const FString& OriginalNameString = OriginalName.ToString();
		if (!FPackageName::DoesPackageExist(OriginalNameString))
		{
			TArray<FAssetData> AssetsInFolder;
			AssetRegistry.GetAssetsByPath(OriginalName, AssetsInFolder, true, false);
			for (const FAssetData& Asset : AssetsInFolder)
			{
				ExpandedAssets.Add(Asset.PackageName);
			}

			NameIt.RemoveCurrent();
		}
		else
		{
			ExpandedAssets.Add(OriginalName);
		}
	}

	TArray<FName> ExpandedAssetArray;
	for (FName ExpandedAsset : ExpandedAssets)
	{
		ExpandedAssetArray.Add(ExpandedAsset);
	}

	return ExpandedAssetArray;
}

void UAssetToolsImpl::PerformAdvancedCopyPackages(TArray<FName> SelectedAssetAndFolderNames, FString TargetPath) const
{	
	TargetPath.RemoveFromEnd(TEXT("/"));

	//TArray<FName> ExpandedAssets = ExpandAssetsAndFoldersToJustAssets(SelectedAssetAndFolderNames);
	FAdvancedCopyParams CopyParams = FAdvancedCopyParams(SelectedAssetAndFolderNames, TargetPath);
	CopyParams.bShouldCheckForDependencies = true;

	// Suppress UI if we're running in unattended mode
	if (FApp::IsUnattended())
	{
		CopyParams.bShouldSuppressUI = true;
	}

	for (auto NameIt = SelectedAssetAndFolderNames.CreateConstIterator(); NameIt; ++NameIt)
	{
		UAdvancedCopyCustomization* CopyCustomization = nullptr;

		const UAssetToolsSettings* Settings = GetDefault<UAssetToolsSettings>();
		FName OriginalName = *NameIt;
		const FString& OriginalNameString = OriginalName.ToString();
		FString SrcFilename;
		UObject* ExistingObject = nullptr;

		if (FPackageName::DoesPackageExist(OriginalNameString, &SrcFilename))
		{
			UPackage* Pkg = LoadPackage(nullptr, *OriginalNameString, LOAD_None);
			if (Pkg)
			{
				FString Name = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(SrcFilename));
				ExistingObject = StaticFindObject(UObject::StaticClass(), Pkg, *Name);
			}
		}

		if (ExistingObject)
		{
			// Try to find the customization in the settings
			for (const FAdvancedCopyMap& Customization : Settings->AdvancedCopyCustomizations)
			{
				if (Customization.ClassToCopy.GetAssetPathString() == ExistingObject->GetClass()->GetPathName())
				{
					UClass* CustomizationClass = Customization.AdvancedCopyCustomization.TryLoadClass<UAdvancedCopyCustomization>();
					if (CustomizationClass)
					{
						CopyCustomization = CustomizationClass->GetDefaultObject<UAdvancedCopyCustomization>();
					}
				}
			}
		}

		// If not able to find class in settings, fall back to default customization
		// by default, folders will use the default customization
		if (CopyCustomization == nullptr)
		{
			CopyCustomization = UAdvancedCopyCustomization::StaticClass()->GetDefaultObject<UAdvancedCopyCustomization>();
		}

		CopyParams.AddCustomization(CopyCustomization);
	}

	InitAdvancedCopyFromCopyParams(CopyParams);
}


void UAssetToolsImpl::InitAdvancedCopyFromCopyParams(FAdvancedCopyParams CopyParams) const
{
	TArray<TMap<FName, FName>> CompleteDependencyMap;
	TArray<TMap<FString, FString>> CompleteDestinationMap;

	TArray<FName> SelectedPackageNames = CopyParams.GetSelectedPackageOrFolderNames();

	FScopedSlowTask SlowTask(SelectedPackageNames.Num(), LOCTEXT("AdvancedCopyPrepareSlowTask", "Preparing Files for Advanced Copy"));
	SlowTask.MakeDialog();

	TArray<UAdvancedCopyCustomization*> CustomizationsToUse = CopyParams.GetCustomizationsToUse();

	for (int32 CustomizationIndex = 0; CustomizationIndex < CustomizationsToUse.Num(); CustomizationIndex++)
	{
		FName Package = SelectedPackageNames[CustomizationIndex];
		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("AdvancedCopy_PreparingDependencies", "Preparing dependencies for {0}"), FText::FromString(Package.ToString())));

		UAdvancedCopyCustomization* CopyCustomization = CustomizationsToUse[CustomizationIndex];
		CopyCustomization->SetPackageThatInitiatedCopy(Package.ToString());
		// Give the customization a chance to edit the copy parameters
		CopyCustomization->EditCopyParams(CopyParams);
		TMap<FName, FName> DependencyMap = TMap<FName, FName>();
		TArray<FName> PackageNamesToCopy;

		// Get all packages to be copied
		GetAllAdvancedCopySources(Package, CopyParams, PackageNamesToCopy, DependencyMap, CopyCustomization);
		
		// Allow the customization to apply any additional filters
		CopyCustomization->ApplyAdditionalFiltering(PackageNamesToCopy);

		TMap<FString, FString> DestinationMap = TMap<FString, FString>();
		GenerateAdvancedCopyDestinations(CopyParams, PackageNamesToCopy, CopyCustomization, DestinationMap);
		CopyCustomization->TransformDestinationPaths(DestinationMap);
		CompleteDestinationMap.Add(DestinationMap);
		CompleteDependencyMap.Add(DependencyMap);
	}

	// Confirm that there is at least one package to move 
	if (CompleteDestinationMap.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AdvancedCopyPackages_NoFilesToMove", "No files were found to move"));
		return;
	}

	// Prompt the user displaying all assets that are going to be migrated
	if (CopyParams.bShouldSuppressUI)
	{
		AdvancedCopyPackages_ReportConfirmed(CopyParams, CompleteDestinationMap);
	}
	else
	{
		const FText ReportMessage = FText::FromString(CopyParams.GetDropLocationForAdvancedCopy());

		SAdvancedCopyReportDialog::FOnReportConfirmed OnReportConfirmed = SAdvancedCopyReportDialog::FOnReportConfirmed::CreateUObject(this, &UAssetToolsImpl::AdvancedCopyPackages_ReportConfirmed);
		SAdvancedCopyReportDialog::OpenPackageReportDialog(CopyParams, ReportMessage, CompleteDestinationMap, CompleteDependencyMap, OnReportConfirmed);
	}
}

void UAssetToolsImpl::AdvancedCopyPackages_ReportConfirmed(FAdvancedCopyParams CopyParams, TArray<TMap<FString, FString>> DestinationMap) const
{
	TArray<UAdvancedCopyCustomization*> CustomizationsToUse = CopyParams.GetCustomizationsToUse();
	for (int32 CustomizationIndex = 0; CustomizationIndex < CustomizationsToUse.Num(); CustomizationIndex++)
	{
		if (!CustomizationsToUse[CustomizationIndex]->CustomCopyValidate(DestinationMap[CustomizationIndex]))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AdvancedCopy_FailedCustomValidate", "Advanced Copy failed because the validation rules set in {0} failed."),
				FText::FromString(CustomizationsToUse[CustomizationIndex]->GetName())));

			return;
		}
	}
	AdvancedCopyPackages(CopyParams, DestinationMap);
}

bool UAssetToolsImpl::IsAssetClassSupported(const UClass* AssetClass) const
{
	TWeakPtr<IAssetTypeActions> AssetTypeActions = GetAssetTypeActionsForClass(AssetClass);
	if (!AssetTypeActions.IsValid())
	{
		return false;
	}

	if (AssetTypeActions.Pin()->IsSupported() == false)
	{
		return false;
	}

	return true;
}

TArray<UFactory*> UAssetToolsImpl::GetNewAssetFactories() const
{
	TArray<UFactory*> Factories;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UFactory::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		UFactory* Factory = Class->GetDefaultObject<UFactory>();
		if (!Factory->ShouldShowInNewMenu() || !ensure(!Factory->GetDisplayName().IsEmpty()) || !IsAssetClassSupported(Factory->GetSupportedClass()))
		{
			continue;
		}

		// For Blueprints, add sub-type filtering using the BP Editor's allow list
		// Otherwise, there's no way to distinguish between generic BP Actors vs BlueprintFunctionLibraries, etc.
		if (UBlueprintFactory* BPFactory = Cast<UBlueprintFactory>(Factory))
		{
			// Restrict BP factories based on their BlueprintType
			if (AllowedBlueprintTypes.Num() > 0 && !AllowedBlueprintTypes.Contains(BPFactory->BlueprintType))
			{
				continue;
			}

			// BPFactor->ParentClass is stored as a UObject instead of a class... most things should be UClass but we'll just skip it if not
			if (UClass* BPParentClass = Cast<UClass>(BPFactory->ParentClass))
			{
				FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
				if (const TSharedPtr<IClassViewerFilter>& GlobalClassFilter = ClassViewerModule.GetGlobalClassViewerFilter())
				{
					TSharedRef<FClassViewerFilterFuncs> ClassFilterFuncs = ClassViewerModule.CreateFilterFuncs();
					FClassViewerInitializationOptions ClassViewerOptions = {};
					if (!GlobalClassFilter->IsClassAllowed(ClassViewerOptions, BPParentClass, ClassFilterFuncs))
					{
						continue;
					}
				}
			}
		}

		Factories.Add(Factory);
	}

	return MoveTemp(Factories);
}

TSharedRef<FNamePermissionList>& UAssetToolsImpl::GetAssetClassPermissionList()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetAssetClassPermissionList(EAssetClassAction::ViewAsset);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
}

TSharedRef<FNamePermissionList>& UAssetToolsImpl::GetAssetClassPermissionList(EAssetClassAction AssetClassAction)
{
	// Convert path list to name list. Slow and unefficient but keeping it for compatibility (this function is deprecated)
	if (AssetClassAction < EAssetClassAction::AllAssetActions)
	{
		TSharedRef<FPathPermissionList> List = AssetClassPermissionList[(int32)AssetClassAction];
		TSharedRef<FNamePermissionList> Result = MakeShared<FNamePermissionList>();
		if (List->IsDenyListAll())
		{
			Result->AddDenyListAll(NAME_None);
		}

		for (const TPair<FString, FPermissionListOwners>& It : List->GetDenyList())
		{
			for (const FName OwnerName : It.Value)
			{
				Result->AddDenyListItem(OwnerName, FName(*FPackageName::GetShortName(*It.Key)));
			}
		}

		for (const TPair<FString, FPermissionListOwners>& It : List->GetAllowList())
		{
			FName Name = *FPackageName::GetShortName(*It.Key);
			if (Result->PassesFilter(Name))
			{
				for (const FName OwnerName : It.Value)
				{
					Result->AddAllowListItem(OwnerName, Name);
				}
			}
		}

		// Block everything if none of the AllowList paths passed
		if (Result->GetAllowList().Num() == 0)
		{
			Result->AddDenyListAll(NAME_None);
		}

		AssetClassPermissionList_DEPRECATED = Result;
		return AssetClassPermissionList_DEPRECATED;
	}
	
	static TSharedRef<FNamePermissionList> Empty = MakeShared<FNamePermissionList>();
	return Empty;
}

TSharedRef<FPathPermissionList>& UAssetToolsImpl::GetAssetClassPathPermissionList(EAssetClassAction AssetClassAction)
{
	if (AssetClassAction < EAssetClassAction::AllAssetActions)
	{
		return AssetClassPermissionList[(int32)AssetClassAction];
	}

	static TSharedRef<FPathPermissionList> Empty = MakeShared<FPathPermissionList>();
	return Empty;
}

TSet<EBlueprintType>& UAssetToolsImpl::GetAllowedBlueprintTypes()
{
	return AllowedBlueprintTypes;
}

void UAssetToolsImpl::AssetClassPermissionListChanged(EAssetClassAction AssetClassAction)
{
	switch (AssetClassAction)
	{
	case EAssetClassAction::CreateAsset:
		for (TSharedRef<IAssetTypeActions>& ActionsIt : AssetTypeActionsList)
		{
			bool bSupported = false;
			if (const UClass* SupportedClass = ActionsIt->GetSupportedClass())
			{
				bSupported = GetAssetClassPathPermissionList(AssetClassAction)->PassesFilter(SupportedClass->GetPathName());
			}
			else
			{
				bSupported = !ActionsIt->GetFilterName().IsNone();
			}

			ActionsIt->SetSupported(bSupported);
		}
		break;
	default:
		break;
	}
}

void UAssetToolsImpl::AddSubContentDenyList(const FString& InMount)
{
	for (const FString& SubContentPath : SubContentDenyListPaths)
	{
		FolderPermissionList->AddDenyListItem("AssetToolsConfigFile", InMount / SubContentPath);
	}
}

void UAssetToolsImpl::OnContentPathMounted(const FString& InAssetPath, const FString& FileSystemPath)
{
	AddSubContentDenyList(InAssetPath);
}

TArray<UObject*> UAssetToolsImpl::ImportAssetsWithDialogImplementation(const FString& DestinationPath, bool bAllowAsyncImport)
{
	if (!GetWritableFolderPermissionList()->PassesStartsWithFilter(DestinationPath))
	{
		NotifyBlockedByWritableFolderFilter();
		return TArray<UObject*>();
	}

	TArray<UObject*> ReturnObjects;
	FString FileTypes, AllExtensions;
	TArray<UFactory*> Factories;

	// Get the list of valid factories
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* CurrentClass = (*It);

		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->bEditorImport)
			{
				Factories.Add(Factory);
			}
		}
	}

	TMultiMap<uint32, UFactory*> FilterIndexToFactory;

	// Generate the file types and extensions represented by the selected factories
	ObjectTools::GenerateFactoryFileExtensions(Factories, FileTypes, AllExtensions, FilterIndexToFactory);

	if (UInterchangeManager::IsInterchangeImportEnabled())
	{
		TArray<FString> InterchangeFileExtensions = UInterchangeManager::GetInterchangeManager().GetSupportedFormats(EInterchangeTranslatorType::Assets);
		ObjectTools::AppendFormatsFileExtensions(InterchangeFileExtensions, FileTypes, AllExtensions, FilterIndexToFactory);
	}

	FileTypes = FString::Printf(TEXT("All Files (%s)|%s|%s"), *AllExtensions, *AllExtensions, *FileTypes);

	// Prompt the user for the filenames
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	int32 FilterIndex = -1;

	if (DesktopPlatform)
	{
		const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ImportDialogTitle", "Import").ToString(),
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT),
			TEXT(""),
			FileTypes,
			EFileDialogFlags::Multiple,
			OpenFilenames,
			FilterIndex
		);
	}

	if (bOpened)
	{
		if (OpenFilenames.Num() > 0)
		{
			UFactory* ChosenFactory = nullptr;
			if (FilterIndex > 0)
			{
				ChosenFactory = *FilterIndexToFactory.Find(FilterIndex);
			}


			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, OpenFilenames[0]);
			const bool bSyncToBrowser = false;
			TArray<TPair<FString, FString>>* FilesAndDestination = nullptr;
			ReturnObjects = ImportAssets(OpenFilenames, DestinationPath, ChosenFactory, bSyncToBrowser, FilesAndDestination, bAllowAsyncImport);
		}
	}

	return ReturnObjects;
}

TSharedRef<FPathPermissionList>& UAssetToolsImpl::GetFolderPermissionList()
{
	return FolderPermissionList;
}

TSharedRef<FPathPermissionList>& UAssetToolsImpl::GetWritableFolderPermissionList()
{
	return WritableFolderPermissionList;
}

bool UAssetToolsImpl::AllPassWritableFolderFilter(const TArray<FString>& InPaths) const
{
	if (WritableFolderPermissionList->HasFiltering())
	{
		for (const FString& Path : InPaths)
		{
			if (!WritableFolderPermissionList->PassesStartsWithFilter(Path))
			{
				return false;
			}
		}
	}

	return true;
}

void UAssetToolsImpl::NotifyBlockedByWritableFolderFilter() const
{
	FSlateNotificationManager::Get().AddNotification(FNotificationInfo(LOCTEXT("NotifyBlockedByWritableFolderFilter", "Folder is locked")));
}

bool UAssetToolsImpl::IsNameAllowed(const FString& Name, FText* OutErrorMessage) const
{
	for (const TPair<FName, FIsNameAllowed>& DelegatePair : IsNameAllowedDelegates)
	{
		if (!DelegatePair.Value.Execute(Name, OutErrorMessage))
		{
			return false;
		}
	}
	return true;
}

void UAssetToolsImpl::RegisterIsNameAllowedDelegate(const FName OwnerName, FIsNameAllowed Delegate)
{
	IsNameAllowedDelegates.Add(OwnerName, Delegate);
}

void UAssetToolsImpl::UnregisterIsNameAllowedDelegate(const FName OwnerName)
{
	IsNameAllowedDelegates.Remove(OwnerName);
}

#undef LOCTEXT_NAMESPACE

