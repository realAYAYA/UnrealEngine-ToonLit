/*
 * Copyright 1995, 2019 Perforce Software.  All rights reserved.
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

class ExtensionDataClient : public ExtensionData
{
	public:

	    // Pre-install validation and installation.
	    ExtensionDataClient( const StrBuf& archiveFile,
	                         p4_std_optional::optional< StrBuf > unzipPath,
	                         Error* e );

	    // Load an existing, installed ext.
	    ExtensionDataClient( const StrBuf& depotFile, const int& depotRev,
	                         const StrBuf& srvExtsDir,
	                         const p4_std_optional::optional< StrBuf > archiveFile,
	                         Error* e );

	    static const std::string nameDelimiter;

	private:

} ;

# else

class ExtensionDataClient
{
	public:

	    ExtensionDataClient( const StrBuf& archiveFile, Error* e ) {}

	    ExtensionDataClient( const StrBuf& depotFile, const int& depotRev,
	                         const StrBuf& srvExtsDir,
	                         StrBuf archiveFile, Error* e ) {}

} ;

# endif
