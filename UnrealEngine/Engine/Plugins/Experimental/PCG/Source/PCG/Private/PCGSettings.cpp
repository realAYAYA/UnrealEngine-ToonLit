// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSettings.h"
#include "PCGNode.h"
#include "PCGSubsystem.h"
#include "Serialization/ArchiveObjectCrc32.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

/** In order to reuse the cache when only debug settings change, we must make sure to ignore these from the CRC check */
class FPCGSettingsObjectCrc32 : public FArchiveObjectCrc32
{
public:
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
	{
#if WITH_EDITORONLY_DATA
		return InProperty && (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DebugSettings) || InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DeterminismSettings));
#else
		return FArchiveObjectCrc32::ShouldSkipProperty(InProperty);
#endif // WITH_EDITORONLY_DATA
	}
};

bool UPCGSettings::operator==(const UPCGSettings& Other) const
{
	if (this == &Other)
	{
		return true;
	}
	else
	{
		FPCGSettingsObjectCrc32 Ar;
		uint32 ThisCrc = Ar.Crc32(const_cast<UPCGSettings*>(this));
		uint32 OtherCrc = Ar.Crc32(const_cast<UPCGSettings*>(&Other));
		return ThisCrc == OtherCrc;
	}
}

uint32 UPCGSettings::GetCrc32() const
{
	FPCGSettingsObjectCrc32 Ar;
	return Ar.Crc32(const_cast<UPCGSettings*>(this));
}

#if WITH_EDITOR
UObject* UPCGSettings::GetJumpTargetForDoubleClick() const
{
	return const_cast<UObject*>(Cast<UObject>(this));
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// This is not true for everything, use a virtual call?
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

FPCGElementPtr UPCGSettings::GetElement() const
{
	if (!CachedElement)
	{
		CacheLock.Lock();

		if (!CachedElement)
		{
			CachedElement = CreateElement();
		}

		CacheLock.Unlock();
	}

	return CachedElement;
}

UPCGNode* UPCGSettings::CreateNode() const
{
	return NewObject<UPCGNode>();
}

#if WITH_EDITOR
void UPCGSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() != GET_MEMBER_NAME_CHECKED(UPCGSettings, DeterminismSettings))
	{
		OnSettingsChangedDelegate.Broadcast(this, IsStructuralProperty(PropertyChangedEvent.GetPropertyName()) ? EPCGChangeType::Structural : EPCGChangeType::Settings);
	}
}

void UPCGSettings::DirtyCache()
{
	if (GEditor)
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		UPCGSubsystem* PCGSubsystem = World ? World->GetSubsystem<UPCGSubsystem>() : nullptr;
		if (PCGSubsystem)
		{
			PCGSubsystem->CleanFromCache(GetElement().Get());
		}
	}
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGSettings::DefaultPointOutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Properties;
}

FPCGElementPtr UPCGTrivialSettings::CreateElement() const
{
	return MakeShared<FPCGTrivialElement>();
}

bool FPCGTrivialElement::ExecuteInternal(FPCGContext* Context) const
{
	// Pass-through
	Context->OutputData = Context->InputData;
	return true;
}