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
		public static int Usage = ErrorOf(ErrorSubsystem.ES_SUPP, 0, ErrorSeverity.E_FAILED, ErrorGeneric.EV_USAGE, 0); //
	}
}
