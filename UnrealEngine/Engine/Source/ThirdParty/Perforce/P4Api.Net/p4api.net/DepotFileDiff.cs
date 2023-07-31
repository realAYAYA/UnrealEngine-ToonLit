/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: DepotFileDiff.cs
 *
 * Author	: wjb
 *
 * Description	: Class used to abstract a depot file diff in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// The types of diffs returned by the server.
	/// </summary>
	[Flags]
	public enum DiffType
	{
		/// <summary>
		/// File contents are different.
		/// </summary>
		Content = 0x0000,
		/// <summary>
		/// File contents are identical but file types are different.
		/// </summary>
		FileType = 0x0001,
		/// <summary>
		/// The left file in the diff has no target file at the
		/// specified name or revision to pair with for a diff.
		/// </summary>
		LeftOnly = 0x0002,
		/// <summary>
		/// The right file in the diff has no source file at the
		/// specified name or revision to pair with for a diff.
		/// </summary>
		RightOnly = 0x0004,
		/// <summary>
		/// File content and file types are identical.
		/// </summary>
		Identical = 0x0008
	}

		/// <remarks>
		/// <br/><b>p4 help diff2</b>
		/// <br/> 
		/// <br/>     diff2 -- Compare one set of depot files to another
		/// <br/> 
		/// <br/>     p4 diff2 [options] fromFile[rev] toFile[rev]
		/// <br/>     p4 diff2 [options] -b branch [[fromFile[rev]] toFile[rev]]
		/// <br/>     p4 diff2 [options] -S stream [-P parent] [[fromFile[rev]] toFile[rev]]
		/// <br/> 
		/// <br/>     	options: -d&lt;flags&gt; -Od -q -t -u
		/// <br/> 
		/// <br/> 	'p4 diff2' runs on the server to compare one set of depot files (the
		/// <br/> 	'source') to another (the 'target').  Source and target file sets
		/// <br/> 	can be specified on the 'p4 diff2' command line or through a branch
		/// <br/> 	view.
		/// <br/> 
		/// <br/> 	With a branch view, fromFile and toFile are optional; fromFile limits
		/// <br/> 	the scope of the source file set, and toFile limits the scope of the
		/// <br/> 	target. If only one file argument is given, it is assumed to be
		/// <br/> 	toFile.
		/// <br/> 
		/// <br/> 	fromFile and toFile can include revision specifiers; by default, the
		/// <br/> 	head revisions are diffed.  See 'p4 help revisions' for details
		/// <br/> 	about specifying file revisions.
		/// <br/> 
		/// <br/> 	'p4 diff2' precedes each diffed file pair with a header line of the
		/// <br/> 	following form:
		/// <br/> 
		/// <br/> 	    ==== source#rev (type) - target#rev (type) ==== summary
		/// <br/> 
		/// <br/> 	A source or target file shown as '&lt;none&gt;' means there is no file
		/// <br/> 	at the specified name or revision to pair with its counterpart.
		/// <br/> 	The summary status is one of the following: 'identical' means file
		/// <br/> 	contents and types are identical, 'types' means file contents are
		/// <br/> 	identical but the types are different, and 'content' means file
		/// <br/> 	contents are different.
		/// <br/> 
		/// <br/> 	The -b flag makes 'p4 diff2' use a user-defined branch view.  (See
		/// <br/> 	'p4 help branch'.) The left side of the branch view is the source
		/// <br/> 	and the right side is the target.
		/// <br/> 
		/// <br/> 	The -S flag makes 'p4 diff2' use a generated branch view that maps
		/// <br/> 	a stream (or its underlying real stream) to its parent.  -P can be
		/// <br/> 	used to generate the branch view using a parent stream other than
		/// <br/> 	the stream's actual parent.
		/// <br/> 
		/// <br/> 	The -d&lt;flags&gt; modify the output of diffs as follows:
		/// <br/> 
		/// <br/> 		-dn (RCS)
		/// <br/> 		-dc[n] (context)
		/// <br/> 		-ds (summary)
		/// <br/> 		-du[n] (unified)
		/// <br/> 		-db (ignore whitespace changes)
		/// <br/> 		-dw (ignore whitespace)
		/// <br/> 		-dl (ignore line endings).
		/// <br/> 
		/// <br/> 	The optional argument to -dc/-du specifies number of context lines.
		/// <br/> 
		/// <br/> 	The -Od flag limits output to files that differ.
		/// <br/> 
		/// <br/> 	The -q omits files that have identical content and types and
		/// <br/> 	suppresses the actual diff for all files.
		/// <br/> 
		/// <br/> 	The -t flag forces 'p4 diff2' to diff binary files.
		/// <br/> 
		/// <br/> 	The -u flag uses the GNU diff -u format and displays only files
		/// <br/> 	that differ. The file names and dates are in Perforce syntax, but
		/// <br/> 	the output can be used by the patch program.
		/// <br/> 
		/// <br/> 
		/// </remarks>
	/// <summary>
	/// A diff between depot files in a Perforce repository. 
	/// </summary>
	public class DepotFileDiff
	{
		public DepotFileDiff()
		{
		}
		public DepotFileDiff(DiffType type,
								  FileSpec leftfile,
								  FileSpec rightfile,
								  string diff
								  )
		{
			Type = type;
			LeftFile = leftfile;
			RightFile = rightfile;
			Diff = diff;
		}

		private StringEnum<DiffType> _type;
		public DiffType Type
		{
			get { return _type; }
			set { _type = value; }
		}
		public FileSpec LeftFile { get; set; }
		public FileSpec RightFile { get; set; }
		public string Diff { get; set; }


        /// <summary>
        /// Read the fields from the tagged output of a diff2 command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'diff2' command</param>
		/// <param name="connection"></param>
        /// <param name="options"></param>
        public void FromGetDepotFileDiffsCmdTaggedOutput(TaggedObject objectInfo, Connection connection, Options options)
		{
			DepotFileDiff ParsedDepotFileDiff = new DepotFileDiff();
			string ldepotfile = string.Empty;
			string rdepotfile = string.Empty;
			int lrev = -1;
			int rrev = -1;

			if (objectInfo.ContainsKey("status"))
			{ _type = objectInfo["status"]; }

			if (objectInfo.ContainsKey("depotFile"))
			{ ldepotfile = objectInfo["depotFile"]; }

			if (objectInfo.ContainsKey("rev"))
			{
				int.TryParse(objectInfo["rev"], out lrev);
			}

			if (objectInfo.ContainsKey("depotFile2"))
			{ rdepotfile = objectInfo["depotFile2"]; }

			if (objectInfo.ContainsKey("rev2"))
			{
				int.TryParse(objectInfo["rev2"], out rrev);
			}

			LeftFile = new FileSpec(new DepotPath(ldepotfile), new Revision(lrev));

			RightFile = new FileSpec(new DepotPath(rdepotfile), new Revision(rrev));

			if (objectInfo["status"] == "content")
			{
				string filepathl = ldepotfile + "#" + lrev;
				string filepathr = rdepotfile + "#" + rrev;
				P4.P4Command getDiffData = new P4Command(connection, "diff2", false, filepathl, filepathr);
			
				P4.P4CommandResult r = getDiffData.Run(options);
				if (r.Success != true)
				{
					P4Exception.Throw(r.ErrorList);
				}
				if (r.TextOutput != null)
				{
					Diff = r.TextOutput.ToString();
				}
				else if (r.InfoOutput != null)
				{
					Diff = r.InfoOutput.ToString();
				}
			}

			ParsedDepotFileDiff = new DepotFileDiff(Type, LeftFile, RightFile, Diff);

		}
	}

}
