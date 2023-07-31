/*
 * Copyright 1995, 2003 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# define DEBUG_MATCH	( p4debug.GetLevel( DT_MAP ) >= 3 )
# define DEBUG_LIST	( p4debug.GetLevel( DT_MAP ) >= 4 )
# define DEBUG_BUILD	( p4debug.GetLevel( DT_MAP ) >= 5 )


class IgnoreTable;
class IgnoreItem;
class IgnorePtrArray;
class IgnoreArray;
class StrArray;
class FileSys;

class Ignore {

    public:
			Ignore();
			~Ignore();

	int		Reject( const StrPtr &path, const StrPtr &ignoreName )
			{ return Reject( path, ignoreName, (char *)NULL ); }

	int		List( const StrPtr &path, const StrPtr &ignoreName,
			      const char *configName, StrArray *outList );
	int		Reject( const StrPtr &path, const StrPtr &ignName,
			        const char *configName, StrBuf *line = 0 );
	int		RejectDir( const StrPtr &path, const StrPtr &ignName,
			           const char *configName, StrBuf *line = 0 );
	int		RejectCheck( const StrPtr &path, int isDir,
			             StrBuf *line = 0 );

	int		GetIgnoreFiles( const StrPtr &ignoreName, int absolute,
			                int relative, StrArray &ignoreFiles );

    private:
	void		BuildIgnoreFiles( const StrPtr &ignoreName );
	int		Build( const StrPtr &path, const StrPtr &ignoreName,
			        const char *configName );
	void		InsertDefaults( IgnorePtrArray *newList );
	void		Insert( StrArray *subList, const char *ignore, 
			        const char *cwd, int lineno );

	int		ParseFile( FileSys *f, const char *cwd,
			           IgnoreArray *list );
	
	IgnoreTable	*ignoreTable;
	IgnorePtrArray	*ignoreList;
	IgnoreArray	*defaultList;
	StrBuf		dirDepth;
	StrBuf		foundDepth;
	StrBuf		configName;

	StrArray	*ignoreFiles;
	StrBuf		ignoreStr;
	int		relatives;
};
