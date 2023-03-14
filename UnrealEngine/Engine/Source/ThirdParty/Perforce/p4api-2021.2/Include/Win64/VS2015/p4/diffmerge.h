/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of the Library RCS.  See rcstest.c.
 */

/*
 * diffmerge.h - 3 way file merge
 *
 * Classes defined:
 *
 *	DiffMerge - control block for merging
 *
 * Public methods:
 *
 *	DiffMerge::DiffMerge() - Merge 3 files to produce integrated result
 *	DiffMerge::~DiffMerge() - dispose of DiffMerge and its contents
 *	DiffMerge::Read() - produce next part of integrated result
 *
 * History:
 *	2-18-97 (seiwald) - translated to C++.
 */

/*
 * SELBITS - the return value from DiffMergeRead
 *
 * 0 means no more output; otherwise the bits are set according to what
 * outputfile is to take the next piece.  The length of DiffMergeRead
 * can be zero while the bits returned are non-zero: this indicates a
 * zero length chunk to be placed in the output file.
 *
 * SEL_CONF indicates a conflict, and is set for each of the legs that
 * are in conflict, including the base.  Thus for a conflict the follow
 * sequence will be seen: 
 *
 *		SEL_CONF | SEL_BASE
 *		SEL_CONF | SEL_LEG1 | SEL_RSLT
 *		SEL_CONF | SEL_LEG2 | SEL_RSLT
 *
 * If changes are identical both lines, they are not in conflict.  The
 * sequence is:
 *
 *		SEL_BASE
 *		SEL_LEG1 | SEL_LEG2 | SEL_RSLT
 *
 * SEL_ALL indicates chunks synchronized between all 3 files.  The
 * actual text comes from LEG2, so that if the underlying diff is
 * ignoring certain changes (like whitespace), the resulting merge
 * will have the last leg (typically "yours") rather than the original
 * unchanged base.
 */

# define SEL_BASE 0x01
# define SEL_LEG1 0x02
# define SEL_LEG2 0x04
# define SEL_RSLT 0x08
# define SEL_ALL (SEL_BASE|SEL_LEG1|SEL_LEG2|SEL_RSLT)
# define SEL_CONF 0x10

class DiffAnalyze;
class DiffDFile;
class DiffFfile;
class DiffFlags;

enum DiffDiffs { 
	DD_EOF,		// End of df1/df2
	DD_LEG1,	// df1 up next
	DD_LEG2,	// df2 up next
	DD_BOTH,	// df1, df2 overlap
	DD_CONF,	// df1, df2 conflict
	DD_ALL,		// all lines
	DD_LAST
} ;

enum GridTypes {
	GRT_OPTIMAL,	// Optimal grid type
	GRT_GUARDED,	// Guarded grid type
	GRT_TWOWAY 	// Two way grid type
} ;

typedef offL_t LineLen;

class DiffMerge {

    public:
			DiffMerge( FileSys *base, FileSys *leg1, FileSys *leg2, 
			    const DiffFlags &fl, LineType lineType, Error *e );

			~DiffMerge();

	int 		Read( char *buf, int len, int *outlen );

	const char	*BitNames( int bits );

	LineLen  	MaxLineLength() const;

    private:

	DiffDiffs	DiffDiff();

	/* State machine for merging. */

	DiffDiffs 	diffDiff;
	GridTypes	gridType;
	int 		state;
	int		oldMode;
	int		diff3behavior;

	/* Base->Yours diff, Base->Theirs diff. */

	DiffDFile 	*df1;
	DiffDFile	*df2;
	DiffDFile	*df3;

	/* Base, yours, theirs. */

	DiffFfile	*bf;
	DiffFfile	*lf1;
	DiffFfile	*lf2;

	/* Empty base */

	FileSys		*emptyFile;

	/* 
	 * For returning data from Read():
	 * 	what leg we're reading from,
	 * 	what selbits we're returning
	 */

	DiffFfile	*readFile;
	int 		selbits;
} ;

