using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{

    /// <summary>
    /// Represents a Perforce server and connection.
    /// </summary>
    public partial class Repository : IDisposable
    {
        private bool multithreaded;

        /// <summary>
        /// Create a repository on the specified server.
        /// </summary>
        /// <param name="server">The repository server.</param>
        /// <param name="_multithreaded">Use a multithreaded connection</param>
        public Repository(Server server, bool _multithreaded = true)
        {
            Server = server;
            multithreaded = _multithreaded;
        }
        /// <summary>
        /// Represents a specific Perforce server. 
        /// </summary>
		public Server Server { get; private set; }

        private Connection _connection;
        /// <summary>
        /// Represents the logical connection between a specific Perforce Server
        /// instance and a specific client application. 
        /// </summary>
        public Connection Connection
        {
            get
            {
                if (_connection == null)
                {
                    _connection = new Connection(Server, multithreaded);
                }
                return _connection;
            }
        }
        /// <summary>
        /// Return a list of FileSpecs of files in the depot that correspond
        /// to the passed-in FileSpecs. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help files</b>
        /// <br/> 
        /// <br/>     files -- List files in the depot
        /// <br/> 
        /// <br/>     p4 files [ -a ] [ -A ] [ -e ] [ -m max ] file[revRange] ...
        /// <br/>     p4 files -U unloadfile ...
        /// <br/> 
        /// <br/> 	List details about specified files: depot file name, revision,
        /// <br/> 	file, type, change action and changelist number of the current
        /// <br/> 	head revision. If client syntax is used to specify the file
        /// <br/> 	argument, the client view mapping is used to determine the
        /// <br/> 	corresponding depot files.
        /// <br/> 
        /// <br/> 	By default, the head revision is listed.  If the file argument
        /// <br/> 	specifies a revision, then all files at that revision are listed.
        /// <br/> 	If the file argument specifies a revision range, the highest revision
        /// <br/> 	in the range is used for each file. For details about specifying
        /// <br/> 	revisions, see 'p4 help revisions'.
        /// <br/> 
        /// <br/> 	The -a flag displays all revisions within the specific range, rather
        /// <br/> 	than just the highest revision in the range.
        /// <br/> 
        /// <br/> 	The -A flag displays files in archive depots.
        /// <br/> 
        /// <br/> 	The -e flag displays files with an action of anything other than
        /// <br/> 	deleted, purged or archived.  Typically this revision is always
        /// <br/> 	available to sync or integrate from.
        /// <br/> 
        /// <br/> 	The -m flag limits files to the first 'max' number of files.
        /// <br/> 
        /// <br/> 	The -U option displays files in the unload depot (see 'p4 help unload'
        /// <br/> 	for more information about the unload depot).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get a maximum of 10 files from the repository:
        ///		<code> 
        ///			
        ///			GetDepotFilesCmdOptions opts =
        ///			new GetDepotFilesCmdOptions(GetDepotFilesCmdFlags.None, 10);
        ///			FileSpec fs = new FileSpec(new DepotPath("//depot/..."), null);
        ///			List&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///			IList&lt;FileSpec&gt; files = Repository.GetDepotFiles(lfs, opts);
        ///			
        ///		</code>
        /// </example>
        /// <seealso cref="GetDepotFilesCmdFlags"/> 
        public IList<FileSpec> GetDepotFiles(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command filesCmd = new P4Command(this, "files", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = filesCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<FileSpec> value = new List<FileSpec>();
            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                string path = obj["depotFile"];
                PathSpec ps = new DepotPath(path);
                int rev = 0;
                int.TryParse(obj["rev"], out rev);
                FileSpec fs = new FileSpec(ps, new Revision(rev));
                value.Add(fs);

            }
            return value;
        }

        /// <summary>
        /// Return a list of Files opened by users / clients.
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help opened</b>
        /// <br/> 
        /// <br/>     opened -- List open files and display file status
        /// <br/> 
        /// <br/>     p4 opened [-a -c changelist# -C client -u user -m max -s] [file ...]
        /// <br/>     p4 opened [-a -x -m max ] [file ...]
        /// <br/> 
        /// <br/> 	Lists files currently opened in pending changelists, or, for
        /// <br/> 	specified files, show whether they are currently opened or locked.
        /// <br/> 	If the file specification is omitted, all files open in the current
        /// <br/> 	client workspace are listed.
        /// <br/> 
        /// <br/> 	Files in shelved changelists are not displayed by this command. To
        /// <br/> 	display shelved changelists, see 'p4 changes -s shelved'; to display
        /// <br/> 	the files in those shelved changelists, see 'p4 describe -s -S'.
        /// <br/> 
        /// <br/> 	The -a flag lists opened files in all clients.  By default, only
        /// <br/> 	files opened by the current client are listed.
        /// <br/> 
        /// <br/> 	The -c changelist# flag lists files opened in the specified
        /// <br/> 	changelist#.
        /// <br/> 
        /// <br/> 	The -C client flag lists files open in the specified client workspace.
        /// <br/> 
        /// <br/> 	The -u user flag lists files opened by the specified user.
        /// <br/> 
        /// <br/> 	The -m max flag limits output to the first 'max' number of files.
        /// <br/> 
        /// <br/> 	The -s option produces 'short' and optimized output when used with
        /// <br/> 	the -a (all clients) option.  For large repositories '-a' can take
        /// <br/> 	a long time when compared to '-as'.
        /// <br/> 
        /// <br/> 	The -x option lists files that are opened 'exclusive'. This option
        /// <br/> 	only applies to a distributed installation where global tracking of
        /// <br/> 	these file types is necessary across servers. The -x option implies
        /// <br/> 	the -a option.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get a maximum of 10 opened files from the repository, opened by
        ///		user fred, opened with any client:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///
        ///			List&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			// null for changelist and client options
        ///			GetOpenedFilesOptions opts =
        ///			new GetOpenedFilesOptions(GetOpenedFilesCmdFlags.AllClients,
        ///			null, null, "fred", 10);
        ///                            
        ///			IList&lt;File&gt; target = Repository.GetOpenedFiles(lfs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetOpenedFilesCmdFlags"/> 
        public IList<File> GetOpenedFiles(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command openedCmd = new P4Command(this, "opened", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = openedCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<File> value = new List<File>();

            DepotPath dps = null;
            ClientPath cps = null;
            int revision = 0;
            Revision rev = new Revision(0);
            Revision haveRev = new Revision(0);
            StringEnum<FileAction> action = null;
            int change = -1;
            FileType type = null;
            DateTime submittime = DateTime.MinValue;
            string user = string.Empty;
            string client = string.Empty;


            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                if (obj.ContainsKey("depotFile"))
                {
                    dps = new DepotPath(obj["depotFile"]);
                }

                if (obj.ContainsKey("clientFile"))
                {
                    cps = new ClientPath(obj["clientFile"]);
                }

                if (obj.ContainsKey("rev"))
                {
                    int.TryParse(obj["rev"], out revision);
                    rev = new Revision(revision);
                }

                if (obj.ContainsKey("haveRev"))
                {
                    int.TryParse(obj["haveRev"], out revision);
                    haveRev = new Revision(revision);
                }

                if (obj.ContainsKey("action"))
                {
                    action = obj["action"];
                    // if the file is marked for add, some
                    // commands will return a tagged field
                    // of rev that is 1. This is not accurate
                    // and can cause other commands that use
                    // File to incorrectly run the command
                    // on a file #1, which can result in
                    // "no such file(s)." So, change rev to
                    // null for files marked for add.
                    if (action == "add")
                    {
                        rev = null;
                    }
                }

                if (obj.ContainsKey("change"))
                {
                    int.TryParse(obj["change"], out change);
                }

                if (obj.ContainsKey("type"))
                {
                    type = new FileType(obj["type"]);
                }

                if (obj.ContainsKey("user"))
                {
                    user = obj["user"];
                }

                if (obj.ContainsKey("client"))
                {
                    client = obj["client"];
                }

                File f = new File(dps, cps, rev, haveRev, change, action, type, submittime, user, client);
                value.Add(f);
            }
            return value;
        }

        /// <summary>
        /// Use the p4 fstat command to get the file metadata for the files
        /// matching the FileSpec. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help fstat</b>
        /// <br/> 
        /// <br/>     fstat -- Dump file info
        /// <br/> 
        /// <br/>     p4 fstat [-F filter -L -T fields -m max -r] [-c | -e changelist#]
        /// <br/> 	[-Ox -Rx -Sx] [-A pattern] [-U] file[rev] ...
        /// <br/> 
        /// <br/> 	Fstat lists information about files, one line per field.  Fstat is
        /// <br/> 	intended for use in Perforce API applications, where the output can
        /// <br/> 	be accessed as variables, but its output is also suitable for parsing
        /// <br/> 	from the client command output in scripts.
        /// <br/> 
        /// <br/> 	The fields that fstat displays are:
        /// <br/> 
        /// <br/> 		attr-&lt;name&gt;          -- attribute value for &lt;name&gt;
        /// <br/> 		attrProp-&lt;name&gt;      -- set if attribute &lt;name&gt; is propagating
        /// <br/> 		clientFile           -- local path (host or Perforce syntax)
        /// <br/> 		depotFile            -- name in depot
        /// <br/> 		movedFile            -- name in depot of moved to/from file
        /// <br/> 		path                 -- local path (host syntax)
        /// <br/> 		isMapped             -- set if file is mapped in the client
        /// <br/> 		shelved              -- set if file is shelved
        /// <br/> 		headAction           -- action at head rev, if in depot
        /// <br/> 		headChange           -- head rev changelist#, if in depot
        /// <br/> 		headRev              -- head rev #, if in depot
        /// <br/> 		headType             -- head rev type, if in depot
        /// <br/> 		headCharset          -- head charset, for unicode type
        /// <br/> 		headTime             -- head rev changelist time, if in depot
        /// <br/> 		headModTime	     -- head rev mod time, if in depot
        /// <br/> 		movedRev             -- head rev # of moved file
        /// <br/> 		haveRev              -- rev had on client, if on client
        /// <br/> 		desc                 -- change description (if -e specified)
        /// <br/> 		digest               -- MD5 digest (fingerprint)
        /// <br/> 		fileSize             -- file size
        /// <br/> 		action               -- open action, if opened
        /// <br/> 		type                 -- open type, if opened
        /// <br/> 		charset              -- open charset, for unicode type
        /// <br/> 		actionOwner          -- user who opened file, if opened
        /// <br/> 		change               -- open changelist#, if opened
        /// <br/> 		resolved             -- resolved integration records
        /// <br/> 		unresolved           -- unresolved integration records
        /// <br/> 		reresolvable         -- reresolvable integration records
        /// <br/> 		otherOpen            -- set if someone else has it open
        /// <br/> 		otherOpen#           -- list of user@client with file opened
        /// <br/> 		otherLock            -- set if someone else has it locked
        /// <br/> 		otherLock#           -- user@client with file locked
        /// <br/> 		otherAction#         -- open action, if opened by someone else
        /// <br/> 		otherChange#         -- changelist, if opened by someone else
        /// <br/> 		openattr-&lt;name&gt;      -- attribute value for &lt;name&gt;
        /// <br/> 		openattrProp-&lt;name&gt;  -- set if attribute &lt;name&gt; is propagating
        /// <br/> 		ourLock              -- set if this user/client has it locked
        /// <br/> 		resolveAction#       -- pending integration record action
        /// <br/> 		resolveBaseFile#     -- pending integration base file
        /// <br/> 		resolveBaseRev#      -- pending integration base rev
        /// <br/> 		resolveFromFile#     -- pending integration from file
        /// <br/> 		resolveStartFromRev# -- pending integration from start rev
        /// <br/> 		resolveEndFromRev#   -- pending integration from end rev
        /// <br/> 		totalFileCount       -- total no. of files, if sorted
        /// <br/> 
        /// <br/> 	The -A &lt;pattern&gt; flag restricts displayed attributes to those that
        /// <br/> 	match 'pattern'.
        /// <br/> 
        /// <br/> 	The -F flag lists only files satisfying the filter expression. This
        /// <br/> 	filter syntax is similar to the one used for 'jobs -e jobview' and is
        /// <br/> 	used to evaluate the contents of the fields in the preceding list.
        /// <br/> 	Filtering is case-sensitive.
        /// <br/> 
        /// <br/> 	        Example: -Ol -F "fileSize &gt; 1000000 &amp; headType=text"
        /// <br/> 
        /// <br/> 	Note: filtering is not optimized with indexes for performance.
        /// <br/> 
        /// <br/> 	The -L flag can be used with multiple file arguments that are in
        /// <br/> 	full depot syntax and include a valid revision number. When this
        /// <br/> 	flag is used the arguments are processed together by building an
        /// <br/> 	internal table similar to a label. This file list processing is
        /// <br/> 	significantly faster than having to call the internal query engine
        /// <br/> 	for each individual file argument. However, the file argument syntax
        /// <br/> 	is strict and the command will not run if an error is encountered.
        /// <br/> 
        /// <br/> 	The -T fields flag returns only the specified fields. The field names
        /// <br/> 	can be specified using a comma- or space-delimited list.
        /// <br/> 
        /// <br/> 	        Example: -Ol -T "depotFile, fileSize"
        /// <br/> 
        /// <br/> 	The -m max flag limits output to the specified number of files.
        /// <br/> 
        /// <br/> 	The -r flag sorts the output in reverse order.
        /// <br/> 
        /// <br/> 	The -c changelist# flag displays files modified by the specified
        /// <br/> 	changelist or after that changelist was submitted.  This operation is
        /// <br/> 	much faster than using a revision range on the affected files.
        /// <br/> 
        /// <br/> 	The -e changelist# flag lists files modified by the specified
        /// <br/> 	changelist. When used with the -Ro flag, only pending changes are
        /// <br/> 	considered, to ensure that files opened for add are included. This
        /// <br/> 	option also displays the change description.
        /// <br/> 
        /// <br/> 	The -O options modify the output as follows:
        /// <br/> 
        /// <br/> 	        -Oa     output attributes set by 'p4 attribute'.
        /// <br/> 
        /// <br/> 	        -Od     output the digest of the attribute.
        /// <br/> 
        /// <br/> 	        -Oe     output attribute values encoded as hex
        /// <br/> 
        /// <br/> 	        -Of     output all revisions for the given files (this
        /// <br/> 	                option suppresses other* and resolve* fields)
        /// <br/> 
        /// <br/> 	        -Ol     output a fileSize and digest field for each revision
        /// <br/> 	                (this may be expensive to compute)
        /// <br/> 
        /// <br/> 	        -Op     output the local file path in both Perforce syntax
        /// <br/> 	                (//client/) as 'clientFile' and host form as 'path'
        /// <br/> 
        /// <br/> 	        -Or     output pending integration record information for
        /// <br/> 	                files opened on the current client, or if used with
        /// <br/> 	                '-e &lt;change&gt; -Rs', on the shelved change
        /// <br/> 
        /// <br/> 	        -Os     exclude client-related data from output
        /// <br/> 
        /// <br/> 	The -R option limits output to specific files:
        /// <br/> 
        /// <br/> 	        -Rc     files mapped in the client view
        /// <br/> 	        -Rh     files synced to the client workspace
        /// <br/> 	        -Rn     files opened not at the head revision
        /// <br/> 	        -Ro     files opened
        /// <br/> 	        -Rr     files opened that have been resolved
        /// <br/> 	        -Rs     files shelved (requires -e)
        /// <br/> 	        -Ru     files opened that need resolving
        /// <br/> 
        /// <br/> 	The -S option changes the order of output:
        /// <br/> 
        /// <br/> 	        -St     sort by filetype
        /// <br/> 	        -Sd     sort by date
        /// <br/> 	        -Sr     sort by head revision
        /// <br/> 	        -Sh     sort by have revision
        /// <br/> 	        -Ss     sort by filesize
        /// <br/> 
        /// <br/> 	The -U flag displays information about unload files in the unload
        /// <br/> 	depot (see 'p4 help unload').
        /// <br/> 
        /// <br/> 	For compatibility, the following flags are also supported:
        /// <br/> 	-C (-Rc) -H (-Rh) -W (-Ro) -P (-Op) -l (-Ol) -s (-Os).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get FileMetaData for //depot/ReadMe.txt:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//depot/MyCode/ReadMe.txt"), null);
        ///			
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.None,
        ///                    null, null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(opts, fs);
        ///		</code>
        ///		To get FileMetaData for files in the depot that need resolving:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.NeedsResolve,
        ///                    null, null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(opts, fs);
        ///		</code>
        ///     To get FileMetaData for files in the depot that are over a specific file
        ///     size and of file type, text:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.None,
        ///                    "fileSize &gt; 1000000 &amp; headType=text", null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(opts, fs);
        ///		</code>
        ///     To get FileMetaData for files in the depot that have been modified at or
        ///     after changelist 20345 and are mapped to the client view:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.ClientMapped,
        ///                    null, null, 20345, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(opts, fs);
        ///		</code>
        ///     To get FileMetaData for files in the depot including attributes which match
        ///     the pattern "tested":
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.Attributes,
        ///                    null, null, 0, null, null, "tested");
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(opts, fs);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileMetadataCmdFlags"/>
        public IList<FileMetaData> GetFileMetaData(Options options, params FileSpec[] filespecs)
        {
            string[] paths = FileSpec.ToEscapedStrings(filespecs);
            P4.P4Command fstatCmd = new P4Command(this, "fstat", true, paths);
            P4.P4CommandResult r = fstatCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<FileMetaData> value = new List<FileMetaData>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                if ((obj.Count <= 2) && (obj.ContainsKey("desc")))
                {
                    // hack, but this not really a file, it's just a
                    // the description of the change if -e option is 
                    // specified, so skip it
                    continue;
                }
                FileMetaData fmd = new FileMetaData();
                fmd.FromFstatCmdTaggedData(obj);
                value.Add(fmd);
            }
            return value;
        }

        /// <summary>
        /// Use the p4 fstat command to get the file metadata for the files
        /// matching the FileSpec. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
		/// <br/><b>p4 help fstat</b>
		/// <br/> 
		/// <br/>     fstat -- Dump file info
		/// <br/> 
		/// <br/>     p4 fstat [-F filter -L -T fields -m max -r] [-c | -e changelist#]
		/// <br/> 	[-Ox -Rx -Sx] [-A pattern] [-U] file[rev] ...
		/// <br/> 
		/// <br/> 	Fstat lists information about files, one line per field.  Fstat is
		/// <br/> 	intended for use in Perforce API applications, where the output can
		/// <br/> 	be accessed as variables, but its output is also suitable for parsing
		/// <br/> 	from the client command output in scripts.
		/// <br/> 
		/// <br/> 	The fields that fstat displays are:
		/// <br/> 
		/// <br/> 		attr-&lt;name&gt;          -- attribute value for &lt;name&gt;
		/// <br/> 		attrProp-&lt;name&gt;      -- set if attribute &lt;name&gt; is propagating
		/// <br/> 		clientFile           -- local path (host or Perforce syntax)
		/// <br/> 		depotFile            -- name in depot
		/// <br/> 		movedFile            -- name in depot of moved to/from file
		/// <br/> 		path                 -- local path (host syntax)
		/// <br/> 		isMapped             -- set if file is mapped in the client
		/// <br/> 		shelved              -- set if file is shelved
		/// <br/> 		headAction           -- action at head rev, if in depot
		/// <br/> 		headChange           -- head rev changelist#, if in depot
		/// <br/> 		headRev              -- head rev #, if in depot
		/// <br/> 		headType             -- head rev type, if in depot
		/// <br/> 		headCharset          -- head charset, for unicode type
		/// <br/> 		headTime             -- head rev changelist time, if in depot
		/// <br/> 		headModTime	     -- head rev mod time, if in depot
		/// <br/> 		movedRev             -- head rev # of moved file
		/// <br/> 		haveRev              -- rev had on client, if on client
		/// <br/> 		desc                 -- change description (if -e specified)
		/// <br/> 		digest               -- MD5 digest (fingerprint)
		/// <br/> 		fileSize             -- file size
		/// <br/> 		action               -- open action, if opened
		/// <br/> 		type                 -- open type, if opened
		/// <br/> 		charset              -- open charset, for unicode type
		/// <br/> 		actionOwner          -- user who opened file, if opened
		/// <br/> 		change               -- open changelist#, if opened
		/// <br/> 		resolved             -- resolved integration records
		/// <br/> 		unresolved           -- unresolved integration records
		/// <br/> 		reresolvable         -- reresolvable integration records
		/// <br/> 		otherOpen            -- set if someone else has it open
		/// <br/> 		otherOpen#           -- list of user@client with file opened
		/// <br/> 		otherLock            -- set if someone else has it locked
		/// <br/> 		otherLock#           -- user@client with file locked
		/// <br/> 		otherAction#         -- open action, if opened by someone else
		/// <br/> 		otherChange#         -- changelist, if opened by someone else
		/// <br/> 		openattr-&lt;name&gt;      -- attribute value for &lt;name&gt;
		/// <br/> 		openattrProp-&lt;name&gt;  -- set if attribute &lt;name&gt; is propagating
		/// <br/> 		ourLock              -- set if this user/client has it locked
		/// <br/> 		resolveAction#       -- pending integration record action
		/// <br/> 		resolveBaseFile#     -- pending integration base file
		/// <br/> 		resolveBaseRev#      -- pending integration base rev
		/// <br/> 		resolveFromFile#     -- pending integration from file
		/// <br/> 		resolveStartFromRev# -- pending integration from start rev
		/// <br/> 		resolveEndFromRev#   -- pending integration from end rev
		/// <br/> 		totalFileCount       -- total no. of files, if sorted
		/// <br/> 
		/// <br/> 	The -A &lt;pattern&gt; flag restricts displayed attributes to those that
		/// <br/> 	match 'pattern'.
		/// <br/> 
		/// <br/> 	The -F flag lists only files satisfying the filter expression. This
		/// <br/> 	filter syntax is similar to the one used for 'jobs -e jobview' and is
		/// <br/> 	used to evaluate the contents of the fields in the preceding list.
		/// <br/> 	Filtering is case-sensitive.
		/// <br/> 
		/// <br/> 	        Example: -Ol -F "fileSize &gt; 1000000 &amp; headType=text"
		/// <br/> 
		/// <br/> 	Note: filtering is not optimized with indexes for performance.
		/// <br/> 
		/// <br/> 	The -L flag can be used with multiple file arguments that are in
		/// <br/> 	full depot syntax and include a valid revision number. When this
		/// <br/> 	flag is used the arguments are processed together by building an
		/// <br/> 	internal table similar to a label. This file list processing is
		/// <br/> 	significantly faster than having to call the internal query engine
		/// <br/> 	for each individual file argument. However, the file argument syntax
		/// <br/> 	is strict and the command will not run if an error is encountered.
		/// <br/> 
		/// <br/> 	The -T fields flag returns only the specified fields. The field names
		/// <br/> 	can be specified using a comma- or space-delimited list.
		/// <br/> 
		/// <br/> 	        Example: -Ol -T "depotFile, fileSize"
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the specified number of files.
		/// <br/> 
		/// <br/> 	The -r flag sorts the output in reverse order.
		/// <br/> 
		/// <br/> 	The -c changelist# flag displays files modified by the specified
		/// <br/> 	changelist or after that changelist was submitted.  This operation is
		/// <br/> 	much faster than using a revision range on the affected files.
		/// <br/> 
		/// <br/> 	The -e changelist# flag lists files modified by the specified
		/// <br/> 	changelist. When used with the -Ro flag, only pending changes are
		/// <br/> 	considered, to ensure that files opened for add are included. This
		/// <br/> 	option also displays the change description.
		/// <br/> 
		/// <br/> 	The -O options modify the output as follows:
		/// <br/> 
		/// <br/> 	        -Oa     output attributes set by 'p4 attribute'.
		/// <br/> 
		/// <br/> 	        -Od     output the digest of the attribute.
		/// <br/> 
		/// <br/> 	        -Oe     output attribute values encoded as hex
		/// <br/> 
		/// <br/> 	        -Of     output all revisions for the given files (this
		/// <br/> 	                option suppresses other* and resolve* fields)
		/// <br/> 
		/// <br/> 	        -Ol     output a fileSize and digest field for each revision
		/// <br/> 	                (this may be expensive to compute)
		/// <br/> 
		/// <br/> 	        -Op     output the local file path in both Perforce syntax
		/// <br/> 	                (//client/) as 'clientFile' and host form as 'path'
		/// <br/> 
		/// <br/> 	        -Or     output pending integration record information for
		/// <br/> 	                files opened on the current client, or if used with
		/// <br/> 	                '-e &lt;change&gt; -Rs', on the shelved change
		/// <br/> 
		/// <br/> 	        -Os     exclude client-related data from output
		/// <br/> 
		/// <br/> 	The -R option limits output to specific files:
		/// <br/> 
		/// <br/> 	        -Rc     files mapped in the client view
		/// <br/> 	        -Rh     files synced to the client workspace
		/// <br/> 	        -Rn     files opened not at the head revision
		/// <br/> 	        -Ro     files opened
		/// <br/> 	        -Rr     files opened that have been resolved
		/// <br/> 	        -Rs     files shelved (requires -e)
		/// <br/> 	        -Ru     files opened that need resolving
		/// <br/> 
		/// <br/> 	The -S option changes the order of output:
		/// <br/> 
		/// <br/> 	        -St     sort by filetype
		/// <br/> 	        -Sd     sort by date
		/// <br/> 	        -Sr     sort by head revision
		/// <br/> 	        -Sh     sort by have revision
		/// <br/> 	        -Ss     sort by filesize
		/// <br/> 
		/// <br/> 	The -U flag displays information about unload files in the unload
		/// <br/> 	depot (see 'p4 help unload').
		/// <br/> 
		/// <br/> 	For compatibility, the following flags are also supported:
		/// <br/> 	-C (-Rc) -H (-Rh) -W (-Ro) -P (-Op) -l (-Ol) -s (-Os).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get FileMetaData for //depot/ReadMe.txt:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//depot/MyCode/ReadMe.txt"), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
		///			lfs.Add(fs);
        ///
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.None,
        ///                    null, null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(lfs, opts);
        ///		</code>
        ///		To get FileMetaData for files in the depot that need resolving:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.NeedsResolve,
        ///                    null, null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(lfs, opts);
        ///		</code>
        ///     To get FileMetaData for files in the depot that are over a specific file
        ///     size and of file type, text:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.None,
        ///                    "fileSize &gt; 1000000 &amp; headType=text", null, 0, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(lfs, opts);
        ///		</code>
        ///     To get FileMetaData for files in the depot that have been modified at or
        ///     after changelist 20345 and are mapped to the client view:
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.ClientMapped,
        ///                    null, null, 20345, null, null, null);
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(lfs, opts);
        ///		</code>
        ///     To get FileMetaData for files in the depot including attributes which match
        ///     the pattern "tested":
        ///		<code> 
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			GetFileMetaDataCmdOptions opts =
        ///			new GetFileMetaDataCmdOptions(GetFileMetadataCmdFlags.Attributes,
        ///                    null, null, 0, null, null, "tested");
        ///                            
        ///			IList&lt;FileMetaData&gt; target = Repository.GetFileMetaData(lfs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileMetadataCmdFlags"/>
        public IList<FileMetaData> GetFileMetaData(IList<FileSpec> filespecs, Options options)
        {
            return GetFileMetaData(options, filespecs.ToArray());
        }

        /// <summary>
        /// Return a list of Files in the depot that correspond to the passed-in
        /// FileSpecs. 
        /// </summary>
        /// <remarks>
		/// <br/><b>p4 help files</b>
		/// <br/> 
		/// <br/>     files -- List files in the depot
		/// <br/> 
		/// <br/>     p4 files [ -a ] [ -A ] [ -e ] [ -m max ] file[revRange] ...
		/// <br/>     p4 files -U unloadfile ...
		/// <br/> 
		/// <br/> 	List details about specified files: depot file name, revision,
		/// <br/> 	file, type, change action and changelist number of the current
		/// <br/> 	head revision. If client syntax is used to specify the file
		/// <br/> 	argument, the client view mapping is used to determine the
		/// <br/> 	corresponding depot files.
		/// <br/> 
		/// <br/> 	By default, the head revision is listed.  If the file argument
		/// <br/> 	specifies a revision, then all files at that revision are listed.
		/// <br/> 	If the file argument specifies a revision range, the highest revision
		/// <br/> 	in the range is used for each file. For details about specifying
		/// <br/> 	revisions, see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -a flag displays all revisions within the specific range, rather
		/// <br/> 	than just the highest revision in the range.
		/// <br/> 
		/// <br/> 	The -A flag displays files in archive depots.
		/// <br/> 
		/// <br/> 	The -e flag displays files with an action of anything other than
		/// <br/> 	deleted, purged or archived.  Typically this revision is always
		/// <br/> 	available to sync or integrate from.
		/// <br/> 
		/// <br/> 	The -m flag limits files to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -U option displays files in the unload depot (see 'p4 help unload'
		/// <br/> 	for more information about the unload depot).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get Files in local depot //depot/...:
        ///		<code> 
        ///			GetDepotFilesCmdOptions opts =
        ///			new GetDepotFilesCmdOptions(GetDepotFilesCmdFlags.None, 0);
        ///			
        ///			FileSpec fs = new FileSpec(new DepotPath("//depot/..."), null);
        ///	
        ///			IList&lt;File&gt; target = Repository.GetFiles(opts, fs);
        ///		</code>
        ///		To get Files in unload depot //Unloaded/...:
        ///		<code>
        ///			GetDepotFilesCmdOptions opts =
        ///			new GetDepotFilesCmdOptions(GetDepotFilesCmdFlags.InUnloadDepot, 0);
        ///                            
        ///			FileSpec fs = new FileSpec(new DepotPath("//Unloaded/..."), null);
        ///	
        ///			IList&lt;File&gt; target = Repository.GetFiles(opts, fs);
        ///     </code>
        /// </example>
        /// <seealso cref="GetDepotFilesCmdFlags"/>
        public IList<File> GetFiles(Options options, params FileSpec[] filespecs)
        {
            P4.P4Command fstatCmd = new P4Command(this, "files", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = fstatCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<File> value = new List<File>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                File val = new File();
                val.ParseFilesCmdTaggedData(obj);
                value.Add(val);
            }
            return value;
        }

        /// <summary>
        /// Return a list of Files in the depot that correspond to the passed-in
        /// FileSpecs. 
        /// </summary>
        /// <remarks>
		/// <br/><b>p4 help files</b>
		/// <br/> 
		/// <br/>     files -- List files in the depot
		/// <br/> 
		/// <br/>     p4 files [ -a ] [ -A ] [ -e ] [ -m max ] file[revRange] ...
		/// <br/>     p4 files -U unloadfile ...
		/// <br/> 
		/// <br/> 	List details about specified files: depot file name, revision,
		/// <br/> 	file, type, change action and changelist number of the current
		/// <br/> 	head revision. If client syntax is used to specify the file
		/// <br/> 	argument, the client view mapping is used to determine the
		/// <br/> 	corresponding depot files.
		/// <br/> 
		/// <br/> 	By default, the head revision is listed.  If the file argument
		/// <br/> 	specifies a revision, then all files at that revision are listed.
		/// <br/> 	If the file argument specifies a revision range, the highest revision
		/// <br/> 	in the range is used for each file. For details about specifying
		/// <br/> 	revisions, see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -a flag displays all revisions within the specific range, rather
		/// <br/> 	than just the highest revision in the range.
		/// <br/> 
		/// <br/> 	The -A flag displays files in archive depots.
		/// <br/> 
		/// <br/> 	The -e flag displays files with an action of anything other than
		/// <br/> 	deleted, purged or archived.  Typically this revision is always
		/// <br/> 	available to sync or integrate from.
		/// <br/> 
		/// <br/> 	The -m flag limits files to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -U option displays files in the unload depot (see 'p4 help unload'
		/// <br/> 	for more information about the unload depot).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get Files in local depot //depot/...:
        ///		<code> 
        ///			FileSpec fs = new FileSpec(new DepotPath("//depot/..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///
        ///			GetDepotFilesCmdOptions opts =
        ///			new GetDepotFilesCmdOptions(GetDepotFilesCmdFlags.None, 0);
        ///                            
        ///			IList&lt;File&gt; target = Repository.GetFiles(lfs, opts);
        ///		</code>
        ///		To get Files in unload depot //Unloaded/...:
        ///		<code>
        ///			FileSpec fs = new FileSpec(new DepotPath("//Unloaded/..."), null);
        ///			IList&lt;FileSpec&gt; lfs = new List&lt;FileSpec&gt;();
        ///			lfs.Add(fs);
        ///			
        ///			GetDepotFilesCmdOptions opts =
        ///			new GetDepotFilesCmdOptions(GetDepotFilesCmdFlags.InUnloadDepot, 0);
        ///
        ///			IList&lt;File&gt; target = Repository.GetFiles(lfs, opts);
        ///     </code>
        /// </example>
        /// <seealso cref="GetDepotFilesCmdFlags"/>
		public IList<File> GetFiles(IList<FileSpec> filespecs, Options options)
        {
            return GetFiles(options, filespecs.ToArray());
        }

        /// <summary>
        /// List selected directory paths in the repository. 
        /// </summary>
        /// <param name="dirs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help dirs</b>
        /// <br/> 
        /// <br/>     dirs -- List depot subdirectories
        /// <br/> 
        /// <br/>     p4 dirs [-C -D -H] [-S stream] dir[revRange] ...
        /// <br/> 
        /// <br/> 	List directories that match the specified file pattern (dir).
        /// <br/> 	This command does not support the recursive wildcard (...).
        /// <br/> 	Use the * wildcard instead.
        /// <br/> 
        /// <br/> 	Perforce does not track directories individually. A path is treated
        /// <br/> 	as a directory if there are any undeleted files with that path as a
        /// <br/> 	prefix.
        /// <br/> 
        /// <br/> 	By default, all directories containing files are listed. If the dir
        /// <br/> 	argument includes a revision range, only directories containing files
        /// <br/> 	in the range are listed. For details about specifying file revisions,
        /// <br/> 	see 'p4 help revisions'.
        /// <br/> 
        /// <br/> 	The -C flag lists only directories that fall within the current
        /// <br/> 	client view.
        /// <br/> 
        /// <br/> 	The -D flag includes directories containing only deleted files.
        /// <br/> 
        /// <br/> 	The -H flag lists directories containing files synced to the current
        /// <br/> 	client workspace.
        /// <br/> 
        /// <br/> 	The -S flag limits output to depot directories mapped in a stream's
        /// <br/> 	client view.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get dirs on the server that fall within the current client view:
        ///		<code> 
        ///			GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.CurrentClientOnly, null);
        ///                            
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(opts, "//* );
        ///		</code>
        ///		To get dirs on the server that contain files synced to the current
        ///		client workspace:
        ///		<code>
        ///			GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.SyncedDirs, null);
        ///                            
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(opts, "//* );
        ///     </code>
        ///		To get dirs on the server that fall under the path //depot/main/:
        ///		<code>
        ///		    GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.None, null);
        ///                            
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(opts, "//depot/main/* );
        ///		</code>
        /// </example>
        /// <seealso cref="GetDepotDirsCmdFlags"/>
        public IList<String> GetDepotDirs(Options options, params string[] dirs)
        {

            P4.P4Command dirsCmd = new P4Command(this, "dirs", false, dirs);
            P4.P4CommandResult r = dirsCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            List<String> value = new List<String>();

            foreach (P4.P4ClientInfoMessage l in r.InfoOutput)
            {
                value.Add(l.Message);
            }
            return value;
        }

        /// <summary>
        /// List selected directory paths in the repository. 
        /// </summary>
        /// <param name="dirs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
		/// <br/><b>p4 help dirs</b>
		/// <br/> 
		/// <br/>     dirs -- List depot subdirectories
		/// <br/> 
		/// <br/>     p4 dirs [-C -D -H] [-S stream] dir[revRange] ...
		/// <br/> 
		/// <br/> 	List directories that match the specified file pattern (dir).
		/// <br/> 	This command does not support the recursive wildcard (...).
		/// <br/> 	Use the * wildcard instead.
		/// <br/> 
		/// <br/> 	Perforce does not track directories individually. A path is treated
		/// <br/> 	as a directory if there are any undeleted files with that path as a
		/// <br/> 	prefix.
		/// <br/> 
		/// <br/> 	By default, all directories containing files are listed. If the dir
		/// <br/> 	argument includes a revision range, only directories containing files
		/// <br/> 	in the range are listed. For details about specifying file revisions,
		/// <br/> 	see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -C flag lists only directories that fall within the current
		/// <br/> 	client view.
		/// <br/> 
		/// <br/> 	The -D flag includes directories containing only deleted files.
		/// <br/> 
		/// <br/> 	The -H flag lists directories containing files synced to the current
		/// <br/> 	client workspace.
		/// <br/> 
		/// <br/> 	The -S flag limits output to depot directories mapped in a stream's
		/// <br/> 	client view.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get dirs on the server that fall within the current client view:
        ///		<code> 
        ///			GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.CurrentClientOnly, null);
        ///         
        ///         IList&lt;String&gt; dirs = new List&lt;String&gt;()
        ///         dirs.Add("//*");
        ///         
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(dirs, opts);
        ///		</code>
        ///		To get dirs on the server that contain files synced to the current
        ///		client workspace:
        ///		<code>
        ///			GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.SyncedDirs, null);
        ///                            
        ///         IList&lt;String&gt; dirs = new List&lt;String&gt;()
        ///         dirs.Add("//*");
        ///         
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(dirs, opts);
        ///     </code>
        ///		To get dirs on the server that fall under the path //depot/main/:
        ///		<code>
        ///		    GetDepotDirsCmdOptions opts =
        ///			new GetDepotDirsCmdOptions(GetDepotDirsCmdFlags.None, null);
        ///                            
        ///         IList&lt;String&gt; dirs = new List&lt;String&gt;()
        ///         dirs.Add("//depot/main/*");
        ///         
        ///			IList&lt;String&gt; target = Repository.GetDepotDirs(dirs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetDepotDirsCmdFlags"/>
        public IList<String> GetDepotDirs(IList<String> dirs, Options options)
        {
            return GetDepotDirs(options, dirs.ToArray());
        }

        /// <summary>
        /// Return the contents of the files identified by the passed-in file specs. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// GetFileContents
        /// <remarks>
        /// <br/><b>p4 help print</b>
        /// <br/> 
        /// <br/>     print -- Write a depot file to standard output
        /// <br/> 
        /// <br/>     p4 print [-a -A -k -o localFile -q -m max] file[revRange] ...
        /// <br/>     p4 print -U unloadfile ...
        /// <br/> 
        /// <br/> 	Retrieve the contents of a depot file to the client's standard output.
        /// <br/> 	The file is not synced.  If file is specified using client syntax,
        /// <br/> 	Perforce uses the client view to determine the corresponding depot
        /// <br/> 	file.
        /// <br/> 
        /// <br/> 	By default, the head revision is printed.  If the file argument
        /// <br/> 	includes a revision, the specified revision is printed.  If the
        /// <br/> 	file argument has a revision range,  then only files selected by
        /// <br/> 	that revision range are printed, and the highest revision in the
        /// <br/> 	range is printed. For details about revision specifiers, see 'p4
        /// <br/> 	help revisions'.
        /// <br/> 
        /// <br/> 	The -a flag prints all revisions within the specified range, rather
        /// <br/> 	than just the highest revision in the range.
        /// <br/> 
        /// <br/> 	The -A flag prints files in archive depots.
        /// <br/> 
        /// <br/> 	The -k flag suppresses keyword expansion.
        /// <br/> 
        /// <br/> 	The -o localFile flag redirects the output to the specified file on
        /// <br/> 	the client filesystem.
        /// <br/> 
        /// <br/> 	The -q flag suppresses the initial line that displays the file name
        /// <br/> 	and revision.
        /// <br/> 
        /// <br/> 	The -m flag limits print to the first 'max' number of files.
        /// <br/> 
        /// <br/> 	The -U option prints files in the unload depot (see 'p4 help unload'
        /// <br/> 	for more information about the unload depot).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the contents of the file //depot/MyCode/README.txt:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(opts, filespec);
        ///		</code>
        ///		To get the contents of the file //depot/MyCode/README.txt redirecting the
        ///		contents to local file C:\Doc\README.txt and supressing the file name
        ///		and revision line:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.Suppress,
        ///		    "C:\\Doc\\README.txt");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(opts, filespec);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileContentsCmdFlags"/>
        public IList<string> GetFileContents(Options options, params FileSpec[] filespecs)
        {
            P4.P4Command printCmd = new P4Command(this, "print", true, FileSpec.ToEscapedStrings(filespecs));
            P4.P4CommandResult r = printCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            IList<string> value = new List<string>();
            if (r.TaggedOutput != null)
            {
                if ((options == null) ||
                (options.ContainsKey("-q") == false))
                {
                    foreach (P4.TaggedObject obj in r.TaggedOutput)
                    {
                        string path = string.Empty;
                        string rev = string.Empty;
                        if (obj.ContainsKey("depotFile"))
                        {
                            value.Add(obj["depotFile"]);
                        }
                    }

                }
            }

            value.Add(r.TextOutput);
            return value;
        }

        /// <summary>
        /// Return the contents of the files identified by the passed-in file specs. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// GetFileContents
        /// <remarks>
		/// <br/><b>p4 help print</b>
		/// <br/> 
		/// <br/>     print -- Write a depot file to standard output
		/// <br/> 
		/// <br/>     p4 print [-a -A -k -o localFile -q -m max] file[revRange] ...
		/// <br/>     p4 print -U unloadfile ...
		/// <br/> 
		/// <br/> 	Retrieve the contents of a depot file to the client's standard output.
		/// <br/> 	The file is not synced.  If file is specified using client syntax,
		/// <br/> 	Perforce uses the client view to determine the corresponding depot
		/// <br/> 	file.
		/// <br/> 
		/// <br/> 	By default, the head revision is printed.  If the file argument
		/// <br/> 	includes a revision, the specified revision is printed.  If the
		/// <br/> 	file argument has a revision range,  then only files selected by
		/// <br/> 	that revision range are printed, and the highest revision in the
		/// <br/> 	range is printed. For details about revision specifiers, see 'p4
		/// <br/> 	help revisions'.
		/// <br/> 
		/// <br/> 	The -a flag prints all revisions within the specified range, rather
		/// <br/> 	than just the highest revision in the range.
		/// <br/> 
		/// <br/> 	The -A flag prints files in archive depots.
		/// <br/> 
		/// <br/> 	The -k flag suppresses keyword expansion.
		/// <br/> 
		/// <br/> 	The -o localFile flag redirects the output to the specified file on
		/// <br/> 	the client filesystem.
		/// <br/> 
		/// <br/> 	The -q flag suppresses the initial line that displays the file name
		/// <br/> 	and revision.
		/// <br/> 
		/// <br/> 	The -m flag limits print to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -U option prints files in the unload depot (see 'p4 help unload'
		/// <br/> 	for more information about the unload depot).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the contents of the file //depot/MyCode/README.txt:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(filespecs, opts);
        ///		</code>
        ///		To get the contents of the file //depot/MyCode/README.txt redirecting the
        ///		contents to local file C:\Doc\README.txt and supressing the file name
        ///		and revision line:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.Suppress,
        ///		    "C:\\Doc\\README.txt");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileContentsCmdFlags"/>
        public IList<string> GetFileContents(IList<FileSpec> filespecs, Options options)
        {
            return GetFileContents(options, filespecs.ToArray());
        }

        /// <summary>
        /// Return the contents of the files identified by the passed-in file specs. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// GetFileContentsEx
        /// <remarks>
        /// <br/><b>p4 help print</b>
        /// <br/> 
        /// <br/>     print -- Write a depot file to standard output
        /// <br/> 
        /// <br/>     p4 print [-a -A -k -o localFile -q -m max] file[revRange] ...
        /// <br/>     p4 print -U unloadfile ...
        /// <br/> 
        /// <br/> 	Retrieve the contents of a depot file to the client's standard output.
        /// <br/> 	The file is not synced.  If file is specified using client syntax,
        /// <br/> 	Perforce uses the client view to determine the corresponding depot
        /// <br/> 	file.
        /// <br/> 
        /// <br/> 	By default, the head revision is printed.  If the file argument
        /// <br/> 	includes a revision, the specified revision is printed.  If the
        /// <br/> 	file argument has a revision range,  then only files selected by
        /// <br/> 	that revision range are printed, and the highest revision in the
        /// <br/> 	range is printed. For details about revision specifiers, see 'p4
        /// <br/> 	help revisions'.
        /// <br/> 
        /// <br/> 	The -a flag prints all revisions within the specified range, rather
        /// <br/> 	than just the highest revision in the range.
        /// <br/> 
        /// <br/> 	The -A flag prints files in archive depots.
        /// <br/> 
        /// <br/> 	The -k flag suppresses keyword expansion.
        /// <br/> 
        /// <br/> 	The -o localFile flag redirects the output to the specified file on
        /// <br/> 	the client filesystem.
        /// <br/> 
        /// <br/> 	The -q flag suppresses the initial line that displays the file name
        /// <br/> 	and revision.
        /// <br/> 
        /// <br/> 	The -m flag limits print to the first 'max' number of files.
        /// <br/> 
        /// <br/> 	The -U option prints files in the unload depot (see 'p4 help unload'
        /// <br/> 	for more information about the unload depot).
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the contents of the file //depot/MyCode/README.txt:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(opts, filespec);
        ///		</code>
        ///		To get the contents of the file //depot/MyCode/README.txt redirecting the
        ///		contents to local file C:\Doc\README.txt and supressing the file name
        ///		and revision line:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.Suppress,
        ///		    "C:\\Doc\\README.txt");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(opts, filespec);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileContentsCmdFlags"/>
        public IList<object> GetFileContentsEx(Options options, params FileSpec[] filespecs)
        {
            P4.P4Command printCmd = new P4Command(this, "print", true, FileSpec.ToEscapedStrings(filespecs));
            P4.P4CommandResult r = printCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            IList<object> value = new List<object>();
            if (r.TaggedOutput != null)
            {
                if ((options == null) ||
                (options.ContainsKey("-q") == false))
                {
                    foreach (P4.TaggedObject obj in r.TaggedOutput)
                    {
                        string path = string.Empty;
                        string revStr = string.Empty;
                        int rev = -1;
                        if (obj.ContainsKey("depotFile"))
                        {
                            string s = obj["depotFile"];
                            int idx = s.LastIndexOf('#');
                            if (idx > 0)
                            {
                                path = s.Substring(0, idx);
                                revStr = s.Substring(idx + 1);
                                int.TryParse(revStr, out rev);
                            }
                            else
                            {
                                path = s;
                            }
                            if (rev >= 0)
                            {
                                value.Add(FileSpec.DepotSpec(path, rev));
                            }
                            else
                            {
                                value.Add(FileSpec.DepotSpec(path));
                            }
                        }
                    }

                }
            }
            if (r.BinaryOutput != null)
            {
                value.Add(r.BinaryOutput);
            }
            else
            {
                value.Add(r.TextOutput);
            }
            return value;
        }

        /// <summary>
        /// Return the contents of the files identified by the passed-in file specs. 
        /// </summary>
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// GetFileContentsEx
        /// <remarks>
		/// <br/><b>p4 help print</b>
		/// <br/> 
		/// <br/>     print -- Write a depot file to standard output
		/// <br/> 
		/// <br/>     p4 print [-a -A -k -o localFile -q -m max] file[revRange] ...
		/// <br/>     p4 print -U unloadfile ...
		/// <br/> 
		/// <br/> 	Retrieve the contents of a depot file to the client's standard output.
		/// <br/> 	The file is not synced.  If file is specified using client syntax,
		/// <br/> 	Perforce uses the client view to determine the corresponding depot
		/// <br/> 	file.
		/// <br/> 
		/// <br/> 	By default, the head revision is printed.  If the file argument
		/// <br/> 	includes a revision, the specified revision is printed.  If the
		/// <br/> 	file argument has a revision range,  then only files selected by
		/// <br/> 	that revision range are printed, and the highest revision in the
		/// <br/> 	range is printed. For details about revision specifiers, see 'p4
		/// <br/> 	help revisions'.
		/// <br/> 
		/// <br/> 	The -a flag prints all revisions within the specified range, rather
		/// <br/> 	than just the highest revision in the range.
		/// <br/> 
		/// <br/> 	The -A flag prints files in archive depots.
		/// <br/> 
		/// <br/> 	The -k flag suppresses keyword expansion.
		/// <br/> 
		/// <br/> 	The -o localFile flag redirects the output to the specified file on
		/// <br/> 	the client filesystem.
		/// <br/> 
		/// <br/> 	The -q flag suppresses the initial line that displays the file name
		/// <br/> 	and revision.
		/// <br/> 
		/// <br/> 	The -m flag limits print to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -U option prints files in the unload depot (see 'p4 help unload'
		/// <br/> 	for more information about the unload depot).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the contents of the file //depot/MyCode/README.txt:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(filespecs, opts);
        ///		</code>
        ///		To get the contents of the file //depot/MyCode/README.txt redirecting the
        ///		contents to local file C:\Doc\README.txt and supressing the file name
        ///		and revision line:
        ///		<code> 
        ///		    GetFileContentsCmdOptions opts = 
        ///		    new GetFileContentsCmdOptions(GetFileContentsCmdFlags.Suppress,
        ///		    "C:\\Doc\\README.txt");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileContents(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileContentsCmdFlags"/>
        public IList<object> GetFileContentsEx(IList<FileSpec> filespecs, Options options)
        {
            return GetFileContentsEx(options, filespecs.ToArray());
        }

        /// <summary>
        /// Get the revision history data for the passed-in file specs. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help filelog</b>
        /// <br/> 
        /// <br/>     filelog -- List revision history of files
        /// <br/> 
        /// <br/>     p4 filelog [-c changelist# -h -i -l -L -t -m max -p -s] file[revRange] ...
        /// <br/> 
        /// <br/> 	List the revision history of the specified files, from the most
        /// <br/> 	recent revision to the first.  If the file specification includes
        /// <br/> 	a revision, the command lists revisions at or prior to the specified
        /// <br/> 	revision.  If the file specification includes a revision range,
        /// <br/> 	the command lists only the specified revisions. See 'p4 help revisions'
        /// <br/> 	for details.
        /// <br/> 
        /// <br/> 	The -c changelist# flag displays files submitted at the specified
        /// <br/> 	changelist number.
        /// <br/> 
        /// <br/> 	The -i flag includes inherited file history. If a file was created by
        /// <br/> 	branching (using 'p4 integrate'), filelog lists the revisions of the
        /// <br/> 	file's ancestors up to the branch points that led to the specified
        /// <br/> 	revision.  File history inherited by renaming (using 'p4 move') is
        /// <br/> 	always displayed regardless of whether -i is specified.
        /// <br/> 
        /// <br/> 	The -h flag displays file content history instead of file name
        /// <br/> 	history.  The list includes revisions of other files that were
        /// <br/> 	branched or copied (using 'p4 integrate' and 'p4 resolve -at') to
        /// <br/> 	the specified revision.  Revisions that were replaced by copying
        /// <br/> 	or branching are omitted, even if they are part of the history of
        /// <br/> 	the specified revision.
        /// <br/> 
        /// <br/> 	The -t flag displays the time as well as the date.
        /// <br/> 
        /// <br/> 	The -l flag lists the full text of the changelist descriptions.
        /// <br/> 
        /// <br/> 	The -L flag lists the full text of the changelist descriptions,
        /// <br/> 	truncated to 250 characters if longer.
        /// <br/> 
        /// <br/> 	The -m max displays at most 'max' revisions per file of the file[rev]
        /// <br/> 	argument specified.
        /// <br/> 
        /// <br/> 	The -p flag is used in conjunction with the '-h' flag to prevent
        /// <br/> 	filelog from following content of promoted task streams. This flag
        /// <br/> 	is useful when there are many child task streams branched from the
        /// <br/> 	file argument supplied.
        /// <br/> 
        /// <br/> 	The -s flag displays a shortened form of filelog that omits
        /// <br/> 	non-contributory integrations.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the file history of the file //depot/MyCode/README.txt
        ///		submitted at change 43578 and showing the full changelist description:
        ///		<code> 
        ///		    GetFileHistoryCmdOptions opts = 
        ///		    new GetFileHistoryCmdOptions(GetFileHistoryCmdFlags.FullDescription
        ///		    43578, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileHistory(opts, filespec);
        ///		</code>
        ///		To get the file history of the file //depot/MyCode/README.txt
        ///		showing both time and date for the 10 latest revisions:
        ///		<code> 
        ///		    GetFileHistoryCmdOptions opts = 
        ///		    new GetFileHistoryCmdOptions(GetFileHistoryCmdFlags.Time,
        ///		    0, 10)
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileHistory(opts, filespec);
        ///		</code>
        /// </example>
        /// <seealso cref="FileLogCmdFlags"/>
        /// <seealso cref="GetFileHistoryCmdFlags"/>
        public IList<FileHistory> GetFileHistory(Options options, params FileSpec[] filespecs)
        {
            P4.P4Command filesCmd = new P4Command(this, "filelog", true, FileSpec.ToEscapedStrings(filespecs));
            P4.P4CommandResult r = filesCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<FileHistory> value = new List<FileHistory>();

            bool dst_mismatch = false;
            string offset = string.Empty;

            if (Server != null && Server.Metadata != null)
            {
                offset = Server.Metadata.DateTimeOffset;
                dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
            }

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                int idx = 0;

                while (true)
                {
                    string key = String.Format("rev{0}", idx);
                    int revision = -1;

                    if (obj.ContainsKey(key))
                        int.TryParse(obj[key], out revision);
                    else
                        break;

                    int changelistid = -1;
                    key = String.Format("change{0}", idx);
                    if (obj.ContainsKey(key))
                        int.TryParse(obj[key], out changelistid);

                    StringEnum<FileAction> action = "None";
                    key = String.Format("action{0}", idx);
                    if (obj.ContainsKey(key))
                        action = obj[key];

                    DateTime date = new DateTime();
                    long unixTime = 0;
                    key = String.Format("time{0}", idx);
                    if (obj.ContainsKey(key))
                        unixTime = Int64.Parse(obj[key]);
                    DateTime UTC = FormBase.ConvertUnixTime(unixTime);
                    DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                        DateTimeKind.Unspecified);
                    date = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);

                    string username = null;
                    key = String.Format("user{0}", idx);
                    if (obj.ContainsKey(key))
                        username = obj[key];

                    string description = null;
                    key = String.Format("desc{0}", idx);
                    if (obj.ContainsKey(key))
                        description = obj[key];

                    string digest = null;
                    key = String.Format("digest{0}", idx);
                    if (obj.ContainsKey(key))
                        digest = obj[key];

                    long filesize = -1;
                    key = String.Format("fileSize{0}", idx);
                    if (obj.ContainsKey(key))
                        long.TryParse(obj[key], out filesize);

                    string clientname = null;
                    key = String.Format("client{0}", idx);
                    if (obj.ContainsKey(key))
                        clientname = obj[key];

                    PathSpec depotpath = new DepotPath(obj["depotFile"]);

                    FileType filetype = null;
                    key = String.Format("type{0}", idx);
                    if (obj.ContainsKey(key))
                        filetype = new FileType(obj[key]);

                    List<RevisionIntegrationSummary> integrationsummaries = new List<RevisionIntegrationSummary>();

                    int idx2 = 0;
                    key = String.Format("how{0},{1}", idx, idx2);
                    while (obj.ContainsKey(key))
                    {
                        string how = obj[key];
                        key = String.Format("file{0},{1}", idx, idx2);
                        string frompath = obj[key];

                        key = String.Format("srev{0},{1}", idx, idx2);
                        string srev = obj[key];

                        VersionSpec startrev = new Revision(-1);

                        if (srev.StartsWith("#h")
                            |
                            srev.StartsWith("#n"))
                        {
                            if (srev.Contains("#none"))
                            {
                                startrev = Revision.None;
                            }

                            if (srev.Contains("#have"))
                            {
                                startrev = Revision.Have;
                            }

                            if (srev.Contains("#head"))
                            {
                                startrev = Revision.Head;
                            }
                        }
                        else
                        {
                            srev = srev.Trim('#');
                            int rev = Convert.ToInt16(srev);
                            startrev = new Revision(rev);
                        }

                        key = String.Format("erev{0},{1}", idx, idx2);
                        string erev = obj[key];

                        VersionSpec endrev = new Revision(-1);

                        if (erev.StartsWith("#h")
                            |
                            erev.StartsWith("#n"))
                        {
                            if (erev.Contains("#none"))
                            {
                                endrev = Revision.None;
                            }

                            if (srev.Contains("#have"))
                            {
                                endrev = Revision.Have;
                            }

                            if (srev.Contains("#head"))
                            {
                                endrev = Revision.Head;
                            }
                        }
                        else
                        {
                            erev = erev.Trim('#');
                            int rev = Convert.ToInt16(erev);
                            endrev = new Revision(rev);
                        }

                        RevisionIntegrationSummary integrationsummary = new RevisionIntegrationSummary(
                              new FileSpec(new DepotPath(frompath),
                              new VersionRange(startrev, endrev)), how);

                        integrationsummaries.Add(integrationsummary);

                        idx2++;
                        key = String.Format("how{0},{1}", idx, idx2);
                    }

                    FileHistory fh = new FileHistory(revision, changelistid, action,
                date, username, filetype, description, digest, filesize, depotpath, clientname, integrationsummaries);

                    value.Add(fh);

                    idx++;

                }
            }
            return value;
        }

        /// <summary>
        /// Get the revision history data for the passed-in file specs. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
		/// <br/><b>p4 help filelog</b>
		/// <br/> 
		/// <br/>     filelog -- List revision history of files
		/// <br/> 
		/// <br/>     p4 filelog [-c changelist# -h -i -l -L -t -m max -p -s] file[revRange] ...
		/// <br/> 
		/// <br/> 	List the revision history of the specified files, from the most
		/// <br/> 	recent revision to the first.  If the file specification includes
		/// <br/> 	a revision, the command lists revisions at or prior to the specified
		/// <br/> 	revision.  If the file specification includes a revision range,
		/// <br/> 	the command lists only the specified revisions. See 'p4 help revisions'
		/// <br/> 	for details.
		/// <br/> 
		/// <br/> 	The -c changelist# flag displays files submitted at the specified
		/// <br/> 	changelist number.
		/// <br/> 
		/// <br/> 	The -i flag includes inherited file history. If a file was created by
		/// <br/> 	branching (using 'p4 integrate'), filelog lists the revisions of the
		/// <br/> 	file's ancestors up to the branch points that led to the specified
		/// <br/> 	revision.  File history inherited by renaming (using 'p4 move') is
		/// <br/> 	always displayed regardless of whether -i is specified.
		/// <br/> 
		/// <br/> 	The -h flag displays file content history instead of file name
		/// <br/> 	history.  The list includes revisions of other files that were
		/// <br/> 	branched or copied (using 'p4 integrate' and 'p4 resolve -at') to
		/// <br/> 	the specified revision.  Revisions that were replaced by copying
		/// <br/> 	or branching are omitted, even if they are part of the history of
		/// <br/> 	the specified revision.
		/// <br/> 
		/// <br/> 	The -t flag displays the time as well as the date.
		/// <br/> 
		/// <br/> 	The -l flag lists the full text of the changelist descriptions.
		/// <br/> 
		/// <br/> 	The -L flag lists the full text of the changelist descriptions,
		/// <br/> 	truncated to 250 characters if longer.
		/// <br/> 
		/// <br/> 	The -m max displays at most 'max' revisions per file of the file[rev]
		/// <br/> 	argument specified.
		/// <br/> 
		/// <br/> 	The -p flag is used in conjunction with the '-h' flag to prevent
		/// <br/> 	filelog from following content of promoted task streams. This flag
		/// <br/> 	is useful when there are many child task streams branched from the
		/// <br/> 	file argument supplied.
		/// <br/> 
		/// <br/> 	The -s flag displays a shortened form of filelog that omits
		/// <br/> 	non-contributory integrations.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the file history of the file //depot/MyCode/README.txt
        ///		submitted at change 43578 and showing the full changelist description:
        ///		<code> 
        ///		    GetFileHistoryCmdOptions opts = 
        ///		    new GetFileHistoryCmdOptions(GetFileHistoryCmdFlags.FullDescription
        ///		    43578, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileHistory(filespecs, opts);
        ///		</code>
        ///		To get the file history of the file //depot/MyCode/README.txt
        ///		showing both time and date for the 10 latest revisions:
        ///		<code> 
        ///		    GetFileHistoryCmdOptions opts = 
        ///		    new GetFileHistoryCmdOptions(GetFileHistoryCmdFlags.Time,
        ///		    0, 10)
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;String&gt; target = Repository.GetFileHistory(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="FileLogCmdFlags"/>
        /// <seealso cref="GetFileHistoryCmdFlags"/>
		public IList<FileHistory> GetFileHistory(IList<FileSpec> filespecs, Options options)
        {
            return GetFileHistory(options, filespecs.ToArray());
        }

        /// <summary>
        /// Get content and existence diff details for two depot files.
        /// </summary>    
        /// <param name="filespecleft"></param>
        /// <param name="filespecright"></param>
        /// <param name="options"></param>
        /// <returns></returns>
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
        /// <example>
        ///		To get the depot file diffs between //depot/main/Program.cs and
        ///		//depot/rel/Program.cs and ignore whitespace changes:
        ///		<code> 
        ///		    GetDepotFileDiffsCmdOptions opts =
        ///		    new GetDepotFileDiffsCmdOptions(GetDepotFileDiffsCmdFlags.IgnoreWhitespaceChanges,
        ///		    0, 0, null,null,null);
        ///         
        ///			IList&lt;DepotFileDiff&gt; target =
        ///			Repository.GetDepotFileDiffs("//depot/main/Program.cs",
        ///			"//depot/rel/Program.cs", opts);
        ///		</code>
        ///		To get the depot files that differ between all files under //depot/main/... and
        ///		//depot/rel/... and display in GNU format only listing files that differ:
        ///		<code> 
        ///		    GetDepotFileDiffsCmdOptions opts =
        ///		    new GetDepotFileDiffsCmdOptions(GetDepotFileDiffsCmdFlags.GNU,
        ///		    0, 0, null,null,null);
        ///         
        ///			IList&lt;DepotFileDiff&gt; target =
        ///			Repository.GetDepotFileDiffs("//depot/main/...", "//depot/rel/...", opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetDepotFileDiffsCmdFlags"/>
        public IList<DepotFileDiff> GetDepotFileDiffs(string filespecleft, string filespecright, Options options)
        {
            P4.P4Command GetDepotFileDiffs = new P4Command(this, "diff2", true, filespecleft, filespecright);

            P4.P4CommandResult r = GetDepotFileDiffs.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<DepotFileDiff> value = new List<DepotFileDiff>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                DepotFileDiff val = new DepotFileDiff();
                val.FromGetDepotFileDiffsCmdTaggedOutput(obj, _connection, options);
                value.Add(val);

            }
            return value;

        }

        /// <summary>
        /// Compare workspace content to depot content
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
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
        /// <example>
        ///		To get the depot file diffs between //depot/main/Program.cs and
        ///		local C:\workspace\depot\rel\Program.cs and ignore whitespace changes:
        ///		<code> 
        ///		    GetDepotFileDiffsCmdOptions opts =
        ///		    new GetDepotFileDiffsCmdOptions(GetDepotFileDiffsCmdFlags.IgnoreWhitespaceChanges,
        ///		    0, 0, 0);
        ///         
        ///         IList&lt;FileSpec&gt; fsl = new List&lt;FileSpec&gt;
        ///         FileSpec fs = new FileSpec(new DepotPath("//depot/TestData/Letters.txt"));
        ///         fsl.Add(fs);
        /// 
        ///			IList&lt;FileDiff&gt; target =
        ///			Repository.GetFileDiffs(fsl, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetDepotFileDiffsCmdFlags"/>
        public IList<FileDiff> GetFileDiffs(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command GetFileDiffs = new P4Command(this, "diff", true, FileSpec.ToEscapedStrings(filespecs));

            P4.P4CommandResult r = GetFileDiffs.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<FileDiff> value = new List<FileDiff>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                FileDiff val = new FileDiff();
                val.FromGetFileDiffsCmdTaggedOutput(obj, _connection, options);
                value.Add(val);
            }
            return value;
        }



        /// <summary>
        /// Return FileAnnotation objects for the listed FileSpecs. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help annotate</b>
        /// <br/> 
        /// <br/>     annotate -- Print file lines and their revisions
        /// <br/> 
        /// <br/>     p4 annotate [-aciIqt -d&lt;flags&gt;] file[revRange] ...
        /// <br/> 
        /// <br/> 	Prints all lines of the specified files, indicating the revision that
        /// <br/> 	introduced each line into the file.
        /// <br/> 
        /// <br/> 	If the file argument includes a revision, then only revisions up to
        /// <br/> 	the specified revision are displayed.  If the file argument has a
        /// <br/> 	revision range, only revisions within that range are displayed. For
        /// <br/> 	details about specifying revisions, see 'p4 help revisions'.
        /// <br/> 
        /// <br/> 	The -a flag includes both deleted files and lines no longer present
        /// <br/> 	at the head revision. In the latter case, both the starting and ending
        /// <br/> 	revision for each line is displayed.
        /// <br/> 
        /// <br/> 	The -c flag directs the annotate command to output changelist numbers
        /// <br/> 	rather than revision numbers for each line.
        /// <br/> 
        /// <br/> 	The -d&lt;flags&gt; change the way whitespace and/or line endings are
        /// <br/> 	treated: -db (ignore whitespace changes), -dw (ignore whitespace),
        /// <br/> 	-dl (ignore line endings).
        /// <br/> 
        /// <br/> 	The -i flag follows branches.  If a file was created by branching,
        /// <br/> 	'p4 annotate' includes the revisions of the source file up to the
        /// <br/> 	branch point, just as 'p4 filelog -i' does.  If a file has history
        /// <br/> 	prior to being created by branching (such as a file that was branched
        /// <br/> 	on top of a deleted file), -i ignores those prior revisions and
        /// <br/> 	follows the source.  -i implies -c.
        /// <br/> 
        /// <br/> 	The -I flag follows all integrations into the file.  If a line was
        /// <br/> 	introduced into the file by a merge, the source of the merge is
        /// <br/> 	displayed as the changelist that introduced the line. If the source
        /// <br/> 	itself was the result of an integration, that source is used instead,
        /// <br/> 	and so on.  -I implies -c and may not be combined with -i.
        /// <br/> 
        /// <br/> 	The -q flag suppresses the one-line header that is displayed by
        /// <br/> 	default for each file.
        /// <br/> 
        /// <br/> 	The -t flag forces 'p4 annotate' to display binary files.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the file annotations of the file //depot/MyCode/README.txt:
        ///		<code> 
        ///		    GetFileAnnotationsCmdOptions opts = 
        ///		    new GetFileAnnotationsCmdOptions(GetFileAnnotationsCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileAnnotation&gt; target = Repository.GetFileAnnotations(filespecs, opts);
        ///		</code>
        ///		To get the file annotations of the file //depot/MyCode/README.txt redirecting the
        ///		contents to local file C:\Doc\README.txt and supressing the one-line header:
        ///		<code> 
        ///		    GetFileAnnotationsCmdOptions opts = 
        ///		    new GetFileAnnotationsCmdOptions(GetFileAnnotationsCmdFlags.Suppress,
        ///		    "C:\\Doc\\README.txt");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileAnnotation&gt; target = Repository.GetFileAnnotations(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileAnnotationsCmdFlags"/>
        public IList<FileAnnotation> GetFileAnnotations(IList<FileSpec> filespecs, Options options)
        {

            P4.P4Command annotateCmd = new P4Command(this, "annotate", true, FileSpec.ToStrings(filespecs));

            P4.P4CommandResult r = annotateCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }

            bool changelist = false;
            string opts;
            if (options != null)
            {
                opts = options.Keys.ToString();
                if (opts.Contains("c"))
                { changelist = true; }
            }

            string dp = null;
            string line = null;
            int lower = -1;
            int upper = -1;
            IList<FileAnnotation> value = new List<FileAnnotation>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                if (obj.ContainsKey("depotFile"))
                {
                    dp = obj["depotFile"];
                    line = null;
                    lower = -1;
                    upper = -1;
                    continue;
                }

                if (obj.ContainsKey("lower"))
                {
                    int l = -1;
                    int.TryParse(obj["lower"], out l);
                    lower = l;
                }

                if (obj.ContainsKey("upper"))
                {
                    int u = -1;
                    int.TryParse(obj["upper"], out u);
                    upper = u;
                }

                if (obj.ContainsKey("data"))
                {
                    line = obj["data"];
                }

                if (dp != null
                    &&
                    line != null
                    &&
                    lower != -1
                    &&
                    upper != -1)
                {
                    FileSpec fs = new FileSpec();
                    if (changelist == true)
                    {
                        fs = new FileSpec(new DepotPath(dp), new VersionRange(new ChangelistIdVersion(lower), new ChangelistIdVersion(upper)));
                    }
                    else
                    {
                        fs = new FileSpec(new DepotPath(dp), new VersionRange(new Revision(lower), new Revision(upper)));
                    }
                    FileAnnotation fa = new FileAnnotation(fs, line);
                    value.Add(fa);
                }
            }
            return value;
        }


        /// <summary>
        /// Tag depot files with the passed-in label. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="labelid"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help tag</b>
        /// <br/> 
        /// <br/>     tag -- Tag files with a label
        /// <br/> 
        /// <br/>     p4 tag [-d -g -n -U] -l label file[revRange] ...
        /// <br/> 
        /// <br/> 	Tag associates the named label with the file revisions specified by
        /// <br/> 	the file argument.  After file revisions are tagged with a label,
        /// <br/> 	revision specifications of the form '@label' can be used to refer
        /// <br/> 	to them.
        /// <br/> 
        /// <br/> 	If the file argument does not include a revision specification, the
        /// <br/> 	head revisions is tagged.  See 'p4 help revisions' for revision
        /// <br/> 	specification options.
        /// <br/> 
        /// <br/> 	If the file argument includes a revision range specification, only
        /// <br/> 	the files with revisions in that range are tagged.  Files with more
        /// <br/> 	than one revision in the range are tagged at the highest revision.
        /// <br/> 
        /// <br/> 	The -d deletes the association between the specified files and the
        /// <br/> 	label, regardless of revision.
        /// <br/> 
        /// <br/> 	The -n flag previews the results of the operation.
        /// <br/> 
        /// <br/> 	Tag can be used with an existing label (see 'p4 help labels') or
        /// <br/> 	with a new one.  An existing label can be used only by its owner,
        /// <br/> 	and only if it is unlocked. (See 'p4 help label').
        /// <br/> 
        /// <br/> 	The -U flag specifies that the new label should be created with the
        /// <br/> 	'autoreload' option (See 'p4 help label'). It has no effect on an
        /// <br/> 	existing label.
        /// <br/> 
        /// <br/> 	To list the file revisions tagged with a label, use 'p4 files
        /// <br/> 	@label'.
        /// <br/> 
        /// <br/> 	The -g flag is used on an Edge Server to update a global label.
        /// <br/> 	Configuring rpl.labels.global=1 reverses this default and causes this
        /// <br/> 	flag to have the opposite meaning.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To tag the file //depot/MyCode/README.txt with build_label:
        ///		<code> 
        ///		    TagCmdOptions opts = 
        ///		    new TagCmdOptions(TagFilesCmdFlags.None, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileSpec&gt; target =
        ///			Repository.TagFiles(filespecs, "build_label", opts);
        ///		</code>
        ///		To remove the association between the file //depot/MyCode/README.txt
        ///		 and build_label:
        ///		<code> 
        ///		    TagCmdOptions opts = 
        ///		    new TagCmdOptions(TagFilesCmdFlags.Delete, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/MyCode/README.txt"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileSpec&gt; target =
        ///			Repository.TagFiles(filespecs, "build_label", opts);
        ///		</code>
        ///		To get a preview list of the files that would be tagged in path
        ///		//depot/main/src with build_label:
        ///		<code> 
        ///		    TagCmdOptions opts = 
        ///		    new TagCmdOptions(TagFilesCmdFlags.ListOnly, null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/main/src/..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileSpec&gt; target =
        ///			Repository.TagFiles(filespecs, "build_label", opts);
        ///		</code>
        /// </example>
        /// <seealso cref="TagFilesCmdFlags"/>
        public IList<FileSpec> TagFiles(IList<FileSpec> filespecs, string labelid, Options options)
        {
            P4.P4Command tagCmd = new P4Command(this, "tag", true, FileSpec.ToStrings(filespecs));
            options["-l"] = labelid;

            P4.P4CommandResult r = tagCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<FileSpec> value = new List<FileSpec>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                string revision = obj["rev"];
                int rev = Convert.ToInt16(revision);
                VersionSpec version = new Revision(rev);
                DepotPath path = new DepotPath(obj["depotFile"]);
                FileSpec fs = new FileSpec(path, version);
                value.Add(fs);
            }
            return value;
        }

        /// <summary>
        /// List fixes affecting files and / or jobs and / or changelists. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help fixes</b>
        /// <br/> 
        /// <br/>     fixes -- List jobs with fixes and the changelists that fix them
        /// <br/> 
        /// <br/>     p4 fixes [-i -m max -c changelist# -j jobName] [file[revRange] ...]
        /// <br/> 
        /// <br/> 	'p4 fixes' list fixed jobs and the number of the changelist that
        /// <br/> 	 contains the fix. Fixes are associated with changelists using the
        /// <br/> 	'p4 fix' command or by editing and submitting changelists.
        /// <br/> 
        /// <br/> 	The 'p4 fixes' command lists both submitted and pending changelists.
        /// <br/> 
        /// <br/> 	By default, 'p4 fixes' lists all fixes.  This list can be limited
        /// <br/> 	as follows: to list fixes for a specified job, use the -j jobName
        /// <br/> 	flag.  To list fixes for a specified changelist, use -c changelist#.
        /// <br/> 	To list fixes that affect specified files, include the file argument.
        /// <br/> 	The file pattern can include wildcards and revision specifiers. For
        /// <br/> 	details about revision specifiers, see 'p4 help revisions'
        /// <br/> 
        /// <br/> 	The -i flag also includes any fixes made by changelists integrated
        /// <br/> 	into the specified files.
        /// <br/> 
        /// <br/> 	The -m max flag limits output to the specified number of job
        /// <br/> 	fixes.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To list the fixes related to job000001:
        ///		<code> 
        ///		    GetFixesCmdOptions opts = 
        ///		    new GetFixesCmdOptions(GetFixesCmdFlags.None, 0, "job000001", 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;Fix&gt; target = Repository.GetFixes(filespecs, opts);
        ///		</code>
        ///		To list the fixes related to changelist 47921:
        ///		<code> 
        ///		    GetFixesCmdOptions opts = 
        ///		    new GetFixesCmdOptions(GetFixesCmdFlags.None, 47921, null, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;Fix&gt; target = Repository.GetFixes(filespecs, opts);
        ///		</code>
        ///		To list the fixes related to path //depot/rel/src that occurred
        ///		between 2014/1/1 and 2014/1/31:
        ///		<code> 
        ///		    GetFixesCmdOptions opts = 
        ///		    new GetFixesCmdOptions(GetFixesCmdFlags.None, 0, null, 0);
        ///         
        ///         VersionRange vr = new VersionRange(new DateTimeVersion(new DateTime(2014, 1, 1)),
        ///         new DateTimeVersion(new DateTime(2014, 1, 31)));
        ///
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), vr);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;Fix&gt; target = Repository.GetFixes(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFixesCmdFlags"/>
        public IList<Fix> GetFixes(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command fixesCmd = new P4Command(this, "fixes", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = fixesCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<Fix> value = new List<Fix>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

                value.Add(Fix.FromFixesCmdTaggedOutput(obj, offset, dst_mismatch));
            }
            return value;
        }

        /// <summary>
        /// Get a list of matching lines in the passed-in file specs. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="pattern"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help grep</b>
        /// <br/> 
        /// <br/>     grep -- Print lines matching a pattern
        /// <br/> 
        /// <br/>     p4 grep [options] -e pattern file[revRange]...
        /// <br/> 
        /// <br/> 	options: -a -i -n -A &lt;num&gt; -B &lt;num&gt; -C &lt;num&gt; -t -s (-v|-l|-L) (-F|-G)
        /// <br/> 
        /// <br/> 	Searches files for lines that match the specified regular expression,
        /// <br/> 	which can contain wildcards.  The parser used by the Perforce server
        /// <br/> 	is based on V8 regexp and might not be compatible with later parsers,
        /// <br/> 	but the majority of functionality is available.
        /// <br/> 
        /// <br/> 	By default the head revision is searched. If the file argument includes
        /// <br/> 	a revision specification, all corresponding revisions are searched.
        /// <br/> 	If the file argument includes a revision range, only files in that
        /// <br/> 	range are listed, and the highest revision in the range is searched.
        /// <br/> 	For details about revision specifiers, see 'p4 help revisions'.
        /// <br/> 
        /// <br/> 	The -a flag searches all revisions within the specified range. By
        /// <br/> 	default only the highest revision in the range is searched.
        /// <br/> 
        /// <br/> 	The -i flag causes the pattern matching to be case-insensitive. By
        /// <br/> 	default, matching is case-sensitive.
        /// <br/> 
        /// <br/> 	The -n flag displays the matching line number after the file revision
        /// <br/> 	number. By default, matches are displayed as revision#: &lt;text&gt;.
        /// <br/> 
        /// <br/> 	The -v flag displays files with non-matching lines.
        /// <br/> 
        /// <br/> 	The -F flag is used to interpret the pattern as a fixed string.
        /// <br/> 
        /// <br/> 	The -G flag is used to interpret the pattern as a regular expression,
        /// <br/> 	which is the default behavior.
        /// <br/> 
        /// <br/> 	The -L flag displays the name of each selected file from which no
        /// <br/> 	output would normally have been displayed. Scanning stops on the
        /// <br/> 	first match.
        /// <br/> 
        /// <br/> 	The -l flag displays the name of each selected file containing
        /// <br/> 	matching text. Scanning stops on the first match.
        /// <br/> 
        /// <br/> 	The -s flag suppresses error messages that result from abandoning
        /// <br/> 	files that have a maximum number of characters in a single line that
        /// <br/> 	are greater than 4096.  By default, an error is reported when grep
        /// <br/> 	abandons such files.
        /// <br/> 
        /// <br/> 	The -t flag searches binary files.  By default, only text files are
        /// <br/> 	searched.
        /// <br/> 
        /// <br/> 	The -A &lt;num&gt; flag displays the specified number of lines of trailing
        /// <br/> 	context after matching lines.
        /// <br/> 
        /// <br/> 	The -B &lt;num&gt; flag displays the specified number of lines of leading
        /// <br/> 	context before matching lines.
        /// <br/> 
        /// <br/> 	The -C &lt;num&gt; flag displays the specified number of lines of output
        /// <br/> 	context.
        /// <br/> 
        /// <br/> 	Regular expressions:
        /// <br/> 
        /// <br/> 	A regular expression is zero or more branches, separated by `|'. It
        /// <br/> 	matches anything that matches one of the branches.
        /// <br/> 
        /// <br/> 	A branch is zero or more pieces, concatenated.  It matches a match
        /// <br/> 	for the first, followed by a match for the second, etc.
        /// <br/> 
        /// <br/> 	A piece is an atom possibly followed by `*', `+', or `?'.  An atom
        /// <br/> 	followed by `*' matches a sequence of 0 or more matches of the atom.
        /// <br/> 	An atom followed by `+' matches a sequence of 1 or more matches of
        /// <br/> 	the atom.  An atom followed by `?' matches a match of the atom, or
        /// <br/> 	the null string.
        /// <br/> 
        /// <br/> 	An atom is a regular expression in parentheses (matching a match for
        /// <br/> 	the regular expression),  a range (see below),  `.'  (matching any
        /// <br/> 	single character),  `^' (matching the null string at the beginning
        /// <br/> 	of the input string),  `$' (matching the null string at the end of
        /// <br/> 	the input string),  a `\' followed by a single character (matching
        /// <br/> 	that character),  or a single character with no other significance
        /// <br/> 	(matching that character).
        /// <br/> 
        /// <br/> 	A range is a sequence of characters enclosed in `[]'.  It normally
        /// <br/> 	matches any single character from the sequence.  If the sequence
        /// <br/> 	begins with `^',  it matches any single character not from the rest
        /// <br/> 	of the sequence.  If two characters in the sequence are separated by
        /// <br/> 	`-', this is shorthand for the full list of ASCII characters between
        /// <br/> 	them (e.g. `[0-9]' matches any decimal digit).  To include a literal
        /// <br/> 	`]' in the sequence, make it the first character (following a possible
        /// <br/> 	`^').  To include a literal `-', make it the first or last character.
        /// <br/> 
        /// <br/> 	The symbols '\&lt;' and '\&gt;' respectively match the empty string at
        /// <br/> 	the beginning and end of a word.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the file line matches for the pattern "OpenConnection" in the 
        ///		file //depot/main/Program.cs with case-insensitive search:
        ///		<code> 
        ///		    GetFileLineMatchesCmdOptions opts = 
        ///		    new GetFileLineMatchesCmdOptions(GetFileLineMatchesCmdFlags.CaseInsensitive,
        ///		    0, 0, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/main/Program.cs"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileLineMatch&gt; target =
        ///			Repository.GetFileLineMatches(filespecs, "OpenConnection" opts);
        ///		</code>
        ///		To get the file line matches for the pattern "OpenConnection" in the 
        ///		file //depot/main/Program.cs showing 2 lines before and after the found
        ///		pattern and showing line numbers:
        ///		<code> 
        ///		    GetFileLineMatchesCmdOptions opts = 
        ///		    new GetFileLineMatchesCmdOptions(GetFileLineMatchesCmdFlags.IncludeLineNumbers,
        ///		    2, 2, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/main/Program.cs"), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileLineMatch&gt; target =
        ///			Repository.GetFileLineMatches(filespecs, "OpenConnection" opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetFileLineMatchesCmdFlags"/>
        public IList<FileLineMatch> GetFileLineMatches(IList<FileSpec> filespecs, string pattern, Options options)
        {
            P4.P4Command grepCmd = new P4Command(this, "grep", true, FileSpec.ToStrings(filespecs));
            options["-e"] = pattern;
            P4.P4CommandResult r = grepCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<FileLineMatch> value = new List<FileLineMatch>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                FileLineMatch val = new FileLineMatch();
                val.ParseGrepCmdTaggedData(obj);
                value.Add(val);
            }
            return value;
        }

        /// <summary>
        /// Get a list of submitted integrations for the passed-in file specs. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help integrated</b>
        /// <br/> 
        /// <br/>     integrated -- List integrations that have been submitted
        /// <br/> 
        /// <br/>     p4 integrated [-r] [-b branch] [file ...]
        /// <br/> 
        /// <br/> 	The p4 integrated command lists integrations that have been submitted.
        /// <br/> 	To list unresolved integrations, use 'p4 resolve -n'.  To list
        /// <br/> 	resolved but unsubmitted integrations, use 'p4 resolved'.
        /// <br/> 
        /// <br/> 	If the -b branch flag is specified, only files integrated from the
        /// <br/> 	source to target files in the branch view are listed.  Qualified
        /// <br/> 	files are listed, even if they were integrated without using the
        /// <br/> 	branch view.
        /// <br/> 
        /// <br/> 	The -r flag reverses the mappings in the branch view, swapping the
        /// <br/> 	target files and source files.  The -b branch flag is required.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the file intergration records for the path //depot/rel defined
        ///		by branch specification main_to_rel: 
        ///		<code> 
        ///         GetSubmittedIntegrationsCmdOptions opts =
        ///         new GetSubmittedIntegrationsCmdOptions(GetSubmittedIntegrationsCmdFlags.None,
        ///         "main_to_rel");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/rel/..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileIntegrationRecord&gt; target =
        ///			Repository.GetSubmittedIntegrations(filespecs, opts);
        ///		</code>
        ///		To get the file intergration records for the path //depot/main defined
        ///		by branch specification main_to_rel in reverse direction:         
        ///		<code> 
        ///         GetSubmittedIntegrationsCmdOptions opts =
        ///         new GetSubmittedIntegrationsCmdOptions(GetSubmittedIntegrationsCmdFlags.ReverseMappings,
        ///         "main_to_rel");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/rel/..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;FileIntegrationRecord&gt; target =
        ///			Repository.GetSubmittedIntegrations(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetSubmittedIntegrationsCmdFlags"/>
        public IList<FileIntegrationRecord> GetSubmittedIntegrations(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command integratedCmd = new P4Command(this, "integrated", true, FileSpec.ToEscapedStrings(filespecs));
            P4.P4CommandResult r = integratedCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<FileIntegrationRecord> value = new List<FileIntegrationRecord>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                FileIntegrationRecord val = new FileIntegrationRecord();
                val.ParseIntegratedCmdTaggedData(obj);
                value.Add(val);

            }
            return value;
        }

        /// <summary>
        /// Get a list of Perforce protection entries for the passed-in file specs 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help protects</b>
        /// <br/> 
        /// <br/>     protects -- Display protections defined for a specified user and path
        /// <br/> 
        /// <br/>     p4 protects [-a | -g group | -u user] [-h host] [-m] [file ...]
        /// <br/> 
        /// <br/> 	'p4 protects' displays the lines from the protections table that
        /// <br/> 	apply to the current user.  The protections table is managed using
        /// <br/> 	the 'p4 protect' command.
        /// <br/> 
        /// <br/> 	If the -a flag is specified, protection lines for all users are
        /// <br/> 	displayed.  If the -g group flag or -u user flag is specified,
        /// <br/> 	protection lines for that group or user are displayed.
        /// <br/> 
        /// <br/> 	If the -h host flag is specified, the protection lines that apply
        /// <br/> 	to the specified host (IP address) are displayed.
        /// <br/> 
        /// <br/> 	If the -m flag is given, a single word summary of the maximum
        /// <br/> 	access level is reported. Note that this summary does not take
        /// <br/> 	exclusions or the specified file path into account.
        /// <br/> 
        /// <br/> 	If the file argument is specified, protection lines that apply to
        /// <br/> 	the specified files are displayed.
        /// <br/> 
        /// <br/> 	The -a/-g/-u flags require 'super' access granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the protections for the user tim for the entire server:
        ///		<code> 
        ///         GetProtectionEntriesCmdOptions opts =
        ///         new GetProtectionEntriesCmdOptions(GetProtectionEntriesCmdFlags.None,
        ///         null, "tim", null);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;ProtectionEntry&gt; target =
        ///			Repository.GetProtectionEntries(filespecs, opts);
        ///		</code>
        ///		To get the protections summary for the group development tim for the entire server
        ///		when connecting from IP address 10.24.4.6:
        ///		<code> 
        ///         GetProtectionEntriesCmdOptions opts =
        ///         new GetProtectionEntriesCmdOptions(GetProtectionEntriesCmdFlags.AccessSummary,
        ///         "development", null, "10.24.4.6");
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;ProtectionEntry&gt; target =
        ///			Repository.GetProtectionEntries(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetProtectionEntriesCmdFlags"/>
        public IList<ProtectionEntry> GetProtectionEntries(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command protectsCmd = new P4Command(this, "protects", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = protectsCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<ProtectionEntry> value = new List<ProtectionEntry>();

            StringEnum<ProtectionMode> mode = null;
            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                string perm = obj["perm"];
                if (perm.StartsWith("="))
                {
                    switch (perm)
                    {
                        case "=open":
                            mode = ProtectionMode.OpenRights;
                            break;
                        case "=read":
                            mode = ProtectionMode.ReadRights;
                            break;
                        case "=branch":
                            mode = ProtectionMode.BranchRights;
                            break;
                        case "=write":
                            mode = ProtectionMode.WriteRights;
                            break;
                        default:
                            mode = null;
                            break;
                    }
                }
                else
                {
                    mode = obj["perm"];
                }
                StringEnum<EntryType> type = "User";
                if (obj.ContainsKey("isgroup"))
                {
                    type = "Group";
                }
                string name = obj["user"];
                string host = obj["host"];
                string path = obj["depotFile"];
                bool unmap = obj.ContainsKey("unmap");
                ProtectionEntry pte = new ProtectionEntry(mode, type, name, host, path, unmap);

                value.Add(pte);
            }
            return value;
        }

        public ProtectionMode GetMaxProtectionAccess(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command protectsCmd = new P4Command(this, "protects", true, FileSpec.ToStrings(filespecs));

            P4.P4CommandResult r = protectsCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return new ProtectionMode();
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return new ProtectionMode();
            }

            StringEnum<ProtectionMode> mode = null;
            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                if (obj.ContainsKey("permMax"))
                {
                    string perm = obj["permMax"];
                    if (perm.StartsWith("="))
                    {
                        switch (perm)
                        {
                            case "=open":
                                mode = ProtectionMode.OpenRights;
                                break;
                            case "=read":
                                mode = ProtectionMode.ReadRights;
                                break;
                            case "=branch":
                                mode = ProtectionMode.BranchRights;
                                break;
                            case "=write":
                                mode = ProtectionMode.WriteRights;
                                break;
                            default:
                                mode = null;
                                break;
                        }
                    }
                    else
                    {
                        mode = obj["permMax"];
                    }
                }
            }
            return mode;
        }

        /// <summary>
        /// List Perforce users assigned to review files. 
        /// </summary>    
        /// <param name="filespecs"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help reviews</b>
        /// <br/> 
        /// <br/>     reviews -- List the users who are subscribed to review files
        /// <br/> 
        /// <br/>     p4 reviews [-C client] [-c changelist#] [file ...]
        /// <br/> 
        /// <br/> 	'p4 reviews' lists all users who have subscribed to review the
        /// <br/> 	specified files.
        /// <br/> 
        /// <br/> 	The -c flag limits the files to the submitted changelist.
        /// <br/> 
        /// <br/> 	The -C flag limits the files to those opened in the specified clients
        /// <br/> 	workspace,  when used with the -c flag limits the workspace to files
        /// <br/> 	opened in the specified changelist.
        /// <br/> 
        /// <br/> 	To subscribe to review files, issue the 'p4 user' command and edit
        /// <br/> 	the 'Reviews field'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the list of users who are reviewing submits to //depot/main/src:
        ///		<code> 
        ///         GetReviewersCmdOptions opts =
        ///         new GetReviewersCmdOptions(GetReviewersCmdFlags.None, 0);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//depot/main/src/..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;User&gt; reviewers =
        ///			Repository.GetReviewers(filespecs, opts);
        ///		</code>
        ///		To get the list of users who are reviewing submitted changelist 83476:
        ///		<code> 
        ///         GetReviewersCmdOptions opts =
        ///         new GetReviewersCmdOptions(GetReviewersCmdFlags.None, 83476);
        ///         
        ///         FileSpec filespec =
        ///         new FileSpec(new DepotPath("//..."), null);
        ///         IList&lt;FileSpec&gt; filespecs = new List&lt;FileSpec&gt;();
        ///         filespecs.Add(filespec);
        ///         
        ///			IList&lt;User&gt; reviewers =
        ///			Repository.GetReviewers(filespecs, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetProtectionEntriesCmdFlags"/>
        public IList<User> GetReviewers(IList<FileSpec> filespecs, Options options)
        {
            P4.P4Command reviewsCmd = new P4Command(this, "reviews", true, FileSpec.ToStrings(filespecs));
            P4.P4CommandResult r = reviewsCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<User> value = new List<User>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                string id = obj["user"];
                string fullname = obj["name"];
                string password = string.Empty;
                string emailaddress = obj["email"];
                DateTime updated = DateTime.MinValue;
                DateTime accessed = DateTime.MinValue;
                string jobview = string.Empty;
                List<string> reviews = new List<string>();
                UserType type = UserType.Standard;
                FormSpec spec = new FormSpec(null, null, null, null, null, null, null, null, null);
                User user = new User(id, fullname, password, emailaddress, updated, accessed, jobview, "perforce", reviews, type, spec);
                value.Add(user);
            }
            return value;
        }

        /// <summary>
        /// Get a FormSpec of the specified form type. 
        /// </summary>    
        /// <param name="spectype"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help spec</b>
        /// <br/> 
        /// <br/>     spec -- Edit spec comments and formatting hints (unsupported)
        /// <br/> 
        /// <br/>     p4 spec [-d -i -o] type
        /// <br/> 
        /// <br/> 	Edit any type of specification: branch, change, client, depot,
        /// <br/> 	group, job, label, spec, stream, triggers, typemap, or user. Only
        /// <br/> 	the comments and the formatting hints can be changed. Any fields
        /// <br/> 	that you add during editing are discarded when the spec is saved.
        /// <br/> 
        /// <br/> 	'p4 jobspec' is equivalent to 'p4 spec job', and any custom spec
        /// <br/> 	(including the job spec) can be deleted with 'p4 spec -d type'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the FormSpec for changelist:
        ///		<code> 
        ///         Options ops = new Options();
        ///			ops["-o"] = null;
        ///         
        ///         FormSpec target = Repository.GetFormSpec(ops, "change");
        ///		</code>
        ///		To get the FormSpec for client:
        ///		<code> 
        ///         Options ops = new Options();
        ///			ops["-o"] = null;
        ///         
        ///         FormSpec target = Repository.GetFormSpec(ops, "clinet");
        ///		</code>
        ///		To get the FormSpec for user:
        ///		<code> 
        ///         Options ops = new Options();
        ///			ops["-o"] = null;
        ///         
        ///         FormSpec target = Repository.GetFormSpec(ops, "user");
        ///		</code>
        /// </example>
        public FormSpec GetFormSpec(Options options, string spectype)
        {
            StringList cmdArgs = new StringList();
            cmdArgs.Add(spectype);
            P4.P4Command specCmd = new P4Command(this, "spec", true, cmdArgs);
            P4.P4CommandResult r = specCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                FormSpec val = FormSpec.FromSpecCmdTaggedOutput(obj);

                return val;
            }

            return null;

        }

        /// <summary>
        /// Get the repository's trigger table. 
        /// </summary>    
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help triggers</b>
        /// <br/> 
        /// <br/>     triggers -- Modify list of server triggers
        /// <br/> 
        /// <br/>     p4 triggers
        /// <br/>     p4 triggers -o
        /// <br/>     p4 triggers -i
        /// <br/> 
        /// <br/> 	'p4 triggers' edits the table of triggers, which are used for
        /// <br/> 	change submission validation, form validation, external authentication,
        /// <br/> 	external job fix integration, external archive integration, and command
        /// <br/> 	policies.
        /// <br/> 
        /// <br/> 	Triggers are administrator-defined commands that the server runs
        /// <br/> 	to perform the following:
        /// <br/> 
        /// <br/> 	Validate changelist submissions.
        /// <br/> 
        /// <br/> 	    The server runs changelist triggers before the file transfer,
        /// <br/> 	    between file transfer and changelist commit, upon commit failure,
        /// <br/> 	    or after the commit.
        /// <br/> 
        /// <br/> 	Validate shelve operations.
        /// <br/> 
        /// <br/> 	    The server runs shelve triggers before files are shelved, after
        /// <br/> 	    files are shelved, or when shelved files have been discarded
        /// <br/> 	    (via shelve -d).
        /// <br/> 
        /// <br/> 	Manipulate and validate forms.
        /// <br/> 
        /// <br/> 	    The server runs form-validating triggers between generating
        /// <br/> 	    and outputting the form, between inputting and parsing the
        /// <br/> 	    form, between parsing and saving the form, or when deleting
        /// <br/> 	    the form.
        /// <br/> 
        /// <br/> 	Authenticate or change a user password.
        /// <br/> 
        /// <br/> 	    The server runs authentication triggers to either validate
        /// <br/> 	    a user password during login or when setting a new password.
        /// <br/> 
        /// <br/> 	Intercept job fix additions or deletions.
        /// <br/> 
        /// <br/> 	    The server run fix triggers prior to adding or deleting a fix
        /// <br/> 	    between a job and changelist.
        /// <br/> 
        /// <br/> 	Access external archive files.
        /// <br/> 
        /// <br/> 	    For files with the +X filetype modifier, the server runs an
        /// <br/> 	    archive trigger to read, write, or delete files in the archive.
        /// <br/> 
        /// <br/> 	Command execution policy.
        /// <br/> 
        /// <br/> 	    Command triggers can be specified to run before and after
        /// <br/> 	    processing of user requests.  Pre-execution triggers can
        /// <br/> 	    prevent the command from running.
        /// <br/> 
        /// <br/> 	The trigger form has a single entry 'Triggers:', followed by any
        /// <br/> 	number of trigger lines.  Each trigger line must be indented with
        /// <br/> 	spaces or tabs in the form. Triggers are executed in the order listed
        /// <br/> 	and if a trigger fails, subsequent triggers are not run.  A trigger
        /// <br/> 	succeeds if the executed command exits returning 0 and fails otherwise.
        /// <br/> 	Normally the failure of a trigger prevents the operation from
        /// <br/> 	completing, except for the commit triggers, which run after the
        /// <br/> 	operation is complete, or the change-failed trigger which is only
        /// <br/> 	advisory.
        /// <br/> 
        /// <br/> 	Each trigger line contains a trigger name, a trigger type, a depot
        /// <br/> 	file path pattern matching where the trigger will be executed, a
        /// <br/> 	command name or form type and a command to run.
        /// <br/> 
        /// <br/> 	Name:   The name of the trigger.  For change triggers, a run of the
        /// <br/> 		same trigger name on contiguous lines is treated as a single
        /// <br/> 		trigger so that multiple paths can be specified.  Only the
        /// <br/> 		command of the first such trigger line is used.
        /// <br/> 
        /// <br/> 	Type:	When the trigger is to execute:
        /// <br/> 
        /// <br/> 		archive:
        /// <br/> 		    Execute an archive trigger for the server to access
        /// <br/> 		    any file with the +X filetype modifier.
        /// <br/> 
        /// <br/> 		auth-check:
        /// <br/> 		service-check:
        /// <br/> 		    Execute an authentication check trigger to verify a
        /// <br/> 		    user's password against an external password manager
        /// <br/> 		    during login or when setting a new password.
        /// <br/> 
        /// <br/> 		auth-check-sso:
        /// <br/> 		    Facilitate a single sign-on user authentication. This
        /// <br/> 		    configuration requires two programs or scripts to run;
        /// <br/> 		    one on the client, the other on the server.
        /// <br/> 
        /// <br/> 		    client:
        /// <br/> 		        Set the environment variable 'P4LOGINSSO' to point to
        /// <br/> 		        a script that can be executed to obtain the user's
        /// <br/> 		        credentials or other information that the server
        /// <br/> 		        trigger can verify.  The client-side script must
        /// <br/> 		        write the message to the standard output
        /// <br/> 		        (max length 128K).
        /// <br/> 
        /// <br/> 		        Example:  P4LOGINSSO=/Users/joe/bin/runsso
        /// <br/> 
        /// <br/> 		        The two variables available to this trigger are
        /// <br/> 		        %P4PORT% and %serverAddress%.  The distinction is
        /// <br/> 		        that serverAddress is the address of the target server
        /// <br/> 		        and the P4PORT is what the client is connecting to,
        /// <br/> 		        which might be an intermediate like a Perforce Proxy.
        /// <br/> 
        /// <br/> 		        These variables can be passed to the client script by
        /// <br/> 		        appending them to the client command string, as in:
        /// <br/> 
        /// <br/> 		        P4LOGINSSO="/Users/joe/bin/runsso %serverAddress%"
        /// <br/> 
        /// <br/> 		    server:
        /// <br/> 		        Execute an authentication (sso) trigger that gets
        /// <br/> 		        this message from the standard input and returns an
        /// <br/> 		        exit status of 0 (for verified) or otherwise failed.
        /// <br/> 
        /// <br/> 		        Example:
        /// <br/> 		            sso auth-check-sso auth "/secure/verify %user%"
        /// <br/> 
        /// <br/> 		    The user must issue the 'p4 login' command, but no
        /// <br/> 		    password prompting is invoked.  If the server
        /// <br/> 		    determines that the user is valid, they are issued a
        /// <br/> 		    Perforce ticket just as if they had logged in with a
        /// <br/> 		    password.
        /// <br/> 
        /// <br/> 		    Pre-2007.2 clients cannot run a client-side single
        /// <br/> 		    sign-on.  Specifying an 'auth-check' trigger as a backup
        /// <br/> 		    for a user to gain access will prompt the user for a
        /// <br/> 		    password if it's an older client or P4LOGINSSO has not
        /// <br/> 		    been configured.
        /// <br/> 
        /// <br/> 		    Unlike passwords which are encrypted, the sso message is
        /// <br/> 		    sent to the server in clear text.
        /// <br/> 
        /// <br/> 		auth-set:
        /// <br/> 		    Execute an authentication set trigger to send a new
        /// <br/> 		    password to an external password manager.
        /// <br/> 
        /// <br/> 		change-submit:
        /// <br/> 		    Execute pre-submit trigger after changelist has been
        /// <br/> 		    created and files locked but prior to file transfer.
        /// <br/> 
        /// <br/> 		change-content:
        /// <br/> 		    Execute mid-submit trigger after file transfer but prior
        /// <br/> 		    to commit.  Files can be accessed by the 'p4 diff2',
        /// <br/> 		    'p4 files', 'p4 fstat', and 'p4 print' commands using
        /// <br/> 		    the revision specification '@=change', where 'change' is
        /// <br/> 		    the pending changelist number passed as %changelist%.
        /// <br/> 
        /// <br/> 		change-commit:
        /// <br/> 		    Execute post-submit trigger after changelist commit.
        /// <br/> 
        /// <br/> 		change-failed:
        /// <br/> 		    Executes only if the changelist commit failed.
        /// <br/> 		    Note that this trigger only fires on errors
        /// <br/> 		    occurring after a commit process has started. It does
        /// <br/> 		    not fire for early usage errors, or due to errors from
        /// <br/> 		    the submit form. In short, if a change-* trigger
        /// <br/> 		    could have run, then the change-failed trigger
        /// <br/> 		    will fire if that commit fails.
        /// <br/> 
        /// <br/> 		command:
        /// <br/> 		    Execute pre/post trigger when users run commands.
        /// <br/> 
        /// <br/> 		edge-submit:
        /// <br/> 		    Execute pre-submit trigger on Edge Server after changelist
        /// <br/> 		    has been created but prior to file transfer.
        /// <br/> 
        /// <br/> 		edge-content:
        /// <br/> 		    Execute mid-submit trigger on Edge Server after file
        /// <br/> 		    transfer but prior to beginning submit on Commit Server.
        /// <br/> 
        /// <br/> 		fix-add:
        /// <br/> 		    Execute fix trigger prior to adding a fix.  The special
        /// <br/> 		    variable %jobs% is available for expansion and must be
        /// <br/> 		    the last argument to the trigger as it expands to one
        /// <br/> 		    argument for each job listed on the 'p4 fix' command.
        /// <br/> 
        /// <br/> 		fix-delete:
        /// <br/> 		    Execute fix trigger prior to deleting a fix.  The special
        /// <br/> 		    variable %jobs% is available for expansion and must be
        /// <br/> 		    the last argument to the trigger as it expands to one
        /// <br/> 		    argument for each job listed on the 'p4 fix -d' command.
        /// <br/> 
        /// <br/> 		form-out:
        /// <br/> 		    Execute form trigger on generation of form.	 Trigger may
        /// <br/> 		    modify form.
        /// <br/> 
        /// <br/> 		form-in:
        /// <br/> 		    Execute form trigger on input of form before its contents
        /// <br/> 		    are parsed and validated.  Trigger may modify form.
        /// <br/> 
        /// <br/> 		form-save:
        /// <br/> 		    Execute form trigger prior to save of form after its
        /// <br/> 		    contents are parsed.
        /// <br/> 
        /// <br/> 		form-commit:
        /// <br/> 		    Execute form trigger after it has been committed, allowing
        /// <br/> 		    access to automatically generated fields (jobname, dates
        /// <br/> 		    etc).  It cannot modify the form.  This trigger for job
        /// <br/> 		    forms is run by 'p4 job' and 'p4 fix' (after the status
        /// <br/> 		    is updated), 'p4 change' (if the job is added or deleted)
        /// <br/> 		    and 'p4 submit' (if the job is associated with the change).
        /// <br/> 		    The 'form-commit' trigger has access to the new job name
        /// <br/> 		    created with 'p4 job', while the 'form-in' and 'form-save'
        /// <br/> 		    triggers are run before the job name is created.  The
        /// <br/> 		    special variable %action% is available on the job
        /// <br/> 		    'form-commit' trigger command line, and is expanded when
        /// <br/> 		    the job is modified by a fix.
        /// <br/> 
        /// <br/> 		form-delete:
        /// <br/> 		    Execute form trigger prior to delete of form after its
        /// <br/> 		    contents are parsed.
        /// <br/> 
        /// <br/> 		shelve-submit:
        /// <br/> 		    Execute pre-shelve trigger after changelist has been
        /// <br/> 		    created but prior to file transfer.
        /// <br/> 
        /// <br/> 		shelve-commit:
        /// <br/> 		    Execute post-shelve trigger after files are shelved.
        /// <br/> 
        /// <br/> 		shelve-delete:
        /// <br/> 		    Execute shelve trigger prior to discarding shelved files.
        /// <br/> 
        /// <br/> 	Path:   For change and submit triggers, a file pattern to match files
        /// <br/> 		in the changelist.  This file pattern can be an exclusion
        /// <br/> 		mapping (-pattern), to exclude files.  For form triggers, the
        /// <br/> 		name of the form (branch, client, etc).  For fix triggers
        /// <br/> 		'fix' is required as the path value.  For authentication
        /// <br/> 		triggers, 'auth' is required as the path value. For archive
        /// <br/> 		triggers, a file pattern to match the name of the file being
        /// <br/> 		accessed in the archive.  Note that, due to lazy copying when
        /// <br/> 		branching files, the name of the file in the archive can not
        /// <br/> 		be the same as the name of the file in the depot.  For command
        /// <br/> 		triggers, use the name of the command to match, e.g.
        /// <br/> 		'pre-user-$cmd' or a regular expression, e.g.
        /// <br/> 		'(pre|post)-user-add'.
        /// <br/> 
        /// <br/> 	Command: The OS command to run for validation.  If the command
        /// <br/> 		contains spaces, enclose it in double quotes.  The
        /// <br/> 		following variables are expanded in the command string.
        /// <br/> 		Use of the triggers.io configurable with a value greater than
        /// <br/> 		zero is recommended, as some vars may be empty or contain
        /// <br/> 		shell metacharacters.
        /// <br/> 
        /// <br/> 		    %//depot/trigger.exe% -- depot paths within %vars%
        /// <br/> 		    are filled with the path to a temporary file containing
        /// <br/> 		    the referenced file's contents.  Only standard and stream
        /// <br/> 		    depot files whose contents is available are valid.
        /// <br/> 		    %argc% -- number of command arguments
        /// <br/> 		    %args% -- command argument string
        /// <br/> 		    %argsQuoted% -- command argument string, CSV delimited
        /// <br/> 		    %client% -- the client issuing the command
        /// <br/> 		    %clientcwd% -- client current working directory
        /// <br/> 		    %clienthost% -- the hostname of the client
        /// <br/> 		    %clientip% -- the IP address of the client
        /// <br/> 		    %clientprog% -- the program name of the client
        /// <br/> 		    %clientversion% -- the version of the client
        /// <br/> 		    %command% -- name of command being run
        /// <br/> 		    %groups% -- list of groups user is a member of
        /// <br/> 		    %intermediateService% -- presence of a Broker/Proxy/etc
        /// <br/> 		    %maxErrorSeverity% -- highest error seen for this cmd
        /// <br/> 		    %maxErrorText% -- text and errno for highest error
        /// <br/> 		    %maxLockTime% -- user-specified override of group value
        /// <br/> 		    %maxResults% -- user-specified override of group value
        /// <br/> 		    %maxScanRows% -- user-specified override of group value
        /// <br/> 		    %quote% -- double quote character
        /// <br/> 		    %serverhost% -- the hostname of the server
        /// <br/> 		    %serverid% -- the value of the server's server.id
        /// <br/> 		    %serverip% -- the IP address of the server
        /// <br/> 		    %servername% -- the value of the server's $P4NAME
        /// <br/> 		    %serverpid% -- the PID of the server
        /// <br/> 		    %serverport% -- the IP address:port of the server
        /// <br/> 		                      preceded by the transport prefix,
        /// <br/> 		                      if needed (i.e. P4PORT)
        /// <br/> 		    %serverroot% -- the value of the server's $P4ROOT
        /// <br/> 		    %serverservices% -- the services provided by the server
        /// <br/> 		    %serverVersion% -- the server's version string
        /// <br/> 		    %terminated% -- if the command was forced to quit early
        /// <br/> 		    %termReason% -- reason for early termination
        /// <br/> 		    %triggerMeta_action% -- command to execute by trigger
        /// <br/> 		    %triggerMeta_depotFile% -- third field in trigger def.
        /// <br/> 		    %triggerMeta_name% -- name from trigger definition
        /// <br/> 		    %triggerMeta_trigger% -- second field in trigger definition
        /// <br/> 		    %user% -- the user issuing the command
        /// <br/> 
        /// <br/> 		    %changelist% -- the changelist being submitted
        /// <br/> 		    %changeroot% -- the root path of files submitted
        /// <br/> 		    %oldchangelist% -- the pre-commit changelist number
        /// <br/> 
        /// <br/> 			(More information can be gathered about the
        /// <br/> 			changelist being submitted by running
        /// <br/> 			'p4 describe %changelist%'.)
        /// <br/> 
        /// <br/> 		    %formfile% -- path to temp file containing form
        /// <br/> 		    %formname% -- the form's name (branch name, etc)
        /// <br/> 		    %formtype% -- the type of form (branch, etc)
        /// <br/> 		    %action% -- added/deleted/submitted on job form-commit
        /// <br/> 
        /// <br/> 		    %jobs% -- list of job names for fix triggers
        /// <br/> 
        /// <br/> 		    %op% -- read/write/delete for archive access
        /// <br/> 		    %file% -- name of archive file
        /// <br/> 		    %rev% -- revision of archive file
        /// <br/> 
        /// <br/> 		    If the command was sent via a proxy, broker, or replica:
        /// <br/> 		    %peerhost% -- the hostname of the proxy/broker/replica
        /// <br/> 		    %peerip% -- the IP address of the proxy/broker/replica
        /// <br/> 		    If the command was sent directly, %peerhost% and
        /// <br/> 		    %peerip% match %clienthost% and %clientip%.
        /// <br/> 
        /// <br/> 		    For a change-* trigger in a distributed installation,
        /// <br/> 		    %submitserverid% -- the server.id where submit was run
        /// <br/> 
        /// <br/> 		    For a post-rmt-Push trigger:
        /// <br/> 		    %firstPushedChange% -- first new changelist number
        /// <br/> 		    %lastPushedChange% -- last new changelist number
        /// <br/> 
        /// <br/> 		    Note that not all variables are available for every
        /// <br/> 		    trigger type.  E.g. argc and argv only show up for
        /// <br/> 		    pre-user-$cmd and change-submit (and so on), but not for
        /// <br/> 		    post-user-$cmd or change-commit.
        /// <br/> 
        /// <br/> 		The command's standard input depends on the value of the
        /// <br/> 		triggers.io configurable.  When it is set to zero, stdin is
        /// <br/> 		empty for change, shelve, fix and command triggers, it
        /// <br/> 		is the file content for the archive, and password for auth
        /// <br/> 		triggers.  When triggers.io is set to 1, stdin is a textual
        /// <br/> 		dictionary containing connection information that the trigger
        /// <br/> 		must read (with the exception of archive/auth triggers,
        /// <br/> 		which behave the same as when triggers.io=0.)
        /// <br/> 
        /// <br/> 		If the command fails, the command's standard output (not
        /// <br/> 		error output) is sent to the client as the text of a trigger
        /// <br/> 		failure error message.
        /// <br/> 
        /// <br/> 		If the command succeeds, the command's standard output is
        /// <br/> 		sent as an unadorned message to the client for all triggers
        /// <br/> 		except archive triggers; for archive triggers, the command's
        /// <br/> 		standard output is the file content.
        /// <br/> 
        /// <br/> 	The -o flag writes the trigger table to the standard output.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	The -i flag reads the trigger table from the standard input.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	'p4 triggers' requires 'super' access granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the trigger table:
        ///		<code> 
        ///         GetTriggerTableCmdOptions opts =
        ///         new GetTriggerTableCmdOptions(GetTriggerTableCmdFlags.Output);
        ///         
        ///			IList&lt;Trigger&gt; target = Repository.GetTriggerTable(opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetTriggerTableCmdFlags"/>
        public IList<Trigger> GetTriggerTable(Options options)
        {
            P4.P4Command triggersCmd = new P4Command(this, "triggers", true);
            P4.P4CommandResult r = triggersCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<Trigger> value = new List<Trigger>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                System.Text.StringBuilder sb = new StringBuilder();
                foreach (KeyValuePair<string, string> key in obj)
                {
                    sb.Remove(0, sb.Length);
                    sb.AppendLine((string.Format("{0} {1}", key.Key.ToString(), key.Value)));
                    string line = sb.ToString();
                    if (line.StartsWith("Triggers"))
                    {
                        line = line.Trim();
                        string[] entries = line.Split(' ');
                        string name = entries[1];
                        string ent = entries[2];
                        ent = ent.Replace("-", "");
                        StringEnum<TriggerType> type = ent;
                        string path = entries[3];
                        string command = entries[4] + " " + entries[5];
                        string ord = entries[0];
                        ord = ord.Remove(0, 8);
                        int order = 0;
                        order = Convert.ToInt16(ord);
                        Trigger trig = new Trigger(name, order, type, path, command);
                        value.Add(trig);
                    }
                }
            }
            return value;
        }

        /// <summary>
        /// Get the repository's type map. 
        /// </summary>    
        /// <returns></returns>
        /// <remarks>
        /// runs the command p4 typemap -o
        /// </remarks>
        /// <remarks>
        /// <br/><b>p4 help typemap</b>
        /// <br/> 
        /// <br/>     typemap -- Edit the filename-to-filetype mapping table
        /// <br/> 
        /// <br/>     p4 typemap
        /// <br/>     p4 typemap -o
        /// <br/>     p4 typemap -i
        /// <br/> 
        /// <br/> 	'p4 typemap' edits a name-to-type mapping table for 'p4 add', which
        /// <br/> 	uses the table to assign a file's filetype based on its name.
        /// <br/> 
        /// <br/> 	The typemap form has a single field, 'TypeMap', followed by any
        /// <br/> 	number of typemap lines.  Each typemap line contains a filetype
        /// <br/> 	and a depot file path pattern:
        /// <br/> 
        /// <br/> 	Filetype:   See 'p4 help filetypes' for a list of valid filetypes.
        /// <br/> 
        /// <br/> 	Path:       Names to be mapped to the filetype.  The mapping is
        /// <br/> 		    a file pattern in depot syntax.  When a user adds a file
        /// <br/> 		    matching this pattern, its default filetype is the
        /// <br/> 		    file type specified in the table.  To exclude files from
        /// <br/> 		    the typemap, use exclusionary (-pattern) mappings.
        /// <br/> 		    To match all files anywhere in the depot hierarchy,
        /// <br/> 		    the pattern must begin with '//...'.  To match files
        /// <br/> 		    with a specified suffix, use '//.../*.suffix' or
        /// <br/> 		    use '//....suffix' (four dots).
        /// <br/> 
        /// <br/> 	Later entries override earlier entries. If no matching entry is found
        /// <br/> 	in the table, 'p4 add' determines the filetype by examining the file's
        /// <br/> 	contents and execution permission bits.
        /// <br/> 
        /// <br/> 	The -o flag writes the typemap table to standard output. The user's
        /// <br/> 	editor is not invoked.
        /// <br/> 
        /// <br/> 	The -i flag reads the typemap table from standard input. The user's
        /// <br/> 	editor is not invoked.
        /// <br/> 
        /// <br/> 	'p4 typemap' requires 'admin' access, which is granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the typemap table:
        ///		<code> 
        ///			IList&lt;TypeMapEntry&gt; target = Repository.GetTypeMap();
        ///		</code>
        /// </example>
        public IList<TypeMapEntry> GetTypeMap()
        {
            P4.P4Command typemapCmd = new P4Command(this, "typemap", true, "-o");
            P4.P4CommandResult r = typemapCmd.Run();
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<TypeMapEntry> value = new List<TypeMapEntry>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                int ord = 0;
                string key = String.Format("TypeMap{0}", ord);

                while (obj.ContainsKey(key))
                {
                    value.Add(new TypeMapEntry(obj[key]));
                    ord++;
                    key = String.Format("TypeMap{0}", ord);
                }
                return value;
            }
            return value;
        }

        /// <summary>
        /// Get the repository's protection table. 
        /// </summary>    
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help protect</b>
        /// <br/> 
        /// <br/>     protect -- Modify protections in the server namespace
        /// <br/> 
        /// <br/>     p4 protect
        /// <br/>     p4 protect -o
        /// <br/>     p4 protect -i
        /// <br/> 
        /// <br/> 	'p4 protect' edits the protections table in a text form.
        /// <br/> 
        /// <br/> 	Each line in the table contains a protection mode, a group/user
        /// <br/> 	indicator, the group/user name, client host ID and a depot file
        /// <br/> 	path pattern. Users receive the highest privilege that is granted
        /// <br/> 	on any line.
        /// <br/> 
        /// <br/> 	Note: remote depots are accessed using the pseudo-user 'remote'.
        /// <br/> 	To control access from other servers that define your server as
        /// <br/> 	a remote server, grant appropriate permissions to the 'remote' user.
        /// <br/> 
        /// <br/> 	     Mode:   The permission level or right being granted or denied.
        /// <br/> 		     Each permission level includes all the permissions above
        /// <br/> 		     it, except for 'review'. Each permission only includes
        /// <br/> 		     the specific right and no lesser rights.  This approach
        /// <br/> 		     enables you to deny individual rights without having to
        /// <br/> 		     re-grant lesser rights. Modes prefixed by '=' are rights.
        /// <br/> 		     All other modes are permission levels.
        /// <br/> 
        /// <br/>       Valid modes are:
        /// <br/> 
        /// <br/> 		     list   - users can see names but not contents of files;
        /// <br/> 			      users can see all non-file related metadata
        /// <br/> 			      (clients, users, changelists, jobs, etc.)
        /// <br/> 
        /// <br/> 		     read   - users can sync, diff, and print files
        /// <br/> 
        /// <br/> 		     open   - users can open files (add, edit. delete,
        /// <br/> 			      integrate)
        /// <br/> 
        /// <br/> 		     write  - users can submit open files
        /// <br/> 
        /// <br/> 		     admin  - permits those administrative commands and
        /// <br/> 			      command options that don't affect the server's
        /// <br/> 			      security.
        /// <br/> 
        /// <br/> 		     super  - access to all commands and command options.
        /// <br/> 
        /// <br/> 		     review - permits access to the 'p4 review' command;
        /// <br/> 			      implies read access
        /// <br/> 
        /// <br/> 		     =read  - if this right is denied, users can't sync,
        /// <br/> 			      diff, or print files
        /// <br/> 
        /// <br/> 		     =branch - if this right is denied, users are not
        /// <br/> 			       permitted to use files as a source
        /// <br/> 			       for 'p4 integrate'
        /// <br/> 
        /// <br/> 		     =open   = if this right is denied, users cannot open
        /// <br/> 			       files (add, edit, delete, integrate)
        /// <br/> 
        /// <br/> 		     =write  = if this right is denied, users cannot submit
        /// <br/> 			       open files
        /// <br/> 
        /// <br/> 	     Group/User indicator: specifies the grantee is a group or user.
        /// <br/> 
        /// <br/> 	     Name:   A Perforce group or user name; can include wildcards.
        /// <br/> 
        /// <br/> 	     Host:   The IP address of a client host; can include wildcards.
        /// <br/> 
        /// <br/> 	             The server can distinguish connections coming from a
        /// <br/> 	             proxy, broker, or replica. The server prepends the string
        /// <br/> 	             'proxy-' to the IP address of the true client of such
        /// <br/> 	             a connection when the server enforces the protections.
        /// <br/> 
        /// <br/> 	             Specify the 'proxy-' prefix for the IP address in the
        /// <br/> 	             Host: field in the protections table to indicate the
        /// <br/> 	             protections that should thus apply.
        /// <br/> 
        /// <br/> 	             For example, 'proxy-*' applies to all connections from
        /// <br/> 	             all proxies, brokers, and replicas, while
        /// <br/> 	             'proxy-10.0.0.5' identifies a client machine with an IP
        /// <br/> 	             address of 10.0.0.5 which is connecting to p4d through
        /// <br/> 	             a proxy, broker, or replica.
        /// <br/> 
        /// <br/> 	             If you wish to write a single set of protections entries
        /// <br/> 	             which apply both to directly-connected clients as well
        /// <br/> 	             as to those which connect via a proxy, broker, or
        /// <br/> 	             replica, you can omit the 'proxy-' prefix and also set
        /// <br/> 	             dm.proxy.protects=0. In this case, the 'proxy-' prefix
        /// <br/> 	             is not prepended to the IP address of connections which
        /// <br/> 	             are made via a proxy, replica or broker.  Note that in
        /// <br/> 	             this scenario, all intermediate proxies, brokers, and
        /// <br/> 	             replicas should be at release 2012.1 or higher.
        /// <br/> 
        /// <br/> 	     Path:   The part of the depot to which access is being granted
        /// <br/> 	             or denied.  To deny access to a depot path, preface the
        /// <br/> 	             path with a "-" character. These exclusionary mappings
        /// <br/> 	             apply to all access levels, even if only one access
        /// <br/> 	             level is specified in the first field.
        /// <br/> 
        /// <br/> 	The -o flag writes the protection table	to the standard output.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	The -i flag reads the protection table from the standard input.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	After protections are defined, 'p4 protect' requires 'super'
        /// <br/> 	access.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the protections table:
        ///		<code> 
        ///         GetProtectionTableCmdOptions opts =
        ///         new GetProtectionTableCmdOptions(GetProtectionTableCmdFlags.Output);
        ///         
        ///			IList&lt;ProtectionEntry&gt; target = Repository.GetProtectionTable(opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetProtectionTableCmdFlags"/>
        [Obsolete("Use GetProtectionTable()")]
        public IList<ProtectionEntry> GetProtectionTable(Options options)
        {
           return GetProtectionTable();
        }

        /// <summary>
        /// Get the repository's protection table. 
        /// </summary>    
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help protect</b>
        /// <br/> 
        /// <br/>     protect -- Modify protections in the server namespace
        /// <br/> 
        /// <br/>     p4 protect
        /// <br/>     p4 protect -o
        /// <br/>     p4 protect -i
        /// <br/> 
        /// <br/> 	'p4 protect' edits the protections table in a text form.
        /// <br/> 
        /// <br/> 	Each line in the table contains a protection mode, a group/user
        /// <br/> 	indicator, the group/user name, client host ID and a depot file
        /// <br/> 	path pattern. Users receive the highest privilege that is granted
        /// <br/> 	on any line.
        /// <br/> 
        /// <br/> 	Note: remote depots are accessed using the pseudo-user 'remote'.
        /// <br/> 	To control access from other servers that define your server as
        /// <br/> 	a remote server, grant appropriate permissions to the 'remote' user.
        /// <br/> 
        /// <br/> 	     Mode:   The permission level or right being granted or denied.
        /// <br/> 		     Each permission level includes all the permissions above
        /// <br/> 		     it, except for 'review'. Each permission only includes
        /// <br/> 		     the specific right and no lesser rights.  This approach
        /// <br/> 		     enables you to deny individual rights without having to
        /// <br/> 		     re-grant lesser rights. Modes prefixed by '=' are rights.
        /// <br/> 		     All other modes are permission levels.
        /// <br/> 
        /// <br/>       Valid modes are:
        /// <br/> 
        /// <br/> 		     list   - users can see names but not contents of files;
        /// <br/> 			      users can see all non-file related metadata
        /// <br/> 			      (clients, users, changelists, jobs, etc.)
        /// <br/> 
        /// <br/> 		     read   - users can sync, diff, and print files
        /// <br/> 
        /// <br/> 		     open   - users can open files (add, edit. delete,
        /// <br/> 			      integrate)
        /// <br/> 
        /// <br/> 		     write  - users can submit open files
        /// <br/> 
        /// <br/> 		     admin  - permits those administrative commands and
        /// <br/> 			      command options that don't affect the server's
        /// <br/> 			      security.
        /// <br/> 
        /// <br/> 		     super  - access to all commands and command options.
        /// <br/> 
        /// <br/> 		     review - permits access to the 'p4 review' command;
        /// <br/> 			      implies read access
        /// <br/> 
        /// <br/> 		     =read  - if this right is denied, users can't sync,
        /// <br/> 			      diff, or print files
        /// <br/> 
        /// <br/> 		     =branch - if this right is denied, users are not
        /// <br/> 			       permitted to use files as a source
        /// <br/> 			       for 'p4 integrate'
        /// <br/> 
        /// <br/> 		     =open   = if this right is denied, users cannot open
        /// <br/> 			       files (add, edit, delete, integrate)
        /// <br/> 
        /// <br/> 		     =write  = if this right is denied, users cannot submit
        /// <br/> 			       open files
        /// <br/> 
        /// <br/> 	     Group/User indicator: specifies the grantee is a group or user.
        /// <br/> 
        /// <br/> 	     Name:   A Perforce group or user name; can include wildcards.
        /// <br/> 
        /// <br/> 	     Host:   The IP address of a client host; can include wildcards.
        /// <br/> 
        /// <br/> 	             The server can distinguish connections coming from a
        /// <br/> 	             proxy, broker, or replica. The server prepends the string
        /// <br/> 	             'proxy-' to the IP address of the true client of such
        /// <br/> 	             a connection when the server enforces the protections.
        /// <br/> 
        /// <br/> 	             Specify the 'proxy-' prefix for the IP address in the
        /// <br/> 	             Host: field in the protections table to indicate the
        /// <br/> 	             protections that should thus apply.
        /// <br/> 
        /// <br/> 	             For example, 'proxy-*' applies to all connections from
        /// <br/> 	             all proxies, brokers, and replicas, while
        /// <br/> 	             'proxy-10.0.0.5' identifies a client machine with an IP
        /// <br/> 	             address of 10.0.0.5 which is connecting to p4d through
        /// <br/> 	             a proxy, broker, or replica.
        /// <br/> 
        /// <br/> 	             If you wish to write a single set of protections entries
        /// <br/> 	             which apply both to directly-connected clients as well
        /// <br/> 	             as to those which connect via a proxy, broker, or
        /// <br/> 	             replica, you can omit the 'proxy-' prefix and also set
        /// <br/> 	             dm.proxy.protects=0. In this case, the 'proxy-' prefix
        /// <br/> 	             is not prepended to the IP address of connections which
        /// <br/> 	             are made via a proxy, replica or broker.  Note that in
        /// <br/> 	             this scenario, all intermediate proxies, brokers, and
        /// <br/> 	             replicas should be at release 2012.1 or higher.
        /// <br/> 
        /// <br/> 	     Path:   The part of the depot to which access is being granted
        /// <br/> 	             or denied.  To deny access to a depot path, preface the
        /// <br/> 	             path with a "-" character. These exclusionary mappings
        /// <br/> 	             apply to all access levels, even if only one access
        /// <br/> 	             level is specified in the first field.
        /// <br/> 
        /// <br/> 	The -o flag writes the protection table	to the standard output.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	The -i flag reads the protection table from the standard input.
        /// <br/> 	The user's editor is not invoked.
        /// <br/> 
        /// <br/> 	After protections are defined, 'p4 protect' requires 'super'
        /// <br/> 	access.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the protections table:
        ///		<code> 
        ///			IList&lt;ProtectionEntry&gt; target = Repository.GetProtectionTable(opts);
        ///		</code>
        /// </example>
        /// <seealso cref="GetProtectionTableCmdFlags"/>
        public IList<ProtectionEntry> GetProtectionTable()
        {
            GetProtectionTableCmdOptions options = new GetProtectionTableCmdOptions(GetProtectionTableCmdFlags.Output);
           
            P4.P4Command protectCmd = new P4Command(this, "protect", true);
            P4.P4CommandResult r = protectCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            List<ProtectionEntry> value = new List<ProtectionEntry>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)

            {
                System.Text.StringBuilder sb = new StringBuilder();
                foreach (KeyValuePair<string, string> key in obj)
                {
                    sb.Remove(0, sb.Length);
                    sb.AppendLine((string.Format("{0} {1}", key.Key.ToString(), key.Value)));
                    string line = sb.ToString();
                    if (line.StartsWith("Protections"))
                    {
                        line = line.Trim();
                        string[] entries = line.Split(' ');
                        StringEnum<ProtectionMode> mode = entries[1];
                        StringEnum<EntryType> type = entries[2];
                        string grouporusername = entries[3];
                        string host = entries[4];
                        string path = entries[5];
                        bool unmap = path.StartsWith("-") ? true : false;
                        ProtectionEntry pe = new ProtectionEntry(mode, type, grouporusername, host, path, unmap);
                        value.Add(pe);
                    }
                }
            }
            return value;
        }

        /// <summary>
        /// Get the Perforce counters for this repository. 
        /// </summary>    
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help counters</b>
        /// <br/> 
        /// <br/>     counters -- Display list of known counters
        /// <br/> 
        /// <br/>     p4 counters [-e nameFilter -m max]
        /// <br/> 
        /// <br/> 	Lists the counters in use by the server.  The server
        /// <br/> 	uses the following counters directly:
        /// <br/> 
        /// <br/> 	    change           Current change number
        /// <br/> 	    job              Current job number
        /// <br/> 	    journal          Current journal number
        /// <br/> 	    lastCheckpointAction Data about the last complete checkpoint
        /// <br/> 	    logger           Event log index used by 'p4 logger'
        /// <br/> 	    traits           Internal trait lot number used by 'p4 attribute'
        /// <br/> 	    upgrade          Server database upgrade level
        /// <br/> 
        /// <br/> 	The -e nameFilter flag lists counters with a name that matches
        /// <br/> 	the nameFilter pattern, for example: -e 'mycounter-*'.
        /// <br/> 
        /// <br/> 	The -m max flag limits the output to the first 'max' counters.
        /// <br/> 
        /// <br/> 	The names 'minClient', 'minClientMessage', 'monitor',
        /// <br/> 	'security', 'masterGenNumber', and 'unicode' are reserved names:
        /// <br/> 	do not use them as ordinary counters.
        /// <br/> 
        /// <br/> 	For general-purpose server configuration, see 'p4 help configure'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the counters on the server:
        ///		<code> 
        ///			IList&lt;Counter&gt; target = Repository.GetCounters(null);
        ///		</code>
        ///		To get the counters on the server that start with the name "build_":
        ///		<code> 
        ///         Options opts = new Options();
        ///         opts["-e"] = "build_*";
        ///			IList&lt;Counter&gt; target = Repository.GetCounters(opts);
        ///		</code>
        /// </example>
        public IList<Counter> GetCounters(Options options)
        {
            P4.P4Command countersCmd = new P4Command(this, "counters", true);
            P4.P4CommandResult r = countersCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            IList<Counter> val = new List<Counter>();

            foreach (P4.TaggedObject obj in r.TaggedOutput)
            {
                string name = obj["counter"];
                string value = obj["value"];

                Counter counter = new Counter(name, value);

                val.Add(counter);
            }
            return val;
        }


        /// <summary>
        /// Get a named Perforce counter value from the repository. 
        /// </summary>    
        /// <param name="name"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help counter</b>
        /// <br/> 
        /// <br/>      counter -- Display, set, or delete a counter
        /// <br/> 
        /// <br/>      p4 counter name
        /// <br/>      p4 counter [-f] name value
        /// <br/>      p4 counter [-f] -d name
        /// <br/>      p4 counter [-f] -i name
        /// <br/>      p4 counter [-f] -m [ pair list ]
        /// <br/> 
        /// <br/> 	The first form displays the value of the specified counter.
        /// <br/> 
        /// <br/> 	The second form sets the counter to the specified value.
        /// <br/> 
        /// <br/> 	The third form deletes the counter.  This option usually has the
        /// <br/> 	same effect as setting the counter to 0.
        /// <br/> 
        /// <br/> 	The -f flag sets or deletes counters used by Perforce,  which are
        /// <br/> 	listed by 'p4 help counters'. Important: Never set the 'change'
        /// <br/> 	counter to a value that is lower than its current value.
        /// <br/> 
        /// <br/> 	The -i flag increments a counter by 1 and returns the new value.
        /// <br/> 	This option is used instead of a value argument and can only be
        /// <br/> 	used with numeric counters.
        /// <br/> 
        /// <br/> 	The fifth form allows multiple operations in one command.
        /// <br/> 	With this, the list is pairs of arguments.  Each pair is either
        /// <br/> 	counter value or '-' counter.  To set a counter use a name and value.
        /// <br/> 	To delete a counter use a '-' followed by the name.
        /// <br/> 
        /// <br/> 	Counters can be assigned textual values as well as numeric ones, 
        /// <br/> 	despite the name 'counter'.
        /// <br/> 
        /// <br/> 	'p4 counter' requires 'review' access granted by 'p4 protect'.
        /// <br/> 	The -f flag requires that the user be an operator or have 'super'
        /// <br/> 	access.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the job counter:
        ///		<code> 
        ///			Counter target = Repository.GetCounter("job", null);
        ///		</code>
        ///		To get the change counter:
        ///		<code> 
        ///			Counter target = Repository.GetCounter("change", null);
        ///		</code>
        ///		To get the journal counter:
        ///		<code> 
        ///			Counter target = Repository.GetCounter("journal", null);
        ///		</code>
        /// </example>
        public Counter GetCounter(String name, Options options)
        {
            P4.P4Command counterCmd = new P4.P4Command(_connection, "counter", true, name);
            P4.P4CommandResult r = counterCmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }
            if ((r.TaggedOutput == null) || (r.TaggedOutput.Count <= 0))
            {
                return null;
            }
            foreach (P4.TaggedObject obj in r.TaggedOutput)

            {
                string countername = obj["counter"];
                string value = obj["value"];

                Counter counter = new Counter(countername, value);
                return counter;
            }
            return null;
        }

        /// <summary>
        /// Delete a Perforce counter from the repository. 
        /// </summary>    
        /// <param name="name"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help counter</b>
        /// <br/> 
        /// <br/>      counter -- Display, set, or delete a counter
        /// <br/> 
        /// <br/>      p4 counter name
        /// <br/>      p4 counter [-f] name value
        /// <br/>      p4 counter [-f] -d name
        /// <br/>      p4 counter [-f] -i name
        /// <br/>      p4 counter [-f] -m [ pair list ]
        /// <br/> 
        /// <br/> 	The first form displays the value of the specified counter.
        /// <br/> 
        /// <br/> 	The second form sets the counter to the specified value.
        /// <br/> 
        /// <br/> 	The third form deletes the counter.  This option usually has the
        /// <br/> 	same effect as setting the counter to 0.
        /// <br/> 
        /// <br/> 	The -f flag sets or deletes counters used by Perforce,  which are
        /// <br/> 	listed by 'p4 help counters'. Important: Never set the 'change'
        /// <br/> 	counter to a value that is lower than its current value.
        /// <br/> 
        /// <br/> 	The -i flag increments a counter by 1 and returns the new value.
        /// <br/> 	This option is used instead of a value argument and can only be
        /// <br/> 	used with numeric counters.
        /// <br/> 
        /// <br/> 	The fifth form allows multiple operations in one command.
        /// <br/> 	With this, the list is pairs of arguments.  Each pair is either
        /// <br/> 	counter value or '-' counter.  To set a counter use a name and value.
        /// <br/> 	To delete a counter use a '-' followed by the name.
        /// <br/> 
        /// <br/> 	Counters can be assigned textual values as well as numeric ones, 
        /// <br/> 	despite the name 'counter'.
        /// <br/> 
        /// <br/> 	'p4 counter' requires 'review' access granted by 'p4 protect'.
        /// <br/> 	The -f flag requires that the user be an operator or have 'super'
        /// <br/> 	access.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To delete a counter named test:
        ///		<code> 
        ///			Counter target = Repository.DeleteCounter("test", null);
        ///		</code>
        ///		To delete a counter named build using -f with a user with 
        ///		super access:
        ///		<code> 
        ///		    Options opts = new Options();
        ///		    opts["-f"] = null;
        ///			Counter target = Repository.DeleteCounter("build", opts);
        ///		</code>
        /// </example>
        public Object DeleteCounter(String name, Options options)
        {
            if (options == null)
            {
                options = new Options();
            }
            options["-d"] = null;
            P4.P4Command Cmd = new P4Command(this, "counter", false, name);
            P4.P4CommandResult r = Cmd.Run(options);
            if (r.Success != true)
            {
                P4Exception.Throw(r.ErrorList);
                return null;
            }

            return r.InfoOutput;

        }

        #region IDisposable Members

        /// <summary>
        /// Clean up after a Repository is used.
        /// Closes and Disposes the underlying connection
        /// </summary>
		public void Dispose()
        {
            if (_connection != null)
                _connection.Dispose();
        }

        #endregion
    }
}
