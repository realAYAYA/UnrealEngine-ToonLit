// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocolDMX.h"

#include "DMXConversions.h"
#include "DMXProtocolSettings.h"
#include "DMXProtocolTypes.h"
#include "RemoteControlLogger.h"
#include "RemoteControlProtocolDMXObjectVersion.h"
#include "RemoteControlProtocolDMXSettings.h"
#include "Interfaces/IDMXProtocol.h"
#include "IO/DMXInputPort.h"
#include "IO/DMXPortManager.h"

#if WITH_EDITOR
#include "IRCProtocolBindingList.h"
#include "IRemoteControlProtocolWidgetsModule.h"
#endif

#define LOCTEXT_NAMESPACE "RemoteControlProtocolDMX"

const FName FRemoteControlProtocolDMX::ProtocolName = TEXT("DMX");

#if WITH_EDITOR

namespace RemoteControlDMXProtocolColumns
{
	static FName Channel = TEXT("Channel");
	static FName Universe = TEXT("Universe");
}

#endif // WITH_EDITOR

uint8 FRemoteControlDMXProtocolEntity::GetRangePropertySize() const
{
	switch (ExtraSetting.DataType)
	{
		default:
		case EDMXFixtureSignalFormat::E8Bit:
			return sizeof(uint8);
			
		case EDMXFixtureSignalFormat::E16Bit:
			return sizeof(uint16);

		// @note 24 bit ints are not available natively, so store as 32bit/4 bytes. This will also affect property clamping.
		case EDMXFixtureSignalFormat::E24Bit:
			return sizeof(uint32);
			
		case EDMXFixtureSignalFormat::E32Bit:
			return sizeof(uint32);
	}
}

const FString& FRemoteControlDMXProtocolEntity::GetRangePropertyMaxValue() const
{
	switch (ExtraSetting.DataType)
	{
		default:
		case EDMXFixtureSignalFormat::E8Bit:
			{
				static const FString UInt8Str = FString::FromInt(TNumericLimits<uint8>::Max());
				return UInt8Str;
			}
				
		case EDMXFixtureSignalFormat::E16Bit:
			{
				static const FString UInt16Str = FString::FromInt(TNumericLimits<uint16>::Max());
				return UInt16Str;
			}

		// @note: This is for the UI so it can be anything, independent of serialization requirements.
		case EDMXFixtureSignalFormat::E24Bit:
			{
				static const FString UInt24Str = FString::FromInt((1 << 24) - 1);
				return UInt24Str;
			}
			
		case EDMXFixtureSignalFormat::E32Bit:
			{
				// FString::FromInt doesn't support values beyond 32 bit signed ints, so use FText::AsNumber
				static const FString UInt32Str = FText::AsNumber(TNumericLimits<uint32>::Max(), &FNumberFormattingOptions::DefaultNoGrouping()).ToString();
				return UInt32Str;
			}
	}
}

#if WITH_EDITOR

void FRemoteControlDMXProtocolEntity::RegisterProperties()
{
	EXPOSE_PROTOCOL_PROPERTY(RemoteControlDMXProtocolColumns::Universe, FRemoteControlDMXProtocolEntityExtraSetting, Universe);
	
	EXPOSE_PROTOCOL_PROPERTY(RemoteControlDMXProtocolColumns::Channel, FRemoteControlDMXProtocolEntityExtraSetting, StartingChannel);
}

#endif // WITH_EDITOR

void FRemoteControlDMXProtocolEntity::Initialize()
{
	// Handle port changes
	FDMXPortManager::Get().OnPortsChanged.AddRaw(this, &FRemoteControlDMXProtocolEntity::UpdateInputPort);

	// Handle project setting changes
	const URemoteControlProtocolDMXSettings* ProtocolDMXSettings = GetDefault<URemoteControlProtocolDMXSettings>();
	ProtocolDMXSettings->GetOnRemoteControlProtocolDMXSettingsChanged().AddRaw(this, &FRemoteControlDMXProtocolEntity::UpdateInputPort);

    // Assign InputPortReference
	UpdateInputPort();
}

void FRemoteControlDMXProtocolEntity::UpdateInputPort()
{
	if (ExtraSetting.bUseDefaultInputPort || !ExtraSetting.InputPortId.IsValid() || !FDMXPortManager::Get().FindInputPortByGuid(ExtraSetting.InputPortId))
	{
		URemoteControlProtocolDMXSettings* RemoteControlDMXSettings = GetMutableDefault<URemoteControlProtocolDMXSettings>();
		if (RemoteControlDMXSettings)
		{
			const FGuid& DefaultInputPortId = RemoteControlDMXSettings->GetOrCreateDefaultInputPortId();
			FDMXInputPortSharedPtr DefaultInputPort = FDMXPortManager::Get().FindInputPortByGuid(DefaultInputPortId);
			if (DefaultInputPort.IsValid())
			{
				ExtraSetting.InputPortId = DefaultInputPortId;
			}
		}
	}
}

bool FRemoteControlDMXProtocolEntity::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FRemoteControlProtocolDMXObjectVersion::GUID);
	
	// Don't actually serialize, just write the custom version for PostSerialize
	return false;
}

void FRemoteControlDMXProtocolEntity::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FRemoteControlProtocolDMXObjectVersion::GUID) < FRemoteControlProtocolDMXObjectVersion::MoveRemoteControlProtocolDMXEntityPropertiesToExtraSettingStruct)
		{
			// Move relevant properties that were moved to the the ExtraSetting member in 5.0 to the ExtraSetting member, so they can be customized. 
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			ExtraSetting.bUseDefaultInputPort = bUseDefaultInputPort_DEPRECATED;
			ExtraSetting.bUseLSB = bUseLSB_DEPRECATED;
			ExtraSetting.DataType = DataType_DEPRECATED;
			ExtraSetting.InputPortId = InputPortId_DEPRECATED;
			ExtraSetting.Universe = Universe_DEPRECATED;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

void FRemoteControlProtocolDMX::Bind(FRemoteControlProtocolEntityPtr InRemoteControlProtocolEntityPtr)
{
	if (!ensure(InRemoteControlProtocolEntityPtr.IsValid()))
	{
		return;
	}

	FRemoteControlDMXProtocolEntity* DMXProtocolEntity = InRemoteControlProtocolEntityPtr->CastChecked<FRemoteControlDMXProtocolEntity>();

	FRemoteControlProtocolEntityWeakPtr* ExistingProtocolBindings = ProtocolsBindings.FindByPredicate([DMXProtocolEntity](const FRemoteControlProtocolEntityWeakPtr& InProtocolEntity)
		{
			if (const FRemoteControlProtocolEntityPtr& ProtocolEntity = InProtocolEntity.Pin())
			{
				const FRemoteControlDMXProtocolEntity* ComparedDMXProtocolEntity = ProtocolEntity->CastChecked<FRemoteControlDMXProtocolEntity>();

				if (ComparedDMXProtocolEntity->ExtraSetting.Universe == DMXProtocolEntity->ExtraSetting.Universe &&
					ComparedDMXProtocolEntity->ExtraSetting.StartingChannel == DMXProtocolEntity->ExtraSetting.StartingChannel &&
					ComparedDMXProtocolEntity->GetPropertyId() == DMXProtocolEntity->GetPropertyId())
				{
					return true;
				}
			}
			
			return false;
		});


	if (ExistingProtocolBindings == nullptr)
	{
		ProtocolsBindings.Emplace(InRemoteControlProtocolEntityPtr);
	}

	DMXProtocolEntity->Initialize();
}

void FRemoteControlProtocolDMX::Unbind(FRemoteControlProtocolEntityPtr InRemoteControlProtocolEntityPtr)
{
	if (!ensure(InRemoteControlProtocolEntityPtr.IsValid()))
	{
		return;
	}

	const FRemoteControlDMXProtocolEntity* DMXProtocolEntity = InRemoteControlProtocolEntityPtr->CastChecked<FRemoteControlDMXProtocolEntity>();
	ProtocolsBindings.RemoveAllSwap(CreateProtocolComparator(DMXProtocolEntity->GetPropertyId()));
}

void FRemoteControlProtocolDMX::OnEndFrame()
{
	if (!ProtocolsBindings.Num())
	{
		return;
	}

	const TArray<FDMXInputPortSharedRef>& InputPorts = FDMXPortManager::Get().GetInputPorts();
	for (const FRemoteControlProtocolEntityWeakPtr& ProtocolEntityWeakPtr : ProtocolsBindings)
	{
		if (const FRemoteControlProtocolEntityPtr& ProtocolEntity = ProtocolEntityWeakPtr.Pin())
		{
			FRemoteControlDMXProtocolEntity* DMXProtocolEntity = ProtocolEntity->CastChecked<FRemoteControlDMXProtocolEntity>();

			const FGuid& PortId =  DMXProtocolEntity->ExtraSetting.InputPortId;				

			const FDMXInputPortSharedRef* InputPortPtr = InputPorts.FindByPredicate([&PortId](const FDMXInputPortSharedRef& InputPort) {
                return InputPort->GetPortGuid() == PortId;
                });

			if (InputPortPtr == nullptr)
			{
				break;
			}

			const FDMXInputPortSharedRef& InputPort = *InputPortPtr;

			// Get universe DMX signal
			InputPort->GameThreadGetDMXSignal(DMXProtocolEntity->ExtraSetting.Universe, DMXProtocolEntity->LastSignalPtr);
			if (DMXProtocolEntity->LastSignalPtr.IsValid())
			{
				const FDMXSignalSharedPtr& LastSignalPtr = DMXProtocolEntity->LastSignalPtr;
				const int32 DMXOffset = DMXProtocolEntity->ExtraSetting.StartingChannel - 1;
				check(DMXOffset >= 0 && DMXOffset < DMX_UNIVERSE_SIZE);

#if WITH_EDITOR
				ProcessAutoBinding(ProtocolEntity);
#endif
				
				ProcessAndApplyProtocolValue(LastSignalPtr, DMXOffset, ProtocolEntity);
			}
		}
	}

	FRemoteControlProtocol::OnEndFrame();
}

void FRemoteControlProtocolDMX::ProcessAndApplyProtocolValue(const FDMXSignalSharedPtr& InSignal, int32 InDMXOffset, const FRemoteControlProtocolEntityPtr& InProtocolEntityPtr)
{
	if (!InSignal->ChannelData.IsValidIndex(InDMXOffset))
	{
		return;
	}
	
	FRemoteControlDMXProtocolEntity* DMXProtocolEntity = InProtocolEntityPtr->CastChecked<FRemoteControlDMXProtocolEntity>();
	const uint8* ChannelData = &InSignal->ChannelData[InDMXOffset];	
	const uint8 NumChannelsToOccupy = FDMXConversions::GetSizeOfSignalFormat(DMXProtocolEntity->ExtraSetting.DataType);

	if(DMXProtocolEntity->CacheDMXBuffer.Num() != NumChannelsToOccupy ||
		FMemory::Memcmp(DMXProtocolEntity->CacheDMXBuffer.GetData(), ChannelData, NumChannelsToOccupy) != 0)
	{
		const uint32 DMXValue = UDMXEntityFixtureType::BytesToInt(DMXProtocolEntity->ExtraSetting.DataType, DMXProtocolEntity->ExtraSetting.bUseLSB, ChannelData);
		
#if WITH_EDITOR
		FRemoteControlLogger::Get().Log(ProtocolName, [&InSignal, DMXValue]
		{
			return FText::Format(LOCTEXT("DMXEventLog","ExternUniverseID {0}, DMXValue {1}"), InSignal->ExternUniverseID, DMXValue);
		});
#endif

		QueueValue(InProtocolEntityPtr, DMXValue);

		// update cached buffer
		DMXProtocolEntity->CacheDMXBuffer = TArray<uint8>(ChannelData, NumChannelsToOccupy);
	}
}

#if WITH_EDITOR
void FRemoteControlProtocolDMX::ProcessAutoBinding(const FRemoteControlProtocolEntityPtr& InProtocolEntityPtr)
{
	// Bind only in Editor
	if (!GIsEditor)
	{
		return;
	}

	// Check if entity is valid
	if (!InProtocolEntityPtr.IsValid())
	{
		return;
	}

	FRemoteControlDMXProtocolEntity* DMXProtocolEntity = InProtocolEntityPtr->CastChecked<FRemoteControlDMXProtocolEntity>();
	
	// Assign binding
	IRemoteControlProtocolWidgetsModule& RCWidgetsModule = IRemoteControlProtocolWidgetsModule::Get();
	const TSharedPtr<IRCProtocolBindingList> RCProtocolBindingList = RCWidgetsModule.GetProtocolBindingList();
	if (RCProtocolBindingList.IsValid())
	{
		if (DMXProtocolEntity->GetBindingStatus() == ERCBindingStatus::Awaiting)
		{
			const TArray<uint8>& ChannelData = DMXProtocolEntity->LastSignalPtr->ChannelData;
			int32 FoundChannelDifference = -1;
			uint8 FoundChannelDifferenceValue = 0;

			if (CacheUniverseDMXBuffer.Num())
			{
				if (ensure(ChannelData.Num() == CacheUniverseDMXBuffer.Num()))
				{
					// Compare buffers
					for (int32 ChannelValueIndex = 0; ChannelValueIndex < ChannelData.Num(); ++ChannelValueIndex)
					{
						const uint8 SignalChannelValue = ChannelData[ChannelValueIndex];
						if (CacheUniverseDMXBuffer[ChannelValueIndex] != SignalChannelValue)
						{
							FoundChannelDifference = ChannelValueIndex;
							FoundChannelDifferenceValue = SignalChannelValue;
							break;
						}
					}	
				}
			}

			if  (FoundChannelDifference >= 0)
			{
				Unbind(InProtocolEntityPtr);
				const int32 FinalChannelValue = FoundChannelDifference + 1;
				DMXProtocolEntity->ExtraSetting.StartingChannel = FinalChannelValue;
				Bind(InProtocolEntityPtr);

				// Print to log
				const FDMXSignalSharedPtr& LastSignalPtr = DMXProtocolEntity->LastSignalPtr;
				FRemoteControlLogger::Get().Log(ProtocolName, [&LastSignalPtr, FinalChannelValue, FoundChannelDifferenceValue]
				{
					return FText::Format(
						LOCTEXT("DMXEventLog2",
								"AutoBinding new value. ExternUniverseID {0}, Channel {1}, New Value {2}"),
						LastSignalPtr->ExternUniverseID, FinalChannelValue, FoundChannelDifferenceValue);
				});
			}

			// Copy buffer
			CacheUniverseDMXBuffer = DMXProtocolEntity->LastSignalPtr->ChannelData;
		}
	}
}

void FRemoteControlProtocolDMX::RegisterColumns()
{
	FRemoteControlProtocol::RegisterColumns();

	REGISTER_COLUMN(RemoteControlDMXProtocolColumns::Channel
		, LOCTEXT("RCPresetChannelColumnHeader", "Channel")
		, ProtocolColumnConstants::ColumnSizeMicro);

	REGISTER_COLUMN(RemoteControlDMXProtocolColumns::Universe
		, LOCTEXT("RCPresetUniverseColumnHeader", "Universe")
		, ProtocolColumnConstants::ColumnSizeMicro);
}

#endif

void FRemoteControlProtocolDMX::UnbindAll()
{
	ProtocolsBindings.Empty();
}

#undef LOCTEXT_NAMESPACE
