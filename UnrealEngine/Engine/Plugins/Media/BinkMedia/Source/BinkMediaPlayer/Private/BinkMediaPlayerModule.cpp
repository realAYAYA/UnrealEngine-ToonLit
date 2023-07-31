// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkFunctionLibrary.h"
#include "BinkMediaPlayerPrivate.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogBink);

#define LOCTEXT_NAMESPACE "BinkMediaPlayerModule"

TSharedPtr<FBinkMovieStreamer, ESPMode::ThreadSafe> MovieStreamer;

TArray< FTexture2DRHIRef > BinkActiveTextureRefs;

#if BINKPLUGIN_UE4_EDITOR
class UFactory;
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "SourceControlHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "IDetailCustomization.h"
#include "IDetailChildrenBuilder.h"

#include "BinkMediaPlayer.h"

struct FBinkMoviePlayerSettingsDetails : public IDetailCustomization {
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShareable(new FBinkMoviePlayerSettingsDetails); }
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override {
		IDetailCategoryBuilder& MoviesCategory = DetailLayout.EditCategory("Movies");

		StartupMoviesPropertyHandle = DetailLayout.GetProperty("StartupMovies");

		TSharedRef<FDetailArrayBuilder> StartupMoviesBuilder = MakeShareable(new FDetailArrayBuilder(StartupMoviesPropertyHandle.ToSharedRef()));
		StartupMoviesBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FBinkMoviePlayerSettingsDetails::GenerateArrayElementWidget));

		MoviesCategory.AddProperty("bWaitForMoviesToComplete");
		MoviesCategory.AddProperty("bMoviesAreSkippable");

		const bool bForAdvanced = false;
		MoviesCategory.AddCustomBuilder(StartupMoviesBuilder, bForAdvanced);
	}

	void GenerateArrayElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder) {
		IDetailPropertyRow& FilePathRow = ChildrenBuilder.AddProperty(PropertyHandle);
		FilePathRow.CustomWidget(false)
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			.MaxDesiredWidth(0.0f)
			.MinDesiredWidth(125.0f)
			[
				SNew(SFilePathPicker)
					.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
					.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file from this computer"))
					.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
					.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
					.FilePath(this, &FBinkMoviePlayerSettingsDetails::HandleFilePathPickerFilePath, PropertyHandle)
					.FileTypeFilter(TEXT("Bink 2 Movie (*.bk2)|*.bk2"))
					.OnPathPicked(this, &FBinkMoviePlayerSettingsDetails::HandleFilePathPickerPathPicked, PropertyHandle)
			];
	}

	FString HandleFilePathPickerFilePath( TSharedRef<IPropertyHandle> Property ) const {
		FString FilePath;
		Property->GetValue(FilePath);
		return FilePath;
	}

	void HandleFilePathPickerPathPicked( const FString& PickedPath, TSharedRef<IPropertyHandle> Property ) {
		const FString MoviesBaseDir = FPaths::ConvertRelativePathToFull( BINKMOVIEPATH );
		const FString FullPath = FPaths::ConvertRelativePathToFull(PickedPath);

		if (FullPath.StartsWith(MoviesBaseDir)) {
			FText FailReason;
			if (SourceControlHelpers::CheckoutOrMarkForAdd(PickedPath, LOCTEXT("MovieFileDescription", "movie"), FOnPostCheckOut(), FailReason)) {
				Property->SetValue(FPaths::GetBaseFilename(FullPath.RightChop(MoviesBaseDir.Len())));
			} else {
				FNotificationInfo Info(FailReason);
				Info.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		} else if (!PickedPath.IsEmpty()) {
			const FString FileName = FPaths::GetCleanFilename(PickedPath);
			const FString DestPath = MoviesBaseDir / FileName;
			FText FailReason;
			if (SourceControlHelpers::CopyFileUnderSourceControl(DestPath, PickedPath, LOCTEXT("MovieFileDescription", "movie"), FailReason)) {
				// trim the path so we just have a partial path with no extension (the movie player expects this)
				Property->SetValue(FPaths::GetBaseFilename(DestPath.RightChop(MoviesBaseDir.Len())));
			} else {
				FNotificationInfo FailureInfo(FailReason);
				FailureInfo.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(FailureInfo);
			}
		} else {
			Property->SetValue(PickedPath);
		}
	}

	TSharedPtr<IPropertyHandle> StartupMoviesPropertyHandle;
};
#endif //BINKPLUGIN_UE4_EDITOR

unsigned bink_gpu_api;
unsigned bink_gpu_api_hdr;
EPixelFormat bink_force_pixel_format = PF_Unknown;

#ifdef BINK_NDA_GPU_ALLOC
extern void* BinkAllocGpu(UINTa Amt, U32 Align);
extern void BinkFreeGpu(void* ptr, UINTa Amt);
#else
#define BinkAllocGpu 0
#define BinkFreeGpu 0
#endif

#ifdef BINK_NDA_CPU_ALLOC
extern void* BinkAllocCpu(UINTa Amt, U32 Align);
extern void BinkFreeCpu(void* ptr);
#elif defined BINK_NO_CPU_ALLOC
#define BinkAllocCpu 0
#define BinkFreeCpu 0
#else
static void *BinkAllocCpu(UINTa Amt, U32 Align) { return FMemory::Malloc(Amt, Align); }
static void BinkFreeCpu(void * ptr) { FMemory::Free(ptr); }
#endif


FString BinkUE4CookOnTheFlyPath(FString path, const TCHAR *filename) 
{
	// If this isn't a shipping build, copy the file to our temp directory (so that cook-on-the-fly works)
	FString toPath = FPaths::ConvertRelativePathToFull(BINKTEMPPATH) + filename;
	FString fromPath = path + filename;
	toPath = toPath.Replace(TEXT("/./"), TEXT("/"));
	fromPath = fromPath.Replace(TEXT("/./"), TEXT("/"));
	FPlatformFileManager::Get().GetPlatformFile().CopyFile(*toPath, *fromPath);
	return fromPath;
}

struct FBinkMediaPlayerModule : IModuleInterface, FTickableGameObject
{
	virtual void StartupModule() override 
	{
		if (IsRunningCommandlet())
		{
			return;
		}

		// TODO: make this an INI setting and/or configurable in Project Settings
		//BinkPluginTurnOnGPUAssist();

		static BINKPLUGININITINFO InitInfo = { 0 };
		InitInfo.queue = 0;
		InitInfo.physical_device = 0;
		InitInfo.alloc = BinkAllocCpu;
		InitInfo.free = BinkFreeCpu;
		InitInfo.gpu_alloc = BinkAllocGpu;
		InitInfo.gpu_free = BinkFreeGpu;
		bink_gpu_api = BinkRHI;

		bPluginInitialized = (bool)BinkPluginInit(0, &InitInfo, bink_gpu_api);

		if (!bPluginInitialized)
		{
			printf("Bink Error: %s\n", BinkPluginError());
		}

		MovieStreamer = MakeShareable(new FBinkMovieStreamer);

		GetMoviePlayer()->RegisterMovieStreamer(MovieStreamer);

#if BINKPLUGIN_UE4_EDITOR
		static ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule && !IsRunningGame())
		{
			SettingsModule->RegisterSettings("Project", "Project", "Bink Movies",
				LOCTEXT("MovieSettingsName", "Bink Movies"),
				LOCTEXT("MovieSettingsDescription", "Bink Movie player settings"),
				GetMutableDefault<UBinkMoviePlayerSettings>()
			);

			//SettingsModule->RegisterViewer("Project", *this);

			static IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FBinkMediaPlayerModule::Initialize);
		
			FEditorDelegates::BeginPIE.AddRaw(this, &FBinkMediaPlayerModule::HandleEditorTogglePIE);
			FEditorDelegates::EndPIE.AddRaw(this, &FBinkMediaPlayerModule::HandleEditorTogglePIE);
		}
#endif
		GetMutableDefault<UBinkMoviePlayerSettings>()->LoadConfig();
	}

	virtual void ShutdownModule() override 
	{
		if (bPluginInitialized)
		{
			BinkPluginShutdown();
			if (overlayHook.IsValid() && GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->OnDrawn().Remove(overlayHook);
			}
		}
#if BINKPLUGIN_UE4_EDITOR
		FEditorDelegates::BeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);
#endif
	}

#if BINKPLUGIN_UE4_EDITOR
	void Initialize(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow) {
		// This overrides the 
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("MoviePlayerSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FBinkMoviePlayerSettingsDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	void HandleEditorTogglePIE(bool bIsSimulating)
	{
		for (TObjectIterator<UBinkMediaPlayer> It; It; ++It)
		{
			UBinkMediaPlayer* Player = *It;
			Player->Close();
			if(Player->StartImmediately)
			{
				Player->InitializePlayer();
			}
		}
	}
#endif

	virtual bool IsTickable() const override { return bPluginInitialized; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FBinkMediaPlayerModule, STATGROUP_Tickables); }

	void DrawBinks() 
	{
		ENQUEUE_RENDER_COMMAND(BinkProcess)([](FRHICommandListImmediate& RHICmdList) 
		{ 
			BINKPLUGINFRAMEINFO FrameInfo = {};
			BinkPluginSetPerFrameInfo(&FrameInfo);
			BinkPluginProcessBinks(0);
			BinkPluginAllScheduled();
			if(BinkActiveTextureRefs.Num())
			{
				BinkPluginDraw(1, 0);
				BinkActiveTextureRefs.Empty(256);
			}
		});
		UBinkFunctionLibrary::Bink_DrawOverlays();
	}

	virtual void Tick(float DeltaTime) override 
	{
		if (GEngine && GEngine->GameViewport) 
		{
			if (overlayHook.IsValid()) 
			{
				GEngine->GameViewport->OnDrawn().Remove(overlayHook);
			}
			overlayHook = GEngine->GameViewport->OnDrawn().AddRaw(this, &FBinkMediaPlayerModule::DrawBinks);
		}
		else 
		{
			DrawBinks();
		}
	}

    FDelegateHandle overlayHook;

#if BINKPLUGIN_UE4_EDITOR
	FDelegateHandle pieBeginHook;
	FDelegateHandle pieEndHook;
#endif
	bool bPluginInitialized = false;
};

IMPLEMENT_MODULE( FBinkMediaPlayerModule, BinkMediaPlayer )

#undef LOCTEXT_NAMESPACE
