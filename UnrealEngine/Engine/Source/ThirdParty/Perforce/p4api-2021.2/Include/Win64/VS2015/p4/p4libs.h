/*
 * Copyright 1995, 2019 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
	Initialize()

	Perform static initialization of libraries required to use the P4 API.
	If the host application handles some of them itself (such as if it has
	its own copy of one of the libraries) then the initialization can
	be skipped by passing a combination of P4LibrariesInits values or-ed
	together.

	InitializeThread()

	Should be the first thing called in a thread.

	ShutdownThread()

	Should be the last thing called in a thread.

	Shutdown()

	Clean up static initializations.  Must be called with the same flags
	as Initialize().

	DisableZlibOptimization()

	The Zlib bundled with the P4API may use SSE3/SSE42/PCLMULQDQ
	instructions.  Certain CPUs do not correctly implement these.  To
	disable their use, call this function.

	DisableFileSysCreateOnIntr()

	The FileSys::Create() function will by default register the new file
	with the global Signaler class instance so when the program is
	interrupted via ctrl-c, any temp files associated with it can be
	cleaned up.  Call this function to disable this functionality.

	EnableFileSysCreateOnIntr()

	Opposite of DisableFileSysCreateOnIntr().
*/

enum P4LibrariesInits
{
	P4LIBRARIES_INIT_P4     = 0x01,
	P4LIBRARIES_INIT_SQLITE = 0x02,
	P4LIBRARIES_INIT_CURL   = 0x04,
	P4LIBRARIES_INIT_OPENSSL= 0x08,
	P4LIBRARIES_INIT_ALL    = 0x0F,
};

class P4Libraries
{
	public:

	    static void Initialize( const int libraries, Error* e );
	    static void Shutdown( const int libraries, Error* e );
	    static void DisableZlibOptimization();
	    static void DisableFileSysCreateOnIntr();
	    static void EnableFileSysCreateOnIntr();

	    // Note that these are internal routines for Perforce products
	    // and are not necessary to call for general P4API use.
	    static void InitializeThread( const int libraries, Error* e );
	    static void ShutdownThread( const int libraries, Error* e );

};
