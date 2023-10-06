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
 * Name		: Repository.Label.cs
 *
 * Author	: wjb
 *
 * Description	: Label operations for the Repository.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new label in the repository.
		/// </summary>
		/// <param name="label">Label specification for the new label</param>
		/// <param name="options">The '-i' flag is required when creating a new label </param>
		/// <returns>The Label object if new label was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller
		/// <br/>
		/// <br/><b>p4 help label</b>
		/// <br/> 
		/// <br/>     label -- Create or edit a label specification
		/// <br/> 
		/// <br/>     p4 label [-f -g -t template] name
		/// <br/>     p4 label -d [-f -g] name
		/// <br/>     p4 label -o [-t template] name
		/// <br/>     p4 label -i [-f -g]
		/// <br/> 
		/// <br/> 	Create  or edit a label. The name parameter is required. The
		/// <br/> 	specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	The label specification form contains the following fields:
		/// <br/> 
		/// <br/> 	Label:       The label name (read only.)
		/// <br/> 
		/// <br/> 	Owner:       The user who created this label.  Can be changed.
		/// <br/> 
		/// <br/> 	Update:      The date that this specification was last modified.
		/// <br/> 
		/// <br/> 	Access:      The date of the last 'labelsync' or use of '@label'
		/// <br/> 		     referencing this label.
		/// <br/> 
		/// <br/> 	Description: A short description of the label (optional).
		/// <br/> 
		/// <br/> 	Options:     Flags to change the label behavior.
		/// <br/> 
		/// <br/> 	             locked	  Prevents users other than the label owner
		/// <br/> 	             unlocked     from changing the specification. Prevents
		/// <br/> 				  the label from being deleted. Prevents the
		/// <br/> 				  owner from running 'p4 labelsync'. For a
		/// <br/> 				  loaded label, prevents 'p4 unload'.
		/// <br/> 
		/// <br/> 	             autoreload	  For a static label, indicates where label
		/// <br/> 	             noautoreload revisions are stored. Specify 'noautoreload'
		/// <br/> 	                     	  to indicate that the revisions should be
		/// <br/> 	                     	  stored in the db.label table. Specify
		/// <br/> 	                     	  'autoreload' to indicate that the revisions
		/// <br/> 	                     	  should be stored in the unload depot.
		/// <br/> 
		/// <br/> 	Revision:    An optional revision specification for an automatic
		/// <br/> 		     label.  Enclose in double quotes if it contains the
		/// <br/> 		     # (form comment) character.  An automatic label can
		/// <br/> 		     be treated as a pure alias of a single revision
		/// <br/> 		     specification (excluding @label) provided that the
		/// <br/> 		     View mapping is empty.
		/// <br/> 
		/// <br/> 	View:        A mapping that selects files from the depot. The
		/// <br/> 		     default view selects all depot files. Only the left
		/// <br/> 		     side of the mapping is used for labels.  Leave this
		/// <br/> 		     field blank when creating an automatic label as
		/// <br/> 		     a pure alias. See 'p4 help views'.
		/// <br/> 
		/// <br/> 	ServerID:    If set, restricts usage to the named server.
		/// <br/> 		     If unset, usage is allowed on any server.
		/// <br/> 
		/// <br/> 	A label is a named collection of revisions.  A label is either
		/// <br/> 	automatic or static.  An automatic label refers to the revisions
		/// <br/> 	given in the View: and Revision: fields.  A static label refers to
		/// <br/> 	the revisions that are associated with the label using the 'p4 tag'
		/// <br/> 	or 'p4 labelsync' commands.  A static label cannot have a Revison:
		/// <br/> 	field. See 'p4 help revisions' for information on using labels as
		/// <br/> 	revision specifiers.  
		/// <br/> 
		/// <br/> 	Only the label owner can run 'p4 labelsync', and only if the label
		/// <br/> 	is unlocked. A label without an owner can be labelsync'd by any user.
		/// <br/> 
		/// <br/> 	Flag -d deletes the specified label. You cannot delete a locked label.
		/// <br/> 	The -f flag forces the delete.
		/// <br/> 
		/// <br/> 	The -o flag writes the label specification to standard output.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a label specification from standard input.  The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -t flag copies the view and options from the template label to
		/// <br/> 	the new label.
		/// <br/> 
		/// <br/> 	The -f flag forces the deletion of a label. By default, locked labels
		/// <br/> 	can only be deleted by their owner.  The -f flag also permits the
		/// <br/> 	Last Modified date to be set.  The -f flag requires 'admin' access,
		/// <br/> 	which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -g flag should be used on an Edge Server to update a global
		/// <br/> 	label. Without -g, the label definition is visible only to users
		/// <br/> 	of this Edge Server. Configuring rpl.labels.global=1 reverses this
		/// <br/> 	default and causes this flag to have the opposite meaning.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///     To create a new label:
        ///     
        /// <code>
        /// 
        ///         Label l = new Label();
        ///         l.Id = "newLabel";
        ///         l.Owner = "admin";
        ///         l.Description = "created by admin";
        ///         l.Options = "unlocked";
        ///         l.ViewMap = new ViewMap();
        ///         string v0 = "//depot/main/...";
        ///         string v1 = "//depot/rel1/...";
        ///         string v2 = "//depot/rel2/...";
        ///         string v3 = "//depot/dev/...";
        ///         l.ViewMap.Add(v0);
        ///         l.ViewMap.Add(v1);
        ///         l.ViewMap.Add(v2);
        ///         l.ViewMap.Add(v3);
        ///         Label newLabel = rep.CreateLabel(l, null);
        ///         
        /// </code>
        /// 
        ///     To create a label using another label as a template:
        /// <code>
        ///         
        ///         Label newLabel2 = rep.CreateLabel(newLabel, 
        ///             new LabelCmdOptions(LabelCmdFlags.None, newLabel.Id));
        /// </code>     
        /// </example>
		public Label CreateLabel(Label label, Options options)
		{
			if (label == null)
			{
				throw new ArgumentNullException("label");

			}
			P4Command cmd = new P4Command(this, "label", true);

			cmd.DataSet = label.ToString();

			if (options == null)
			{
				options = new Options((LabelCmdFlags.Input), null);
			}
			if (options.ContainsKey("-i") == false)
			{
				options["-i"] = null;
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return label;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Create a new label in the repository.
		/// </summary>
		/// <param name="label">Label specification for the new label</param>
		/// <returns>The Label object if new label was created, null if creation failed</returns>
        /// <example>
        ///     To create a new label:
        ///     
        ///     <code>
        /// 
        ///         Label l = new Label();
        ///         l.Id = "newLabel";
        ///         l.Owner = "admin";
        ///         l.Description = "created by admin";
        ///         l.Options = "unlocked";
        ///         l.ViewMap = new ViewMap();
        ///         string v0 = "//depot/main/...";
        ///         string v1 = "//depot/rel1/...";
        ///         string v2 = "//depot/rel2/...";
        ///         string v3 = "//depot/dev/...";
        ///         l.ViewMap.Add(v0);
        ///         l.ViewMap.Add(v1);
        ///         l.ViewMap.Add(v2);
        ///         l.ViewMap.Add(v3);
        ///         Label newLabel = rep.CreateLabel(l, null);
        ///         
        ///     </code>
        ///</example>
		public Label CreateLabel(Label label)
		{
			return CreateLabel(label, null);
		}
		/// <summary>
		/// Update the record for a label in the repository
		/// </summary>
		/// <param name="label">Label specification for the label being updated</param>
		/// <returns>The Label object if new depot was saved, null if creation failed</returns>
        /// <example>
        /// 
        ///     To lock a label:
        ///     
        /// <code>
        ///         Label l = rep.GetLabel("admin_label");
        ///         l.Locked = true;
        ///         rep.UpdateLabel(l);
        ///         
        /// </code>
        /// </example>
		public Label UpdateLabel(Label label)
		{
			return CreateLabel(label, null);
		}
        /// <summary>
        /// Get the record for an existing label from the repository.
        /// </summary>
        /// <param name="label">Label name</param>
        /// <param name="template">Template to use (if required)</param>
        /// <param name="options">Flags used when fetching an existing label</param>
        /// <returns>The Label object if label was found, null if creation failed</returns>
        /// <example>
        /// 
        ///     To get the admin_label with the gobal option:
        /// 
        /// <code>
        /// 
        ///         LabelCmdOptions opts = new LabelCmdOptions(LabelCmdFlags.Global,null);
        ///         Label label = rep.GetLabel("admin_label", null, opts);
        ///         
        /// </code>
        /// </example>
        public Label GetLabel(string label, string template, Options options)
		{
			if (label == null)
			{
				throw new ArgumentNullException("label");

			}
			P4Command cmd = new P4Command(this, "label", true, label);

			if (options == null)
			{
				options = new Options((LabelCmdFlags.Output), template);
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
				Label value = new Label();

 				value.FromLabelCmdTaggedOutput(results.TaggedOutput[0]);

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Get the record for an existing label from the repository.
        /// </summary>
        /// <param name="label">Label name</param>
        /// <returns>The Label object if label was found, null if creation failed</returns>
        /// <example>
        /// 
        ///     To get the admin_label:
        /// 
        /// <code>
        /// 
        ///         string targetLabel = "admin_label";
        ///         Label l = rep.GetLabel(targetLabel);
        ///         
        /// </code>
        /// </example>
		public Label GetLabel(string label)
		{
			return GetLabel(label, null, null);
		}

		/// <summary>
		/// Get a list of labels from the repository
		/// </summary>
		/// <returns>A list containing the matching labels</returns>
	    /// <remarks>
		/// <br/><b>p4 help labels</b>
		/// <br/> 
		/// <br/>     labels -- Display list of defined labels
		/// <br/> 
		/// <br/>     p4 labels [-t] [-u user] [[-e|-E] nameFilter -m max] [file[revrange]]
		/// <br/>     p4 labels [-t] [-u user] [[-e|-E] nameFilter -m max] [-a|-s serverID]
		/// <br/>     p4 labels -U
		/// <br/> 
		/// <br/> 	Lists labels defined in the server.
		/// <br/> 
		/// <br/> 	If files are specified, 'p4 labels' lists the labels that contain
		/// <br/> 	those files.  If you include a file specification, automatic labels
		/// <br/> 	and labels with the 'autoreload' option set are omitted from the list.
		/// <br/> 	If the file specification includes a revision range, 'p4 labels'
		/// <br/> 	lists labels that contain the specified revisions.
		/// <br/> 
		/// <br/> 	See 'p4 help revisions' for details about specifying revisions.
		/// <br/> 
		/// <br/> 	The -t flag displays the time as well as the date.
		/// <br/> 
		/// <br/> 	The -u user flag lists labels owned by the specified user.
		/// <br/> 
		/// <br/> 	The -e nameFilter flag lists labels with a name that matches
		/// <br/> 	the nameFilter pattern, for example: -e 'svr-dev-rel*'. The -e flag
		/// <br/> 	uses the server's normal case-sensitivity rules. The -E flag makes
		/// <br/> 	the matching case-insensitive, even on a case-sensitive server.
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the first 'max' number of labels.
		/// <br/> 
		/// <br/> 	The -U flag lists unloaded labels (see 'p4 help unload').
		/// <br/> 
		/// <br/> 	The -a and -s flags are useful in a distributed server installation
		/// <br/> 	(see 'p4 help distributed') in order to see the names of local labels
		/// <br/> 	stored on other Edge Servers. These flags are not allowed if the
		/// <br/> 	command includes a file specification.
		/// <br/> 
		/// <br/> 	The -a flag specifies that all labels should be displayed, not just
		/// <br/> 	those that are bound to this server.
		/// <br/> 
		/// <br/> 	The -s serverID flag specifies that only those labels bound to the
		/// <br/> 	specified serverID should be displayed.
		/// <br/> 
		/// <br/> 	On an Edge Server, if neither -s nor -a is specified, only those
		/// <br/> 	local labels bound to this Edge Server are displayed. Labels created
		/// <br/> 	on the Commit Server are global, and are also included in the output.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///     
        ///     To get 50 labels on a distributed server installation:
        ///     
        ///     <code>
        /// 
        ///         LabelsCmdOptions opts = new LabelsCmdOptions(LabelsCmdFlags.All,
        ///             null, null, 50, null, null);
        ///         IList&#60;Label&#62; labels = rep.GetLabels(opts);
        ///     
        ///     </code>
        /// 
        ///     To get labels which contain files with the path //depot/Modifiers/...:
        ///     
        ///     <code>
        ///         FileSpec path = new FileSpec(new DepotPath("//depot/Modifiers/..."), null);
        ///         Options ops = new Options();
        ///         IList&#60;Label&#62; l = rep.GetLabels(ops, path); 
        ///         
        ///     </code>
        /// </example>
		public IList<Label> GetLabels(Options options, params FileSpec[] files)
		{
			P4Command cmd = null;
			if ((files != null) && (files.Length > 0))
			{
				cmd = new P4Command(this, "labels", true, FileSpec.ToEscapedStrings(files));
			}
			else
			{
				cmd = new P4Command(this, "labels", true);
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                    
                }

				List<Label> value = new List<Label>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Label label = new Label();
					label.FromLabelsCmdTaggedOutput(obj, offset, dst_mismatch);
					value.Add(label);
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
		/// Delete a label from the repository
		/// </summary>
		/// <param name="label">The label to be deleted</param>
		/// <param name="options">The 'f' and '-d' flags are valid when deleting an
		/// existing label</param>
        /// <example>
        /// 
        ///  To delete the label admin_label:   
        /// 
        /// <code>
        ///     
        ///     Label deleteTarget = new Label();
        ///     deleteTarget.Id = "admin_label";
        ///     rep.DeleteLabel(deleteTarget, null);
        ///     
        /// </code>
        /// 
        /// </example>
		public void DeleteLabel(Label label, Options options)
		{
			if (label == null)
			{
				throw new ArgumentNullException("label");

			}
			P4Command cmd = new P4Command(this, "label", true, label.Id);

			if (options == null)
			{
				options = new Options(LabelCmdFlags.Delete, null);
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
	}
}
