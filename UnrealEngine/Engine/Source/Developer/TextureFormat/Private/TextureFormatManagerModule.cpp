// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Interfaces/ITextureFormatManagerModule.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "Modules/ModuleManager.h"
#include "TextureFormatManager.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatManager, Log, All);

/**
 * Module for the target platform manager
 */
class FTextureFormatManagerModule
	: public ITextureFormatManagerModule
{
public:

	enum class EInitPhase
	{
		JustConstructedNotInit = 0,
		Invalidated = 1,
		GetTextureFormatsInProgressDontTouch = 2,
		GetTextureFormatsPartialOkayToRead = 3, // values >= here are okay to make queries
		GetTextureFormatsDone = 4
	};

	/** Default constructor. */
	FTextureFormatManagerModule()
		: ModuleName(TEXT("TextureFormat"))
		, bForceCacheUpdate(true)
		, bModuleChangeCallbackEnabled(false)
		, TextureFormatsInitPhase(EInitPhase::JustConstructedNotInit)
	{
		// Calling a virtual function from a constructor, but with no expectation that a derived implementation of this
		// method would be called.  This is solely to avoid duplicating code in this implementation, not for polymorphism.
		FTextureFormatManagerModule::Invalidate();
		
		// add AFTER Invalidate :
		FModuleManager::Get().OnModulesChanged().AddRaw(this, &FTextureFormatManagerModule::ModulesChangesCallback);
	}

	/** Destructor. */
	virtual ~FTextureFormatManagerModule() = default;

	virtual void ShutdownModule()
	{
		FModuleManager::Get().OnModulesChanged().RemoveAll(this);
	}

	virtual const TArray<const ITextureFormat*>& GetTextureFormats() override
	{
		FScopeLock Lock(&ModuleMutex);

		// should not be called recursively while I am building the list :
		check( TextureFormatsInitPhase!= EInitPhase::GetTextureFormatsInProgressDontTouch );

		// bForceCacheUpdate should be true on first call, so we don't need a separate static init flag
		if ( bForceCacheUpdate )
		{
			// turn off flag immediately so that repeated calls to GetTextureFormats will not come in here again
			bForceCacheUpdate = false;
			bModuleChangeCallbackEnabled = false; // don't re-call me from my own module loads
			TextureFormatsInitPhase = EInitPhase::GetTextureFormatsInProgressDontTouch;

			// note the first time this is done is from FTargetPlatformManagerModule::FTargetPlatformManagerModule()
			//	so calls to it are dangerous

			TextureFormats.Empty(TextureFormats.Num());
			TextureFormatMetadata.Empty(TextureFormatMetadata.Num());

			TArray<FName> Modules;

			FModuleManager::Get().FindModules(TEXT("*TextureFormat*"), Modules);

			if (!Modules.Num())
			{
				UE_LOG(LogTextureFormatManager, Error, TEXT("No texture formats found!"));
			}			
					
			TArray<FTextureFormatMetadata> BaseModules;
			TArray<FTextureFormatMetadata> ChildModules;

			{
				// unlock the mutex to avoid deadlock during module loading: T0 locks ModuleMutex (M0), loads the module and this broadcasts 
				// FModuleManager::ModulesChangesEvent thread-safe delegate that locks its internal mutex (M1), while T1 loads a module ->
				// broadcasts FModuleManager::ModulesChangesEvent (this locks M1) -> FTextureFormatManagerModule::ModulesChangesCallback that
				// locks M0. at least it's what TSan reports. this can be a false positive, but there's no need to keep the mutex locked anyway, so it's
				// better than just silencing TSan
				FScopeUnlock ScopeUnlock(&ModuleMutex);

				for (int32 Index = 0; Index < Modules.Num(); Index++)
				{
					if (Modules[Index] != ModuleName) // Avoid our own module when going through this list that was gathered by name
					{
						ITextureFormatModule* Module = FModuleManager::LoadModulePtr<ITextureFormatModule>(Modules[Index]);
						if (Module)
						{
							FTextureFormatMetadata ModuleMeta;
							ModuleMeta.Module = Module;
							ModuleMeta.ModuleName = Modules[Index];
							if (Module->CanCallGetTextureFormats())
							{
								ChildModules.Add(ModuleMeta);
							}
							else
							{
								BaseModules.Add(ModuleMeta);
							}
						}
					}
				}
			}
			
			// first populate TextureFormats[] with all Base Modules
			// 
			for (int32 Index = 0; Index < BaseModules.Num(); Index++)
			{
				ITextureFormatModule* Module = BaseModules[Index].Module;

				ITextureFormat* Format = Module->GetTextureFormat();
				if (Format != nullptr)
				{
				
					// I want to see this log by default in Cook+Editor , but not in TBW
					#ifndef VerboseIfNotEditor
					#if WITH_EDITOR
					#define VerboseIfNotEditor	Display
					#else
					#define VerboseIfNotEditor	Verbose
					#endif
					#endif

					UE_LOG(LogTextureFormatManager, VerboseIfNotEditor,TEXT("Loaded Base TextureFormat: %s"),*BaseModules[Index].ModuleName.ToString());
						
					TextureFormats.Add(Format);
					TextureFormatMetadata.Add(BaseModules[Index]);
				}
			}
			
			// Init phase 3 means you are now allowd to call GetTextureFormats() and you will get only the Base formats
			TextureFormatsInitPhase = EInitPhase::GetTextureFormatsPartialOkayToRead;

			// run through the Child formats and call GetTextureFormat() on them
			// this could call back to me and do GetTextureFormats() which will get only the base formats
			for (int32 Index = 0; Index < ChildModules.Num(); Index++)
			{
				ITextureFormatModule* Module = ChildModules[Index].Module;

				ITextureFormat* Format = Module->GetTextureFormat();
				if (Format != nullptr)
				{
					UE_LOG(LogTextureFormatManager,VerboseIfNotEditor,TEXT("Loaded Child TextureFormat: %s"),*ChildModules[Index].ModuleName.ToString());

					// do not add me to TextureFormats yet
				}
			}
			
			// back up phase to 2, no calls to GetTextureFormats() allowed now
			TextureFormatsInitPhase = EInitPhase::GetTextureFormatsInProgressDontTouch;

			for (int32 Index = 0; Index < ChildModules.Num(); Index++)
			{
				ITextureFormatModule* Module = ChildModules[Index].Module;

				// GetTextureFormat was already done so this should just return a stored pointer, no more init
				ITextureFormat* Format = Module->GetTextureFormat();
				if (Format != nullptr)
				{
					// now add to the list :
					TextureFormats.Add(Format);
					TextureFormatMetadata.Add(ChildModules[Index]);
				}
			}

			// all done :
			TextureFormatsInitPhase = EInitPhase::GetTextureFormatsDone;
			bModuleChangeCallbackEnabled = true;
		}

		check( (int)TextureFormatsInitPhase >= (int)EInitPhase::GetTextureFormatsPartialOkayToRead );

		return TextureFormats;
	}
	
	virtual const ITextureFormat* FindTextureFormat(FName Name) override
	{
		// just pass through to FindTextureFormatAndModule
		FName ModuleNameUnused;
		ITextureFormatModule* ModuleUnused;
		return FindTextureFormatAndModule(Name, ModuleNameUnused, ModuleUnused);
	}
	
	virtual const class ITextureFormat* FindTextureFormatAndModule(FName Name, FName& OutModuleName, ITextureFormatModule*& OutModule) override
	{
		FScopeLock Lock(&ModuleMutex);
		check( (int)TextureFormatsInitPhase >= (int)EInitPhase::GetTextureFormatsPartialOkayToRead );

		// Called to ensure the arrays are populated
		// dangerous and not necessary, removed :
		//GetTextureFormats();
		check( ! bForceCacheUpdate );

		for (int32 Index = 0; Index < TextureFormats.Num(); Index++)
		{
			TArray<FName> Formats;

			TextureFormats[Index]->GetSupportedFormats(Formats);

			for (int32 FormatIndex = 0; FormatIndex < Formats.Num(); FormatIndex++)
			{
				if (Formats[FormatIndex] == Name)
				{
					const FTextureFormatMetadata& FoundMeta = TextureFormatMetadata[Index];
					OutModuleName = FoundMeta.ModuleName;
					OutModule = FoundMeta.Module;
					return TextureFormats[Index];
				}
			}
		}

		return nullptr;
	}

	virtual void Invalidate() override
	{
		// don't lock `GetTextureFormats()` as it does own synchronisation
		{
			FScopeLock Lock(&ModuleMutex);
			// this is called from the constructor
			TextureFormatsInitPhase = EInitPhase::Invalidated;
			bForceCacheUpdate = true;
		}

		GetTextureFormats();
	}

private:

	void ModulesChangesCallback(FName InModuleName, EModuleChangeReason ReasonForChange)
	{
		// don't lock `Invalidate()` as it does own synchronisation
		bool bLocalModuleChangeCallbackEnabled;
		{
			FScopeLock Lock(&ModuleMutex);
			bLocalModuleChangeCallbackEnabled = bModuleChangeCallbackEnabled;
		}

		if (bLocalModuleChangeCallbackEnabled && (InModuleName != ModuleName) && InModuleName.ToString().Contains(TEXT("TextureFormat")))
		{
			// when a "TextureFormat" module is loaded, rebuild my list
			Invalidate();
		}
	}

	const FName ModuleName;

	TArray<const ITextureFormat*> TextureFormats;

	struct FTextureFormatMetadata
	{
		FName ModuleName;
		ITextureFormatModule* Module;
	};
	TArray<FTextureFormatMetadata> TextureFormatMetadata;

	// Flag to force reinitialization of all cached data. This is needed to have up-to-date caches
	// in case of a module reload of a TextureFormat-Module.
	bool bForceCacheUpdate;

	// Flag to avoid redunant reloads
	bool bModuleChangeCallbackEnabled;

	// Track tricky initialization progress
	EInitPhase TextureFormatsInitPhase;

	FCriticalSection ModuleMutex;
};

IMPLEMENT_MODULE(FTextureFormatManagerModule, TextureFormat);
