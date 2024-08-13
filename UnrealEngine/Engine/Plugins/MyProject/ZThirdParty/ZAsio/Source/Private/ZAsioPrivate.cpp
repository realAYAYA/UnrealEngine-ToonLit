#include "ZAsioPrivate.h"

DEFINE_LOG_CATEGORY(LogZAsio);

#define LOCTEXT_NAMESPACE "ZAsioModule"

class FZAsioModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
	}
	
	virtual void ShutdownModule() override
	{
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FZAsioModule, ZAsio);
