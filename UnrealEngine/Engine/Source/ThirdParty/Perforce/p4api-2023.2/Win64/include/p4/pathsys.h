/*
 * Copyright 1995, 2003 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * PathSys.h - OS specific pathnames
 *
 * Public classes:
 *
 *	PathSys - a StrBuf with virtual path manipulations
 *
 * Public methods:
 *
 *	StrBuf::Set() - set value in local syntax
 *	StrBuf::Text() - get value in local syntax
 *
 *	PathSys::SetCanon() - combine (local) root and canonical path
 *	PathSys::SetLocal() - combine (local) root and local path
 *
 *		If root is empty, local is used.
 *		If local is empty, results are not defined.
 *		If canonical path is empty, trailing slash is appended to path.
 *		(Trailing slash might cause problems with Stat() on windows.)
 *		Local can begin with relative references.
 *
 *	PathSys::GetCanon() - strip root and return rest as canon
 *	PathSys::ToParent() - strip (and return) last element of path
 *
 * NB: SetLocal() can take "this" as root, but SetCanon() cannot.
 *
 * Static functions:
 *
 *	Create() - returns an appropriate PathSys, given an OS type flag.
 *	GetOS() - returns a string for the OS name
 */

# ifdef HAS_CPP11

# include <memory>

class PathSys;
using PathSysUPtr = std::unique_ptr< PathSys >;

# endif



class PathSys : public StrBuf {

    public:
    	virtual		~PathSys();

	virtual void	SetCanon( const StrPtr &root, const StrPtr &canon ) = 0;
	virtual void	SetLocal( const StrPtr &root, const StrPtr &local ) = 0;

	virtual int	GetCanon( const StrPtr &root, StrBuf &t ) = 0;
	virtual int	ToParent( StrBuf *file = 0 ) = 0;
	virtual int	IsUnderRoot( const StrPtr &root ) = 0;
	virtual void	SetCharSet( int = 0 );

	void		Expand();

	static PathSys *Create();
	static PathSys *Create( const StrPtr &os, Error *e );
# ifdef HAS_CPP11
	static PathSysUPtr CreateUPtr();
	static PathSysUPtr CreateUPtr( const StrPtr &os, Error *e );
# endif
	static const char *GetOS();

    private:
	static PathSys *Create( int os );
} ;

