// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteMessages.h"

#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "HAL/PlatformCrt.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/UnrealTemplate.h"


namespace UE::RemoteExecution
{
	FAddTasksRequest::FAddTasksRequest()
		: DoNotCache(false)
	{
	}

	FCbObject FAddTasksRequest::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (RequirementsHash != FIoHash::Zero)
		{
			Writer.AddObjectAttachment("r", RequirementsHash);
		}
		if (!TaskHashes.IsEmpty())
		{
			Writer.BeginArray("t");
			for (const FIoHash& Hash : TaskHashes)
			{
				Writer.AddObjectAttachment(Hash);
			}
			Writer.EndArray();
		}
		Writer.AddBool("nc", DoNotCache);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FAddTasksRequest::Load(const FCbObjectView& CbObjectView)
	{
		TaskHashes.Empty();
		RequirementsHash = CbObjectView["n"].AsObjectAttachment();
		for (FCbFieldView& CbFieldView : CbObjectView["t"].AsArrayView())
		{
			TaskHashes.Add(CbFieldView.AsObjectAttachment());
		}
		DoNotCache = CbObjectView["nc"].AsBool();
	}

	FGetTaskUpdateResponse::FGetTaskUpdateResponse()
		: Time(0)
		, State(EComputeTaskState::Queued)
		, Outcome(EComputeTaskOutcome::Success)
	{
	}

	FCbObject FGetTaskUpdateResponse::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (TaskHash != FIoHash::Zero)
		{
			Writer.AddObjectAttachment("h", TaskHash);
		}
		Writer.AddDateTime("t", Time);
		Writer.AddInteger("s", (int32)State);
		Writer.AddInteger("o", (int32)Outcome);
		Writer.AddString("d", Detail);
		if (ResultHash != FIoHash::Zero)
		{
			Writer.AddObjectAttachment("r", ResultHash);
		}
		Writer.AddString("a", AgentId);
		Writer.AddString("l", LeaseId);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FGetTaskUpdateResponse::Load(const FCbObjectView& CbObjectView)
	{
		TaskHash = CbObjectView["h"].AsObjectAttachment();
		Time = CbObjectView["t"].AsDateTime();
		State = (EComputeTaskState)CbObjectView["s"].AsInt32();
		Outcome = (EComputeTaskOutcome)CbObjectView["o"].AsInt32();
		Detail = FString(CbObjectView["d"].AsString());
		ResultHash = CbObjectView["r"].AsObjectAttachment();
		AgentId = FString(CbObjectView["a"].AsString());
		LeaseId = FString(CbObjectView["l"].AsString());
	}

	FCbObject FGetTaskUpdatesResponse::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (!Updates.IsEmpty())
		{
			Writer.BeginArray("u");
			for (const FGetTaskUpdateResponse& Update : Updates)
			{
				Writer.AddObject(Update.Save());
			}
			Writer.EndArray();
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FGetTaskUpdatesResponse::Load(const FCbObjectView& CbObjectView)
	{
		Updates.Empty();
		for (FCbFieldView& CbFieldView : CbObjectView["u"].AsArrayView())
		{
			FGetTaskUpdateResponse Response;
			Response.Load(CbFieldView.AsObjectView());
			Updates.Add(MoveTemp(Response));
		}
	}

	FFileNode::FFileNode()
		: Size(0)
		, Attributes(0)
	{
	}

	FCbObject FFileNode::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddString("n", Name);
		Writer.AddBinaryAttachment("h", Hash);
		Writer.AddInteger("s", Size);
		Writer.AddInteger("a", Attributes);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FFileNode::Load(const FCbObjectView& CbObjectView)
	{
		Name = FString(CbObjectView["n"].AsString());
		Hash = CbObjectView["h"].AsBinaryAttachment();
		Size = CbObjectView["s"].AsInt64();
		Attributes = CbObjectView["a"].AsInt32();
	}

	FCbObject FDirectoryNode::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddString("n", Name);
		Writer.AddObjectAttachment("h", Hash);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FDirectoryNode::Load(const FCbObjectView& CbObjectView)
	{
		Name = FString(CbObjectView["n"].AsString());
		Hash = CbObjectView["h"].AsObjectAttachment();
	}

	FCbObject FDirectoryTree::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (!Files.IsEmpty())
		{
			Writer.BeginArray("f");
			for (const FFileNode& File : Files)
			{
				Writer.AddObject(File.Save().AsView());
			}
			Writer.EndArray();
		}
		if (!Directories.IsEmpty())
		{
			Writer.BeginArray("d");
			for (const FDirectoryNode& Directory : Directories)
			{
				Writer.AddObject(Directory.Save().AsView());
			}
			Writer.EndArray();
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FDirectoryTree::Load(const FCbObjectView& CbObjectView)
	{
		Files.Empty();
		Directories.Empty();

		for (FCbFieldView& CbFieldView : CbObjectView["f"].AsArrayView())
		{
			FFileNode FileNode;
			FileNode.Load(CbFieldView.AsObjectView());
			Files.Add(MoveTemp(FileNode));
		}
		for (FCbFieldView& CbFieldView : CbObjectView["d"].AsArrayView())
		{
			FDirectoryNode DirectoryNode;
			DirectoryNode.Load(CbFieldView.AsObjectView());
			Directories.Add(MoveTemp(DirectoryNode));
		}
	}

	FRequirements::FRequirements()
		: Exclusive(false)
	{
	}

	FCbObject FRequirements::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddString("c", Condition);
		if (!Resources.IsEmpty())
		{
			Writer.BeginArray("r");
			for (const TPair<FString, int32>& Resource : Resources)
			{
				Writer.BeginArray();
				Writer.AddString(Resource.Key);
				Writer.AddInteger(Resource.Value);
				Writer.EndArray();
			}
			Writer.EndArray();
		}
		Writer.AddBool("e", Exclusive);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FRequirements::Load(const FCbObjectView& CbObjectView)
	{
		Resources.Empty();

		Condition = FString(CbObjectView["c"].AsString());
		for (FCbFieldView& CbFieldView : CbObjectView["r"].AsArrayView())
		{
			FCbArrayView Pair = CbFieldView.AsArrayView();
			if (Pair.Num() != 2)
			{
				continue;
			}
			FCbFieldViewIterator Iter = Pair.CreateViewIterator();
			FString Key = FString(Iter.AsString());
			int32 Value = (++Iter).AsInt32();
			Resources.Add(MoveTemp(Key), MoveTemp(Value));
		}
		Exclusive = CbObjectView["e"].AsBool();
	}

	FCbObject FTask::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddString("e", Executable);
		if (!Arguments.IsEmpty())
		{
			Writer.BeginArray("a");
			for (const FString& Argument : Arguments)
			{
				Writer.AddString(Argument);
			}
			Writer.EndArray();
		}
		if (!EnvVars.IsEmpty())
		{
			Writer.BeginArray("v");
			for (const TPair<FString, FString>& EnvVar : EnvVars)
			{
				Writer.BeginArray();
				Writer.AddString(EnvVar.Key);
				Writer.AddString(EnvVar.Value);
				Writer.EndArray();
			}
			Writer.EndArray();
		}
		Writer.AddString("w", WorkingDirectory);
		Writer.AddObjectAttachment("s", SandboxHash);
		if (RequirementsHash != FIoHash::Zero)
		{
			Writer.AddObjectAttachment("r", RequirementsHash);
		}
		if (!OutputPaths.IsEmpty())
		{
			Writer.BeginArray("o");
			for (const FString& OutputPath : OutputPaths)
			{
				Writer.AddString(OutputPath);
			}
			Writer.EndArray();
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FTask::Load(const FCbObjectView& CbObjectView)
	{
		Arguments.Empty();
		EnvVars.Empty();
		OutputPaths.Empty();

		Executable = FString(CbObjectView["e"].AsString());
		for (FCbFieldView& CbFieldView : CbObjectView["a"].AsArrayView())
		{
			Arguments.Add(FString(CbFieldView.AsString()));
		}
		for (FCbFieldView& CbFieldView : CbObjectView["v"].AsArrayView())
		{
			FCbArrayView Pair = CbFieldView.AsArrayView();
			if (Pair.Num() != 2)
			{
				continue;
			}
			FCbFieldViewIterator Iter = Pair.CreateViewIterator();
			FString Key = FString(Iter.AsString());
			FString Value = FString((++Iter).AsString());
			EnvVars.Add(MoveTemp(Key), MoveTemp(Value));
		}
		WorkingDirectory = FString(CbObjectView["w"].AsString());
		SandboxHash = CbObjectView["s"].AsObjectAttachment();
		RequirementsHash = CbObjectView["r"].AsObjectAttachment();
		for (FCbFieldView& CbFieldView : CbObjectView["o"].AsArrayView())
		{
			OutputPaths.Add(FString(CbFieldView.AsString()));
		}
	}

	FTaskResult::FTaskResult()
		: ExitCode(0)
	{
	}

	FCbObject FTaskResult::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddInteger("e", ExitCode);
		if (!StdOutHash.IsZero())
		{
			Writer.AddBinaryAttachment("so", StdOutHash);
		}
		if (!StdErrHash.IsZero())
		{
			Writer.AddBinaryAttachment("se", StdErrHash);
		}
		if (!OutputHash.IsZero())
		{
			Writer.AddObjectAttachment("o", OutputHash);
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FTaskResult::Load(const FCbObjectView& CbObjectView)
	{
		ExitCode = CbObjectView["e"].AsInt32();
		StdOutHash = CbObjectView["so"].AsBinaryAttachment();
		StdErrHash = CbObjectView["se"].AsBinaryAttachment();
		OutputHash = CbObjectView["o"].AsObjectAttachment();
	}
}
