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
 * Name		: FileDiff.cs
 *
 * Author	: wjb
 *
 * Description	: Class used to abstract a file diff.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
    /// <remarks>
    /// <br/><b>p4 help diff</b>
    /// <br/> 
    /// <br/> 	diff -- Diff utility for comparing workspace content to depot content.
    /// <br/> 	(For comparing two depot paths, see p4 diff2.)
    /// <br/>
    /// <br/> 	p4 diff[-d &lt;flags&gt; -f - m max - Od - s &lt;flag&gt; -t] [file[rev] ...]
    /// <br/>
    /// <br/> 	On the client machine, diff a client file against the corresponding
    /// <br/>
    /// <br/> 	revision in the depot.The file is compared only if the file is
    /// <br/> 	opened for edit or a revision is provided.See 'p4 help revisions'
    /// <br/> 	for details about specifying revisions.
    /// <br/>
    /// <br/> 	If the file specification is omitted, all open files are diffed.
    /// <br/> 	This option can be used to view pending changelists.
    /// <br/>
    /// <br/> 	The -d&lt;flags&gt; modify the output as follows:
    /// <br/> 	    -dn (RCS),
    /// <br/> 	    -dc[n] (context),
    /// <br/> 	    -ds (summary),
    /// <br/> 	    -du[n] (unified),
    /// <br/> 	    -db (ignore whitespace changes),
    /// <br/>       -dw(ignore whitespace),
    /// <br/>       -dl(ignore line endings).
    /// <br/>   The optional argument to -dc/-du specifies number of context lines.
    /// <br/>
    /// <br/>   The -f flag diffs every file, regardless of whether they are opened
    /// <br/>   or the client has synced the specified revision.  This option can be
    /// <br/>   used to verify the contents of the client workspace.
    /// <br/>
    /// <br/>   The -m max flag limits output to the first 'max' number of files,
    /// <br/>   unless the -s flag is used, in which case it is ignored.
    /// <br/>
    /// <br/>   The -Od flag limits output to files that differ.
    /// <br/>
    /// <br/>   The -s options lists the files that satisfy the following criteria:
    /// <br/>
    /// <br/>       -sa Opened files that differ from the revision
    /// <br/>                in the depot or are missing.
    /// <br/>
    /// <br/>       -sb Files that have been opened for integrate, resolved,
    /// <br/>           and subsequently modified.
    /// <br/>
    /// <br/>       -sd Unopened files that are missing on the client.
    /// <br/>
    /// <br/>       -se Unopened files that differ from the revision
    /// <br/>                in the depot.
    /// <br/>
    /// <br/>       -sl Every unopened file, along with the status of
    /// <br/>           'same, 'diff', or 'missing' as compared to the
    /// <br/>           corresponding revision in the depot.
    /// <br/>
    /// <br/>       -sr Opened files that do not differ from the revision in
    /// <br/>           the depot.
    /// <br/>
    /// <br/>   Note that if a revision is provided in the file specification, the -s
    /// <br/>   options compare the file(s) regardless of whether they are opened
    /// <br/>   or the client has synced the specified revision.
    /// <br/>
    /// <br/>   The -t flag forces 'p4 diff' to diff binary files.
    /// <br/>
    /// <br/>   If the environment variable $P4DIFF is set, the specified diff
    /// <br/>   program is launched in place of the default Perforce client diff.
    /// <br/>   The -d&lt;flags&gt; option can be used to pass arguments to the diff
    /// <br/>   program.  Because the -s flag is only implemented internally, any
    /// <br/>   -d&lt;flags&gt; option used with the -s&lt;flag&gt; is ignored.To configure a
    /// <br/>   diff program for Unicode files, set the environment variable
    /// <br/>   $P4DIFFUNICODE.Specify the file's character set as the first
    /// <br/>   argument to the program.
    /// <br/>
    /// <br/>
    /// <br/>   See 'p4 help-graph diff' for information on using this command with
    /// <br/>   graph depots.
    /// <br/> 
    /// <br/> 
    /// </remarks>
    /// <summary>
    /// A diff between workspace content and depot content. 
    /// </summary>
    public class FileDiff
	{
		public FileDiff()
		{
		}
		public FileDiff(FileType type,
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

        public FileType Type;

		public FileSpec LeftFile { get; set; }
		public FileSpec RightFile { get; set; }
		public string Diff { get; set; }


        /// <summary>
        /// Read the fields from the tagged output of a diff command
        /// </summary>
        /// <param name="objectInfo">Tagged output from the 'diff' command</param>
		/// <param name="connection"></param>
        /// <param name="options"></param>
        public void FromGetFileDiffsCmdTaggedOutput(TaggedObject objectInfo, Connection connection, Options options)
        {
            FileDiff ParsedFileDiff = new FileDiff();
            string depotfile = string.Empty;
            string clienttfile = string.Empty;
            int rev = -1;
            string type = string.Empty;

            if (objectInfo.ContainsKey("depotFile"))
            { depotfile = objectInfo["depotFile"]; }

            if (objectInfo.ContainsKey("clientFile"))
            { clienttfile = objectInfo["clientFile"]; }

            if (objectInfo.ContainsKey("rev"))
            {
                int.TryParse(objectInfo["rev"], out rev);
            }

            if (objectInfo.ContainsKey("type"))
            { Type = new FileType(objectInfo["type"]); }

            LeftFile = new FileSpec(new DepotPath(depotfile), new Revision(rev));

            RightFile = new FileSpec(new DepotPath(clienttfile), null);

            Diff = connection.LastResults.TextOutput;

            ParsedFileDiff = new FileDiff(Type, LeftFile, RightFile, Diff);
		}
	}

}
