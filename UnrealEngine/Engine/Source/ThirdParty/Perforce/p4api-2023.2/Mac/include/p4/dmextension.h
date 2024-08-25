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

// Shim between higher level Rh* data and the plumbing.

struct ExtensionCallerData
{
	virtual ~ExtensionCallerData();

	// Extensions

	std::function< void( const char* msg ) > SetClientMsg
	    = []( const char* msg ){};
	std::function< bool( const char *desc, const long total,
	    const long position ) > ProgressSet
	    = []( const char *desc, const long total, 
	          const long position ){ return true; };
	std::function< std::tuple< const bool, std::string >( const char* form, 
	        std::function< bool( const char * ) > fn )
	    > ClientEditData = []( const char* form,
	            std::function< bool( const char * ) > fn ){
	        return std::make_tuple< bool, std::string >( true, std::string() ); };
	std::function< bool( const long pos ) > ProgressIncrement
	    = []( const long pos ){ return true; };
	std::function< std::tuple< bool, std::string > ( const char* msg ) > ClientOutputText
	    = []( const char* msg ){ return std::make_tuple( true, std::string( "Success" ) ); };
	std::function< std::tuple< bool, std::string > ( const int level, const char* msg ) 
	        > ReportError
	    = []( const int level, const char* msg )
	        { return std::make_tuple< bool, std::string >( true,
	            std::string( "Success" ) ); };
	std::function< std::tuple< bool, std::string > ( std::map< std::string, 
	        std::string > tagmsg ) > FstatInfo
	    = []( std::map< std::string, std::string > tagmsg ){
	        return std::make_tuple< bool, std::string >( true, 
	            std::string( "Success" ) ); };
	std::function< bool( const char* user, const char *path, 
	    int perm ) > CheckPermission
	    = []( const char* user, const char *path, int perm ){ return false; };
	std::function< void() > SetExtExecError
	    = [](){};
	StrBuf archDir, dataDir;

	std::string defaultLocale; // manifest
	std::string userLocale, userCharset, userLanguage; // client vars
	std::vector< std::string > supportedLocales; // manifest
	std::string setLocale; // extension-supplied or default
	// locale -> key -> message
	std::unordered_map< std::string, std::unordered_map< std::string,
	                                                     std::string > >
	    translationMap;

	int apiVersion = 0;
} ;

class Extension : public p4script
{
	public:

	     Extension( const SCR_VERSION v, const int apiVersion,
	                p4_std_optional::optional<
	                std::unique_ptr< ExtensionCallerData > > ecd, Error* e,
	                const bool alloc = false );
	    virtual ~Extension();

	    void LoadFile( const char* file, Error *e );
	    virtual void doBindings( Error* e );
	    ExtensionCallerData* GetECD() 
	    {  return ecd ? &**ecd : nullptr; }

	    p4_std_any::p4_any RunCallBack( const char* name, Error* e );

	protected:

	    class extImpl;
	    class extImpl53;

	    std::unique_ptr< extImpl > rhePimpl;

	private:

	    p4_std_optional::optional< std::unique_ptr< ExtensionCallerData > > ecd;
} ;

class Extension::extImpl
{
	public:

	             extImpl( Extension& p, Error *e );
	    virtual ~extImpl();

	    virtual void doBindings( Error* e ) = 0;

	    virtual p4_std_any::p4_any
	    RunCallBack( const char* name, Error* e ) = 0;

	protected:

	    Extension& parent;
} ;

class Extension::extImpl53 : public Extension::extImpl
{
	public:

	     extImpl53( Extension& p, Error *e );
	    virtual ~extImpl53();

	    virtual void doBindings( Error* e );

	    p4_std_any::p4_any RunCallBack( const char* name, Error* e );
} ;

# else

class FileSys;

struct ExtensionCallerData
{
};

class Extension
{
	public:

	    ExtensionCallerData* GetECD() { return &ecd; }

	private:

	    ExtensionCallerData ecd;
} ;

# endif
