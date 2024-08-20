#pragma once
#include "google/protobuf/message.h"
#include "Misc/Fnv.h"

class MCOMMON_API FMyTools
{

public:

	static FDateTime Now();// 获取UTC当前日期
	static FDateTime LocalNow();// 获取本地当前日期

	static FString GetProjectVersion();// 获取项目版本号

	static void ToString(const FString& In, std::string* Out);

	static int64 GeneratePbMessageTypeId(const ::google::protobuf::Message* Pb)
	{
		const std::string& PbName = Pb->GetDescriptor()->full_name();
		return FFnv::MemFnv64(PbName.c_str(), PbName.size());
	}

	template<typename T>
	static int64 GeneratePbMessageTypeId()
	{
		T Dummy;
		return GeneratePbMessageTypeId(&Dummy);
	}
};
