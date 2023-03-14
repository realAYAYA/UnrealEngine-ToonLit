// Copyright Epic Games, Inc. All Rights Reserved.

#include "MIDIDeviceInputController.h"

#include "MIDIDeviceController.h"
#include "MIDIDeviceLog.h"
#include "portmidi.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MIDIDeviceInputController)

UMIDIDeviceInputController::~UMIDIDeviceInputController()
{
	// Clean everything up before we're garbage collected
	UMIDIDeviceInputController::ShutdownDevice();
}

void UMIDIDeviceInputController::StartupDevice(const int32 InitDeviceID, const int32 InitMIDIBufferSize, bool& bOutWasSuccessful)
{
	bOutWasSuccessful = false;

	this->DeviceID = InitDeviceID;
	this->PMMIDIStream = nullptr;
	this->MIDIBufferSize = 0;

	const PmDeviceID PMDeviceID = this->DeviceID;

	if(InitMIDIBufferSize > 0)
	{
		const PmDeviceInfo* PMDeviceInfo = Pm_GetDeviceInfo(PMDeviceID);
		if(PMDeviceInfo != nullptr)
		{
			// Is the device already in use?  If so, spit out a warning
			if(PMDeviceInfo->opened != 0)
			{
				UE_LOG(LogMIDIDevice, Warning, TEXT("Failed to bind to MIDI device '%s' (ID: %i): Device is already in use"), ANSI_TO_TCHAR(PMDeviceInfo->name), PMDeviceID);
				return;
			}

			// Make sure the device is setup for input/output
			if(PMDeviceInfo->input == 0)
			{
				UE_LOG(LogMIDIDevice, Warning, TEXT("Failed to bind to MIDI device '%s' (ID: %i): Device not setup to receive MIDI"), ANSI_TO_TCHAR(PMDeviceInfo->name), PMDeviceID);
				return;
			}
			

			// @todo midi: Add options for timing/latency (see timeproc, and pm_Synchronize)

			const PmError PMError = Pm_OpenInput(&this->PMMIDIStream, PMDeviceID, NULL, MIDIBufferSize, NULL, NULL);
			if (PMError == pmNoError)
			{
				check(this->PMMIDIStream != nullptr);

				this->DeviceName = ANSI_TO_TCHAR(PMDeviceInfo->name);
				this->MIDIBufferSize = InitMIDIBufferSize;

				// Good to go!
				bOutWasSuccessful = true;
			}
			else
			{
				this->PMMIDIStream = nullptr;
				const FString ErrorText = MIDIDeviceInternal::ParsePmError(PMError);
				UE_LOG(LogMIDIDevice, Error, TEXT("Unable to open input connection to MIDI device ID %i (%s) (PortMidi error: %s)."), PMDeviceID, ANSI_TO_TCHAR(PMDeviceInfo->name), *ErrorText);
			}
		}
		else
		{
			UE_LOG(LogMIDIDevice, Error, TEXT("Unable to query information about MIDI device (PortMidi device ID: %i)." ), PMDeviceID);
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Error, TEXT("The specified MIDI Buffer Size must be greater than zero." ));
	}
}

void UMIDIDeviceInputController::ShutdownDevice()
{
	if(this->PMMIDIStream != nullptr)
	{
		const PmError PMError = Pm_Close(this->PMMIDIStream);

		if (PMError != pmNoError)
		{
			const FString ErrorText = MIDIDeviceInternal::ParsePmError(PMError);
			UE_LOG(LogMIDIDevice, Error, TEXT("Encounter an error when closing the input connection to MIDI device ID %i (%s) (PortMidi error: %s)."), this->DeviceID, *this->DeviceName, *ErrorText);
		}

		this->PMMIDIStream = nullptr;
	}
}

void UMIDIDeviceInputController::ProcessIncomingMIDIEvents()
{
	if(this->PMMIDIStream != nullptr)
	{
		// Static that we'll copy event data to every time.  This stuff isn't multi-threaded right now, so this is fine.
		static TArray<PmEvent> PMMIDIEvents;
		PMMIDIEvents.SetNum(MIDIBufferSize, false);

		const int32 PMEventCount = Pm_Read(this->PMMIDIStream, PMMIDIEvents.GetData(), PMMIDIEvents.Num());

		// if Pm_Read returns a negative result, it's an error
		if(PMEventCount < 0)
		{
			const FString ErrorText = MIDIDeviceInternal::ParsePmError(static_cast<const PmError>(PMEventCount));
			UE_LOG(LogMIDIDevice, Error, TEXT("Encountered an error when processing incoming MIDI events for device ID %i (%s) (PortMidi error: %s)."), this->DeviceID, *this->DeviceName, *ErrorText);
		}
		else
		{
			for(int32 PMEventIndex = 0; PMEventIndex < PMEventCount; ++PMEventIndex)
			{
				const PmEvent& PMEvent = PMMIDIEvents[PMEventIndex];
				const PmMessage& PMMessage = PMEvent.message;

				const int32 PMTimestamp = PMEvent.timestamp;
				const int32 PMMessageStatus = Pm_MessageStatus(PMMessage);
				const int32 PMMessageData1 = Pm_MessageData1(PMMessage);
				const int32 PMMessageData2 = Pm_MessageData2(PMMessage);
				const int32 PMType = (PMMessageStatus & 0xF0) >> 4;
				const int32 PMChannel = (PMMessageStatus % 16) + 1;

				// Send our event
				{
					const int32 Channel = PMChannel;
					const int32 RawEventType = PMType;
					const int32 Timestamp = PMTimestamp;

					this->OnMIDIRawEvent.Broadcast(this, Timestamp, RawEventType, Channel, PMMessageData1, PMMessageData2);

					EMIDIEventType EventType = EMIDIEventType::Unknown;
					EMIDIEventType PossibleEventType = (EMIDIEventType)PMType;
					switch(PossibleEventType)
					{
					case EMIDIEventType::NoteOn:
						{
							// Check velocity is not zero
							if (PMMessageData2 > 0)
							{
								this->OnMIDINoteOn.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
								EventType = PossibleEventType;
							}
							else
							{
								// If velocity is 0 we are actually turning the note off
								this->OnMIDINoteOff.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
								EventType = EMIDIEventType::NoteOff;
							}

						}
						break;

					case EMIDIEventType::NoteOff:
						{
							this->OnMIDINoteOff.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
							EventType = PossibleEventType;
						}
						break;

					case EMIDIEventType::PitchBend:
						{
							int32 pitchBend = (PMMessageData2 & 0x7F) << 7;

							pitchBend |= (PMMessageData1 & 0x7F);

							this->OnMIDIPitchBend.Broadcast(this, Timestamp, Channel, pitchBend);

							EventType = PossibleEventType;
						}
						break;

					case EMIDIEventType::NoteAfterTouch:
						{
							this->OnMIDIAftertouch.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
							EventType = PossibleEventType;
						}
						break;

					case EMIDIEventType::ControlChange:
						{
							this->OnMIDIControlChange.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
							EventType = PossibleEventType;
						}
						break;

					case EMIDIEventType::ProgramChange:
						{
							this->OnMIDIProgramChange.Broadcast(this, Timestamp, Channel, PMMessageData1, PMMessageData2);
							EventType = PossibleEventType;
						}
						break;

					case EMIDIEventType::ChannelAfterTouch:
						{
							this->OnMIDIChannelAftertouch.Broadcast(this, Timestamp, Channel, PMMessageData1);
							EventType = PossibleEventType;
						}
						break;
					}
				}
			}
		}
	}
}

