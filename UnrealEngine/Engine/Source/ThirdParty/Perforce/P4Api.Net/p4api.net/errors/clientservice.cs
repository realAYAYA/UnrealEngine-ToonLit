using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text;

namespace Perforce.P4
{
	public partial class P4ClientError
	{
		public static int clientUpdate = ErrorOf(ErrorSubsystem.ES_DM, 6370, ErrorSeverity.E_INFO, ErrorGeneric.EV_NONE, 2); //""
	}
}
