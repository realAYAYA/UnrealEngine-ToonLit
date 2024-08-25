// Copyright Epic Games, Inc. All Rights Reserved.

#include "Broadcast/AvaBroadcast.h"
#include "Async/Async.h"
#include "AvaMediaSerializationUtils.h"
#include "Containers/UnrealString.h"
#include "Delegates/IDelegateInstance.h"
#include "Formatters/XmlArchiveInputFormatter.h"
#include "Formatters/XmlArchiveOutputFormatter.h"
#include "HAL/FileManager.h"
#include "IAvaMediaModule.h"
#include "Misc/Paths.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY(LogAvaBroadcast);

#define LOCTEXT_NAMESPACE "AvaBroadcast"

namespace UE::AvaBroadcast::Private
{
	// Tested with server in game and editor modes: This function is called only when the server receives some broadcast queries.
	static FString GetXmlSaveFilepath()
	{
		FString BroadcastConfigName;

		// Allowing command line specification of the broadcast configuration file name. This allows
		// starting the same project (from a shared location) with different configurations.
		if (FParse::Value(FCommandLine::Get(),TEXT("MotionDesignBroadcastConfig="), BroadcastConfigName))
		{
			// Ensure it has an xml extension.
			if (!FPaths::GetExtension(BroadcastConfigName).Equals(TEXT("xml"), ESearchCase::IgnoreCase))
			{
				// Appending extension instead of using SetExtension to respect
				// dot naming convention for xml/yaml config files.
				BroadcastConfigName += TEXT(".xml");
			}
		}
		else
		{
			// When launching the server from the same project location, we want to avoid loading the same
			// broadcast configuration as the client. The server needs a clean configuration.
			const bool bIsServerRunning = (IAvaMediaModule::IsModuleLoaded() && IAvaMediaModule::Get().IsPlaybackServerStarted());	
			BroadcastConfigName = bIsServerRunning ? TEXT("MotionDesignServerBroadcastConfig.xml") : TEXT("MotionDesignBroadcastConfig.xml");
		}
		
		return FPaths::ProjectConfigDir() / BroadcastConfigName;
	}
}

UAvaBroadcast& UAvaBroadcast::Get()
{
	static UAvaBroadcast* Broadcast = nullptr;
	
	if (!IsValid(Broadcast))
	{
		static const TCHAR* PackageName = TEXT("/Temp/AvaMedia/AvaBroadcast");
		
		UPackage* const BroadcastPackage = CreatePackage(PackageName);
		BroadcastPackage->SetFlags(RF_Transient);
		BroadcastPackage->AddToRoot();

		//Don't Mark as Transient for "marking package dirty"
		Broadcast = NewObject<UAvaBroadcast>(BroadcastPackage
			, TEXT("AvaBroadcast")
			, RF_Transactional | RF_Standalone);

		Broadcast->AddToRoot();
#if WITH_EDITOR
		Broadcast->LoadBroadcast();
#else
		Broadcast->CreateProfile(NAME_None, /*bMakeCurrentProfile*/ true);
		Broadcast->EnsureValidCurrentProfile();
		Broadcast->UpdateProfileNames();
#endif
	}
	
	check(Broadcast);
	return *Broadcast;
}

void UAvaBroadcast::BeginDestroy()
{
	for (TPair<FName, FAvaBroadcastProfile>& Pair : Profiles)
	{
		Pair.Value.BeginDestroy();
	}
	Super::BeginDestroy();
}

UAvaBroadcast* UAvaBroadcast::GetBroadcast()
{
	return &UAvaBroadcast::Get();
}

void UAvaBroadcast::StartBroadcast()
{
	if (IsBroadcastingAllChannels())
	{
		return;
	}

	FAvaBroadcastProfile& Profile = GetCurrentProfile();
	Profile.StartChannelBroadcast();
}

void UAvaBroadcast::StopBroadcast()
{
	GetCurrentProfile().StopChannelBroadcast();
}

bool UAvaBroadcast::IsBroadcastingAnyChannel() const
{
	return GetCurrentProfile().IsBroadcastingAnyChannel();
}

bool UAvaBroadcast::IsBroadcastingAllChannels() const
{
	return GetCurrentProfile().IsBroadcastingAllChannels();
}

bool UAvaBroadcast::ConditionalStartBroadcastChannel(const FName& InChannelName)
{
	FAvaBroadcastOutputChannel& Channel = GetCurrentProfile().GetChannelMutable(InChannelName);
	if (!Channel.IsValidChannel())
	{
		UE_LOG(LogAvaBroadcast, Error,
			TEXT("Start Broadcast failed: Channel \"%s\" of Profile \"%s\" is not valid."),
			*InChannelName.ToString(), *GetCurrentProfileName().ToString());
		return false;
	}
	
	if (Channel.GetState() == EAvaBroadcastChannelState::Idle)
	{
		Channel.StartChannelBroadcast();
	}
	return true;
}


TArray<FName> UAvaBroadcast::GetProfileNames() const
{
	TArray<FName> ProfileNames;
	Profiles.GetKeys(ProfileNames);
	return ProfileNames;
}

const TMap<FName, FAvaBroadcastProfile>& UAvaBroadcast::GetProfiles() const
{
	return Profiles;
}

FName UAvaBroadcast::CreateProfile(FName InProfileName, bool bMakeCurrentProfile)
{
	if (InProfileName == NAME_None)
	{
		InProfileName = FName(TEXT("Profile"), 0);
	}
	
	FAvaBroadcastProfile& Profile = CreateProfileInternal(InProfileName);
	
	if (bMakeCurrentProfile)
	{
		SetCurrentProfile(Profile.GetName());
	}
	
	return Profile.GetName();
}

bool UAvaBroadcast::DuplicateProfile(FName InNewProfile, FName InTemplateProfile, bool bMakeCurrentProfile)
{
	const FAvaBroadcastProfile* const TemplateProfile = Profiles.Find(InTemplateProfile);

	if (TemplateProfile)
	{		
		if (InNewProfile == NAME_None)
		{
			InNewProfile = InTemplateProfile;
		}

		FAvaBroadcastProfile& Profile = CreateProfileInternal(InNewProfile);
		FAvaBroadcastProfile::CopyProfiles(*TemplateProfile, Profile);
		
		if (bMakeCurrentProfile)
		{
			SetCurrentProfile(Profile.GetName());
		}
		
		return true;
	}
	
	return false;
}

bool UAvaBroadcast::DuplicateCurrentProfile(FName InProfileName, bool bMakeCurrentProfile)
{
	return DuplicateProfile(InProfileName, CurrentProfile, bMakeCurrentProfile);
}

bool UAvaBroadcast::RemoveProfile(FName InProfileName)
{
	const bool bRemovingCurrentProfile = CurrentProfile == InProfileName;
	const bool bIsLastRemainingProfile = Profiles.Num() == 1;
	const bool bIsBroadcasting = IsBroadcastingAnyChannel();
	
	//The only condition that would prevent us from doing Removal is if we're currently Broadcasting and we want to remove Current Profile.
	const bool bCanRemoveProfile = !bIsLastRemainingProfile && !(bIsBroadcasting && bRemovingCurrentProfile);
	
	if (bCanRemoveProfile)
	{
		const int32 RemoveCount = Profiles.Remove(InProfileName);

		//If Removing Current Profile, we need to find a new Current Profile
		if (bRemovingCurrentProfile)
		{
			CurrentProfile = NAME_None;
			EnsureValidCurrentProfile();
		}
		
		return RemoveCount > 0;
	}
	return false;
}

bool UAvaBroadcast::CanRenameProfile(FName InProfileName, FName InNewProfileName, FText* OutErrorMessage) const
{
	if (InNewProfileName.IsNone())
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = LOCTEXT("RenameError_ProfileNone", "Invalid profile name.");
		}
		return false;
	}

	if (Profiles.Contains(InNewProfileName))
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = LOCTEXT("RenameError_ProfileExists", "Profile name already exists.");
		}
		return false;
	}
	
	const bool bRenamingCurrentProfile = (CurrentProfile == InProfileName);
	const bool bIsBroadcasting = IsBroadcastingAnyChannel();

	if (bIsBroadcasting && bRenamingCurrentProfile)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = LOCTEXT("RenameError_ProfileInUse", "Profile is currently Broadcasting Channels.");
		}
		return false;
	}
	
	return true;
}

bool UAvaBroadcast::RenameProfile(FName InProfileName, FName InNewProfileName)
{
	if (CanRenameProfile(InProfileName, InNewProfileName))
	{
		FAvaBroadcastProfile Profile = MoveTemp(Profiles[InProfileName]);
		Profiles.Remove(InProfileName);
		Profile.ProfileName = InNewProfileName;
		Profiles.Add(InNewProfileName, MoveTemp(Profile));
		
		if (CurrentProfile == InProfileName)
		{
			CurrentProfile = InNewProfileName;
		}
		return true;
	}
	return false;
}

bool UAvaBroadcast::SetCurrentProfile(FName InProfileName)
{
	const bool bIsBroadcasting = IsBroadcastingAnyChannel();
	
	//Can only set a new Current Profile if not Broadcasting.
	if (!bIsBroadcasting && CurrentProfile != InProfileName && Profiles.Contains(InProfileName))
	{
		if (GetCurrentProfile().IsValidProfile())
		{
			// Deallocate previous profile's resources.
			GetCurrentProfile().UpdateChannels(false);
		}
		
		CurrentProfile = InProfileName;
		
		// Allocate new profile's resources.
		GetCurrentProfile().UpdateChannels(true);
		
		QueueNotifyChange(EAvaBroadcastChange::CurrentProfile);
		return true;
	}
	return false;
}

FAvaBroadcastProfile& UAvaBroadcast::GetProfile(FName InProfileName)
{
	if (Profiles.Contains(InProfileName))
	{
		return Profiles[InProfileName];
	}
	return FAvaBroadcastProfile::GetNullProfile();
}

const FAvaBroadcastProfile& UAvaBroadcast::GetProfile(FName InProfileName) const
{
	if (Profiles.Contains(InProfileName))
	{
		return Profiles[InProfileName];
	}
	return FAvaBroadcastProfile::GetNullProfile();
}

#if WITH_EDITOR
void UAvaBroadcast::LoadBroadcast()
{
	using namespace UE::AvaBroadcast::Private;
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*GetXmlSaveFilepath()));

	if (FileReader.IsValid())
	{
		FXmlArchiveInputFormatter InputFormatter(*FileReader, this);
		UE::AvaMediaSerializationUtils::SerializeObject(InputFormatter, this);
		FileReader->Close();
	}
	
	if (Profiles.Num() > 0)
	{
		for (TPair<FName, FAvaBroadcastProfile>& Profile : Profiles)
		{
			const bool bIsProfileActive = (Profile.Key == CurrentProfile);
			Profile.Value.PostLoadProfile(bIsProfileActive, this);
		}
	}
	else
	{
		CreateProfile(NAME_None, true);
		SaveBroadcast();
	}
	
	EnsureValidCurrentProfile();
	UpdateProfileNames();
}

void UAvaBroadcast::SaveBroadcast()
{
	bool bIsBroadcastSaved = false;
	using namespace UE::AvaBroadcast::Private;
	TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*GetXmlSaveFilepath()));
	if (FileWriter.IsValid())	
	{
		FXmlArchiveOutputFormatter XmlOutput(*FileWriter);
		XmlOutput.SerializeObjectsInPlace(true); // We want the media outputs nested.
		UE::AvaMediaSerializationUtils::SerializeObject(XmlOutput, this);
		bIsBroadcastSaved = XmlOutput.SaveDocumentToInnerArchive();
		GetPackage()->SetDirtyFlag(false);
		FileWriter->Close();
	}
	if (!bIsBroadcastSaved)
	{
		UE_LOG(LogAvaBroadcast, Error, TEXT("Failed to save broadcast configuration.")); 
	}
}

FString UAvaBroadcast::GetBroadcastSaveFilepath() const
{
	return UE::AvaBroadcast::Private::GetXmlSaveFilepath();
}
#endif

void UAvaBroadcast::QueueNotifyChange(EAvaBroadcastChange InChange)
{
	if (InChange != EAvaBroadcastChange::None)
	{		
		const bool bCreateAsyncTask = (QueuedBroadcastChanges == EAvaBroadcastChange::None);
		EnumAddFlags(QueuedBroadcastChanges, InChange);

		if (bCreateAsyncTask)
		{
			TWeakObjectPtr<UAvaBroadcast> ThisWeak(this);
			AsyncTask(ENamedThreads::GameThread, [ThisWeak]()
			{
				if (ThisWeak.IsValid())
				{
					ThisWeak->OnBroadcastChanged.Broadcast(ThisWeak->QueuedBroadcastChanges);
					ThisWeak->QueuedBroadcastChanges = EAvaBroadcastChange::None;
				}
			});	
		}
	}
}

FDelegateHandle UAvaBroadcast::AddChangeListener(FOnAvaBroadcastChanged::FDelegate&& InDelegate)
{
	return OnBroadcastChanged.Add(MoveTemp(InDelegate));
}

void UAvaBroadcast::RemoveChangeListener(FDelegateHandle InDelegateHandle)
{
	OnBroadcastChanged.Remove(InDelegateHandle);
}

void UAvaBroadcast::RemoveChangeListener(const void* InUserObject)
{
	OnBroadcastChanged.RemoveAll(InUserObject);
}

int32 UAvaBroadcast::GetChannelNameCount() const
{
	return ChannelNames.Num();
}

int32 UAvaBroadcast::GetChannelIndex(FName InChannelName) const
{
	return ChannelNames.Find(InChannelName);
}

FName UAvaBroadcast::GetChannelName(int32 ChannelIndex) const
{
	if (ChannelNames.IsValidIndex(ChannelIndex))
	{
		return ChannelNames[ChannelIndex];
	}
	return NAME_None;
}

FName UAvaBroadcast::GetOrAddChannelName(int32 ChannelIndex)
{
	if (ChannelIndex < 0)
	{
		return NAME_None;
	}
	
	if (ChannelIndex < ChannelNames.Num())
	{
		return ChannelNames[ChannelIndex];
	}
	
	//Generate new items and set a unique names for them
	{
		//Store the current names as a Set for Fast Search
		TSet ChannelNamesSet(ChannelNames);

		//Add the New Items as Defaulted
		const int32 OldItemCount = ChannelNames.Num();
		ChannelNames.AddDefaulted(ChannelIndex - OldItemCount + 1);
		ChannelNamesSet.Reserve(ChannelNames.Num());
		
		FName UniqueName = TEXT("Channel");
		uint32 UniqueIndex = 1;
		
		for (int32 Index = OldItemCount; Index <= ChannelIndex; ++Index)
		{
			do
			{
				UniqueName.SetNumber(UniqueIndex++);
			}
			while (ChannelNamesSet.Contains(UniqueName));
			
			ChannelNamesSet.Add(UniqueName);
			ChannelNames[Index] = UniqueName;
		}
	}
	
	return ChannelNames[ChannelIndex];
}

int32 UAvaBroadcast::AddChannelName(FName InChannelName)
{
	return ChannelNames.AddUnique(InChannelName);
}

TArray<int32> UAvaBroadcast::BuildChannelIndices() const
{
	TArray<int32> ChannelIndices;
	ChannelIndices.Reserve(ChannelNames.Num());
	for (const TPair<FName, FAvaBroadcastProfile>& Pair : Profiles)
	{
		for (const FAvaBroadcastOutputChannel& Channel : Pair.Value.Channels)
		{
			ChannelIndices.AddUnique(Channel.GetChannelIndex());
		}
	}
	return ChannelIndices;
}

void UAvaBroadcast::UpdateChannelNames()
{
	// This is called when a channel is added or removed from a profile.
	// We have to reconcile all channel names from all the profiles.

	// First pass, build the new channel names list.
	//
	// Different profile may have sub-set of channels.
	// Ex:
	// Profile 1: channel1, channel3
	// Profile 2: channel1, channel2
	//	
	// Sorting by channel indices. The channel indices are
	// used for the connections (pins) in the playback graph.
	// So we want to preserve that order.
	TArray<int32> ChannelIndices = BuildChannelIndices();
	ChannelIndices.Sort();

	// Build the new channel names list from the sorted channel indices.
	TArray<FName> NewChannelNames;
	NewChannelNames.Reserve(ChannelIndices.Num());
	for (const int32 Index : ChannelIndices)
	{
		NewChannelNames.Add(GetOrAddChannelName(Index));
	}
	
	if (NewChannelNames == ChannelNames)
	{
		return;
	}
	
	// Update the channel indices in all profiles. 
	for (TPair<FName, FAvaBroadcastProfile>& Pair : Profiles)
	{
		for (FAvaBroadcastOutputChannel& Channel : Pair.Value.Channels)
		{
			Channel.SetChannelIndex(NewChannelNames.Find(Channel.GetChannelName()));
		}
	}

	TArray<FName> RemovedNames;
	for (const FName& PreviousChannelName : ChannelNames)
	{
		if (!NewChannelNames.Contains(PreviousChannelName))
		{
			RemovedNames.Add(PreviousChannelName);
		}
	}
	
	// We can finally update the ChannelNames array.
	// Channel.GetChannelName() (above) was still using old ChannelNames to find new indices.
	ChannelNames = NewChannelNames;

	// Housekeeping for internal data:
	// Remove ChannelType and PinnedChannels entries that where removed.
	for (const FName& RemovedChannelName : RemovedNames)
	{
		ChannelTypes.Remove(RemovedChannelName);
		PinnedChannels.Remove(RemovedChannelName);
	}
}

bool UAvaBroadcast::CanRenameChannel(FName InChannelName, FName InNewChannelName) const
{
	//Make sure new Channel name is valid, and we don't have it already in the List
	return InNewChannelName != NAME_None
		&& InChannelName != InNewChannelName
		&& true == ChannelNames.Contains(InChannelName)
		&& false == ChannelNames.Contains(InNewChannelName);
}

bool UAvaBroadcast::RenameChannel(FName InChannelName, FName InNewChannelName)
{
	if (CanRenameChannel(InChannelName, InNewChannelName))
	{
		const int32 Index = GetChannelIndex(InChannelName);
		check(ChannelNames.IsValidIndex(Index));
		
		ChannelNames[Index] = InNewChannelName;

		if (const EAvaBroadcastChannelType* ExistingChannelType = ChannelTypes.Find(InChannelName))
		{
			const EAvaBroadcastChannelType ExistingChannelTypeCopy = *ExistingChannelType;
			ChannelTypes.Remove(InChannelName);
			ChannelTypes.Add(InNewChannelName, ExistingChannelTypeCopy);
		}

		if (const FName* ExistingPinnedProfileName = PinnedChannels.Find(InChannelName))
		{
			const FName ExistingPinnedProfileNameCopy = *ExistingPinnedProfileName;
			PinnedChannels.Remove(InChannelName);
			PinnedChannels.Add(InNewChannelName, ExistingPinnedProfileNameCopy);
		}
		
		QueueNotifyChange(EAvaBroadcastChange::ChannelRename);
		return true;
	}
	return false;
}

void UAvaBroadcast::SetChannelType(FName InChannelName, EAvaBroadcastChannelType InChannelType)
{
	ChannelTypes.Add(InChannelName, InChannelType);
}

EAvaBroadcastChannelType UAvaBroadcast::GetChannelType(FName InChannelName) const
{
	const EAvaBroadcastChannelType* ChannelType = ChannelTypes.Find(InChannelName);
	// For backward compatibility, if the channel type is not set, defaults to Broadcast.
	return ChannelType ? *ChannelType : EAvaBroadcastChannelType::Program;
}

void UAvaBroadcast::PinChannel(FName InChannelName, FName InProfileName)
{
	PinnedChannels.Add(InChannelName, InProfileName);
}

void UAvaBroadcast::UnpinChannel(FName InChannelName)
{
	PinnedChannels.Remove(InChannelName);
}

FName UAvaBroadcast::GetPinnedChannelProfileName(FName InChannelName) const
{
	const FName* ProfileName = PinnedChannels.Find(InChannelName);
	return ProfileName ? *ProfileName : NAME_None;
}

void UAvaBroadcast::RebuildProfiles()
{
	for (TPair<FName, FAvaBroadcastProfile>& Profile : Profiles)
	{
		const bool bIsProfileActive = Profile.Key == CurrentProfile;
		Profile.Value.UpdateChannels(bIsProfileActive);
	}
}

#if WITH_EDITOR
void UAvaBroadcast::PostEditUndo()
{
	UObject::PostEditUndo();
	for (TPair<FName, FAvaBroadcastProfile>& Profile : Profiles)
	{
		const bool bIsProfileActive = Profile.Key == CurrentProfile;
		Profile.Value.UpdateChannels(bIsProfileActive);
	}
	QueueNotifyChange(EAvaBroadcastChange::All);
}
#endif

FAvaBroadcastProfile& UAvaBroadcast::CreateProfileInternal(FName InProfileName)
{	
	uint32 UniqueIndex = FMath::Max(1, InProfileName.GetNumber());
	
	while (Profiles.Contains(InProfileName))
	{
		InProfileName.SetNumber(++UniqueIndex);
	};
		
	FAvaBroadcastProfile& Profile = Profiles.Add(InProfileName, {this, InProfileName});
	Profile.AddChannel();
	return Profile;
}

void UAvaBroadcast::EnsureValidCurrentProfile()
{
	if (CurrentProfile == NAME_None || !Profiles.Contains(CurrentProfile))
	{
		for (TPair<FName, FAvaBroadcastProfile>& Profile : Profiles)
		{
			SetCurrentProfile(Profile.Key);
			break;
		}
	}
}

void UAvaBroadcast::UpdateProfileNames()
{
	for (TPair<FName, FAvaBroadcastProfile>& Pair : Profiles)
	{
		Pair.Value.ProfileName = Pair.Key;
	}
}

#undef LOCTEXT_NAMESPACE
