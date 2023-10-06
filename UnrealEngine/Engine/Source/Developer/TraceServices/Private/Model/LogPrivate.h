// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/Model/Log.h"
#include "Templates/SharedPointer.h"
#include "Common/PagedArray.h"
#include "Model/Tables.h"
#include "Misc/OutputDeviceHelper.h"

namespace TraceServices
{

class FAnalysisSessionLock;
class FStringStore;

struct FLogMessageSpec
{
	FLogCategoryInfo* Category = nullptr;
	const TCHAR* File = nullptr;
	const TCHAR* FormatString = nullptr;
	int32 Line;
	ELogVerbosity::Type Verbosity;
};

struct FLogMessageInternal
{
	FLogMessageSpec* Spec = nullptr;
	double Time;
	const TCHAR* Message = nullptr;
};

class FLogProvider
	: public ILogProvider
	, public IEditableLogProvider
{
public:
	explicit FLogProvider(IAnalysisSession& Session);
	virtual ~FLogProvider() {}

	//////////////////////////////////////////////////
	// Read operations

	virtual uint64 GetMessageCount() const override;
	virtual bool ReadMessage(uint64 Index, TFunctionRef<void(const FLogMessageInfo&)> Callback) const override;
	virtual void EnumerateMessages(double IntervalStart, double IntervalEnd, TFunctionRef<void(const FLogMessageInfo&)> Callback) const override;
	virtual void EnumerateMessagesByIndex(uint64 Start, uint64 End, TFunctionRef<void(const FLogMessageInfo&)> Callback) const override;

	virtual uint64 GetCategoryCount() const override { return Categories.Num(); }
	virtual void EnumerateCategories(TFunctionRef<void(const FLogCategoryInfo&)> Callback) const override;

	virtual const IUntypedTable& GetMessagesTable() const override { return MessagesTable; }

	//////////////////////////////////////////////////
	// Edit operations

	virtual uint64 RegisterCategory() override;
	virtual FLogCategoryInfo& GetCategory(uint64 CategoryPointer) override;

	FLogMessageSpec& GetMessageSpec(uint64 LogPoint);
	virtual void UpdateMessageCategory(uint64 LogPoint, uint64 InCategoryPointer) override;
	virtual void UpdateMessageFormatString(uint64 LogPoint, const TCHAR* InFormatString) override;
	virtual void UpdateMessageFile(uint64 LogPoint, const TCHAR* InFile, int32 InLine) override;
	virtual void UpdateMessageVerbosity(uint64 LogPoint, ELogVerbosity::Type InVerbosity) override;
	virtual void UpdateMessageSpec(uint64 LogPoint, uint64 InCategoryPointer, const TCHAR* InFormatString, const TCHAR* InFile, int32 InLine, ELogVerbosity::Type InVerbosity) override;
	virtual void AppendMessage(uint64 LogPoint, double Time, const uint8* FormatArgs) override;
	virtual void AppendMessage(uint64 LogPoint, double Time, const TCHAR* Text) override;
	void AppendMessage(uint64 LogPoint, double Time, const FString& Message);

	//////////////////////////////////////////////////

private:
	void ConstructMessage(uint64 Id, TFunctionRef<void(const FLogMessageInfo&)> Callback) const;

	enum
	{
		FormatBufferSize = 65536
	};

	IAnalysisSession& Session;
	TMap<uint64, FLogCategoryInfo*> CategoryMap;
	TMap<uint64, FLogMessageSpec*> SpecMap;
	TPagedArray<FLogCategoryInfo> Categories;
	TPagedArray<FLogMessageSpec> MessageSpecs;
	TPagedArray<FLogMessageInternal> Messages;
	TCHAR FormatBuffer[FormatBufferSize];
	TCHAR TempBuffer[FormatBufferSize];
	TTableView<FLogMessageInternal> MessagesTable;
};

} // namespace TraceServices
