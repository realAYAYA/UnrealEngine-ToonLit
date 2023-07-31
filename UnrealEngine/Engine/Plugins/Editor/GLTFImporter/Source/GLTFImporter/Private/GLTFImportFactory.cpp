// Copyright Epic Games, Inc. All Rights Reserved.

#include "GLTFImportFactory.h"

#include "GLTFImportOptions.h"
#include "GLTFImporterContext.h"
#include "GLTFImporterModule.h"
#include "UI/GLTFOptionsWindow.h"

#include "Engine/StaticMesh.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "IMessageLogListing.h"
#include "Interfaces/IMainFrameModule.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "Materials/Material.h"
#include "MessageLogModule.h"
#include "PackageTools.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLTFImportFactory)

DEFINE_LOG_CATEGORY(LogGLTF);

#define LOCTEXT_NAMESPACE "GLTFFactory"

namespace GLTFImporterImpl
{
	bool ShowOptionsWindow(const FString& Filepath, const FString& PackagePath, UGLTFImportOptions& ImporterOptions)
	{
		TSharedPtr<SWindow> ParentWindow;
		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow                = MainFrame.GetParentWindow();
		}

		TSharedRef<SWindow> Window = SNew(SWindow).Title(LOCTEXT("GLTFImportOptionsTitle", "glTF Import Options")).SizingRule(ESizingRule::Autosized);

		TSharedPtr<SGLTFOptionsWindow> OptionsWindow;
		Window->SetContent(
		    SAssignNew(OptionsWindow, SGLTFOptionsWindow)
		        .ImportOptions(&ImporterOptions)
		        .WidgetWindow(Window)
		        .FileNameText(FText::Format(LOCTEXT("GLTFImportOptionsFileName", "  Import File  :    {0}"),
		                                    FText::FromString(FPaths::GetCleanFilename(Filepath))))
		        .FilePathText(FText::FromString(Filepath))
		        .PackagePathText(FText::Format(LOCTEXT("GLTFImportOptionsPackagePath", "  Import To   :    {0}"), FText::FromString(PackagePath))));

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
		return OptionsWindow->ShouldImport();
	}

	void ShowLogMessages(const TArray<GLTF::FLogMessage>& Messages)
	{
		if (Messages.Num() > 0)
		{
			FMessageLogModule&             MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			TSharedRef<IMessageLogListing> LogListing       = (MessageLogModule.GetLogListing("LoadErrors"));
			LogListing->ClearMessages();
			for (const GLTF::FLogMessage& Error : Messages)
			{
				EMessageSeverity::Type Severity =
				    Error.Get<0>() == GLTF::EMessageSeverity::Error ? EMessageSeverity::Error : EMessageSeverity::Warning;
				LogListing->AddMessage(FTokenizedMessage::Create(Severity, FText::FromString(Error.Get<1>())));
			}
			MessageLogModule.OpenMessageLog("LoadErrors");
		}
	}
}

UGLTFImportFactory::UGLTFImportFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , GLTFImporterModule(&IGLTFImporterModule::Get())
{
	bCreateNew    = false;
	bEditAfterNew = false;
	bEditorImport = true;   // binary / general file source
	bText         = false;  // text source

	SupportedClass = UStaticMesh::StaticClass();

	Formats.Add(TEXT("gltf;GL Transmission Format"));
	Formats.Add(TEXT("glb;GL Transmission Format (Binary)"));
}

UObject* UGLTFImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
                                               const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Parms);
	AdditionalImportedObjects.Empty();

	Warn->Log(Filename);

	TStrongObjectPtr<UGLTFImportOptions> ImporterOptions(NewObject<UGLTFImportOptions>(GetTransientPackage(), TEXT("GLTF Importer Options")));
	// show import options window
	if (!IsAutomatedImport())
	{
		const FString Filepath    = FPaths::ConvertRelativePathToFull(Filename);
		const FString PackagePath = InParent->GetPathName();

		if (!GLTFImporterImpl::ShowOptionsWindow(Filepath, PackagePath, *ImporterOptions))
		{
			bOutOperationCanceled = true;
			return nullptr;
		}
	}
	else
	{
		ImporterOptions->bGenerateLightmapUVs = false;
		ImporterOptions->ImportScale = 100.0f;

		if (AssetImportTask->Options != nullptr)
		{
			UGLTFImportOptions* Options = Cast<UGLTFImportOptions>(AssetImportTask->Options);
			if (Options == nullptr)
			{
				UE_LOG(LogGLTF, Display, TEXT("The options set in the Asset Import Task are not of type UGLTFImportOptions and will be ignored"));
			}
			else
			{
				ImporterOptions->bGenerateLightmapUVs = Options->bGenerateLightmapUVs;
				ImporterOptions->ImportScale = Options->ImportScale;
			}
		}
	}

	FGLTFImporterContext& Context = GLTFImporterModule->GetImporterContext();
	// setup import options
	{
		Context.StaticMeshFactory.SetUniformScale(ImporterOptions->ImportScale);
		Context.StaticMeshFactory.SetGenerateLightmapUVs(ImporterOptions->bGenerateLightmapUVs);
	}

	UObject* Object = nullptr;
	if (Context.OpenFile(Filename))
	{
		const FString AssetName      = Context.Asset.Name;
		const FString NewPackageName = UPackageTools::SanitizePackageName(*(FPaths::GetPath(InParent->GetName()) / AssetName));
		UObject*      ParentPackage  = NewPackageName == InParent->GetName() ? InParent : CreatePackage(*NewPackageName);

		const TArray<UStaticMesh*>& CreatedMeshes = Context.CreateMeshes(ParentPackage, Flags, false);
		Context.CreateMaterials(ParentPackage, Flags);
		UpdateMeshes();

		if (CreatedMeshes.Num() == 1)
		{
			Object = CreatedMeshes[0];
		}
		else if (CreatedMeshes.Num() != 0)
		{
			Object = CreatedMeshes[0]->GetOutermost();
			AdditionalImportedObjects.Append(CreatedMeshes);
		}
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Object);

	GLTFImporterImpl::ShowLogMessages(Context.GetLogMessages());

	return Object;
}

bool UGLTFImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if( Extension == TEXT("gltf") || Extension == TEXT("glb") )
	{
		return true;
	}
	return false;
}

TArray<FString> UGLTFImportFactory::GetFormats() const
{
	static TArray<FString> EmptyArray;
	static const auto CVarGltf = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.GLTF"));
	const bool bUseLegacyGltf = (!CVarGltf || !CVarGltf->GetBool());
	if (bUseLegacyGltf)
	{
		return Formats;
	}
	return EmptyArray;
}

void UGLTFImportFactory::CleanUp()
{
	// cleanup any resources/buffers

	FGLTFImporterContext& Context = GLTFImporterModule->GetImporterContext();
	Context.StaticMeshFactory.CleanUp();

	Context.Asset.Clear(8 * 1024, 512);
	Super::CleanUp();
}

void UGLTFImportFactory::UpdateMeshes() const
{
	const FGLTFImporterContext& Context   = GLTFImporterModule->GetImporterContext();
	const TArray<UStaticMesh*>& Meshes    = Context.StaticMeshFactory.GetMeshes();
	const TArray<UMaterial*>&   Materials = Context.Materials;
	check(Materials.Num() == Context.Asset.Materials.Num());

	int32 MeshIndex = 0;
	for (UStaticMesh* StaticMesh : Meshes)
	{
		const GLTF::FMesh& GltfMesh = Context.Asset.Meshes[MeshIndex++];

		for (int32 PrimIndex = 0; PrimIndex < GltfMesh.Primitives.Num(); ++PrimIndex)
		{
			const GLTF::FPrimitive& Primitive = GltfMesh.Primitives[PrimIndex];

			UMaterialInterface* Material = nullptr;
			if (Primitive.MaterialIndex != INDEX_NONE)
			{
				Material = static_cast<UMaterialInterface*>(Materials[Primitive.MaterialIndex]);
				check(Material);
			}

			StaticMesh->GetStaticMaterials()[PrimIndex].MaterialInterface = Material;
		}

		StaticMesh->MarkPackageDirty();
		StaticMesh->PostEditChange();
		FAssetRegistryModule::AssetCreated(StaticMesh);
	}
}

#undef LOCTEXT_NAMESPACE

