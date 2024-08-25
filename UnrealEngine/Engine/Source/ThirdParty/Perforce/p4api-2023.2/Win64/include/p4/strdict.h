/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * StrDict.h - a set/get dictionary interface
 *
 * Classes:
 *
 *	StrDict - a GetVar/PutVar dictionary interface
 *
 * Methods:
 *
 */

class Error;

class StrVarName : public StrRef {

    public:
		StrVarName( const char *buf, p4size_t length )
		{
	            if( length >= sizeof( varName ) )
	                length = sizeof( varName ) - 1;
		    memcpy( varName, buf, length );
		    varName[ length ] = 0;
		    Set( varName, length );
		}

		StrVarName( const StrPtr &name, int x );
		StrVarName( const StrPtr &name, int x, int y );

    private:
		char varName[64];
} ;

class StrDictIterator {
    public:
	virtual ~StrDictIterator(){};
	virtual int	Get( StrRef &var, StrRef &val ) = 0;
	virtual void	Next() = 0;
	virtual void	Reset() = 0;
} ;

class StrDict {

    public:

		StrDict() : iterator( 0 ) {}
	virtual	~StrDict();

	// Handy wrappers

	void	CopyVars( StrDict &other );

	void	SetVar( const char *var );
	void	SetVar( const char *var, int value );
# ifdef HAVE_INT64
	void	SetVar( const char *var, long value );
	void	SetVar( const char *var, P4INT64 value );
# endif
	void	SetVar( const char *var, const char *value );
	void	SetVar( const char *var, const StrPtr *value );
	void	SetVar( const char *var, const StrPtr &value );
	void	SetVar( const StrPtr &var, const StrPtr &value )
		{ VSetVar( var, value ); }

	void	SetVarV( const char *arg );
	void	SetArgv( int argc, char *const *argv );
	void	SetVar( const StrPtr &var, int x, const StrPtr &val );
	void	SetVar( const char *var, int x, const StrPtr &val );
	void	SetVar( const char *var, int x, int y, const StrPtr &val );

	StrPtr *GetVar( const char *var );
	StrPtr *GetVar( const char *var, Error *e );
	StrPtr *GetVar( const StrPtr &var, int x );
	StrPtr *GetVar( const StrPtr &var, int x, int y );
	StrPtr *GetVar( const StrPtr &var )
		{ return VGetVar( var ); }

	int	GetVar( int x, StrRef &var, StrRef &val )
		{ return VGetVarX( x, var, val ); }
	
	int	GetVarCCompare( const char *var, StrBuf &val );
	int	GetVarCCompare( const StrPtr &var, StrBuf &val );
	int	GetCount()
		{ return VGetCount(); };

	void	ReplaceVar( const char *var, const char *value );
	void	ReplaceVar( const StrPtr &var, const StrPtr &value );
	void	RemoveVar( const char *var );
	void	RemoveVar( const StrPtr &var ) { VRemoveVar( var ); }

	void	Clear()		// useful for clearing underlying storage,
		{ VClear(); }	//   e.g. StrBuf::Clear()
	void	Reset()		// useful for freeing underlying storage,
		{ VReset(); }	//   e.g. StrBuf::Reset()
	
	int 	Save( FILE * out );
	int 	Load( FILE * out );

	virtual StrDictIterator *GetIterator();

    protected:

	// Get/Set vars, provided by subclass

	virtual StrPtr *VGetVar( const StrPtr &var ) = 0;
	virtual void	VSetVar( const StrPtr &var, const StrPtr &val );
	virtual void	VRemoveVar( const StrPtr &var );
	virtual int	VGetVarX( int x, StrRef &var, StrRef &val );
	virtual void	VSetError( const StrPtr &var, Error *e );
	virtual void	VClear();
	virtual void	VReset();
	virtual int	VGetCount() = 0;

	StrDictIterator *iterator;
} ;

class StrDictBasicIterator : public StrDictIterator {
    public:
	StrDictBasicIterator( StrDict *dict ) { i = 0; this->dict = dict; }

	virtual int Get(StrRef &var, StrRef &val) {
	    return dict->GetVar( i, var, val ); }

	virtual void Next() { i++; }

	virtual void Reset() { i = 0; }

    private:
	int	i;
	StrDict	*dict;
} ;

