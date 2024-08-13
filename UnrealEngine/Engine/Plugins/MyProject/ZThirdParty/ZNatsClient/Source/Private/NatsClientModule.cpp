#include "NatsClientModule.h"
#include "NatsClientImpl.h"

DEFINE_LOG_CATEGORY(LogZNats);
#define LOCTEXT_NAMESPACE "FZNatsClientModule"

class FZNatsClientModule : public INatsClientModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}
	
	virtual void ShutdownModule() override
	{
	}

	virtual FNatsClientPtr CreateNatsClient() override
	{
		FNatsClientPtr Ptr = MakeShared<FNatsClientImpl>();

		return Ptr;
	}
	
};

#undef LOCTEXT_NAMESPACE	
IMPLEMENT_MODULE(FZNatsClientModule, ZNatsClient)
