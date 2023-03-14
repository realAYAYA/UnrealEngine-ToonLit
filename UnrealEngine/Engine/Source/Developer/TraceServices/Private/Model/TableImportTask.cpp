// Copyright Epic Games, Inc. All Rights Reserved.

#include "TableImportTask.h"

#include "Async/TaskGraphInterfaces.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/FileHelper.h"
#include "Tasks/Task.h"

namespace TraceServices
{

#define LOCTEXT_NAMESPACE "TableImportTask"

FTableImportTask::FTableImportTask(const FString& InFilePath, FName InTableId, FTableImportService::TableImportCallback InCallback)
	: Callback(InCallback)
	, FilePath(InFilePath)
	, TableId(InTableId)
{
}

FTableImportTask::~FTableImportTask()
{
}

void FTableImportTask::operator()()
{
	ETableImportResult Result = ImportTable();

	TSharedPtr<FTableImportCallbackParams> Params = MakeShared<FTableImportCallbackParams>();
	Params->TableId = TableId;
	Params->Result = Result;
	Params->Table = Table;
	Params->Messages = Messages;

	FFunctionGraphTask::CreateAndDispatchWhenReady(
		[Callback = this->Callback, Params]()
		{
			Callback(Params);
		},
		TStatId(),
		nullptr,
		ENamedThreads::GameThread);
}

ETableImportResult FTableImportTask::ImportTable()
{
	Table = MakeShared<TImportTable<FImportTableRow>>();

	TArray<FString> Lines;
	bool Result = LoadFileToStringArray(FilePath, Lines);

	if (!Result)
	{
		AddError(FText::Format(LOCTEXT("FailedToReadMsg", "Import failed because file {0} could not be read:"), FText::FromString(FilePath)));
		return ETableImportResult::EFail;
	}

	if (Lines.Num() < 2)
	{
		AddError(FText::Format(LOCTEXT("NotEnoughtLinesMsg", "Import failed because the files did not contain a minimum of 2 lines."), FText::FromString(FilePath)));
		return ETableImportResult::EFail;
	}

	if (FilePath.EndsWith(TEXT(".csv")))
	{
		Separator = TEXT(",");
	}
	else if (FilePath.EndsWith(TEXT(".tsv")))
	{
		Separator = TEXT("\t");
	}

	if (!ParseHeader(Lines[0]))
	{
		return ETableImportResult::EFail;
	}

	if (!CreateLayout(Lines[1]))
	{
		return ETableImportResult::EFail;
	}

	if (!ParseData(Lines))
	{
		return ETableImportResult::EFail;
	}

	return ETableImportResult::ESuccess;
}

bool FTableImportTask::ParseHeader(const FString& HeaderLine)
{
	HeaderLine.ParseIntoArray(ColumnNames, *Separator);
	if (ColumnNames.Num() == 0)
	{
		AddError(FText::Format(LOCTEXT("NoColumnsMsg", "Import failed because the file did not contain any columns."), FText::FromString(FilePath)));
		return false;
	}

	return true;
}

bool FTableImportTask::CreateLayout(const FString& Line)
{
	TArray<FString> Values;
	SplitLineIntoValues(Line, Values);

	if (Values.Num() != ColumnNames.Num())
	{
		AddError(LOCTEXT("ValuesMismatchMsg1", "Import failed because the number of values on line 1 does not match the number of columns in the header line."));
		return false;
	}

	TTableLayout<FImportTableRow>& Layout = Table->EditLayout();
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		auto ProjectorFunc = [Index](const FImportTableRow& Row)
		{
			return Row.GetValue(Index);
		};

		if (Values[Index].IsNumeric())
		{
			if (Values[Index].Contains(TEXT(".")))
			{
				Layout.AddColumn<double>(*ColumnNames[Index], ProjectorFunc);
			}
			else
			{
				Layout.AddColumn<uint32>(*ColumnNames[Index], ProjectorFunc);
			}
		}
		else
		{
			Layout.AddColumn<const TCHAR*>(*ColumnNames[Index], ProjectorFunc);
		}
	}

	return true;
}

bool FTableImportTask::ParseData(TArray<FString>& Lines)
{
	TTableLayout<FImportTableRow>& Layout = Table->EditLayout();
	bool Restart = false;

	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		TArray<FString> Values;
		SplitLineIntoValues(Lines[LineIndex], Values);

		if (Values.Num() != ColumnNames.Num())
		{
			AddError(FText::Format(LOCTEXT("ValuesMismatchMsg2", "Import failed because the number of values on line {0} does not match the number of columns in the header line."), LineIndex + 1));
			return false;
		}

		FImportTableRow& NewRow = Table->AddRow();
		NewRow.SetNumValues(Values.Num());
		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			ETableColumnType ColumnType = Layout.GetColumnType(ValueIndex);
			const TCHAR* Value = *Values[ValueIndex];
			if (ColumnType == ETableColumnType::TableColumnType_CString)
			{
				const TCHAR* StoredValue = Table->GetStringStore().Store(Value);
				NewRow.SetValue(ValueIndex, StoredValue);
			}
			else if (ColumnType == ETableColumnType::TableColumnType_Double)
			{
				if (!Values[ValueIndex].IsNumeric())
				{
					Layout.SetColumnType(ValueIndex, ETableColumnType::TableColumnType_CString);
					Restart = true;
					break;
				}

				NewRow.SetValue(ValueIndex, FCString::Atod(Value));
			}
			else if (ColumnType == ETableColumnType::TableColumnType_Int)
			{
				if (!Values[ValueIndex].IsNumeric())
				{
					Layout.SetColumnType(ValueIndex, ETableColumnType::TableColumnType_CString);
					Restart = true;
					break;
				}
				else if (Values[ValueIndex].Contains(TEXT(".")))
				{
					Layout.SetColumnType(ValueIndex, ETableColumnType::TableColumnType_Double);
					Restart = true;
					break;
				}

				NewRow.SetValue(ValueIndex, FCString::Atoi64(Value));
			}
		}

		if (Restart)
		{
			TSharedPtr<TImportTable<FImportTableRow >> NewTable = MakeShared<TImportTable<FImportTableRow>>();
			NewTable->EditLayout() = Layout;
			Table = NewTable;
			ParseData(Lines);

			break;
		}
	}

	return true;
}

void FTableImportTask::SplitLineIntoValues(const FString& InLine, TArray<FString>& OutValues)
{
	//Parse the line into values. Separators inside quotes are ignored.
	int Start = 0;
	bool IsInQuotes = false;
	int Index;
	for (Index = 0; Index < InLine.Len(); ++Index)
	{
		if (InLine[Index] == TEXT('"') && (Index == 0 || InLine[Index - 1] != TEXT('\\')))
		{
			IsInQuotes = !IsInQuotes;
		}
		else if(!IsInQuotes && InLine[Index] == Separator[0])
		{
			FString Value = InLine.Mid(Start, Index - Start);
			OutValues.Add(Value.TrimQuotes());
			Start = Index + 1;
		}
	}

	FString Value = InLine.Mid(Start, Index - Start);
	OutValues.Add(Value.TrimQuotes());
}

bool FTableImportTask::LoadFileToStringArray(const FString &InFilePath, TArray<FString>& Lines)
{
	// We use this function instead of FFileHelper::LoadFileToArray because that one does not support very large files.

	auto Visitor = [&Lines](FStringView Line)
	{
		Lines.Add(FString(Line));
	};

	return FFileHelper::LoadFileToStringWithLineVisitor(*FilePath, Visitor);
}

void FTableImportTask::AddError(const FText& Msg)
{
	Messages.Add(FTokenizedMessage::Create(EMessageSeverity::Error, Msg));
}

void FTableImportService::ImportTable(const FString& InPath, FName TableId, FTableImportService::TableImportCallback InCallback)
{
	UE::Tasks::Launch(UE_SOURCE_LOCATION, FTableImportTask(InPath, TableId, InCallback));
}

#undef LOCTEXT_NAMESPACE

} // namespace TraceServices
