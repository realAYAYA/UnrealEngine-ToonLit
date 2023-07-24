// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAudioCodecRegistry.h"
#include "IAudioCodec.h"
#include "Features/IModularFeatures.h"
#include "Misc/CoreMisc.h"
#include "Templates/SubclassOf.h"
#include "DSP/DeinterleaveView.h"

#include "Serialization/BulkData.h"
#include "Serialization/BulkDataWriter.h"

#include "GenericPlatform/GenericPlatformCriticalSection.h"

DEFINE_LOG_CATEGORY(LogAudioCodec);

namespace Audio
{
	class FCodecRegistry : public ICodecRegistry
	{		
		// Ownership.
		TArray<TUniquePtr<ICodec>> LifetimeArray;				// Ownership is held here for all registered codecs

		// Look ups. (these contain pointers to the above and must be updated when we add/remove from lifetime array).
		TMap<FName, TArray<FCodecPtr>> NameLookupMap;			// UniqueName -> [ICodecs] (ordered by version) (i.e. PcmGeneric v1, PcmGeneric v2 etc).
		TMap<FName, TArray<FCodecPtr>> FamilyNameLookupMap;		// FamilyName -> [ICodecs] (ordered by version) 

		// RW lock on above containers.
		mutable FRWLock ContainersLock;

		//TMap<FName, TArray<FCodecPtr>> FamilyLookup;	// FamilyName -> [ICodecs]

		// On Editor all other platform codecs with be discovered here
		// Version|FamilyName|ImplName|		Flags| Features
		// [0]    [Opus]     [OpusHwXbox]	[D]   [HwDecoder][HwResampler]
		// [0]    [Opus]     [Opus]			[D+E]

		// Anything matching the same version and family will be compatible.
		// This should mean that we could test hardware/software implementations against each other.				

		void InsertSortedByVersion(TArray<FCodecPtr>& InOutArray, FCodecPtr InCodecRawPtr)
		{
			// Store each identically named codec in order of version. (highest version first).
			InOutArray.Emplace(InCodecRawPtr);
			InOutArray.StableSort([](const ICodec& L, const ICodec& R) -> bool {
				return L.GetDetails().Version > R.GetDetails().Version;
			});
		}

		bool RegisterCodec(TUniquePtr<ICodec>&& InCodecPtr) override
		{
			if (!audio_ensure(InCodecPtr))
			{
				return false;
			}

			// Lock for writing.
			FRWScopeLock ScopeLock(ContainersLock, SLT_Write);
			
			// Make sure we haven't got this pointer for some reason.
			if( !audio_ensure(!LifetimeArray.Contains(InCodecPtr)) )
			{
				return false;
			}

			// Make sure we haven't already registered one with the same name and version.
			if( !audio_ensure(!LifetimeArray.ContainsByPredicate([&InDetails = InCodecPtr->GetDetails()](const TUniquePtr<ICodec>& i)  { return i->GetDetails() == InDetails; }) ) )
			{
				return false;
			}
					   
			// Take ownership
			int32 Index = LifetimeArray.Emplace(MoveTemp(InCodecPtr));
			if( audio_ensure(Index >= 0) && audio_ensure(LifetimeArray[Index] != nullptr) )
			{	
				FCodecPtr CodecRawPtr = LifetimeArray[Index].Get();
				const FCodecDetails& Details = CodecRawPtr->GetDetails();
					
				// Insert in the look ups.
				InsertSortedByVersion(NameLookupMap.FindOrAdd(Details.Name), CodecRawPtr);
				InsertSortedByVersion(FamilyNameLookupMap.FindOrAdd(Details.FamilyName), CodecRawPtr);

				UE_LOG(LogAudioCodec, Log, TEXT("Registered %s"), *Details.ToString());
				return true;
			}
			return false;
		}

		bool UnregisterCodec(ICodecRegistry::FCodecPtr InCodecPtr) override
		{
			// Lock for writing.
			FRWScopeLock ScopeLock(ContainersLock, SLT_Write);

			if (!audio_ensure(InCodecPtr))
			{
				return false;
			}

			auto FindLambda = [InCodecPtr](const TUniquePtr<ICodec>& i) -> bool { return i.Get() == InCodecPtr; };

			auto RemoveLamda = [InCodecPtr](TMap<FName,TArray<FCodecPtr>>& InMap, FName InMapKey) -> void
			{			
				TArray<FCodecPtr>& VersionArray = InMap.FindChecked(InMapKey);
				VersionArray.Remove(InCodecPtr);
				if (VersionArray.Num() == 0)
				{
					InMap.Remove(InMapKey);
				}
			};

			if (audio_ensure(LifetimeArray.ContainsByPredicate(FindLambda)))
			{
				UE_LOG(LogAudioCodec, Log, TEXT("Unregistered %s"), *InCodecPtr->GetDetails().ToString());

				// Remove from Look ups.
				RemoveLamda(FamilyNameLookupMap, InCodecPtr->GetDetails().FamilyName);
				RemoveLamda(NameLookupMap, InCodecPtr->GetDetails().Name);

				// Kill the object.
				audio_ensure(LifetimeArray.RemoveAll(FindLambda) == 1);

				return true;
			}
			return false;
		}

		FCodecPtr FindCodecByFamilyName(
			FName InFamilyName, int32 InVersion) const override
		{
			// Lock for reading.
			FRWScopeLock ScopeLock(ContainersLock, SLT_ReadOnly);
			
			// We might want to pass in the platform name of the codec, but for now assume we mean the current one.
			FName InPlatformName = FPlatformMisc::GetUBTPlatform();

			// Find matching family. 
			if( const TArray<FCodecPtr> *pCodecArray = FamilyNameLookupMap.Find(InFamilyName) )
			{
				// Match version+platform.
				FCodecPtr const * pCodec = pCodecArray->FindByPredicate([InVersion, InPlatformName](const FCodecPtr& i) -> bool 
				{ 
					// Version INDEX_NONE will return the latest version. 
					return (i->GetDetails().Version == InVersion || InVersion == INDEX_NONE) && i->SupportsPlatform(InPlatformName);	
				} );

				if( pCodec )
				{
					return *pCodec;
				}
			}
			return nullptr;
		}

		FCodecPtr FindCodecByName(
			FName InName, int32 InVersion ) const override
		{	
			if(!audio_ensure(InName.IsValid()))
			{
				return nullptr;
			}

			// Lock for reading.
			FRWScopeLock ScopeLock(ContainersLock, SLT_ReadOnly);

			if (TArray<FCodecPtr> const *pVersionArray = NameLookupMap.Find(InName))
			{
				if (pVersionArray->Num() > 0)
				{
					// Asking for latest?
					if (InVersion == INDEX_NONE)
					{
						// First item is the latest.
						audio_ensure((*pVersionArray)[0]);
						return (*pVersionArray)[0];
					}
					else
					{
						// Find a specific version. (todo: use binary search)
						if (FCodecPtr const *pFound = pVersionArray->FindByPredicate([InVersion](const FCodecPtr& i) -> bool { return i->GetDetails().Version == InVersion; }))
						{
							audio_ensure(pFound);
							return *pFound;
						}
					}
				}				
			}
			return nullptr;
		}
		
		virtual ICodecRegistry::FCodecPtr FindCodecByParsingInput(
			IDecoderInput* InObject) const override
		{
			if( !audio_ensure(InObject))
			{
				return nullptr;
			}
			
			// Look in the input stream for the header sections.
			FFormatDescriptorSection Format;
			if( !audio_ensure(InObject->FindSection(Format)) )
			{
				return nullptr;
			}

			FCodecPtr Ptr = FindCodecByName(
				Format.CodecName,
				Format.CodecVersion);

			if( Ptr )
			{
				return Ptr;
			}

			FCodecPtr FamilyPtr = FindCodecByFamilyName(
				Format.CodecFamilyName,
				Format.CodecVersion);
			
			if (FamilyPtr)
			{
				return FamilyPtr;
			}


			// Fail.
			return nullptr;		
		}

		FCodecPtr FindDefaultCodec(
			FName InPlatformName) const override
		{
			// If we didn't specify a platform codec, use the current 
			if( InPlatformName.IsNone() )
			{
				InPlatformName = FPlatformMisc::GetUBTPlatform();
			}

			// Temp. We need to have config settings to define this but for now, hard-code PCM.
			static const FName NAME_Pcm = TEXT("Pcm");
			if( FCodecPtr pPcmCodec = FindCodecByFamilyName(NAME_Pcm, INDEX_NONE))
			{
				return pPcmCodec;
			}					   

			UE_LOG(LogAudioCodec, Warning, TEXT("No default codec defined for %s"), *InPlatformName.GetPlainNameString());
			return nullptr;
		}

	public:		
		virtual ~FCodecRegistry()
		{}

		FCodecRegistry()
		{
			/*IModularFeatures::Get().OnModularFeatureRegistered().AddLambda([this](const FName& Type, IModularFeature* ModularFeature) 
			{
				if (Type == ICodec::GetModularFeatureName() && ModularFeature)
				{
					RegisterCodec(FCodecPtr(ModularFeature));
				}
			});

			IModularFeatures::Get().OnModularFeatureUnregistered().AddLambda([this](const FName& Type, IModularFeature* ModularFeature) 
			{
				if (Type == ICodec::GetModularFeatureName() && ModularFeature)
				{
					UnregisterCodec(FCodecPtr(ModularFeature));
				}
			});			*/
		}

	};

	// Static instance.
	TUniquePtr<Audio::ICodecRegistry> ICodecRegistry::Instance;

	ICodecRegistry& ICodecRegistry::Get()
	{
		// FIXME: Protect against race for the construction of the singleton.
		if (!Instance.IsValid())
		{
			Instance = MakeUnique<FCodecRegistry>();
		}
		return *Instance;
	}

} //namespace Audio
