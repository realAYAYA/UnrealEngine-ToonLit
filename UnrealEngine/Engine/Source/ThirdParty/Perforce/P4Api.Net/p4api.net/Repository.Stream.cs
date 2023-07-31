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
 * Name		: Repository.Stream.cs
 *
 * Author	: wjb
 *
 * Description	: Stream operations for the Reposity.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	public partial class Repository
    {
    	/// <summary>
		/// Create a new stream in the repository.
		/// </summary>
		/// <param name="stream">Stream specification for the new stream</param>
		/// <param name="options">The '-i' flag is required when creating a new stream</param>
		/// <returns>The Stream object if new stream was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller
		/// <br/>
		/// <br/><b>p4 help stream</b>
		/// <br/> 
		/// <br/>     stream -- Create, delete, or modify a stream specification
		/// <br/> 
		/// <br/>     p4 stream [-P parent] -t type name
		/// <br/>     p4 stream [-f] [-d] [-o [-v]] [-P parent] -t type name
		/// <br/>     p4 stream -i [-f] 
		/// <br/> 
		/// <br/> 	A stream specification ('spec') names a path in a stream depot to be
		/// <br/> 	treated as a stream.  (See 'p4 help streamintro'.)  The spec also
		/// <br/> 	defines the stream's lineage, its view, and its expected flow of
		/// <br/> 	change.
		/// <br/> 
		/// <br/> 	The 'p4 stream' command puts the stream spec into a temporary file and
		/// <br/> 	invokes the editor configured by the environment variable $P4EDITOR.
		/// <br/> 	When creating a stream, the type of the stream must be specified with
		/// <br/> 	the '-t' flag.  Saving the file creates or modifies the stream spec.
		/// <br/> 
		/// <br/> 	Creating a stream spec does not branch a new stream.  To branch a
		/// <br/> 	stream, use 'p4 copy -r -S stream', where 'stream' is the name of a
		/// <br/> 	stream spec.
		/// <br/> 
		/// <br/> 	The stream spec contains the following fields:
		/// <br/> 
		/// <br/> 	Stream:   The stream's path in a stream depot, of the form
		/// <br/> 	          //depotname/streamname. This is both the name of the stream
		/// <br/> 	          spec and the permanent, unique identifier of the stream.
		/// <br/> 
		/// <br/> 	Update:   The date this stream spec was last changed.
		/// <br/> 
		/// <br/> 	Access:   The date of the last command used with this spec.
		/// <br/> 
		/// <br/> 	Owner:    The stream's owner. A stream can be owned by a user, or
		/// <br/> 	          owned by a group. Can be changed.
		/// <br/> 
		/// <br/> 	Name:     An alternate name of the stream, for use in display outputs.
		/// <br/> 	          Defaults to the 'streamname' portion of the stream path.
		/// <br/> 	          Can be changed. 
		/// <br/> 
		/// <br/> 	Parent:   The parent of this stream. Can be 'none' if the stream type
		/// <br/> 	          is 'mainline',  otherwise must be set to an existing stream
		/// <br/> 	          identifier, of the form //depotname/streamname.
		/// <br/> 	          Can be changed.
		/// <br/> 
		/// <br/> 	Type:     'mainline', 'virtual', 'development', 'release' or 'task'.
		/// <br/> 	          Defines the role of a stream: A 'mainline' may not have a
		/// <br/> 	          parent. A 'virtual' stream is not a stream but an alternate
		/// <br/> 	          view of its parent stream.  The 'development' and 'release'
		/// <br/> 	          streams have controlled flow. Can be changed.  A 'task'
		/// <br/> 	          stream is a lightweight short-lived stream that only
		/// <br/> 	          promotes edited files to the repository; branched and
		/// <br/> 	          integrated files are stored in shadow tables that are
		/// <br/> 	          removed when the task stream is deleted or unloaded.
		/// <br/> 
		/// <br/> 	          Flow control is provided by 'p4 copy -S' and 'p4 merge -S'.
		/// <br/> 	          These commands restrict the flow of change as follows:
		/// <br/> 
		/// <br/> 	          Stream Type   Direction of flow     Allowed with
		/// <br/> 	          -----------   -----------------     ------------
		/// <br/> 	          development   to parent stream      'p4 copy'
		/// <br/> 	          task          to parent stream      'p4 copy'
		/// <br/> 	          release       to parent stream      'p4 merge'
		/// <br/> 	          development   from parent stream    'p4 merge'
		/// <br/> 	          release       from parent stream    'p4 copy'
		/// <br/> 
		/// <br/> 	Description: An optional description of the stream.
		/// <br/> 
		/// <br/> 	Options:  Flags to configure stream behavior. Defaults are marked *:
		/// <br/> 
		/// <br/> 	          unlocked *      Indicates whether the stream spec is locked
		/// <br/> 	          locked          against modifications. If locked, the spec
		/// <br/> 	                          may not be deleted, and only its owner or
		/// <br/> 	                          group users can modify it.
		/// <br/> 
		/// <br/> 	          allsubmit *     Indicates whether all users or only the
		/// <br/> 	          ownersubmit     owner (or group users) of the stream may
		/// <br/> 	                          submit changes to the stream path.
		/// <br/> 
		/// <br/> 	          toparent *      Indicates if controlled flow from the
		/// <br/> 	          notoparent      stream to its parent is expected to occur.
		/// <br/> 
		/// <br/> 	          fromparent *    Indicates if controlled flow to the stream
		/// <br/> 	          nofromparent    from its parent is expected to occur.
		/// <br/> 
		/// <br/> 	          mergedown *     Indicates if merge flow is restricted or
		/// <br/> 	          mergeany        merge is permitted from any other stream.
		/// <br/> 
		/// <br/> 	          The [no]fromparent and [no]toparent options determine if 
		/// <br/> 	          'p4 copy -S' and 'p4 merge -S' allow change to flow between
		/// <br/> 	          a stream and its parent. A 'virtual' stream must have its
		/// <br/> 	          flow options set as 'notoparent' and 'nofromparent'. Flow
		/// <br/> 	          options are ignored for 'mainline' streams.
		/// <br/> 
		/// <br/> 	Paths:    One or more lines that define file paths in the stream view.
		/// <br/> 	          Each line is of the form:
		/// <br/> 
		/// <br/> 	              &lt;path_type&gt; &lt;view_path&gt; [&lt;depot_path&gt;]
		/// <br/> 
		/// <br/> 	          where &lt;path_type&gt; is a single keyword, &lt;view_path&gt; is a file
		/// <br/> 	          path with no leading slashes, and the optional &lt;depot_path&gt;
		/// <br/> 	          is a file path beginning with '//'.  Both &lt;view_path&gt; and
		/// <br/> 	          &lt;depot_path&gt; may contain trailing wildcards, but no leading
		/// <br/> 	          or embedded wildcards.  Lines in the Paths field may appear
		/// <br/> 	          in any order.  A duplicated &lt;view_path&gt; overrides its
		/// <br/> 	          preceding entry.
		/// <br/> 
		/// <br/> 	          For example:
		/// <br/> 
		/// <br/> 	              share   src/...
		/// <br/> 	              import  lib/abc/...  //over/there/abc/...
		/// <br/> 	              isolate bin/*
		/// <br/> 
		/// <br/> 	          Default is:
		/// <br/> 
		/// <br/> 	              share   ...
		/// <br/> 
		/// <br/> 	          The &lt;path_type&gt; keyword must be one of:
		/// <br/> 
		/// <br/> 	          share:  &lt;view_path&gt; will be included in client views and
		/// <br/> 	                  in branch views. Files in this path are accessible
		/// <br/> 	                  to workspaces, can be submitted to the stream, and
		/// <br/> 	                  can be integrated with the parent stream.
		/// <br/> 
		/// <br/> 	          isolate: &lt;view_path&gt; will be included in client views but
		/// <br/> 	                   not in branch views. Files in this path are
		/// <br/> 	                   accessible to workspaces, can be submitted to the
		/// <br/> 	                   stream, but are not integratable with the parent
		/// <br/> 	                   stream. 
		/// <br/> 
		/// <br/> 	          import: &lt;view_path&gt; will be included in client views but
		/// <br/> 	                  not in branch views. Files in this path are mapped
		/// <br/> 	                  as in the parent stream's view (the default) or to
		/// <br/> 	                  &lt;depot_path&gt; (optional); they are accessible to
		/// <br/> 	                  workspaces, but can not be submitted or integrated
		/// <br/> 	                  to the stream.  If &lt;depot_path&gt; is used it may
		/// <br/> 	                  include a changelist specifier; clients of that
		/// <br/> 	                  stream will be limited to seeing revisions at that
		/// <br/> 	                  change or lower within that depot path.
		/// <br/> 
		/// <br/> 	          import+: &lt;view_path&gt; same as 'import' except that files can
		/// <br/> 	                   be submitted to the import path.
		/// <br/> 
		/// <br/> 	          exclude: &lt;view_path&gt; will be excluded from client views
		/// <br/> 	                   and branch views. Files in this path are not
		/// <br/> 	                   accessible to workspaces, and can't be submitted
		/// <br/> 	                   or integrated to the stream.
		/// <br/> 
		/// <br/> 	          Paths are inherited by child stream views. A child stream's
		/// <br/> 	          paths can downgrade the inherited view, but not upgrade it.
		/// <br/> 	          (For instance, a child stream can downgrade a shared path to
		/// <br/> 	          an isolated path, but it can't upgrade an isolated path to a
		/// <br/> 	          shared path.) Note that &lt;depot_path&gt; is relevant only when
		/// <br/> 	          &lt;path_type&gt; is 'import'.
		/// <br/> 
		/// <br/> 	Remapped: Optional; one or more lines that define how stream view paths
		/// <br/> 	          are to be remapped in client views. Each line is of the form:
		/// <br/> 
		/// <br/> 	              &lt;view_path_1&gt; &lt;view_path_2&gt;
		/// <br/> 
		/// <br/> 	          where &lt;view_path_1&gt; and &lt;view_path_2&gt; are Perforce view paths
		/// <br/> 	          with no leading slashes and no leading or embedded wildcards.
		/// <br/> 	          For example:
		/// <br/> 
		/// <br/> 	              ...    x/...
		/// <br/> 	              y/*    y/z/*
		/// <br/> 
		/// <br/> 	          Line ordering in the Remapped field is significant; if more
		/// <br/> 	          than one line remaps the same files, the later line has
		/// <br/> 	          precedence.  Remapping is inherited by child stream client
		/// <br/> 	          views.
		/// <br/> 
		/// <br/> 	Ignored: Optional; a list of file or directory names to be ignored in
		/// <br/> 	         client views. For example:
		/// <br/> 
		/// <br/> 	             /tmp      # ignores files named 'tmp'
		/// <br/> 	             /tmp/...  # ignores dirs named 'tmp'
		/// <br/> 	             .tmp      # ignores file names ending in '.tmp'
		/// <br/> 
		/// <br/> 	         Lines in the Ignored field may appear in any order.  Ignored
		/// <br/> 	         names are inherited by child stream client views.
		/// <br/> 
		/// <br/> 	The -d flag causes the stream spec to be deleted.  A stream spec may
		/// <br/> 	not be deleted if it is referenced by child streams or stream clients.
		/// <br/> 	Deleting a stream spec does not remove stream files, but it does mean
		/// <br/> 	changes can no longer be submitted to the stream's path.
		/// <br/> 
		/// <br/> 	The -o flag causes the stream spec to be written to the standard
		/// <br/> 	output. The user's editor is not invoked. -v may be used with -o to
		/// <br/> 	expose the automatically generated client view for this stream.
		/// <br/> 	('p4 help branch' describes how to expose the branch view.)
		/// <br/> 
		/// <br/> 	The -P flag can be used to insert a value into the Parent field of a
		/// <br/> 	new stream spec. It has no effect on an existing spec.
		/// <br/> 
		/// <br/> 	The -t flag is used to insert a value into the type field of a
		/// <br/> 	new stream spec and to adjust the default fromparent option
		/// <br/> 	for a new 'release' -type stream. The flag has no effect on an
		/// <br/> 	existing spec.
		/// <br/> 
		/// <br/> 	The -i flag causes a stream spec to be read from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag allows a user other than the owner to modify or delete a
		/// <br/> 	locked stream. It requires 'admin' access granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        /// 
        ///     To create a new mainline stream:      
        ///     <code>
        ///     
        ///         Stream main  = new Stream();
        ///         string mainTargetId = "//Rocket/mainline";
        ///         main.Id = mainTargetId;
        ///         main.Type = StreamType.Mainline;
        ///         main.Parent = new DepotPath("none");
        ///         main.Options = new StreamOptionEnum(StreamOption.None);
        ///         main.Name = "mainline";
        ///         main.Paths = new ViewMap();
        ///         MapEntry p1 = new MapEntry(MapType.Import, new DepotPath("..."), null);
        ///         main.Paths.Add(p1);
        ///         MapEntry p2 = new MapEntry(MapType.Share, new DepotPath("core/gui/..."), null);
        ///         main.Paths.Add(p2);
        ///         main.OwnerName = "admin";
        ///         Stream mainline = rep.CreateStream(main, null);
        ///     
        ///     </code>
        ///     
        ///     To create a new development type stream with the parent //Rocket/mainline:     
        ///     <code>
        ///     
        ///          Stream dev = new Stream();
        ///          string developmentTargetId = "//Rocket/dev";
        ///          dev.Id = developmentTargetId;
        ///          dev.Type = StreamType.Development;
        ///          dev.Parent = new DepotPath("//Rocket/mainline");
        ///          dev.Name = "releasetest";
        ///          dev.Options = new StreamOptionEnum(StreamOption.None);
        ///          dev.Paths = new ViewMap();
        ///          MapEntry devp1 = new MapEntry(MapType.Share, new DepotPath("..."), null);
        ///          dev.Paths.Add(devp1);
        ///          dev.OwnerName = "admin";
        ///          Stream dev1 = rep.CreateStream(dev, null);
        ///          
        ///     </code>
        ///     
        /// </example>
        /// <seealso cref="StreamCmdFlags"/>
 		public Stream CreateStream(Stream stream, Options options)
		{
			if (stream == null)
			{
				throw new ArgumentNullException("stream");
			}
			P4Command cmd = new P4Command(this, "stream", true);

            stream.ParentView = GetParentView(stream);

            cmd.DataSet = stream.ToString();

			if (options == null)
			{
                options = new Options();
			}
			options["-i"] = null;
			
			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return stream;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
        /// <summary>
		/// Create a new stream in the repository.
		/// </summary>
		/// <param name="stream">Stream specification for the new stream</param>
		/// <returns>The Stream object if new stream was created, null if creation failed</returns>
        /// <example>
        ///     To create a new locked release type stream in the repository, with ignored and remapped
        ///     paths:
        ///     <code>
        ///        Stream s = new Stream();
        ///        string targetId = "//Rocket/rel1";
        ///        s.Id = targetId;
        ///        s.Type = StreamType.Release;
        ///        s.Options = new StreamOptionEnum(StreamOption.Locked | StreamOption.NoToParent);
        ///        s.Parent = new DepotPath("//Rocket/main");
        ///        s.Name = "Release1";
        ///        s.Paths = new ViewMap();
        ///        MapEntry p1 = new MapEntry(MapType.Import, new DepotPath("..."), null);
        ///        s.Paths.Add(p1);
        ///        MapEntry p2 = new MapEntry(MapType.Share, new DepotPath("core/gui/..."), null);
        ///        s.Paths.Add(p2);
        ///        s.OwnerName = "admin";
        ///        s.Description = "release stream for first release";
        ///        s.Ignored = new ViewMap();
        ///        MapEntry ig1 = new MapEntry(MapType.Include, new DepotPath(".tmp"), null);
        ///        s.Ignored.Add(ig1);
        ///        MapEntry ig2 = new MapEntry(MapType.Include, new DepotPath("/bmps/..."), null);
        ///        s.Ignored.Add(ig2);
        ///        MapEntry ig3 = new MapEntry(MapType.Include, new DepotPath("/test"), null);
        ///        s.Ignored.Add(ig3);
        ///        MapEntry ig4 = new MapEntry(MapType.Include, new DepotPath(".jpg"), null);
        ///        s.Ignored.Add(ig4);
        ///        s.Remapped = new ViewMap();
        ///        MapEntry re1 = new MapEntry(MapType.Include, new DepotPath("..."), new DepotPath("x/..."));
        ///        s.Remapped.Add(re1);
        ///        MapEntry re2 = new MapEntry(MapType.Include, new DepotPath("y/*"), new DepotPath("y/z/*"));
        ///        s.Remapped.Add(re2);
        ///        MapEntry re3 = new MapEntry(MapType.Include, new DepotPath("ab/..."), new DepotPath("a/..."));
        ///        s.Remapped.Add(re3);
        ///        
        ///        Stream newStream = rep.CreateStream(s);
        ///     </code>
        /// </example>
		public Stream CreateStream(Stream stream)
        {
            return CreateStream(stream, null);
        }
	
        /// <summary>
        /// Update the record for a stream in the repository
        /// </summary>
        /// <param name="stream">Stream specification for the stream being updated</param>
        /// <returns>The Stream object if new stream was saved, null if creation failed</returns>
        /// <example>
        ///  To set the locked option on a stream:
        /// <code>
        ///       Stream streamToUpdate = rep.GetStream("//Rocket/GUI");
        ///       streamToUpdate.Options |= StreamOption.Locked; 
        ///       streamToUpdate = rep.UpdateStream(streamToUpdate);
        /// </code>
        /// </example>
        public Stream UpdateStream(Stream stream)
        {
            return CreateStream(stream);
        }
        /// <summary>
        /// Update the record for a stream in the repository
        /// </summary>
        /// <param name="stream">Stream specification for the stream being updated</param>
        /// <param name="options">options/flags</param>
        /// <returns>The Stream object if new stream was saved, null if creation failed</returns>
        /// 
        /// <example>
        ///  To update a locked stream when connected as an admin user:
        /// <code>
        /// 
        ///       Stream streamToUpdate = rep.GetStream("//Rocket/GUI");
        ///       // set locked option
        ///       streamToUpdate.Options |= StreamOption.Locked; 
        ///       streamToUpdate = rep.UpdateStream(streamToUpdate);
        ///       streamToUpdate.Description = "edited";
        ///       string parent = streamToUpdate.Parent.ToString();
        ///       string type = streamToUpdate.Type.ToString();       
        ///       streamToUpdate = rep.UpdateStream(streamToUpdate, 
        ///         new StreamCmdOptions(StreamCmdFlags.Force, parent, type ));
        /// </code>
        /// </example>
        public Stream UpdateStream(Stream stream, Options options)
		{
			return CreateStream(stream, options);
		}
        /// <summary>
        /// Get the record for an existing stream from the repository.
        /// </summary>
        /// <param name="stream">Stream name</param>
        /// <param name="options">There are no valid flags to use when fetching an existing stream</param>
        /// <returns>The Stream object if new stream was found, null if creation failed</returns>
        /// <example>
        /// 
        ///     Get the stream with the stream Id "//Rocket/GUI":
        ///     <code>
        ///     
        ///         string targetStream = "//Rocket/GUI";
        ///         Stream s = rep.GetStream(targetStream, null, null);
        ///     
        ///     </code>
        ///    Get stream spec for a new development type stream with the parent 
        ///    //Rocket/MAIN:
        ///     <code>
        ///     
        ///         string targetStream = "//Rocket/GUI2";
        ///         string parentStream = "//Rocket/MAIN";
        ///         Stream stream = rep.GetStream(targetStream,
        ///             new StreamCmdOptions(StreamCmdFlags.None, parentStream, 
        ///             StreamType.Development.ToString()));
        ///     
        ///     </code>
        /// </example>
        public Stream GetStream(string stream, Options options)
        {
            if (stream == null)
            {
                throw new ArgumentNullException("stream");

            }
            P4Command cmd = new P4Command(this, "stream", true, stream);

            if (options == null)
            {
                options = new StreamCmdOptions((StreamCmdFlags.Output), null, null);
            }
            if (options.ContainsKey("-o") == false)
            {
                options["-o"] = null;
            }

            P4CommandResult results = cmd.Run(options);
            if (results.Success)
            {
                if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
                {
                    return null;
                }
                Stream value = new Stream();

                value.FromStreamCmdTaggedOutput(results.TaggedOutput[0]);

                return value;
            }
            else
            {
                P4Exception.Throw(results.ErrorList);
            }
            return null;
        }
        /// <summary>
        /// Get the record for an existing stream from the repository.
        /// </summary>
        /// <param name="stream">Stream name</param>
        /// <param name="parent">Parent name</param>
        /// <param name="options">There are no valid flags to use when fetching an existing stream</param>
        /// <returns>The Stream object if new stream was found, null if creation failed</returns>
        /// <example>
        /// 
        ///     Get the stream with the stream Id "//Rocket/GUI":
        ///     <code>
        ///     
        ///         string targetStream = "//Rocket/GUI";
        ///         Stream s = rep.GetStream(targetStream, null, null);
        ///     
        ///     </code>
        ///    Get stream spec for a new development type stream with the parent 
        ///    //Rocket/MAIN:
        ///     <code>
        ///     
        ///         string targetStream = "//Rocket/GUI2";
        ///         string parentStream = "//Rocket/MAIN";
        ///         Stream stream = rep.GetStream(targetStream, parentStream,
        ///             new StreamCmdOptions(StreamCmdFlags.None, parentStream, 
        ///             StreamType.Development.ToString()));
        ///     
        ///     </code>
        /// </example>
        [Obsolete("Use GetStream(string stream, Options options)")]
        public Stream GetStream(string stream, string parent, Options options)
		{
            if (stream == null)
            {
                throw new ArgumentNullException("stream");
            }

            if (options == null)
            {
                options = new StreamCmdOptions((StreamCmdFlags.Output), null, null);
            }

            if (options.ContainsKey("-o") == false)
            {
                options["-o"] = null;
            }

            if (!string.IsNullOrEmpty(parent))
            {
                options["-P"] = parent;
            }

            return GetStream(stream, options);
		}
        /// <summary>
        /// Get the record for an existing stream from the repository.
        /// </summary>
        /// <param name="stream">Stream name</param>
        /// <returns>The Stream object if new stream was found, null if creation failed</returns>
        /// <example>
        ///     Get the stream with the stream Id "//Rocket/GUI":
        ///     <code>
        ///         string targetStream = "//Rocket/GUI";
        ///         Stream s = rep.GetStream(targetStream);
        ///     </code>
        /// </example>
		public Stream GetStream(string stream)
		{
			return GetStream(stream, null);
		}
        /// <summary>
        /// Get a list of streams from the repository
        /// </summary>
        /// <param name="options">options for the streams command<see cref="StreamsCmdOptions"/></param>
        /// <param name="files">files to filter results by</param>
        /// <returns>A list containing the matching streams</returns>
        /// <remarks>
        /// <br/><b>p4 help streams</b>
        /// <br/> 
        /// <br/>     streams -- Display list of streams
        /// <br/> 
        /// <br/>     p4 streams [-U -F filter -T fields -m max] [streamPath ...]
        /// <br/> 
        /// <br/> 	Reports the list of all streams currently known to the system.  If
        /// <br/> 	a 'streamPath' argument is specified, the list of streams is limited
        /// <br/> 	to those matching the supplied path. Unloaded task streams are not
        /// <br/> 	listed by default.
        /// <br/> 
        /// <br/> 	For each stream, a single line of output lists the stream depot path,
        /// <br/> 	the type, the parent stream depot path, and the stream name.
        /// <br/> 
        /// <br/> 	The -F filter flag limits the output to files satisfying the expression
        /// <br/> 	given as 'filter'.  This filter expression is similar to the one used
        /// <br/> 	by 'jobs -e jobview',  except that fields must match those above and
        /// <br/> 	are case sensitive.
        /// <br/> 
        /// <br/> 	        e.g. -F "Parent=//Ace/MAIN &amp; Type=development"
        /// <br/> 
        /// <br/> 	Note: the filtering takes place post-compute phase; there are no
        /// <br/> 	indexes to optimize performance.
        /// <br/> 
        /// <br/> 	The -T fields flag (used with tagged output) limits the fields output
        /// <br/> 	to those specified by a list given as 'fields'.  These field names can
        /// <br/> 	be separated by a space or a comma.
        /// <br/> 
        /// <br/> 	        e.g. -T "Stream, Owner"
        /// <br/> 
        /// <br/> 	The -m max flag limits output to the first 'max' number of streams.
        /// <br/> 
        /// <br/> 	The -U flag lists unloaded task streams (see 'p4 help unload').
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///         To get the first 3 development type streams with the parent //flow/mainline:
        ///         
        /// <code>
        ///         IList&#60;Stream&#62; = rep.GetStreams(new Options(StreamsCmdFlags.None,
        ///                    "Parent=//flow/mainline &amp; Type=development", null, "//...", 3));
        /// </code>
        /// </example>
        public IList<Stream> GetStreams(Options options, params FileSpec[] files)
		{
			P4Command cmd = null;
			if ((files != null) && (files.Length > 0))
			{
				cmd = new P4Command(this, "streams", true, FileSpec.ToStrings(files));
			}
			else
			{
				cmd = new P4Command(this, "streams", true);
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<Stream> value = new List<Stream>();

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Stream stream = new Stream();
					stream.FromStreamsCmdTaggedOutput(obj,offset, dst_mismatch);
					value.Add(stream);
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Delete a stream from the repository
		/// </summary>
		/// <param name="stream">The stream to be deleted</param>
		/// <param name="options">Only the '-f' flag is valid when deleting an existing stream</param>
        /// <example>
        ///  To delete a locked stream with no child streams or active clients as an admin user:
        ///  <code>
        ///     Stream stream = rep.GetStream("//Rocket/GUI");
        ///     StreamCmdOptions opts = new StreamCmdOptions(StreamCmdFlags.Force,
        ///                                            null, null); 
        ///     rep.DeleteStream(stream, opts);                  
        ///  </code>
        /// </example>
		public void DeleteStream(Stream stream, Options options)
		{
			if (stream == null)
			{
				throw new ArgumentNullException("stream");

			}
			P4Command cmd = new P4Command(this, "stream", true, stream.Id);

			if (options == null)
			{
				options = new Options((StreamCmdFlags.Delete), null, null);
			}
            if (options.ContainsKey("-d") == false)
            {
                options["-d"] = null;
            }

            P4CommandResult results = cmd.Run(options);
			if (results.Success == false)
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
        
        /// <summary>
		/// Get the integration status for a stream in the repository
        /// </summary>
        /// <param name="stream">The stream to get integration status on</param>
        /// <param name="options">options for the istat command</param>
        /// <returns>The integration status of the stream</returns>
        /// <remarks>
		/// <br/><b>p4 help istat</b>
		/// <br/> 
		/// <br/>     istat -- Show/cache a stream's integration status
		/// <br/> 
		/// <br/>     p4 istat [ -a -c -r -s ] stream
		/// <br/> 
		/// <br/> 	'p4 istat' shows a stream's cached integration status with respect
		/// <br/> 	to its parent. If the cache is stale, either because newer changes
		/// <br/> 	have been submitted or the stream's branch view has changed, 'p4 
		/// <br/> 	istat' checks for pending integrations and updates the cache before
		/// <br/> 	showing status. 
		/// <br/> 
		/// <br/> 	Pending integrations are shown only if they are expected by the
		/// <br/> 	stream; that is, only if they are warranted by the stream's type
		/// <br/> 	and its fromParent/toParent flow options. (See 'p4 help stream'.)
		/// <br/> 
		/// <br/> 	The -r flag shows the status of integration to the stream from its
		/// <br/> 	parent. By default, status of integration in the other direction is
		/// <br/> 	shown, from the stream to its parent.
		/// <br/> 
		/// <br/> 	The -a flag shows status of integration in both directions.
		/// <br/> 
		/// <br/> 	The -c flag forces 'p4 istat' to assume the cache is stale; it
		/// <br/> 	causes a search for pending integrations.  Use of this flag can
		/// <br/> 	impact server performance.
		/// <br/> 
		/// <br/> 	The -s flag shows cached state without refreshing stale data.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        /// 
        ///     Get the direction of integration for stream "//Rocket/GUI" with respect
        ///     to its parent:
        ///     <code>
        /// 
        ///         Stream s = rep.GetStream("//Rocket/GUI",null,null);
        ///         StreamMetaData smd = rep.GetStreamMetaData(s, null);
        ///         StreamMetaData.IntegAction action = smd.IntegToParentHow;
        ///         
        ///     </code>
        /// 
        ///     Get the direction of integration for stream "//Rocket/GUI" from its parent:    
        ///     <code>
        ///     
        ///         Stream s = rep.GetStream("//Rocket/GUI",null,null);
        ///         StreamMetaData smd = rep.GetStreamMetaData(s,new Options(GetStreamMetaDataCmdFlags.Reverse));
        ///         StreamMetaData.IntegAction action = smd.IntegFromParentHow;
        ///
        ///     </code>
        /// </example>    
       public StreamMetaData GetStreamMetaData(Stream stream, Options options)
        {
            if (stream == null)
            {
                throw new ArgumentNullException("stream");

            }
            P4Command cmd = new P4Command(this, "istat", true, stream.Id);

            P4CommandResult results = cmd.Run(options);
            if (results.Success)
            {
                if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
                {
                    return null;
                }
                StreamMetaData value = new StreamMetaData();
                value.FromIstatCmdTaggedData((results.TaggedOutput[0]));

                return value;
            }
            else
            {
                P4Exception.Throw(results.ErrorList);
            }
            return null;
        }

        private ParentView GetParentView(Stream stream)
        {
            // If ParentView is explicitly set return that value
            if (!stream.ParentView.Equals(ParentView.None))
            {
                return stream.ParentView;
            }
            if (stream.Type == StreamType.Mainline)
            {
                return GetStream(stream.Id, null).ParentView;
            }
            else
            {
                if ( stream.Parent == null )
                {
                    throw new ArgumentNullException("Parent");
                }

                return GetStream(stream.Id, new StreamCmdOptions(StreamCmdFlags.None, stream.Parent.ToString(),stream.Type.ToString())).ParentView;                
                //return GetStream(stream.Id, stream.Parent.ToString(), null).ParentView;
            }
        }
	}
}
