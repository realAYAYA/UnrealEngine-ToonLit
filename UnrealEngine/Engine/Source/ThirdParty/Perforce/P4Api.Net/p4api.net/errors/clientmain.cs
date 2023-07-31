
namespace Perforce.P4
{
	public partial class P4ClientError
	{
        /// <summary>
        /// Error related to Usage
        /// </summary>
		public static int usage = ErrorOf(ErrorSubsystem.ES_OS, 0, ErrorSeverity.E_FAILED, ErrorGeneric.EV_NONE, 0); //"p4 -h for usage."
	}
}
