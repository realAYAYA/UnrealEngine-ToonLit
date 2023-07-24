/*
 * Copyright 1997 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 *
 * Diff code written by James Strickland, May 1997.
 */

/*
 * Diff walkers: produce output according to diff style by walking
 * the diff results.
 *
 * Classes defined:
 *
 *	Diff - compare two files, outputting the differences
 * 	DiffFlags - parse diff's -d<flags>
 *
 *	Diff::Diff - set up for diff output
 *	Diff::DiffWithFlags - produces diff output according to flags
 *	Diff::DiffContext - produces diff -c output to a file
 *	Diff::DiffUnified - produces diff -u output to a file
 *	Diff::DiffNorm - produces normal diff output to a file
 *	Diff::DiffRcs - produces diff -n output to a file
 *	Diff::DiffHTML - produces html markup
 *	Diff::DiffSummary - produces a single summary line
 *
 *	Diff::CloseOutput - finish write and collect error status
 */

class DiffAnalyze;
class DiffFlags;
class Error;
class FileSys;
class Sequence;
class StrPtr;
typedef signed int LineNo;

class Diff {

    public:

			Diff();
			~Diff();

	void		SetInput( FileSys *fx, FileSys *fy, 
				const DiffFlags &flags, Error *e );
	void		SetOutput( const char *fout, Error *e );
	void		SetOutput( FILE *fout );
	void		CloseOutput( Error *e );

	void		DiffWithFlags( const DiffFlags &flags );

	void		DiffContext( int c = 0 );
	void		DiffUnified( int c = 0 );
        void            DiffUnifiedDeleteFile( FileSys *f, Error *e );
	void		DiffNorm();
	void		DiffRcs();
	void		DiffHTML();
	void		DiffSummary();

	void		DiffFast() { fastMaxD = 1; }

	int		GetChunkCnt() { return (chunkCnt); }

    private:

	void		Walker( const char *flags, Sequence *s, 
				LineNo sx, LineNo sy );

	Sequence	*spx;
	Sequence	*spy;
	FILE		*out;
	DiffAnalyze	*diff;
        const DiffFlags *flags;
	int		closeOut;
	LineType	lineType;
	const char	*newLines;
	int		fastMaxD;
	int		chunkCnt;

} ;

class DiffFlags {

    public:
			DiffFlags() { Init( "" ); }
			DiffFlags( const char *flags ) { Init( flags ); }
			DiffFlags( const StrPtr *flags ) { Init( flags ); }

	void		Init( const char *flags );
	void		Init( const StrPtr *flags );

	enum Type     { Normal, Context, Unified, Rcs, HTML, Summary } type;
	enum Sequence { Line, Word, DashL, DashB, DashW, WClass } sequence;
	enum Grid     { Optimal, Guarded, TwoWay, Diff3, GuardedDiff3 } grid;

	int		contextCount;
} ;
