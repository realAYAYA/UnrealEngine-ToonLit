#define LOCTEXT_NAMESPACE "FNetLib"

class FNetLibModule : public IModuleInterface
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

IMPLEMENT_MODULE(FNetLibModule, NetLib)