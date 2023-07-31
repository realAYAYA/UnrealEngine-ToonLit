/*
 * Copyright 1995, 2018 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# ifdef HAS_EXTENSIONS

# include <map>
# include <string>
# include <vector>
# include <p4_optional.h>
# include <functional>
# include <unordered_map>
# include <unordered_set>

# include <p4script.h>

// Manage extension metadata and storage.

class ExtensionData
{
	public:

	    // Pre-install validation and installation.
	    ExtensionData( const StrBuf& archiveFile,
	                   p4_std_optional::optional< StrBuf > unzipPath, Error* e );

	    // Load an existing, installed ext.
	    ExtensionData( const StrBuf& depotFile, const int& depotRev,
	                   const StrBuf& srvExtsDir,
	                   const p4_std_optional::optional< StrBuf > archiveFile,
	                   Error* e );

	   virtual ~ExtensionData();

	    void SetKey( const char* key );
	    void SetRevision( const int rev );
	    void SetSrvExtDir( const StrBuf& dir );
	    void SetProduct( const char* p );

	    bool checkProductCompat( Error* e ) const;
	    bool Install( Error* e );
	    bool LoadMetadata( Error* e );
	    bool LoadManifest( Error* e );
	    bool LoadTranslations( Error* e );

	    StrBuf GetDepotPath( const StrPtr& extsDepot );
	    static StrBuf GetExtNameFromDepotPath( const StrPtr& depotFile );
	    static StrBuf GetUUIDFromDepotFile( const StrBuf& depotFile );
	    static std::tuple< StrBuf, StrBuf, StrBuf > 
	        ParseFullname( const StrPtr& fullname, Error *e );
	    static StrBuf MakeFullname( const std::string& namespc,
	        const std::string& extname, const std::string& extrev );

	    int GetAPIVersion() const;
	    StrPtr* GetDescription();
	    StrPtr* GetNamespc();
	    StrPtr* GetName();
	    StrPtr* GetRevisionStr();
	    StrPtr* GetUUID();
	    StrPtr* GetVersion();
	    StrBuf GetScriptMainPath();
	    StrBuf GetArchDir();
	    StrPtr* GetDeveloper();
	    StrPtr* GetDefaultLocale();
	    StrPtr* GetLicense();
	    StrPtr* GetDeveloperUrl();
	    StrPtr* GetHomepageUrl();
	    StrBuf SerializeCompatProds() const;
	    std::unordered_map< std::string,
	                        std::unordered_map< std::string, std::string > >
	        GetTranslationMap();
	    int GetRevision() const;
	    SCR_VERSION GetScrVersion() const;

	    // Directory where the extension itself is unpacked
	    StrBuf GetExtSrvExtDir() const;

	    // Directory where files an extension creates are stored
	    StrBuf GetExtSrvExtDataDir() const;

	    static const std::string nameDelimiter;
	    static const std::string revDelimiter;

	protected:

	    virtual bool ValidateManifest( Error* e ) const;

	    std::pair< StrBuf, StrBuf >
	    SplitSpecUUID( const StrBuf& SpecAndUUID );

	    FileSysUPtr Unzip( const StrBuf &zipFileName,
	                       p4_std_optional::optional< StrBuf > unzipPath,
	                       p4_std_optional::optional< StrBuf > oneFile, Error *e );

	    StrBuf srvExtsDir; // server.extensions.dir

	    FileSysUPtr archiveDir;

	    StrBuf Spec;
	    int revision{};
	    StrBuf revisionStr;

	    std::string prod;

	    // Manifest data:

	    int manifestVersion{}, apiVersion{};

	    std::vector< std::string > supportedLocales;
	    // locale ('en_US') -> key ('extensionName') -> message
	    std::unordered_map< std::string,
	                        std::unordered_map< std::string, std::string > >
	        translationMap;

	    std::unordered_set< std::string > compatProds;

	    StrBuf name, namespc, version, versionName, description, license,
	           licenseBody, defaultLocale, homepageUrl,
	           key, developerName, developerUrl, runtimeLanguage,
	           runtimeVersion;
} ;

# else

//# include <p4_optional.h>

class ExtensionData
{
	public:


	    ExtensionData( const StrBuf& archiveFile, Error* e ) {}

	    ExtensionData( const StrBuf& depotFile, const int& depotRev,
	                   const StrBuf& srvExtsDir,
	                   StrBuf archiveFile, Error* e ) {}

	    bool Install( Error* e ) { return false; }
	    bool LoadManifest( Error* e ) { return false; }
	    StrBuf GetDepotPath( const StrPtr& extsDepot )
	    { StrBuf r; return r; }
	    StrPtr* GetName() { return NULL; }
	    StrPtr* GetVersion() { return NULL; }
	    void SetSrvExtDir( const StrBuf& dir ) {}
	    void SetRevision( const int rev ) {}
	    static StrBuf GetExtNameFromDepotPath( const StrPtr& depotFile )
	    { StrBuf r; return r; }
	    static StrBuf GetUUIDFromDepotFile( const StrBuf& depotFile )
	    { return StrBuf(); }
	    static void
	        ParseFullname( const StrPtr& fullname, Error *e )
	    { return; }
	    static StrBuf MakeFullname( const char* namespc,
	        const char* extname ) { return StrBuf(); }
	    StrPtr* GetNamespc() { return NULL; }
	    bool LoadMetadata( Error* e ) { return false; }

	    static const char* nameDelimiter;
	    static const char* revDelimiter;
} ;

# endif
