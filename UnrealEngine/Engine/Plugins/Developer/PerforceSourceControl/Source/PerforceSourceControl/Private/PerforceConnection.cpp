// Copyright Epic Games, Inc. All Rights Reserved.

#include "PerforceConnection.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ISourceControlModule.h"
#include "Logging/MessageLog.h"
#include "Memory/SharedBuffer.h"
#include "Misc/Paths.h"
#include "PerforceSourceControlProvider.h"
#include "PerforceSourceControlSettings.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "PerforceConnection"

#define FROM_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? TCHAR_TO_UTF8(InText) : TCHAR_TO_ANSI(InText))
#define TO_TCHAR(InText, bIsUnicodeServer) (bIsUnicodeServer ? UTF8_TO_TCHAR(InText) : ANSI_TO_TCHAR(InText))

namespace
{
	enum class EP4ClientUserFlags
	{
		None			= 0,
		/** The server uses unicode */
		UnicodeServer	= 1 << 0,
		/** Binary data returned by commands should be collected in the DataBuffer member */
		CollectData		= 1 << 1,
	};

	ENUM_CLASS_FLAGS(EP4ClientUserFlags);
}

const FString FP4Record::EmptyStr;


/** Custom ClientUser class for handling results and errors from Perforce commands */
class FP4ClientUser : public ClientUser
{
public:
	FP4ClientUser(FP4RecordSet& InRecords, EP4ClientUserFlags InFlags, TArray<FText>& InOutErrorMessages)
		: ClientUser()
		, Flags(InFlags)
		, Records(InRecords)
		, OutErrorMessages(InOutErrorMessages)
	{

	}

	/**  Called by P4API when the results from running a command are ready. */
	virtual void OutputStat(StrDict* VarList) override
	{
		FP4Record Record;
		StrRef Var, Value;

		// Iterate over each variable and add to records
		for (int32 Index = 0; VarList->GetVar(Index, Var, Value); Index++)
		{
			Record.Add(TO_TCHAR(Var.Text(), IsUnicodeServer()), TO_TCHAR(Value.Text(), IsUnicodeServer()));
		}

		// Try to reserve the data array if we have one as we just got the filesize info.
		// NOTE: This code is built on the assumption that the data array will contain the
		// data sent via p4 print and our code only called p4 print on a single file at the 
		// time.
		if (IsCollectingData() && DataBuffer.GetAllocatedSize() == 0)
		{
			const FString& SizeAsString = Record(TEXT("fileSize"));
			if (!SizeAsString.IsEmpty())
			{
				const int64 Size = FCString::Atoi64(*SizeAsString);
				DataBuffer.Reserve(Size);
			}

		}

		Records.Add(Record);
	}

	/** Called by P4API when it output a chunk of text data from a file (commonly via P4 Print) */
	virtual void OutputText(const char* DataPtr, int DataLength) override
	{
		if (IsCollectingData())
		{
			DataBuffer.Append(DataPtr, DataLength);
		}
		else
		{
			ClientUser::OutputText(DataPtr, DataLength);
		}
	}

	/**  Called by P4API when it output a chunk of binary data from a file (commonly via P4 Print) */
	void OutputBinary(const char* DataPtr, int DataLength)
	{
		if (IsCollectingData())
		{
			DataBuffer.Append(DataPtr, DataLength);
		}
		else
		{
			ClientUser::OutputText(DataPtr, DataLength);
		}
	}

	/** Called by P4API when an error message is avaliable. */
	virtual void HandleError(Error* InError) override
	{
		StrBuf ErrorMessage;
		InError->Fmt(&ErrorMessage);
		OutErrorMessages.Add(FText::FromString(FString(TO_TCHAR(ErrorMessage.Text(), IsUnicodeServer()))));
	}

	inline bool IsUnicodeServer() const
	{
		return EnumHasAnyFlags(Flags, EP4ClientUserFlags::UnicodeServer);
	}

	inline bool IsCollectingData() const
	{
		return EnumHasAnyFlags(Flags, EP4ClientUserFlags::CollectData);
	}

	/** Returns DataBuffer as a FSharedBuffer, note that once called DataBuffer will be empty */
	inline FSharedBuffer ReleaseData()
	{
		return MakeSharedBufferFromArray(MoveTemp(DataBuffer));
	}

	EP4ClientUserFlags Flags;
	FP4RecordSet& Records;
	TArray<FText>& OutErrorMessages;

private:
	TArray64<char> DataBuffer;
};

/** A class used instead of FP4ClientUser for handling changelist create command */
class FP4CreateChangelistClientUser : public FP4ClientUser
{
public:
	FP4CreateChangelistClientUser(FP4RecordSet& InRecords, EP4ClientUserFlags InFlags, TArray<FText>& InOutErrorMessages, const FText& InDescription, const TArray<FString>& InFiles, ClientApi &InP4Client)
		:	FP4ClientUser(InRecords, InFlags, InOutErrorMessages)
		,	Description(InDescription)
		,	ChangelistNumber(0)
		,	Files(InFiles)
		,	P4Client(InP4Client)
	{
	}

	/** Called by P4API when the changelist is created. */
	virtual void OutputInfo(ANSICHAR Level, const ANSICHAR *Data) override
	{
		const int32 ChangeTextLen = FCString::Strlen(TEXT("Change "));
		if (FString(TO_TCHAR(Data, IsUnicodeServer())).StartsWith(TEXT("Change ")))
		{
			ChangelistNumber = FCString::Atoi(TO_TCHAR(Data + ChangeTextLen, IsUnicodeServer()));
		}
	}

	/** Called by P4API on "change -i" command. OutBuffer is filled with changelist specification text. */
	virtual void InputData(StrBuf* OutBuffer, Error* OutError) override
	{
		FString OutputDesc;
		OutputDesc += TEXT("Change:\tnew\n\n");
		OutputDesc += TEXT("Client:\t");
		OutputDesc += TO_TCHAR(P4Client.GetClient().Text(), IsUnicodeServer());
		OutputDesc += TEXT("\n\n");
		OutputDesc += TEXT("User:\t");
		OutputDesc += TO_TCHAR(P4Client.GetUser().Text(), IsUnicodeServer());
		OutputDesc += TEXT("\n\n");
		OutputDesc += TEXT("Status:\tnew\n\n");
		OutputDesc += TEXT("Description:\n");
		{
			TArray<FString> DescLines;
			Description.ToString().ParseIntoArray(DescLines, TEXT("\n"), false);
			for (const FString& DescLine : DescLines)
			{
				OutputDesc += TEXT("\t");
				OutputDesc += DescLine;
				OutputDesc += TEXT("\n");
			}
		}
		OutputDesc += TEXT("\n");
		OutputDesc += TEXT("Files:\n");
		for (const FString& FileName : Files)
		{
			OutputDesc += TEXT("\t");
			OutputDesc += FileName;
			OutputDesc += TEXT("\n");
		}
		OutputDesc += TEXT("\n");

		OutBuffer->Append(FROM_TCHAR(*OutputDesc, IsUnicodeServer()));
	}

	FText Description;
	int32 ChangelistNumber;
	TArray<FString> Files;
	ClientApi& P4Client;
};

/** A class used instead of FP4ClientUser for handling changelist edit command */
class FP4EditChangelistClientUser : public FP4ClientUser
{
public:
	FP4EditChangelistClientUser(FP4RecordSet& OutRecords, EP4ClientUserFlags InFlags, TArray<FText>& InOutErrorMessages, const FText& InDescription, int32 InChangelistNumber, const FP4RecordSet& InRecords, ClientApi& InP4Client)
		: FP4ClientUser(OutRecords, InFlags, InOutErrorMessages)
		, Description(InDescription)
		, ChangelistNumber(InChangelistNumber)
		, P4Client(InP4Client)
		, PreviousRecords(InRecords)
	{
	}

	/** Called by P4API when the changelist is updated. */
	virtual void OutputInfo(ANSICHAR Level, const ANSICHAR* Data) override
	{
		const int32 ChangeTextLen = FCString::Strlen(TEXT("Change "));
		if (FString(TO_TCHAR(Data, IsUnicodeServer())).StartsWith(TEXT("Change ")))
		{
			ChangelistNumber = FCString::Atoi(TO_TCHAR(Data + ChangeTextLen, IsUnicodeServer()));
		}
	}

	/** Called by P4API on "change -i" command. OutBuffer is filled with changelist specification text. */
	virtual void InputData(StrBuf* OutBuffer, Error* OutError) override
	{
		FString OutputDesc;
		const FP4Record& Record = PreviousRecords[0];

		for (const auto& Field : Record)
		{
			// Skip some fields that aren't required (as evidenced by the CreateChangelist before
			if (Field.Key == TEXT("Date") ||
				Field.Key == TEXT("Type") ||
				Field.Key == TEXT("specFormatted") ||
				Field.Key == TEXT("func"))
			{
				continue;
			}
			else if (Field.Key == TEXT("extraTag0"))
			{
				// Changelists with shelved files will have additional tags which will be rejected in the edit
				break;
			}

			if (Field.Key == TEXT("Description"))
			{
				// Description case: we will replace the current description by the new one provided
				OutputDesc += TEXT("Description:\n");
				{
					TArray<FString> DescLines;
					Description.ToString().ParseIntoArray(DescLines, TEXT("\n"), false);
					for (const FString& DescLine : DescLines)
					{
						OutputDesc += TEXT("\t");
						OutputDesc += DescLine;
						OutputDesc += TEXT("\n");
					}
				}
				OutputDesc += TEXT("\n");
			}
			else if (Field.Key.Contains(TEXT("Files")))
			{
				if (Field.Key == TEXT("Files0"))
				{
					OutputDesc += TEXT("Files:\n");
				}

				OutputDesc += TEXT("\t");
				OutputDesc += Field.Value;
				OutputDesc += TEXT("\n\n");
			}
			else
			{
				// General case: just put back what is already present in the record
				OutputDesc += Field.Key;
				OutputDesc += TEXT(":\t");
				OutputDesc += Field.Value;
				OutputDesc += TEXT("\n\n");
			}
		}

		OutBuffer->Append(FROM_TCHAR(*OutputDesc, IsUnicodeServer()));
	}

	FText Description;
	int32 ChangelistNumber;
	ClientApi& P4Client;
	FP4RecordSet PreviousRecords;
};

/** An extended version of FP4ClientUser that allows the standard input stream to be overriden */
class FP4CommandWithStdInputClientUser : public FP4ClientUser
{
public:
	FP4CommandWithStdInputClientUser(FStringView InStdInput, FP4RecordSet& InRecords, EP4ClientUserFlags InFlags, TArray<FText>& InOutErrorMessages, ClientApi& InP4Client)
		: FP4ClientUser(InRecords, InFlags, InOutErrorMessages)
		, P4Client(InP4Client)
		, StdInput(InStdInput)
	{
	}


	virtual void InputData(StrBuf* OutBuffer, Error* OutError) override
	{	
		OutBuffer->Append(FROM_TCHAR(*StdInput, IsUnicodeServer()));
	}

protected:
	ClientApi& P4Client;
	FString StdInput;
};

/** Custom ClientUser class for handling login commands */
class FP4LoginClientUser : public FP4ClientUser
{
public:
	FP4LoginClientUser(const FString& InPassword, FP4RecordSet& InRecords, EP4ClientUserFlags InFlags, TArray<FText>& InOutErrorMessages)
		:	FP4ClientUser(InRecords, InFlags, InOutErrorMessages)
		,	Password(InPassword)
	{
	}

	/** Called when we are prompted for a password */
	virtual void Prompt( const StrPtr& InMessage, StrBuf& OutPrompt, int NoEcho, Error* InError ) override
	{
		OutPrompt.Set(FROM_TCHAR(*Password, IsUnicodeServer()));
	}

	/** Password to use when logging in */
	FString Password;
};


class FP4KeepAlive : public KeepAlive
{
public:
	FP4KeepAlive(FOnIsCancelled InIsCancelled)
		: IsCancelled(InIsCancelled)
	{
	}

	/** Called when the perforce connection wants to know if it should stay connected */
	virtual int IsAlive() override
	{
		if(IsCancelled.IsBound() && IsCancelled.Execute())
		{
			return 0;
		}

		return 1;
	}

	FOnIsCancelled IsCancelled;
};

static bool TestLoginConnection(FPerforceSourceControlProvider& SCCProvider, ClientApi& P4Client, bool bIsUnicodeServer, TArray<FText>& OutErrorMessages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforce::TestLoginConnection);

	FP4RecordSet Records;
	EP4ClientUserFlags Flags = bIsUnicodeServer ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;
	FP4ClientUser User(Records, Flags, OutErrorMessages);

	const char* ArgV[] = { "-s" };

	P4Client.SetArgv(1, const_cast<char* const*>(ArgV));
	P4Client.Run("login", &User);

	SCCProvider.SetLastErrors(OutErrorMessages);

	return OutErrorMessages.IsEmpty();
}

/**
 * Runs "client" command to test if the connection is actually OK. ClientApi::Init() only checks
 * if it can connect to server, doesn't verify user name nor workspace name.
 */
static bool TestClientConnection(FPerforceSourceControlProvider& SCCProvider, ClientApi& P4Client, const FString& ClientSpecName, bool bIsUnicodeServer, TArray<FText>& OutErrorMessages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforce::TestClientConnection);

	FP4RecordSet Records;
	EP4ClientUserFlags Flags = bIsUnicodeServer ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;
	FP4ClientUser User(Records, Flags, OutErrorMessages);

	UTF8CHAR* ClientSpecUTF8Name = nullptr;
	if(bIsUnicodeServer)
	{
		FTCHARToUTF8 UTF8String(*ClientSpecName);
		ClientSpecUTF8Name = new UTF8CHAR[UTF8String.Length() + 1];
		FMemory::Memcpy(ClientSpecUTF8Name, UTF8String.Get(), UTF8String.Length() + 1);
	}
	else
	{		
		ClientSpecUTF8Name = new UTF8CHAR[ClientSpecName.Len() + 1];
		FMemory::Memcpy(ClientSpecUTF8Name, TCHAR_TO_ANSI(*ClientSpecName), ClientSpecName.Len() + 1);
	}

	const char *ArgV[] = { "-o", (char*)ClientSpecUTF8Name };

	P4Client.SetArgv(2, const_cast<char*const*>(ArgV));
	P4Client.Run("client", &User);

	// clean up args
	delete [] ClientSpecUTF8Name;

	SCCProvider.SetLastErrors(OutErrorMessages);

	// If there are error messages, user name is most likely invalid. Otherwise, make sure workspace actually
	// exists on server by checking if we have it's update date.
	bool bConnectionOK = OutErrorMessages.Num() == 0 && Records.Num() > 0 && Records[0].Contains(TEXT("Update"));
	if (!bConnectionOK && OutErrorMessages.Num() == 0)
	{
		OutErrorMessages.Add(LOCTEXT("InvalidWorkspace", "Invalid Workspace"));
	}

	return bConnectionOK;
}

static bool CheckUnicodeStatus(ClientApi& P4Client, bool& bIsUnicodeServer, TArray<FText>& OutErrorMessages)
{
	FP4RecordSet Records;
	FP4ClientUser User(Records, EP4ClientUserFlags::None, OutErrorMessages);

	P4Client.Run("info", &User);

	if(Records.Num() > 0)
	{
		bIsUnicodeServer = Records[0].Find(TEXT("unicode")) != nullptr;
	}
	else
	{
		bIsUnicodeServer = false;
	}

	return OutErrorMessages.Num() == 0;
}

FPerforceConnection::FPerforceConnection(const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& InSCCProvider)
	: bEstablishedConnection(false)
	, bIsUnicode(false)
	, SCCProvider(InSCCProvider)
{
	EstablishConnection(InConnectionInfo);
}

FPerforceConnection::~FPerforceConnection()
{
	Disconnect();
}

bool FPerforceConnection::AutoDetectWorkspace(const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider, FString& OutWorkspaceName)
{
	bool Result = false;
	FMessageLog SourceControlLog("SourceControl");

	//before even trying to summon the window, try to "smart" connect with the default server/username
	TArray<FText> ErrorMessages;
	FPerforceConnection Connection(InConnectionInfo, SCCProvider);
	TArray<FString> ClientSpecList;
	Connection.GetWorkspaceList(InConnectionInfo, FOnIsCancelled(), ClientSpecList, ErrorMessages);

	//if only one client spec matched (and default connection info was correct)
	if (ClientSpecList.Num() == 1)
	{
		OutWorkspaceName = ClientSpecList[0];
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("WorkspaceName"), FText::FromString(OutWorkspaceName) );
		SourceControlLog.Info(FText::Format(LOCTEXT("ClientSpecAutoDetect", "Auto-detected Perforce client spec: '{WorkspaceName}'"), Arguments));
		Result = true;
	}
	else if (ClientSpecList.Num() > 0)
	{
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine1", "Source Control unable to auto-login due to ambiguous client specs"));
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine2", "  Please select a client spec in the Perforce settings dialog"));
		SourceControlLog.Warning(LOCTEXT("AmbiguousClientSpecLine3", "  If you are unable to work with source control, consider checking out the files by hand temporarily"));

		// List out the clientspecs that were found to be ambiguous
		SourceControlLog.Info(LOCTEXT("AmbiguousClientSpecListTitle", "Ambiguous client specs..."));
		for (int32 Index = 0; Index < ClientSpecList.Num(); Index++)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("ClientSpecName"), FText::FromString(ClientSpecList[Index]) );
			SourceControlLog.Info(FText::Format(LOCTEXT("AmbiguousClientSpecListItem", "...{ClientSpecName}"), Arguments));
		}
	}

	return Result;
}

bool FPerforceConnection::Login(const FPerforceConnectionInfo& InConnectionInfo)
{
	TArray<FText> ErrorMessages;

	FP4RecordSet Records;
	FP4LoginClientUser User(InConnectionInfo.Password, Records, EP4ClientUserFlags::None, ErrorMessages);

	const char *ArgV[] = { "-a" };
	P4Client.SetArgv(1, const_cast<char*const*>(ArgV));
	P4Client.Run("login", &User);

	if(ErrorMessages.Num())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Login failed"));
		for(auto ErrorMessage : ErrorMessages)
		{
			UE_LOG(LogSourceControl, Error, TEXT("%s"), *ErrorMessage.ToString());
		}
	}

	return ErrorMessages.Num() == 0;
}

bool FPerforceConnection::EnsureValidConnection(FString& InOutServerName, FString& InOutUserName, FString& InOutWorkspaceName,
												const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider, 
												EConnectionOptions Options)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforceConnection::EnsureValidConnection);

	bool bIsUnicodeServer = false;
	bool bConnectionOK = false;

	FMessageLog SourceControlLog("SourceControl");

	FString NewServerName = InOutServerName;
	FString NewUserName = InOutUserName;
	FString NewClientSpecName = InOutWorkspaceName;

	ClientApi TestP4;
	TestP4.SetProg("UE");
	TestP4.SetProtocol("tag", "");
	TestP4.SetProtocol("enableStreams", "");

	if (!NewServerName.IsEmpty())
	{
		TestP4.SetPort(TCHAR_TO_ANSI(*NewServerName));

		if (!InConnectionInfo.HostOverride.IsEmpty())
		{
			TestP4.SetHost(TCHAR_TO_ANSI(*InConnectionInfo.HostOverride));
		}
	}

	// Add easy access to the localized error message if needed
	auto GetFailedToConnectMessage = []() -> FText
	{
		return LOCTEXT("P4ErrorConnection_FailedToConnect", "P4ERROR: Failed to connect to source control provider.");
	};

	Error P4Error;
	TestP4.Init(&P4Error);

	bConnectionOK = !P4Error.Test();
	if (!bConnectionOK)
	{
		//Connection FAILED
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		SourceControlLog.Error(GetFailedToConnectMessage());
		SourceControlLog.Error(FText::FromString(ANSI_TO_TCHAR(ErrorMessage.Text())));
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("OwningSystem"), FText::FromString(SCCProvider.GetOwnerName()));
		Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
		Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
		SourceControlLog.Error(FText::Format(LOCTEXT("P4ConnectErrorConnection_Details", "OwningSystem={OwningSystem}, Port={PortName}, Ticket={Ticket}"), Arguments));
	}

	// run an info command to determine unicode status
	if(bConnectionOK)
	{
		TArray<FText> ErrorMessages;

		bConnectionOK = CheckUnicodeStatus(TestP4, bIsUnicodeServer, ErrorMessages);
		if(!bConnectionOK)
		{
			SourceControlLog.Error(LOCTEXT("P4ErrorConnection_CouldNotDetermineUnicodeStatus", "P4ERROR: Could not determine server unicode status."));
			SourceControlLog.Error(ErrorMessages.Num() > 0 ? ErrorMessages[0] : LOCTEXT("P4ErrorConnection_UnknownError", "Unknown error"));
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwningSystem"), FText::FromString(SCCProvider.GetOwnerName()));
			Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
			Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
			SourceControlLog.Error(FText::Format(LOCTEXT("P4UnicodeErrorConnection_Details", "OwningSystem={OwningSystem}, Port={PortName}, Ticket={Ticket}"), Arguments));
		}
		else
		{
			if(bIsUnicodeServer)
			{
				// set translation mode. From here onwards we need to use UTF8 when using text args
				TestP4.SetTrans(CharSetApi::UTF_8);
			}

			// now we have determined unicode status, we can set the values that can be specified in non-ansi characters
			TestP4.SetCwd(FROM_TCHAR(*FPaths::RootDir(), bIsUnicodeServer));
			TestP4.SetUser(FROM_TCHAR(*NewUserName, bIsUnicodeServer));
			TestP4.SetClient(FROM_TCHAR(*NewClientSpecName, bIsUnicodeServer));
			TestP4.SetPassword(FROM_TCHAR(*InConnectionInfo.Ticket, bIsUnicodeServer));

		}
	}

	// Test that we have a valid p4 ticket
	if (bConnectionOK)
	{
		TArray<FText> ErrorMessages;
		bConnectionOK = TestLoginConnection(SCCProvider, TestP4, bIsUnicodeServer, ErrorMessages);

		if (!bConnectionOK)
		{
			FString ServerName = TO_TCHAR(TestP4.GetPort().Text(), bIsUnicodeServer);
			FString UserName = TO_TCHAR(TestP4.GetUser().Text(), bIsUnicodeServer);

			SourceControlLog.Error(GetFailedToConnectMessage());
			SourceControlLog.Error(ErrorMessages.Num() > 0 ? ErrorMessages[0] : LOCTEXT("P4ErrorConnection_InvalidToken", "Unable to log in"));
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwningSystem"), FText::FromString(SCCProvider.GetOwnerName()));
			Arguments.Add(TEXT("PortName"), FText::FromString(MoveTemp(ServerName)));
			Arguments.Add(TEXT("UserName"), FText::FromString(MoveTemp(UserName)));

			SourceControlLog.Error(FText::Format(LOCTEXT("P4LoginErrorConnection_Details", "OwningSystem={OwningSystem}, Port={PortName}, User={UserName}"), Arguments));
		}
	}

	const bool bRequireWorkspace = !EnumHasAllFlags(Options, EConnectionOptions::WorkspaceOptional);

	// Try to auto detect the client if none were specified and we require one
	if (bConnectionOK && bRequireWorkspace && NewClientSpecName.IsEmpty())
	{
		FPerforceConnectionInfo AutoCredentials = InConnectionInfo;
		AutoCredentials.Port = TO_TCHAR(TestP4.GetPort().Text(), bIsUnicodeServer);
		AutoCredentials.UserName = TO_TCHAR(TestP4.GetUser().Text(), bIsUnicodeServer);

		bConnectionOK = FPerforceConnection::AutoDetectWorkspace(AutoCredentials, SCCProvider, NewClientSpecName);
		if (bConnectionOK)
		{
			TestP4.SetClient(FROM_TCHAR(*NewClientSpecName, bIsUnicodeServer));
		}
	}

	// Test that we found a valid client if we require one
	if (bConnectionOK && bRequireWorkspace)
	{
		TArray<FText> ErrorMessages;

		bConnectionOK = TestClientConnection(SCCProvider, TestP4, NewClientSpecName, bIsUnicodeServer, ErrorMessages);
		
		if (!bConnectionOK)
		{
			SourceControlLog.Error(GetFailedToConnectMessage());
			SourceControlLog.Error(ErrorMessages.Num() > 0 ? ErrorMessages[0] : LOCTEXT("P4ErrorConnection_InvalidWorkspace", "Invalid workspace"));
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OwningSystem"), FText::FromString(SCCProvider.GetOwnerName()));
			Arguments.Add( TEXT("PortName"), FText::FromString(NewServerName) );
			Arguments.Add( TEXT("UserName"), FText::FromString(NewUserName) );
			Arguments.Add( TEXT("ClientSpecName"), FText::FromString(NewClientSpecName) );
			Arguments.Add( TEXT("Ticket"), FText::FromString(InConnectionInfo.Ticket) );
			SourceControlLog.Error(FText::Format(LOCTEXT("P4ClientErrorConnection_Details", "OwningSystem={OwningSystem}, Port={PortName}, User={UserName}, ClientSpec={ClientSpecName}, Ticket={Ticket}"), Arguments));
		}
	}

	//whether successful or not, disconnect to clean up
	TestP4.Final(&P4Error);
	if (bConnectionOK && P4Error.Test())
	{
		//Disconnect FAILED
		bConnectionOK = false;
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		SourceControlLog.Error(LOCTEXT("P4ErrorFailedDisconnect", "P4ERROR: Failed to disconnect from Server."));
		SourceControlLog.Error(FText::FromString(TO_TCHAR(ErrorMessage.Text(), bIsUnicodeServer)));
	}

	//if never specified, take the default connection values
	if (NewServerName.IsEmpty())
	{
		NewServerName = TO_TCHAR(TestP4.GetPort().Text(), bIsUnicodeServer);
	}

	if (NewUserName.IsEmpty())
	{
		NewUserName = TO_TCHAR(TestP4.GetUser().Text(), bIsUnicodeServer);
	}

	if (NewClientSpecName.IsEmpty() && bRequireWorkspace)
	{
		NewClientSpecName = TO_TCHAR(TestP4.GetClient().Text(), bIsUnicodeServer);
		if (NewClientSpecName == TO_TCHAR(TestP4.GetHost().Text(), bIsUnicodeServer))
		{
			// If the client spec name is the same as host name, assume P4 couldn't get the actual
			// spec name for this host and let GetPerforceLogin() try to find a proper one.
			bConnectionOK = false;
		}
	}

	if (bConnectionOK)
	{
		InOutServerName = NewServerName;
		InOutUserName = NewUserName;
		InOutWorkspaceName = NewClientSpecName;
	}

	return bConnectionOK;
}

bool FPerforceConnection::GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, FOnIsCancelled InOnIsCanceled, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforceConnection::GetWorkspaceList)

	if(bEstablishedConnection)
	{
		TArray<FString> Params;
		bool bAllowWildHosts = !GIsBuildMachine;
		Params.Add(TEXT("-u"));
		Params.Add(InConnectionInfo.UserName);

		FP4RecordSet Records;
		bool bConnectionDropped = false;
		bool bCommandOK = RunCommand(TEXT("clients"), Params, Records, OutErrorMessages, InOnIsCanceled, bConnectionDropped);

		if (bCommandOK)
		{
			FString ApplicationPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ISourceControlModule::Get().GetSourceControlProjectDir()).ToLower();

			FString LocalHostName = InConnectionInfo.HostOverride;
			if(LocalHostName.Len() == 0)
			{
				// No host override, check environment variable
				LocalHostName = FPlatformMisc::GetEnvironmentVariable(TEXT("P4HOST"));
			}

			if (LocalHostName.Len() == 0)
			{
				// no host name, use local machine name
				LocalHostName = FString(FPlatformProcess::ComputerName()).ToLower();
			}
			else
			{
				LocalHostName = LocalHostName.ToLower();
			}

			for (const FP4Record& ClientRecord : Records)
			{
				const FString& ClientName = ClientRecord(TEXT("client"));
				const FString& HostName = ClientRecord(TEXT("Host"));
				FString ClientRootPath = ClientRecord(TEXT("Root")).ToLower();

				//this clientspec has to be meant for this machine ( "" hostnames mean any host can use ths clientspec in p4 land)
				bool bHostNameMatches = (LocalHostName == HostName.ToLower());
				bool bHostNameWild = (HostName.Len() == 0);

				if( bHostNameMatches || (bHostNameWild && bAllowWildHosts) )
				{
					// A workspace root could be "null" which allows the user to map depot locations to different drives.
					// Allow these workspaces since we already allow workspaces mapped to drive letters.
					const bool bIsNullClientRootPath = (ClientRootPath == TEXT("null"));

					//make sure all slashes point the same way
					ClientRootPath = ClientRootPath.Replace(TEXT("\\"), TEXT("/"));
					ApplicationPath = ApplicationPath.Replace(TEXT("\\"), TEXT("/"));

					if (!ClientRootPath.EndsWith(TEXT("/")))
					{
						ClientRootPath += TEXT("/");
					}

					// Only allow paths that are ACTUALLY legit for this application
					if (bIsNullClientRootPath || ApplicationPath.Contains(ClientRootPath) )
					{
						OutWorkspaceList.Add(ClientName);
					}
					else
					{
						UE_LOG(LogSourceControl, Verbose, TEXT(" %s client specs rejected due to root directory mismatch (%s)"), *ClientName, *ClientRootPath);
					}

					//Other useful fields: Description, Owner, Host

				}
				else
				{
					UE_LOG(LogSourceControl, Verbose, TEXT(" %s client specs rejected due to host name mismatch (%s)"), *ClientName, *HostName);
				}
			}
		}

		return bCommandOK;
	}

	return false;
}

bool FPerforceConnection::IsValidConnection()
{
	return bEstablishedConnection && !P4Client.Dropped();
}

void FPerforceConnection::Disconnect()
{
	Error P4Error;

	P4Client.Final(&P4Error);

	if (P4Error.Test())
	{
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);
		UE_LOG(LogSourceControl, Error, TEXT("P4ERROR: Failed to disconnect from Server."));
		UE_LOG(LogSourceControl, Error, TEXT("%s"), TO_TCHAR(ErrorMessage.Text(), bIsUnicode));
	}
}

bool FPerforceConnection::RunCommand(	const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, 
										TOptional<FSharedBuffer>& OutData, TArray<FText>& OutErrorMessage, 
										FOnIsCancelled InIsCancelled, bool& OutConnectionDropped, ERunCommandFlags RunFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FPerforceConnection::RunCommand_%s"), *InCommand));

	if (!bEstablishedConnection)
	{
		return false;
	}

	const bool bLogCommandDetails = !EnumHasAllFlags(RunFlags, ERunCommandFlags::DisableCommandLogging);

	FString FullCommand = InCommand;

	// Prepare arguments
	int32 ArgC = InParameters.Num();
	UTF8CHAR** ArgV = new UTF8CHAR*[ArgC];
	for (int32 Index = 0; Index < ArgC; Index++)
	{
		if(bIsUnicode)
		{
			FTCHARToUTF8 UTF8String(*InParameters[Index]);
			ArgV[Index] = new UTF8CHAR[UTF8String.Length() + 1];
			FMemory::Memcpy(ArgV[Index], UTF8String.Get(), UTF8String.Length() + 1);
		}
		else
		{
			ArgV[Index] = new UTF8CHAR[InParameters[Index].Len() + 1];
			FMemory::Memcpy(ArgV[Index], TCHAR_TO_ANSI(*InParameters[Index]), InParameters[Index].Len() + 1);
		}
		
		if (bLogCommandDetails)
		{
			FullCommand += TEXT(" ");
			FullCommand += InParameters[Index];
		}
	}

	if (bLogCommandDetails)
	{
		UE_LOG( LogSourceControl, Log, TEXT("Attempting 'p4 %s'"), *FullCommand );
	}

	double SCCStartTime = FPlatformTime::Seconds();

	P4Client.SetArgv(ArgC, (char**)ArgV);

	FP4KeepAlive KeepAlive(InIsCancelled);
	P4Client.SetBreak(&KeepAlive);

	OutRecordSet.Reset();

	EP4ClientUserFlags ClientUserFlags = EP4ClientUserFlags::None;
	ClientUserFlags |= bIsUnicode ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;
	ClientUserFlags |= OutData ? EP4ClientUserFlags::CollectData : EP4ClientUserFlags::None;
	
	FP4ClientUser User(OutRecordSet, ClientUserFlags, OutErrorMessage);
	if (EnumHasAllFlags(RunFlags, ERunCommandFlags::Quiet))
	{
		User.SetQuiet();
	}

	P4Client.Run(FROM_TCHAR(*InCommand, bIsUnicode), &User);
	if ( P4Client.Dropped() )
	{
		OutConnectionDropped = true;
	}

	if (OutData)
	{
		OutData = User.ReleaseData();
	}	

	P4Client.SetBreak(nullptr);

	// Free arguments
	for (int32 Index = 0; Index < ArgC; Index++)
	{
		delete [] ArgV[Index];
	}
	delete [] ArgV;

	// Only report connection related errors to avoid clearing of connection related error messages
	if (InCommand != TEXT("info"))
	{
		SCCProvider.SetLastErrors(OutErrorMessage);
	}

	if (bLogCommandDetails)
	{
		const double ExecutionTime = FPlatformTime::Seconds() - SCCStartTime;
		UE_CLOG((LogSourceControl.GetVerbosity() == ELogVerbosity::VeryVerbose) || (ExecutionTime >= 0.1), LogSourceControl, Log, TEXT("P4 execution time: %0.4f seconds. Command: %s"), ExecutionTime, *FullCommand);
	}

	return OutRecordSet.Num() > 0;
}

int32 FPerforceConnection::CreatePendingChangelist(const FText &Description, const TArray<FString>& Files, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages)
{
	TArray<FString> Params;
	FP4RecordSet Records;
	EP4ClientUserFlags Flags = bIsUnicode ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;

	const char *ArgV[] = { "-i" };
	P4Client.SetArgv(1, const_cast<char*const*>(ArgV));

	FP4KeepAlive KeepAlive(InIsCancelled);
	P4Client.SetBreak(&KeepAlive);

	FP4CreateChangelistClientUser User(Records, Flags, OutErrorMessages, Description, Files, P4Client);
	P4Client.Run("change", &User);

	P4Client.SetBreak(nullptr);

	return User.ChangelistNumber;
}

int32 FPerforceConnection::EditPendingChangelist(const FText& NewDescription, int32 ChangelistNumber, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages)
{
	FP4RecordSet PreviousRecords;

	// Get changelist current specification
	{
		TArray<FString> Params;
		TOptional<FSharedBuffer> CommandData;
		bool bConnectionDropped = false;
		const ERunCommandFlags RunFlags = ERunCommandFlags::DisableCommandLogging;

		Params.Add(TEXT("-o"));
		// TODO : make this work also for default changelist, but should really be a Create
		Params.Add(FString::Printf(TEXT("%d"), ChangelistNumber));

		if (!RunCommand(TEXT("change"), Params, PreviousRecords, CommandData, OutErrorMessages, InIsCancelled, bConnectionDropped, RunFlags))
		{
			return 0;
		}
	}

	// Update description with the new description
	{
		TArray<FString> Params;
		FP4RecordSet Records;
		EP4ClientUserFlags Flags = bIsUnicode ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;

		const char* ArgV[] = { "-i" };
		P4Client.SetArgv(1, const_cast<char* const*>(ArgV));

		FP4KeepAlive KeepAlive(InIsCancelled);
		P4Client.SetBreak(&KeepAlive);

		FP4EditChangelistClientUser User(Records, Flags, OutErrorMessages, NewDescription, ChangelistNumber, PreviousRecords, P4Client);
		P4Client.Run("change", &User);

		P4Client.SetBreak(nullptr);

		return (OutErrorMessages.Num() == 0 ? User.ChangelistNumber : 0);
	}
}

bool FPerforceConnection::CreateWorkspace(FStringView WorkspaceSpec, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforceConnection::CreateWorkspace);

	TArray<FString> Params;
	FP4RecordSet Records;

	EP4ClientUserFlags Flags = bIsUnicode ? EP4ClientUserFlags::UnicodeServer : EP4ClientUserFlags::None;

	const char* ArgV[] = { "-i" };
	P4Client.SetArgv(1, const_cast<char* const*>(ArgV));

	FP4KeepAlive KeepAlive(InIsCancelled);
	P4Client.SetBreak(&KeepAlive);

	FP4CommandWithStdInputClientUser User(WorkspaceSpec, Records, Flags, OutErrorMessages, P4Client);
	User.SetQuiet();	// p4 client does not return tagged output, so any output messages will be
						// printed to stdout. Setting this will prevent that.

	P4Client.Run("client", &User);

	P4Client.SetBreak(nullptr);
	
	return OutErrorMessages.Num() == 0;
}

void FPerforceConnection::EstablishConnection(const FPerforceConnectionInfo& InConnectionInfo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPerforceConnection::EstablishConnection);

	// Verify Input. ServerName and UserName are required
	if ( InConnectionInfo.Port.IsEmpty() || InConnectionInfo.UserName.IsEmpty() )
	{
		return;
	}

	//Connection assumed successful
	bEstablishedConnection = true;

	UE_LOG(LogSourceControl, Verbose, TEXT("Attempting P4 connection: %s/%s"), *InConnectionInfo.Port, *InConnectionInfo.UserName);

	P4Client.SetProg("UE");
	P4Client.SetProtocol("tag", "");
	P4Client.SetProtocol("enableStreams", "");

	//Set configuration based params
	P4Client.SetPort(TCHAR_TO_ANSI(*InConnectionInfo.Port));

	Error P4Error;
	if(InConnectionInfo.HostOverride.Len() > 0)
	{
		UE_LOG(LogSourceControl, Verbose, TEXT(" ... overriding host" ));
		P4Client.SetHost(TCHAR_TO_ANSI(*InConnectionInfo.HostOverride));
	}

	UE_LOG(LogSourceControl, Verbose, TEXT(" ... connecting" ));

	//execute the connection to perforce using the above settings
	P4Client.Init(&P4Error);

	//ensure the connection is valid
	UE_LOG(LogSourceControl, Verbose, TEXT(" ... validating connection" ));
	if (P4Error.Test())
	{
		bEstablishedConnection = false;
		StrBuf ErrorMessage;
		P4Error.Fmt(&ErrorMessage);

		UE_LOG(LogSourceControl, Error, TEXT("P4ERROR: Invalid connection to server."));
		UE_LOG(LogSourceControl, Error, TEXT("%s"), ANSI_TO_TCHAR(ErrorMessage.Text()));
	}
	else
	{
		TArray<FString> Params;
		TArray<FText> ErrorMessages;
		FP4RecordSet Records;
		TOptional<FSharedBuffer> CommandData;
		bool bConnectionDropped = false;
		const ERunCommandFlags RunFlags = ERunCommandFlags::DisableCommandLogging;

		UE_LOG(LogSourceControl, Verbose, TEXT(" ... checking unicode status" ));

		if (RunCommand(TEXT("info"), Params, Records, CommandData, ErrorMessages, FOnIsCancelled(), bConnectionDropped, RunFlags))
		{
			// Get character encoding
			bIsUnicode = Records[0].Find(TEXT("unicode")) != nullptr;
			if(bIsUnicode)
			{
				P4Client.SetTrans(CharSetApi::UTF_8);
				UE_LOG(LogSourceControl, Verbose, TEXT(" server is unicode" ));
			}

			// Now we know our unicode status we can gather the client root
			P4Client.SetUser(FROM_TCHAR(*InConnectionInfo.UserName, bIsUnicode));

			if (InConnectionInfo.Ticket.Len() > 0)
			{
				P4Client.SetPassword(FROM_TCHAR(*InConnectionInfo.Ticket, bIsUnicode));
			}
			else if (InConnectionInfo.Password.Len() > 0)
			{
				Login(InConnectionInfo);
			}


			if (InConnectionInfo.Workspace.Len())
			{
				P4Client.SetClient(FROM_TCHAR(*InConnectionInfo.Workspace, bIsUnicode));
			}

			P4Client.SetCwd(FROM_TCHAR(*FPaths::RootDir(), bIsUnicode));

			// Gather the client root
			UE_LOG(LogSourceControl, Verbose, TEXT(" ... getting info" ));
			bConnectionDropped = false;
			if (RunCommand(TEXT("info"), Params, Records, CommandData, ErrorMessages, FOnIsCancelled(), bConnectionDropped, RunFlags))
			{
				UE_LOG(LogSourceControl, Verbose, TEXT(" ... getting clientroot" ));
				ClientRoot = Records[0](TEXT("clientRoot"));

				//make sure all slashes point the same way
				ClientRoot = ClientRoot.Replace(TEXT("\\"), TEXT("/"));
			}
		}
	}
}

FString FPerforceConnection::GetUser()
{
	if (bEstablishedConnection)
	{
		return TO_TCHAR(P4Client.GetUser().Text(), bIsUnicode);
	}
	else
	{
		return FString();
	}
}

FScopedPerforceConnection::FScopedPerforceConnection( FPerforceSourceControlCommand& InCommand )
	: Connection(nullptr)
	, Concurrency(InCommand.Concurrency)
{
	Initialize(InCommand.ConnectionInfo, InCommand.Worker->GetSCCProvider());
	if (IsValid())
	{
		InCommand.MarkConnectionAsSuccessful();
	}
}

FScopedPerforceConnection::FScopedPerforceConnection(EConcurrency::Type InConcurrency, FPerforceSourceControlProvider& SCCProvider)
	: Connection(nullptr)
	, Concurrency(InConcurrency)
{
	Initialize(SCCProvider.AccessSettings().GetConnectionInfo(), SCCProvider);
}

void FScopedPerforceConnection::Initialize( const FPerforceConnectionInfo& InConnectionInfo, FPerforceSourceControlProvider& SCCProvider)
{
	if(Concurrency == EConcurrency::Synchronous)
	{
		// Synchronous commands reuse the same persistent connection to reduce
		// the number of expensive connection attempts.
		if (SCCProvider.EstablishPersistentConnection() )
		{
			Connection = SCCProvider.GetPersistentConnection();
		}
	}
	else
	{
		// Async commands form a new connection for each attempt because
		// using the persistent connection is not threadsafe
		FPerforceConnection* NewConnection = new FPerforceConnection(InConnectionInfo, SCCProvider);
		if ( NewConnection->IsValidConnection() )
		{
			Connection = NewConnection;
		}
	}
}

FScopedPerforceConnection::~FScopedPerforceConnection()
{
	if(Connection != nullptr)
	{
		if(Concurrency == EConcurrency::Asynchronous)
		{
			// Remove this connection as it is only temporary
			Connection->Disconnect();
			delete Connection;
		}
		
		Connection = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
