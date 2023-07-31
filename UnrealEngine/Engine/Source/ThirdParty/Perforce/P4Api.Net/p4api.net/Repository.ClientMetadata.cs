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
 * Name		: Repository.ClientMetadata.cs
 *
 * Author	: wjb
 *
 * Description	: p4 info operations for client application data.
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
		/// Get client application information.
		/// </summary>
		/// <returns>Information avout a connection's client-side application context.</returns>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help info</b>
		/// <br/> 
		/// <br/>     info -- Display client/server information
		/// <br/> 
		/// <br/>     p4 info [-s]
		/// <br/> 
		/// <br/> 	Info lists information about the current client (user name,
		/// <br/> 	client name, applicable client root, client current directory,
		/// <br/> 	and the client IP address) and some server information (server
		/// <br/> 	IP address, server root, date, uptime, version and license data).
		/// <br/> 
		/// <br/> 	The -s option produces 'short' output that omits any information
		/// <br/> 	that requires a database lookup such as the client root).
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        /// 
        ///     To get the current client host name:
        ///     <code>
        ///     
        ///         ClientMetadata c = rep.GetClientMetadata();
        ///         string machinename = c.HostName;
        ///         
        ///     </code>
        /// </example>
		public ClientMetadata GetClientMetadata()
		{
			
			P4Command cmd = new P4Command(this, "info", true);

			P4CommandResult results = cmd.Run();
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				ClientMetadata value = new ClientMetadata();
				foreach (TaggedObject obj in results.TaggedOutput)
				{ 
					value.FromGetClientMetadataCmdTaggedOutput(obj); 
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

	}
}
