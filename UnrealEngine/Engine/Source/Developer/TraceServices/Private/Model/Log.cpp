// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/Log.h"
#include "Model/LogPrivate.h"

#include "AnalysisServicePrivate.h"
#include "Common/FormatArgs.h"

namespace TraceServices
{

FLogProvider::FLogProvider(IAnalysisSession& InSession)
	: Session(InSession)
	, Categories(InSession.GetLinearAllocator(), 128)
	, MessageSpecs(InSession.GetLinearAllocator(), 1024)
	, Messages(InSession.GetLinearAllocator(), 1024)
	, MessagesTable(Messages)
{
	MessagesTable.EditLayout().
		AddColumn(&FLogMessageInternal::Time, TEXT("Time")).
		AddColumn<const TCHAR*>([](const FLogMessageInternal& Message)
			{
				return ToString(Message.Spec->Verbosity);
			},
			TEXT("Verbosity")).
		AddColumn<const TCHAR*>([](const FLogMessageInternal& Message)
			{
				return Message.Spec->Category->Name;
			},
			TEXT("Category")).
		AddColumn<const TCHAR*>([](const FLogMessageInternal& Message)
			{
				return Message.Spec->File;
			},
			TEXT("File")).
		AddColumn<int32>([](const FLogMessageInternal& Message)
			{
				return Message.Spec->Line;
			},
			TEXT("Line")).
		AddColumn(&FLogMessageInternal::Message, TEXT("Message"));
}

uint64 FLogProvider::RegisterCategory()
{
	static uint64 IdGenerator = 0;
	return IdGenerator++;
}

FLogCategoryInfo& FLogProvider::GetCategory(uint64 CategoryPointer)
{
	Session.WriteAccessCheck();
	if (CategoryMap.Contains(CategoryPointer))
	{
		return *CategoryMap[CategoryPointer];
	}
	else
	{
		FLogCategoryInfo& Category = Categories.PushBack();
		Category.Name = TEXT("N/A");
		Category.DefaultVerbosity = ELogVerbosity::All;
		CategoryMap.Add(CategoryPointer, &Category);
		return Category;
	}
}

FLogMessageSpec& FLogProvider::GetMessageSpec(uint64 LogPoint)
{
	Session.WriteAccessCheck();
	if (SpecMap.Contains(LogPoint))
	{
		return *SpecMap[LogPoint];
	}
	else
	{
		FLogMessageSpec& Spec = MessageSpecs.PushBack();
		SpecMap.Add(LogPoint, &Spec);
		return Spec;
	}
}

void FLogProvider::UpdateMessageCategory(uint64 LogPoint, uint64 InCategoryPointer)
{
	Session.WriteAccessCheck();
	FLogMessageSpec& LogMessageSpec = GetMessageSpec(LogPoint);
	LogMessageSpec.Category = &GetCategory(InCategoryPointer);
}

void FLogProvider::UpdateMessageFormatString(uint64 LogPoint, const TCHAR* InFormatString)
{
	Session.WriteAccessCheck();
	FLogMessageSpec& LogMessageSpec = GetMessageSpec(LogPoint);
	LogMessageSpec.FormatString = InFormatString;
}

void FLogProvider::UpdateMessageFile(uint64 LogPoint, const TCHAR* InFile, int32 InLine)
{
	Session.WriteAccessCheck();
	FLogMessageSpec& LogMessageSpec = GetMessageSpec(LogPoint);
	LogMessageSpec.File = InFile;
	LogMessageSpec.Line = InLine;
}

void FLogProvider::UpdateMessageVerbosity(uint64 LogPoint, ELogVerbosity::Type InVerbosity)
{
	Session.WriteAccessCheck();
	FLogMessageSpec& LogMessageSpec = GetMessageSpec(LogPoint);
	LogMessageSpec.Verbosity = InVerbosity;
}

void FLogProvider::UpdateMessageSpec(uint64 LogPoint, uint64 InCategoryPointer, const TCHAR* InFormatString, const TCHAR* InFile, int32 InLine, ELogVerbosity::Type InVerbosity)
{
	Session.WriteAccessCheck();
	FLogMessageSpec& LogMessageSpec = GetMessageSpec(LogPoint);
	LogMessageSpec.Category = &GetCategory(InCategoryPointer);
	LogMessageSpec.FormatString = InFormatString;
	LogMessageSpec.File = InFile;
	LogMessageSpec.Line = InLine;
	LogMessageSpec.Verbosity = InVerbosity;
}

void FLogProvider::AppendMessage(uint64 LogPoint, double Time, const uint8* FormatArgs)
{
	Session.WriteAccessCheck();
	FLogMessageSpec** FindSpec = SpecMap.Find(LogPoint);
	if (FindSpec && (*FindSpec)->Verbosity != ELogVerbosity::SetColor)
	{
		FLogMessageInternal& InternalMessage = Messages.PushBack();
		InternalMessage.Time = Time;
		InternalMessage.Spec = *FindSpec;
		FFormatArgsHelper::Format(FormatBuffer, FormatBufferSize - 1, TempBuffer, FormatBufferSize - 1, InternalMessage.Spec->FormatString, FormatArgs);
		InternalMessage.Message = Session.StoreString(FormatBuffer);
		Session.UpdateDurationSeconds(Time);
	}
}

void FLogProvider::AppendMessage(uint64 LogPoint, double Time, const TCHAR* Text)
{
	Session.WriteAccessCheck();
	FLogMessageSpec** FindSpec = SpecMap.Find(LogPoint);
	if (FindSpec && (*FindSpec)->Verbosity != ELogVerbosity::SetColor)
	{
		FLogMessageInternal& InternalMessage = Messages.PushBack();
		InternalMessage.Time = Time;
		InternalMessage.Spec = *FindSpec;
		InternalMessage.Message = Text;
		Session.UpdateDurationSeconds(Time);
	}
}

void FLogProvider::AppendMessage(uint64 LogPoint, double Time, const FString& Message)
{
	Session.WriteAccessCheck();
	FLogMessageSpec** FindSpec = SpecMap.Find(LogPoint);
	if (FindSpec && (*FindSpec)->Verbosity != ELogVerbosity::SetColor)
	{
		FLogMessageInternal& InternalMessage = Messages.PushBack();
		InternalMessage.Time = Time;
		InternalMessage.Spec = *FindSpec;
		InternalMessage.Message = Session.StoreString(Message);
		Session.UpdateDurationSeconds(Time);
	}
}

uint64 FLogProvider::GetMessageCount() const
{
	Session.ReadAccessCheck();
	return Messages.Num();
}

bool FLogProvider::ReadMessage(uint64 Index, TFunctionRef<void(const FLogMessageInfo&)> Callback) const
{
	Session.ReadAccessCheck();
	if (Index >= Messages.Num())
	{
		return false;
	}
	ConstructMessage(Index, Callback);
	return true;
}

void FLogProvider::EnumerateMessages(double IntervalStart, double IntervalEnd, TFunctionRef<void(const FLogMessageInfo&)> Callback) const
{
	Session.ReadAccessCheck();
	if (IntervalStart > IntervalEnd)
	{
		return;
	}
	uint64 MessageCount = Messages.Num();
	for (uint64 Index = 0; Index < MessageCount; ++Index)
	{
		double Time = Messages[Index].Time;
		if (IntervalStart <= Time && Time <= IntervalEnd)
		{
			ConstructMessage(Index, Callback);
		}
	}
}

void FLogProvider::EnumerateMessagesByIndex(uint64 Start, uint64 End, TFunctionRef<void(const FLogMessageInfo&)> Callback) const
{
	Session.ReadAccessCheck();
	uint64 Count = Messages.Num();
	if (Start >= Count)
	{
		return;
	}
	if (End > Count)
	{
		End = Count;
	}
	if (Start >= End)
	{
		return;
	}
	for (uint64 Index = Start; Index < End; ++Index)
	{
		ConstructMessage(Index, Callback);
	}
}

void FLogProvider::ConstructMessage(uint64 Index, TFunctionRef<void(const FLogMessageInfo&)> Callback) const
{
	const FLogMessageInternal& InternalMessage = Messages[Index];
	FLogMessageInfo Message;
	Message.Index = Index;
	Message.Time = InternalMessage.Time;
	Message.Category = InternalMessage.Spec->Category;
	Message.File = InternalMessage.Spec->File;
	Message.Line = InternalMessage.Spec->Line;
	Message.Verbosity = InternalMessage.Spec->Verbosity;
	Message.Message = InternalMessage.Message;
	Callback(Message);
}

void FLogProvider::EnumerateCategories(TFunctionRef<void(const FLogCategoryInfo&)> Callback) const
{
	Session.ReadAccessCheck();
	for (auto Iterator = Categories.GetIteratorFromItem(0); Iterator; ++Iterator)
	{
		Callback(*Iterator);
	}
}

FName GetLogProviderName()
{
	static const FName Name("LogProvider");
	return Name;
}

const ILogProvider& ReadLogProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<ILogProvider>(GetLogProviderName());
}

IEditableLogProvider& EditLogProvider(IAnalysisSession& Session)
{
	return *Session.EditProvider<IEditableLogProvider>(GetLogProviderName());
}

void FormatString(TCHAR* OutputString, uint32 OutputStringCount, const TCHAR* FormatString, const uint8* FormatArgs)
{
	TCHAR* TempBuffer = (TCHAR*)FMemory_Alloca(OutputStringCount * sizeof(TCHAR));
	FFormatArgsHelper::Format(OutputString, OutputStringCount - 1, TempBuffer, OutputStringCount - 1, FormatString, FormatArgs);
}

} // namespace TraceServices
