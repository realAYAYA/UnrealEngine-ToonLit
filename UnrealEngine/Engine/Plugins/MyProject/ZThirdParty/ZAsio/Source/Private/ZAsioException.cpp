#include "ZAsioException.h"
#include "ZAsioPrivate.h"

void AsioExceptionHandle(const std::string& Message)
{
	UE_LOG(LogZAsio, Error, TEXT("AsioException %s"), UTF8_TO_TCHAR(Message.c_str()));
}
