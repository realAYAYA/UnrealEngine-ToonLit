/*
 * Copyright 1995, 1997 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/* 
 *  This Class Wraps the p4api Enviro class in order to correct
 *  Some bad behavior that Enviro has under windows NT
 *  We need to be able to Set() variables and Get() the same value back
 *  By default in Enviro,  the Get() will always return what is in the registry or environment and ignore
 *  any more Local Sets
 *  
 *  So we added the Update() method with allows you to set a Local Override to Enviro.
 *  Update(value,NULL)  will clear a Local Override, Allowing Get() to search deeper
 *  Update(value,"") will return a Local Override, So Get() will not continue to search deeper for a value,
 *  
 *  There is a lot of code in this class duplicated from the p4api Enviro class,  once Enviro is updated it may be possible
 *  to get rid of a lot of the duplication
 */


#include "stdafx.h"

#include <msgsupp.h>
#include <vararray.h>
#include <strarray.h>
#include <pathsys.h>
#include <debug.h>

#include "P4BridgeEnviro.h"
#include <strtable.h>

#if 1
	// use the default class
#else
struct EnviroItem {
	StrBuf	var;
	StrBuf	value;
	Enviro::ItemType type;
	StrBuf	origin;
	int	checked;
} ;

/*
 * EnviroTable -- cached variable settings
 */

class EnviroTable : public VarArray {

    public:
			~EnviroTable();

	EnviroItem	*GetItem( const StrRef &var );
	EnviroItem	*PutItem( const StrRef &var );
	void		RemoveType( Enviro::ItemType ty );
	void        RemoveItem( const StrRef &var );
} ;

/*
 * Remove any items with the given variable name from the table
 */
  
void
EnviroTable::RemoveItem(const StrRef &var )
{
	EnviroItem *a;

	for( int i = Count(); i > 0 ; )
	{
	    a = static_cast<EnviroItem *>(Get(--i));

	    if( !a->var.SCompare( var ) )
	    {
		      delete a;
			  Remove( i );
		}
	}
}

P4BridgeEnviro::P4BridgeEnviro() : Enviro()
{
	symbolTab = 0;
	configFiles = new StrArray;
}

P4BridgeEnviro::~P4BridgeEnviro()
{
	delete configFiles;
	delete symbolTab;
}

void
P4BridgeEnviro::Setup()
{
	if( !symbolTab )
	{
	    symbolTab = new EnviroTable;

	    LoadEnviro( 0 );
	}
}

void
P4BridgeEnviro::Update( const char *var, const char *value )
{
	EnviroItem *a = GetItem( var );

	a->type = UPDATE;
	a->value.Set( value );
}

void
P4BridgeEnviro::ClearUpdate( const char *var) const
{
	symbolTab->RemoveItem( StrRef( const_cast<char *>(var)));
}

/*
 * See if a variable is Set, if so return it's value, else return NULL
 */

char *
P4BridgeEnviro::Get( const char *var )
{
	char *lvar = GetLocal(var);
	if (! lvar)
	{
		// Value not in the Local cache
		return Enviro::Get(var);  // Look in deeper layers
	} 

	return lvar;   // return a string
}

/*
 * See if a value is in the local environ cache
 * If not, return a null
 * If so return the string value
 */
 
char *
P4BridgeEnviro::GetLocal( const char *var )
{
	// Make sure symbol table is there
	Setup();

	EnviroItem *a = symbolTab->GetItem( StrRef( const_cast<char *>(var)));

	if (! a)
	{
		return NULL; // not in the local cache
	}

	return a->value.Text();   // an empty string will translate to NULL later.
}


EnviroItem *
P4BridgeEnviro::GetItem( const char *var )
{
	// Make sure symbol table is there
	Setup();

	// attempt to return cached value
	// null string means unset

	EnviroItem *a = symbolTab->PutItem( StrRef( (char *)var ) );

	//a->type = UPDATE;

	return a;
}

void	P4BridgeEnviro::Config( const StrPtr &cwd )
{
	StrBuf setFile;
	char *s;
	if( ( s = Get( "P4CONFIG" ) ) )
	    setFile.Set( s );
	else
	    return;

	LoadConfig( cwd, setFile, 0);
}

const StrPtr &
P4BridgeEnviro::GetConfig()
{
	// configFile - name of last P4CONFIG file found by Config()

	if( !configFile.Length() )
	    configFile.Set( "noconfig" );

	return configFile;
}

const StrArray *
P4BridgeEnviro::GetConfigs()
{
	// configFiles - names of all P4CONFIG files found by Config()

	return configFiles;
}



static void
WriteItem( FileSys *newf, const char *var, const char *value, Error *e )
{
	newf->Write( var, static_cast<int>(strlen(var)), e );
	newf->Write( "=", 1, e );
	newf->Write( value, static_cast<int>(strlen(value)), e );
	newf->Write( "\n", 1, e );
}

//
// Given a working directory and the name of the .P4CONFIG file
//  Load the P4CONFIG file

void
P4BridgeEnviro::LoadConfig( const StrPtr &cwd, const StrBuf setFile, int checkSyntax )
{
	// We don't care about errors
	Error e;

	// Make sure symbol tab is there
	Setup();

	// If we're reloading after changing directory, we need to drop any
	// settings from previously loaded P4CONFIG files.

	symbolTab->RemoveType( CONFIG );
	LoadEnviro( 0 );
	configFile.Clear();
	configFiles->Clear();

	// client up the directory tree, looking for setFile

	PathSys *p = PathSys::Create();
	PathSys *q = PathSys::Create();
	FileSys *f = FileSys::Create( FileSysType( FST_TEXT|FST_L_CRLF ) );

# if defined(OS_NT)
	if( charset )
	{
	    p->SetCharSet( charset );
	    q->SetCharSet( charset );
	    f->SetCharSetPriv( charset );
	}
# endif

	// Start with current dir

	p->Set( cwd );

	do {
	    // Can we find the file?

	    e.Clear();
	    q->SetLocal( *p, setFile );
	    f->Set( *q );
	    f->Open( FOM_READ, &e );

	    if( e.Test() )
		continue;

	    // Save the name of the config file
	    
	    configFile.Set( f->Name() );
	    configFiles->Put()->Set( f->Name() );

	    // Slurp contents into client name

	    ReadConfig( f, &e, checkSyntax, CONFIG );

	    f->Close( &e );
	}
	while( p->ToParent() );

	// free & clear

	delete f;
	delete q;
	delete p;
}
void
P4BridgeEnviro::ReadConfig( FileSys *f, Error *e, int checkSyntax, Enviro::ItemType ty )
{
	StrBuf line, var;

	while( f->ReadLine( &line, e ) )
	{
	    line.TruncateBlanks();

	    char *equals = strchr( line.Text(), '=' );
	    if( !equals ) continue;

	    // tunable ?

	    p4debug.SetLevel( line.Text() );

	    // Just variable name

	    var.Set( line.Text(), static_cast<p4size_t>(equals - line.Text()) );

	    if( checkSyntax &&
	            var.Text()[0] != '#' && !IsKnown( var.Text() ) &&
	            !p4tunable.IsKnown( var.Text() ) )
	    {
		StrBuf errBuf;
		e->Set( MsgSupp::NoSuchVariable ) << var;
		e->Fmt( &errBuf );
		p4debug.printf( "%s", errBuf.Text() );
		e->Clear();
	    }

	    // Set as config'ed

	    EnviroItem *a = GetItem( var.Text() );
	    if( a->type <  ty ) continue;
	    if( a->type == ty && a->origin.Length() ) continue;

	    StrRef cnfdir( "$configdir" );
	    if( configFile.Length() && line.Contains( cnfdir ) )
	    {
		// if value is $configdir then use real config dir
		PathSys *p = PathSys::Create();

		p->Set( configFile );
		p->ToParent();

		StrBuf t;

		StrOps::Replace( t, StrRef( equals+1 ), cnfdir, *p );

		a->value.Set( t );

		delete p;
	    }
	    else
		a->value.Set( equals + 1 );

	    a->type = ty;
	    a->origin.Set( f->Path() );
	    a->checked = 0;
	}
}
#endif
