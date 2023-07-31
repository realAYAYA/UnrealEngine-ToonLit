// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsRealTimeStylusPlugin.h"

#if PLATFORM_WINDOWS

HRESULT FWindowsRealTimeStylusPlugin::QueryInterface(const IID& InterfaceID, void** Pointer)
{
	if ((InterfaceID == __uuidof(IStylusSyncPlugin)) || (InterfaceID == IID_IUnknown))
	{
		*Pointer = this;
		AddRef();
		return S_OK;
	}
	else if ((InterfaceID == IID_IMarshal) && (FreeThreadedMarshaler != nullptr))
	{
		return FreeThreadedMarshaler->QueryInterface(InterfaceID, Pointer);
	}

	*Pointer = nullptr;
	return E_NOINTERFACE;
}

HRESULT FWindowsRealTimeStylusPlugin::StylusDown(IRealTimeStylus* InRealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketSize, LONG* Packet, LONG** InOutPackets)
{
	FTabletContextInfo* TabletContext = FindTabletContext(StylusInfo->tcid);
	if (TabletContext != nullptr)
	{
		TabletContext->WindowsState.IsTouching = true;
	}
	return S_OK;
}

HRESULT FWindowsRealTimeStylusPlugin::StylusUp(IRealTimeStylus* InRealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketSize, LONG* Packet, LONG** InOutPackets)
{
	FTabletContextInfo* TabletContext = FindTabletContext(StylusInfo->tcid);
	if (TabletContext != nullptr)
	{
		// we know this is not touching
		TabletContext->WindowsState.IsTouching = false;
		TabletContext->WindowsState.NormalPressure = 0;
	}
	return S_OK;
}

static void SetupPacketDescriptions(IRealTimeStylus* InRealTimeStylus, FTabletContextInfo& TabletContext)
{
	ULONG NumPacketProperties = 0;
	PACKET_PROPERTY* PacketProperties = nullptr;
	HRESULT hr = InRealTimeStylus->GetPacketDescriptionData(TabletContext.ID, nullptr, nullptr, &NumPacketProperties, &PacketProperties);
	if (SUCCEEDED(hr) && PacketProperties != nullptr)
	{
		for (ULONG PropIdx = 0; PropIdx < NumPacketProperties; ++PropIdx)
		{
			PACKET_PROPERTY CurrentProperty = PacketProperties[PropIdx];

			EWindowsPacketType PacketType = EWindowsPacketType::None;
			if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_X)
			{
				PacketType = EWindowsPacketType::X;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_Y)
			{
				PacketType = EWindowsPacketType::Y;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_Z)
			{
				PacketType = EWindowsPacketType::Z;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_PACKET_STATUS)
			{
				PacketType = EWindowsPacketType::Status;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE)
			{
				PacketType = EWindowsPacketType::NormalPressure;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_TANGENT_PRESSURE)
			{
				PacketType = EWindowsPacketType::TangentPressure;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_BUTTON_PRESSURE)
			{
				PacketType = EWindowsPacketType::ButtonPressure;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_ALTITUDE_ORIENTATION)
			{
				PacketType = EWindowsPacketType::Altitude;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_AZIMUTH_ORIENTATION)
			{
				PacketType = EWindowsPacketType::Azimuth;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_TWIST_ORIENTATION)
			{
				PacketType = EWindowsPacketType::Twist;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION)
			{
				PacketType = EWindowsPacketType::XTilt;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION)
			{
				PacketType = EWindowsPacketType::YTilt;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_WIDTH)
			{
				PacketType = EWindowsPacketType::Width;
			}
			else if (CurrentProperty.guid == GUID_PACKETPROPERTY_GUID_HEIGHT)
			{
				PacketType = EWindowsPacketType::Height;
			}

			int32 CreatedIdx = TabletContext.PacketDescriptions.Emplace();
			FPacketDescription& PacketDescription = TabletContext.PacketDescriptions[CreatedIdx];
			PacketDescription.Type = PacketType;
			PacketDescription.Minimum = CurrentProperty.PropertyMetrics.nLogicalMin;
			PacketDescription.Maximum = CurrentProperty.PropertyMetrics.nLogicalMax;
			PacketDescription.Resolution = CurrentProperty.PropertyMetrics.fResolution;
		}

		::CoTaskMemFree(PacketProperties);
	}
}

static void SetupTabletSupportedPackets(TComPtr<IRealTimeStylus> InRealTimeStylus, FTabletContextInfo& TabletContext)
{
	IInkTablet* InkTablet;
	InRealTimeStylus->GetTabletFromTabletContextId(TabletContext.ID, &InkTablet);

	int16 Supported;

	BSTR GuidBSTR;
	
	GuidBSTR = SysAllocString(STR_GUID_X);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::X);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_Y);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Y);
	}
	
	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_Z);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Z);
		TabletContext.AddSupportedInput(EStylusInputType::Z);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_PAKETSTATUS);
	
	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Status);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_NORMALPRESSURE);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::NormalPressure);
		TabletContext.AddSupportedInput(EStylusInputType::Pressure);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_TANGENTPRESSURE);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::TangentPressure);
		TabletContext.AddSupportedInput(EStylusInputType::TangentPressure);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_BUTTONPRESSURE);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::ButtonPressure);
		TabletContext.AddSupportedInput(EStylusInputType::ButtonPressure);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_AZIMUTHORIENTATION);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Azimuth);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_ALTITUDEORIENTATION);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Altitude);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_XTILTORIENTATION);
	
	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::XTilt);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_YTILTORIENTATION);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::YTilt);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_TWISTORIENTATION);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Twist);
		TabletContext.AddSupportedInput(EStylusInputType::Twist);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_WIDTH);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Width);
	}

	SysFreeString(GuidBSTR);
	GuidBSTR = SysAllocString(STR_GUID_HEIGHT);

	InkTablet->IsPacketPropertySupported(GuidBSTR, &Supported);
	if (Supported)
	{
		TabletContext.SupportedPackets.Add(EWindowsPacketType::Height);
	}

	SysFreeString(GuidBSTR);

	if (TabletContext.SupportedPackets.Contains(EWindowsPacketType::X) &&
		TabletContext.SupportedPackets.Contains(EWindowsPacketType::Y))
	{
		TabletContext.AddSupportedInput(EStylusInputType::Position);
	}

	if (TabletContext.SupportedPackets.Contains(EWindowsPacketType::XTilt) &&
		TabletContext.SupportedPackets.Contains(EWindowsPacketType::YTilt))
	{
		TabletContext.AddSupportedInput(EStylusInputType::Tilt);
	}

	if (TabletContext.SupportedPackets.Contains(EWindowsPacketType::Width) &&
		TabletContext.SupportedPackets.Contains(EWindowsPacketType::Height))
	{
		TabletContext.AddSupportedInput(EStylusInputType::Size);
	}
}

FTabletContextInfo* FWindowsRealTimeStylusPlugin::FindTabletContext(TABLET_CONTEXT_ID TabletID)
{
	for (FTabletContextInfo& TabletContext : *TabletContexts)
	{
		if (TabletContext.ID == TabletID)
		{
			return &TabletContext;
		}
	}
	return nullptr;
}

void FWindowsRealTimeStylusPlugin::AddTabletContext(IRealTimeStylus* InRealTimeStylus, TABLET_CONTEXT_ID TabletID)
{
	FTabletContextInfo* FoundContext = FindTabletContext(TabletID);
	if (FoundContext == nullptr)
	{
		FoundContext = &TabletContexts->Emplace_GetRef();
		FoundContext->ID = TabletID;
	}

	SetupTabletSupportedPackets(InRealTimeStylus, *FoundContext);
	SetupPacketDescriptions(InRealTimeStylus, *FoundContext);
}

void FWindowsRealTimeStylusPlugin::RemoveTabletContext(IRealTimeStylus* InRealTimeStylus, TABLET_CONTEXT_ID TabletID)
{
	for (int32 ExistingIdx = 0; ExistingIdx < TabletContexts->Num(); ++ExistingIdx)
	{
		if ((*TabletContexts)[ExistingIdx].ID == TabletID)
		{
			TabletContexts->RemoveAt(ExistingIdx);
			break;
		}
	}
}

HRESULT FWindowsRealTimeStylusPlugin::RealTimeStylusEnabled(IRealTimeStylus* InRealTimeStylus, ULONG Num, const TABLET_CONTEXT_ID* InTabletContexts)
{
	for (ULONG TabletIdx = 0; TabletIdx < Num; ++TabletIdx)
	{
		AddTabletContext(InRealTimeStylus, InTabletContexts[TabletIdx]);
	}

	return S_OK;
}

HRESULT FWindowsRealTimeStylusPlugin::RealTimeStylusDisabled(IRealTimeStylus* InRealTimeStylus, ULONG Num, const TABLET_CONTEXT_ID* InTabletContexts)
{
	for (ULONG TabletIdx = 0; TabletIdx < Num; ++TabletIdx)
	{
		RemoveTabletContext(InRealTimeStylus, InTabletContexts[TabletIdx]);
	}

	return S_OK;
}

HRESULT FWindowsRealTimeStylusPlugin::TabletAdded(IRealTimeStylus* InRealTimeStylus, IInkTablet* InkTablet)
{
	TABLET_CONTEXT_ID TabletID;
	if (SUCCEEDED(InRealTimeStylus->GetTabletContextIdFromTablet(InkTablet, &TabletID)))
	{
		AddTabletContext(InRealTimeStylus, TabletID);
	}

	return S_OK;
}

HRESULT FWindowsRealTimeStylusPlugin::TabletRemoved(IRealTimeStylus* InRealTimeStylus, LONG iTabletIndex)
{
	ULONG TabletContextCount = 0;
	TABLET_CONTEXT_ID* AllContexts = nullptr;
	InRealTimeStylus->GetAllTabletContextIds(&TabletContextCount, &AllContexts);
	
	if (AllContexts == nullptr || iTabletIndex < 0 || iTabletIndex >= (LONG) TabletContextCount)
	{
		return E_INVALIDARG;
	}
	
	RemoveTabletContext(InRealTimeStylus, AllContexts[iTabletIndex]);
	return S_OK;
}

static float Normalize(int Value, const FPacketDescription& Desc)
{
	return (float) (Value - Desc.Minimum) / (float) (Desc.Maximum - Desc.Minimum);
}

static float ToDegrees(int Value, const FPacketDescription& Desc)
{
	return Value / Desc.Resolution;
}

void FWindowsRealTimeStylusPlugin::HandlePacket(IRealTimeStylus* InRealTimeStylus, const StylusInfo* StylusInfo, ULONG PacketCount, ULONG PacketBufferLength, LONG* Packets)
{
	FTabletContextInfo* TabletContext = FindTabletContext(StylusInfo->tcid);
	if (TabletContext == nullptr)
	{
		return;
	}

	TabletContext->SetDirty();
	TabletContext->WindowsState.IsInverted = StylusInfo->bIsInvertedCursor;

	ULONG PropertyCount = PacketBufferLength / PacketCount;

	for (ULONG i = 0; i < PropertyCount; ++i)
	{
		const FPacketDescription& PacketDescription = TabletContext->PacketDescriptions[i];

		float Normalized = Normalize(Packets[i], PacketDescription);

		switch (PacketDescription.Type)
		{
			case EWindowsPacketType::X:
				TabletContext->WindowsState.Position.X = Packets[i];
				break;
			case EWindowsPacketType::Y:
				TabletContext->WindowsState.Position.Y = Packets[i];
				break;
			case EWindowsPacketType::Status:
				break;
			case EWindowsPacketType::Z:
				TabletContext->WindowsState.Z = Normalized;
				break;
			case EWindowsPacketType::NormalPressure:
				TabletContext->WindowsState.NormalPressure = Normalized;
				break;
			case EWindowsPacketType::TangentPressure:
				TabletContext->WindowsState.TangentPressure = Normalized;
				break;
			case EWindowsPacketType::Twist:
				TabletContext->WindowsState.Twist = ToDegrees(Packets[i], PacketDescription);
				break;
			case EWindowsPacketType::XTilt:
				TabletContext->WindowsState.Tilt.X = ToDegrees(Packets[i], PacketDescription);
				break;
			case EWindowsPacketType::YTilt:
				TabletContext->WindowsState.Tilt.Y = ToDegrees(Packets[i], PacketDescription);
				break;
			case EWindowsPacketType::Width:
				TabletContext->WindowsState.Size.X = Normalized;
				break;
			case EWindowsPacketType::Height:
				TabletContext->WindowsState.Size.Y = Normalized;
				break;
		}
	}
}

HRESULT FWindowsRealTimeStylusPlugin::Packets(IRealTimeStylus* InRealTimeStylus, const StylusInfo* StylusInfo,
	ULONG PacketCount, ULONG PacketBufferLength, LONG* Packets, ULONG* InOutPackets, LONG** PtrInOutPackets)
{
	HandlePacket(InRealTimeStylus, StylusInfo, PacketCount, PacketBufferLength, Packets);
	return S_OK;
}

HRESULT FWindowsRealTimeStylusPlugin::InAirPackets(IRealTimeStylus* InRealTimeStylus, const StylusInfo* StylusInfo,
	ULONG PacketCount, ULONG PacketBufferLength, LONG* Packets, ULONG* InOutPackets, LONG** PtrInOutPackets)
{
	HandlePacket(InRealTimeStylus, StylusInfo, PacketCount, PacketBufferLength, Packets);
	return S_OK;
}

#endif // PLATFORM_WINDOWS