/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# ifndef P4_CLIENTAPI_H
# define P4_CLIENTAPI_H

# include "stdhdrs.h"

# include "strbuf.h"
# include "strdict.h"
# include "error.h"
# include "handler.h"

# include "filesys.h"

# include "p4tags.h"

# include "clientmerge.h"
# include "clientresolvea.h"
# include "clientuser.h"

# include "keepalive.h"

/*
 * ClientApi - the Perforce client API
 *
 * Basic flow:
 *
 *	ClientUser ui;
 *	ClientApi client;
 *
 *	// SetPort(), SetProtocol() must happen _before_ the Init().
 *
 *	client.SetPort( somefunctionof( client.GetPort() ) ); //optional
 *	client.SetProtocol( "var", "value" ); //optional
 *
 *	client.Init( e );
 *
 *	// GetClient(), SetBreak(), SetProg() must happen _after_ the Init().
 *
 *	client.SetBreak( &k );      // optional
 *
 *	client.SetProg( "MyApp" );  // optional
 *
 *	client.SetVersion( "version" ); // optional
 *
 *	client.SetClient( somefunctionof( client.GetClient() ) ); //optional
 *	client.SetCwd( somefunctionof( client.GetCwd() ) ); //optional
 *	client.SetUser( somefunctionof( client.GetUser() ) ); //optional
 *
 *	while( !client.Dropped() )
 *	{
 *	    client.SetArgv( argc, argv )
 *	    client.Run( func, &ui )
 *	}
 *
 *	return client.Final( e );
 *
 * Public methods:
 *
 *	ClientApi::SetUi() - reset the ClientUser object used
 *	ClientApi::SetProtocol() - ask for special server treatment
 *	ClientApi::GetProtocol() - get a protocol capability
 *		SetProtocol() is called before Init(); GetProtocol() after Run().
 *
 *	ClientApi::Init() - establish connection and prepare to run commands.
 *
 *	ClientApi::SetVar() - set variable
 *	ClientApi::SetVarV() - set variable using var=value syntax
 *	ClientApi::SetArgv() - set unnamed variables (args for Run())
 *	ClientApi::GetVar() - get variable
 *
 *	ClientApi::Run() - run a single command
 *	ClientApi::Final() - clean up end of connection, returning error count.
 *	ClientApi::Dropped() - check if connection is no longer serviceable
 *	ClientApi::GetErrors() - get count of errors returned by server.
 *
 *	ClientApi::RunTag() - run a single command (potentially) asynchronously.
 *	ClientApi::WaitTag() - wait for a RunTag()/all RunTag()s to complete.
 *
 *	ClientApi::SetCharset()
 *	ClientApi::SetClient()
 *	ClientApi::SetCwd()
 *	ClientApi::SetHost()
 *	ClientApi::SetLanguage()
 *	ClientApi::SetPassword()
 *	ClientApi::SetPort()
 *	ClientApi::SetUser() - set client, current directory, host, port, or
 *		user, overridding all defaults.  SetPort() must be called
 *		before Init() in order to take effect.  The others must be
 *		set before Run() to take effect.
 *
 *		SetCwd() additionally searches for a new P4CONFIG file.
 *
 *	ClientApi::SetBreak() - set a subclassed KeepAlive object (only
 *		method IsAlive returns zero on dropped connection). Must
 *		be called after Init() it order to take affect.
 *
 *	ClientApi::SetProg() - set the name of the application program,
 *		this will show up in 'p4 monitor' and server log output.
 *		Should be called before Init().
 *
 *	ClientApi::SetVersion() - sets the version string of the application.
 *		If not called, the version defaults to protocolClient. This
 *		will be appended to the program name in 'p4 monitor' and
 *		server log output.  It should be called after Init() and
 *		before each call to Run().
 *
 *	ClientApi::SetTicketFile() - set the location of the users ticketfile,
 *		must be the full pathname to the file and not a directory.
 *
 *	ClientApi::SetTrustFile() - set the location of the trustfile,
 *		must be the full pathname to the file and not a directory.
 *
 *	ClientApi::SetExecutable() - set the location of the physical client
 *		executable program file. This is needed by the network
 *	        parallelism features (parallel sync/submit etc.) so that they
 *	        can spawn more copies of the program as needed.
 *
 *	ClientApi::DefineCharset()
 *	ClientApi::DefineClient()
 *	ClientApi::DefineHost()
 *	ClientApi::DefineLanguage()
 *	ClientApi::DefinePassword()
 *	ClientApi::DefinePort()
 *	ClientApi::DefineUser() - sets client, port, or user in the registry
 *		and (so as to take permanent effect) then calls SetClient(),
 *		etc. to take immediate effect.
 *
 *	ClientApi::GetBuild()
 *	ClientApi::GetCharset()
 *	ClientApi::GetClient()
 *	ClientApi::GetCwd()
 *	ClientApi::GetExecutable()
 *	ClientApi::GetHost()
 *	ClientApi::GetLanguage()
 *	ClientApi::GetOs()
 *	ClientApi::GetPassword()
 *	ClientApi::GetPort()
 *	ClientApi::GetUser() - get current directory, client, OS, port or user,
 *		as determined by defaults or by corresponding set value.
 *		GetClient()/GetHost() are not valid until after Init()
 *		establishes the connection, because the hostname of the
 *		local endpoint may serve as the default client name.
 *
 *	Client::GetConfig() - get the filename pointed to by P4CONFIG, as
 *		determined by enviro::Config().
 *
 *	ClientApi::SetIgnorePassword() - This function ignores passwords
 *		that are found in the registry (NT), host environments or
 *		configuration files.  If this function is set then only
 *		passwords supplied through SetPassword() will be honored.
 *		Tickets continue to work as normal. Must be called before
 *		Init() in order to take affect.
 */

class Client;
class ClientScript;
class Ignore;

class ClientApi : public StrDict {

    public:
	// caller's main interface
	
			ClientApi();
			ClientApi( Enviro *env );

	virtual		~ClientApi();

	void		SetTrans( int output, int content = -2,
				int fnames = -2, int dialog = -2 );

	void		SetProtocol( const char *p, const char *v );
	void		SetProtocol( const char *p );
	void		SetProtocolV( const char *p );
	StrPtr *	GetProtocol( const char *v );

	void		Init( Error *e );
	void		Run( const char *func, ClientUser *ui );
	int		Final( Error *e );
	int		Dropped();
	int		GetErrors();
	int		GetFatals();
	int		GetTrans();
	int		IsUnicode();

	void		RunTag( const char *func, ClientUser *ui );
	void		WaitTag( ClientUser *ui = 0 );

	void		SetArgv( int ac, char *const *av );
	void		SetCharset( const char *c );
	void		SetClient( const char *c );
	void		SetCwd( const char *c );
	void		SetCwdNoReload( const char *c );
	void		SetHost( const char *c );
	void		SetIgnoreFile( const char *c );
	void		SetLanguage( const char *c );
	void		SetPassword( const char *c );
	void		SetPort( const char *c );
	void		SetUser( const char *c );
	void		SetProg( const char *c );
	void		SetVersion( const char *c );
	void		SetTicketFile( const char *c );
	void		SetTrustFile( const char *c );
	void		SetEnviroFile( const char *c );

	void		SetCharset( const StrPtr *c );
	void		SetClient( const StrPtr *c );
	void		SetCwd( const StrPtr *c );
	void		SetCwdNoReload( const StrPtr *c );
	void		SetExecutable( const StrPtr *c );
	void		SetHost( const StrPtr *c );
	void		SetIgnoreFile( const StrPtr *c );
	void		SetLanguage( const StrPtr *c );
	void		SetPassword( const StrPtr *c );
	void		SetPort( const StrPtr *c );
	void		SetUser( const StrPtr *c );
	void		SetProg( const StrPtr *c );
	void		SetVersion( const StrPtr *c );
	void		SetTicketFile( const StrPtr *c );
	void		SetTrustFile( const StrPtr *c );
	void		SetEnviroFile( const StrPtr *c );

	void		SetBreak( KeepAlive *k );

	void		DefineCharset( const char *c, Error *e );
	void		DefineClient( const char *c, Error *e );
	void		DefineHost( const char *c, Error *e );
	void		DefineIgnoreFile( const char *c, Error *e );
	void		DefineLanguage( const char *c, Error *e );
	void		DefinePassword( const char *c, Error *e );
	void		DefinePort( const char *c, Error *e );
	void		DefineUser( const char *c, Error *e );

	const int	&GetAPI();
	const StrPtr	&GetCharset();
	const StrPtr	&GetClient();
	const StrPtr	&GetClientNoHost();
	const StrPtr	&GetCwd();
	const StrPtr	&GetExecutable();
	const StrPtr	&GetHost();
	const StrPtr	&GetIgnoreFile();
	const StrPtr	&GetLanguage();
	const StrPtr	&GetOs();
	const StrPtr	&GetPassword();
	const StrPtr	&GetPassword( const StrPtr *user );
	const StrPtr	&GetPort();
	const StrPtr	&GetProg();
	const StrPtr	&GetUser();
	const StrPtr	&GetConfig();
	const StrArray	*GetConfigs();
	const StrPtr	&GetBuild();
	const StrPtr	&GetVersion();

	Enviro *	GetEnviro();
	Ignore *	GetIgnore();

	void		SetIgnorePassword();

	StrPtr 		*GetEVar( const StrPtr &var );
	void		SetEVar( const StrPtr &var, const StrPtr &val );
	void		VSetVarV( const char *var );

	void		EnableExtensions( Error* e );
	void		DisableExtensions();
	bool		ExtensionsEnabled();
	void		SetExtension( ClientScript* cs, Error* e,
				      const bool callerOwns = false );

    public:
	// The old interface, where ui was held from the start

			ClientApi( ClientUser *ui );
	void		SetUi( ClientUser *i );
	void		Run( const char *func );

	// Internal wrapping constructor

			ClientApi( Client *client );

    private:
	// Our StrDict implementation
	// Set strdict.h for various GetVar/SetVar calls

	StrPtr 		*VGetVar( const StrPtr &var );
	void		VSetVar( const StrPtr &var, const StrPtr &val );
	int		VGetCount();

    private:
	Client		*client;	// wrapped up RPC
	ClientUser	*ui;		// the old way
	int		 ourClient;

} ;

# endif
