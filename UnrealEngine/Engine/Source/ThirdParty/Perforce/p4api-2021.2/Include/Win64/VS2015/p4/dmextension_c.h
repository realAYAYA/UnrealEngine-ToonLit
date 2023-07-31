/*
 * Copyright 1995, 2018 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# ifdef HAS_EXTENSIONS

# include <map>
# include <p4_any.h>
# include <tuple>
# include <string>
# include <vector>
# include <p4_optional.h>
# include <functional>
# include <unordered_map>
# include <unordered_set>

# include <p4script.h>

class FileSys;
class Client;
class ClientUser;

struct ExtensionCallerDataC : public ExtensionCallerData
{
	std::string func, sourcePath;

	Client* client;
	ClientUser* ui;

	std::function< int( StrBuf&, StrBuf&, int argc,
	    std::vector< std::string > argv,
	    std::unordered_map< std::string, std::string > ssodict,
	    Error* ) > loginSSO;
} ;

class ExtensionClient : public Extension
{
	public:

	    ExtensionClient( const SCR_VERSION v, const int apiVersion,
	                     p4_std_optional::optional<
	                     std::unique_ptr< ExtensionCallerData > > ecd,
	                     Error* e );

	    virtual ~ExtensionClient();

	    void DisableExtensionBinding();

	protected:

	    class extImpl53client;

};

class ExtensionClient::extImpl53client : public Extension::extImpl53
{
	public:

	     extImpl53client( Extension& p, Error* e );
	    ~extImpl53client();

	    void doBindings( Error* e );

	    void DisableExtensionBinding();

} ;

# else

# include <p4script.h>

struct ExtensionCallerDataC
{
};

class ExtensionClient : public Extension
{
	public:

	    ExtensionClient( const SCR_VERSION v,
	                     const char* ecd,
	                     Error* e )
	    {
	        e->Set( MsgScript::ExtScriptNotInBuild );
	    }

	    void SetMaxMem( int ){}
	    void SetMaxTime( int ){}
	    bool doFile( const char*, Error* ){ return false; }
	    bool doStr( const char*, Error* ){ return false; }

	    void DisableExtensionBinding();
};

# endif
