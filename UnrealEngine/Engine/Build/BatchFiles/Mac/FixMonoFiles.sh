#!/bin/sh

CUR_DIR=`pwd`
cd ../../../Binaries/ThirdParty/Mono/Mac

SCRIPT_VERSION=2

UPDATE_LINKS=false
if [ ! -f FixMonoFiles.version ]; then
	UPDATE_LINKS=true
elif [ `cat FixMonoFiles.version` != "$SCRIPT_VERSION" ]; then
	UPDATE_LINKS=true
fi

function try_symlink {
	if [ -f $(dirname $2)/$1 ]; then
		if [ ! -L $2 ]; then
			ln -s $1 $2
		elif [ "$UPDATE_LINKS" = true ]; then
			rm $2
			ln -s $1 $2
		fi
	fi
}

try_symlink mono-sgen64 bin/mono
try_symlink ../gac/Accessibility/4.0.0.0__b03f5f7f11d50a3a/Accessibility.dll lib/mono/4.5/Accessibility.dll
try_symlink ../gac/Commons.Xml.Relaxng/4.0.0.0__0738eb9f132ed756/Commons.Xml.Relaxng.dll lib/mono/4.5/Commons.Xml.Relaxng.dll
try_symlink ../gac/CustomMarshalers/4.0.0.0__b03f5f7f11d50a3a/CustomMarshalers.dll lib/mono/4.5/CustomMarshalers.dll
try_symlink ../gac/I18N.CJK/4.0.0.0__0738eb9f132ed756/I18N.CJK.dll lib/mono/4.5/I18N.CJK.dll
try_symlink ../gac/I18N.MidEast/4.0.0.0__0738eb9f132ed756/I18N.MidEast.dll lib/mono/4.5/I18N.MidEast.dll
try_symlink ../gac/I18N.Other/4.0.0.0__0738eb9f132ed756/I18N.Other.dll lib/mono/4.5/I18N.Other.dll
try_symlink ../gac/I18N.Rare/4.0.0.0__0738eb9f132ed756/I18N.Rare.dll lib/mono/4.5/I18N.Rare.dll
try_symlink ../gac/I18N.West/4.0.0.0__0738eb9f132ed756/I18N.West.dll lib/mono/4.5/I18N.West.dll
try_symlink ../gac/I18N/4.0.0.0__0738eb9f132ed756/I18N.dll lib/mono/4.5/I18N.dll
try_symlink ../gac/Microsoft.Build.Engine/4.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Engine.dll lib/mono/4.5/Microsoft.Build.Engine.dll
try_symlink ../gac/Microsoft.Build.Framework/4.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Framework.dll lib/mono/4.5/Microsoft.Build.Framework.dll
try_symlink ../gac/Microsoft.Build.Tasks.v4.0/4.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Tasks.v4.0.dll lib/mono/4.5/Microsoft.Build.Tasks.v4.0.dll
try_symlink ../gac/Microsoft.Build.Utilities.v4.0/4.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Utilities.v4.0.dll lib/mono/4.5/Microsoft.Build.Utilities.v4.0.dll
try_symlink ../gac/Microsoft.Build/4.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.dll lib/mono/4.5/Microsoft.Build.dll
try_symlink ../gac/Microsoft.CSharp/4.0.0.0__b03f5f7f11d50a3a/Microsoft.CSharp.dll lib/mono/4.5/Microsoft.CSharp.dll
try_symlink ../gac/Microsoft.VisualBasic/10.0.0.0__b03f5f7f11d50a3a/Microsoft.VisualBasic.dll lib/mono/4.5/Microsoft.VisualBasic.dll
try_symlink ../gac/Microsoft.VisualC/10.0.0.0__b03f5f7f11d50a3a/Microsoft.VisualC.dll lib/mono/4.5/Microsoft.VisualC.dll
try_symlink ../gac/Microsoft.Web.Infrastructure/1.0.0.0__31bf3856ad364e35/Microsoft.Web.Infrastructure.dll lib/mono/4.5/Microsoft.Web.Infrastructure.dll
try_symlink ../gac/Mono.Btls.Interface/4.0.0.0__0738eb9f132ed756/Mono.Btls.Interface.dll lib/mono/4.5/Mono.Btls.Interface.dll
try_symlink ../gac/Mono.CSharp/4.0.0.0__0738eb9f132ed756/Mono.CSharp.dll lib/mono/4.5/Mono.CSharp.dll
try_symlink ../gac/Mono.CodeContracts/4.0.0.0__0738eb9f132ed756/Mono.CodeContracts.dll lib/mono/4.5/Mono.CodeContracts.dll
try_symlink ../gac/Mono.CompilerServices.SymbolWriter/4.0.0.0__0738eb9f132ed756/Mono.CompilerServices.SymbolWriter.dll lib/mono/4.5/Mono.CompilerServices.SymbolWriter.dll
try_symlink ../gac/Mono.Data.Sqlite/4.0.0.0__0738eb9f132ed756/Mono.Data.Sqlite.dll lib/mono/4.5/Mono.Data.Sqlite.dll
try_symlink ../gac/Mono.Data.Tds/4.0.0.0__0738eb9f132ed756/Mono.Data.Tds.dll lib/mono/4.5/Mono.Data.Tds.dll
try_symlink ../gac/Mono.Debugger.Soft/4.0.0.0__0738eb9f132ed756/Mono.Debugger.Soft.dll lib/mono/4.5/Mono.Debugger.Soft.dll
try_symlink ../gac/Mono.Http/4.0.0.0__0738eb9f132ed756/Mono.Http.dll lib/mono/4.5/Mono.Http.dll
try_symlink ../gac/Mono.Management/4.0.0.0__0738eb9f132ed756/Mono.Management.dll lib/mono/4.5/Mono.Management.dll
try_symlink ../gac/Mono.Messaging/4.0.0.0__0738eb9f132ed756/Mono.Messaging.dll lib/mono/4.5/Mono.Messaging.dll
try_symlink ../gac/Mono.Parallel/4.0.0.0__0738eb9f132ed756/Mono.Parallel.dll lib/mono/4.5/Mono.Parallel.dll
try_symlink ../gac/Mono.Posix/4.0.0.0__0738eb9f132ed756/Mono.Posix.dll lib/mono/4.5/Mono.Posix.dll
try_symlink ../gac/Mono.Profiler.Log/4.0.0.0__0738eb9f132ed756/Mono.Profiler.Log.dll lib/mono/4.5/Mono.Profiler.Log.dll
try_symlink ../gac/Mono.Security.Win32/4.0.0.0__0738eb9f132ed756/Mono.Security.Win32.dll lib/mono/4.5/Mono.Security.Win32.dll
try_symlink ../gac/Mono.Security/4.0.0.0__0738eb9f132ed756/Mono.Security.dll lib/mono/4.5/Mono.Security.dll
try_symlink ../gac/Mono.Simd/4.0.0.0__0738eb9f132ed756/Mono.Simd.dll lib/mono/4.5/Mono.Simd.dll
try_symlink ../gac/Mono.Tasklets/4.0.0.0__0738eb9f132ed756/Mono.Tasklets.dll lib/mono/4.5/Mono.Tasklets.dll
try_symlink ../gac/Mono.WebBrowser/4.0.0.0__0738eb9f132ed756/Mono.WebBrowser.dll lib/mono/4.5/Mono.WebBrowser.dll
try_symlink ../gac/Mono.WebServer2/0.4.0.0__0738eb9f132ed756/Mono.WebServer2.dll lib/mono/4.5/Mono.WebServer2.dll
try_symlink ../gac/Mono.XBuild.Tasks/4.0.0.0__0738eb9f132ed756/Mono.XBuild.Tasks.dll lib/mono/4.5/Mono.XBuild.Tasks.dll
try_symlink ../gac/PEAPI/4.0.0.0__0738eb9f132ed756/PEAPI.dll lib/mono/4.5/PEAPI.dll
try_symlink ../gac/SMDiagnostics/0.0.0.0__b77a5c561934e089/SMDiagnostics.dll lib/mono/4.5/SMDiagnostics.dll
try_symlink ../gac/System.ComponentModel.Composition/4.0.0.0__b77a5c561934e089/System.ComponentModel.Composition.dll lib/mono/4.5/System.ComponentModel.Composition.dll
try_symlink ../gac/System.ComponentModel.DataAnnotations/4.0.0.0__31bf3856ad364e35/System.ComponentModel.DataAnnotations.dll lib/mono/4.5/System.ComponentModel.DataAnnotations.dll
try_symlink ../gac/System.Configuration.Install/4.0.0.0__b03f5f7f11d50a3a/System.Configuration.Install.dll lib/mono/4.5/System.Configuration.Install.dll
try_symlink ../gac/System.Configuration/4.0.0.0__b03f5f7f11d50a3a/System.Configuration.dll lib/mono/4.5/System.Configuration.dll
try_symlink ../gac/System.Core/4.0.0.0__b77a5c561934e089/System.Core.dll lib/mono/4.5/System.Core.dll
try_symlink ../gac/System.Data.DataSetExtensions/4.0.0.0__b77a5c561934e089/System.Data.DataSetExtensions.dll lib/mono/4.5/System.Data.DataSetExtensions.dll
try_symlink ../gac/System.Data.Entity/4.0.0.0__b77a5c561934e089/System.Data.Entity.dll lib/mono/4.5/System.Data.Entity.dll
try_symlink ../gac/System.Data.Linq/4.0.0.0__b77a5c561934e089/System.Data.Linq.dll lib/mono/4.5/System.Data.Linq.dll
try_symlink ../gac/System.Data.OracleClient/4.0.0.0__b77a5c561934e089/System.Data.OracleClient.dll lib/mono/4.5/System.Data.OracleClient.dll
try_symlink ../gac/System.Data.Services.Client/4.0.0.0__b77a5c561934e089/System.Data.Services.Client.dll lib/mono/4.5/System.Data.Services.Client.dll
try_symlink ../gac/System.Data.Services/4.0.0.0__b77a5c561934e089/System.Data.Services.dll lib/mono/4.5/System.Data.Services.dll
try_symlink ../gac/System.Data/4.0.0.0__b77a5c561934e089/System.Data.dll lib/mono/4.5/System.Data.dll
try_symlink ../gac/System.Deployment/4.0.0.0__b03f5f7f11d50a3a/System.Deployment.dll lib/mono/4.5/System.Deployment.dll
try_symlink ../gac/System.Design/4.0.0.0__b03f5f7f11d50a3a/System.Design.dll lib/mono/4.5/System.Design.dll
try_symlink ../gac/System.DirectoryServices.Protocols/4.0.0.0__b03f5f7f11d50a3a/System.DirectoryServices.Protocols.dll lib/mono/4.5/System.DirectoryServices.Protocols.dll
try_symlink ../gac/System.DirectoryServices/4.0.0.0__b03f5f7f11d50a3a/System.DirectoryServices.dll lib/mono/4.5/System.DirectoryServices.dll
try_symlink ../gac/System.Drawing.Design/4.0.0.0__b03f5f7f11d50a3a/System.Drawing.Design.dll lib/mono/4.5/System.Drawing.Design.dll
try_symlink ../gac/System.Drawing/4.0.0.0__b03f5f7f11d50a3a/System.Drawing.dll lib/mono/4.5/System.Drawing.dll
try_symlink ../gac/System.Dynamic/4.0.0.0__b03f5f7f11d50a3a/System.Dynamic.dll lib/mono/4.5/System.Dynamic.dll
try_symlink ../gac/System.EnterpriseServices/4.0.0.0__b03f5f7f11d50a3a/System.EnterpriseServices.dll lib/mono/4.5/System.EnterpriseServices.dll
try_symlink ../gac/System.IO.Compression.FileSystem/4.0.0.0__b77a5c561934e089/System.IO.Compression.FileSystem.dll lib/mono/4.5/System.IO.Compression.FileSystem.dll
try_symlink ../gac/System.IO.Compression/4.0.0.0__b77a5c561934e089/System.IO.Compression.dll lib/mono/4.5/System.IO.Compression.dll
try_symlink ../gac/System.IdentityModel.Selectors/4.0.0.0__b77a5c561934e089/System.IdentityModel.Selectors.dll lib/mono/4.5/System.IdentityModel.Selectors.dll
try_symlink ../gac/System.IdentityModel/4.0.0.0__b77a5c561934e089/System.IdentityModel.dll lib/mono/4.5/System.IdentityModel.dll
try_symlink ../gac/System.Json.Microsoft/4.0.0.0__31bf3856ad364e35/System.Json.Microsoft.dll lib/mono/4.5/System.Json.Microsoft.dll
try_symlink ../gac/System.Json/4.0.0.0__31bf3856ad364e35/System.Json.dll lib/mono/4.5/System.Json.dll
try_symlink ../gac/System.Management/4.0.0.0__b03f5f7f11d50a3a/System.Management.dll lib/mono/4.5/System.Management.dll
try_symlink ../gac/System.Messaging/4.0.0.0__b03f5f7f11d50a3a/System.Messaging.dll lib/mono/4.5/System.Messaging.dll
try_symlink ../gac/System.Net.Http.Formatting/4.0.0.0__31bf3856ad364e35/System.Net.Http.Formatting.dll lib/mono/4.5/System.Net.Http.Formatting.dll
try_symlink ../gac/System.Net.Http.WebRequest/4.0.0.0__b03f5f7f11d50a3a/System.Net.Http.WebRequest.dll lib/mono/4.5/System.Net.Http.WebRequest.dll
try_symlink ../gac/System.Net.Http/4.0.0.0__b03f5f7f11d50a3a/System.Net.Http.dll lib/mono/4.5/System.Net.Http.dll
try_symlink ../gac/System.Net/4.0.0.0__b03f5f7f11d50a3a/System.Net.dll lib/mono/4.5/System.Net.dll
try_symlink ../gac/System.Numerics.Vectors/4.0.0.0__b03f5f7f11d50a3a/System.Numerics.Vectors.dll lib/mono/4.5/System.Numerics.Vectors.dll
try_symlink ../gac/System.Numerics/4.0.0.0__b77a5c561934e089/System.Numerics.dll lib/mono/4.5/System.Numerics.dll
try_symlink ../gac/System.Reflection.Context/4.0.0.0__b77a5c561934e089/System.Reflection.Context.dll lib/mono/4.5/System.Reflection.Context.dll
try_symlink ../gac/System.Runtime.Caching/4.0.0.0__b03f5f7f11d50a3a/System.Runtime.Caching.dll lib/mono/4.5/System.Runtime.Caching.dll
try_symlink ../gac/System.Runtime.DurableInstancing/4.0.0.0__31bf3856ad364e35/System.Runtime.DurableInstancing.dll lib/mono/4.5/System.Runtime.DurableInstancing.dll
try_symlink ../gac/System.Runtime.Remoting/4.0.0.0__b77a5c561934e089/System.Runtime.Remoting.dll lib/mono/4.5/System.Runtime.Remoting.dll
try_symlink ../gac/System.Runtime.Serialization.Formatters.Soap/4.0.0.0__b03f5f7f11d50a3a/System.Runtime.Serialization.Formatters.Soap.dll lib/mono/4.5/System.Runtime.Serialization.Formatters.Soap.dll
try_symlink ../gac/System.Runtime.Serialization/4.0.0.0__b77a5c561934e089/System.Runtime.Serialization.dll lib/mono/4.5/System.Runtime.Serialization.dll
try_symlink ../gac/System.Security/4.0.0.0__b03f5f7f11d50a3a/System.Security.dll lib/mono/4.5/System.Security.dll
try_symlink ../gac/System.ServiceModel.Activation/4.0.0.0__31bf3856ad364e35/System.ServiceModel.Activation.dll lib/mono/4.5/System.ServiceModel.Activation.dll
try_symlink ../gac/System.ServiceModel.Discovery/4.0.0.0__31bf3856ad364e35/System.ServiceModel.Discovery.dll lib/mono/4.5/System.ServiceModel.Discovery.dll
try_symlink ../gac/System.ServiceModel.Internals/0.0.0.0__b77a5c561934e089/System.ServiceModel.Internals.dll lib/mono/4.5/System.ServiceModel.Internals.dll
try_symlink ../gac/System.ServiceModel.Routing/4.0.0.0__31bf3856ad364e35/System.ServiceModel.Routing.dll lib/mono/4.5/System.ServiceModel.Routing.dll
try_symlink ../gac/System.ServiceModel.Web/4.0.0.0__31bf3856ad364e35/System.ServiceModel.Web.dll lib/mono/4.5/System.ServiceModel.Web.dll
try_symlink ../gac/System.ServiceModel/4.0.0.0__b77a5c561934e089/System.ServiceModel.dll lib/mono/4.5/System.ServiceModel.dll
try_symlink ../gac/System.ServiceProcess/4.0.0.0__b03f5f7f11d50a3a/System.ServiceProcess.dll lib/mono/4.5/System.ServiceProcess.dll
try_symlink ../gac/System.Threading.Tasks.Dataflow/4.0.0.0__b77a5c561934e089/System.Threading.Tasks.Dataflow.dll lib/mono/4.5/System.Threading.Tasks.Dataflow.dll
try_symlink ../gac/System.Transactions/4.0.0.0__b77a5c561934e089/System.Transactions.dll lib/mono/4.5/System.Transactions.dll
try_symlink ../gac/System.Web.Abstractions/4.0.0.0__31bf3856ad364e35/System.Web.Abstractions.dll lib/mono/4.5/System.Web.Abstractions.dll
try_symlink ../gac/System.Web.ApplicationServices/4.0.0.0__31bf3856ad364e35/System.Web.ApplicationServices.dll lib/mono/4.5/System.Web.ApplicationServices.dll
try_symlink ../gac/System.Web.DynamicData/4.0.0.0__31bf3856ad364e35/System.Web.DynamicData.dll lib/mono/4.5/System.Web.DynamicData.dll
try_symlink ../gac/System.Web.Extensions.Design/4.0.0.0__31bf3856ad364e35/System.Web.Extensions.Design.dll lib/mono/4.5/System.Web.Extensions.Design.dll
try_symlink ../gac/System.Web.Extensions/4.0.0.0__31bf3856ad364e35/System.Web.Extensions.dll lib/mono/4.5/System.Web.Extensions.dll
try_symlink ../gac/System.Web.Http.SelfHost/4.0.0.0__31bf3856ad364e35/System.Web.Http.SelfHost.dll lib/mono/4.5/System.Web.Http.SelfHost.dll
try_symlink ../gac/System.Web.Http.WebHost/4.0.0.0__31bf3856ad364e35/System.Web.Http.WebHost.dll lib/mono/4.5/System.Web.Http.WebHost.dll
try_symlink ../gac/System.Web.Http/4.0.0.0__31bf3856ad364e35/System.Web.Http.dll lib/mono/4.5/System.Web.Http.dll
try_symlink ../gac/System.Web.Mobile/4.0.0.0__b03f5f7f11d50a3a/System.Web.Mobile.dll lib/mono/4.5/System.Web.Mobile.dll
try_symlink ../gac/System.Web.Mvc/3.0.0.0__31bf3856ad364e35/System.Web.Mvc.dll lib/mono/4.5/System.Web.Mvc.dll
try_symlink ../gac/System.Web.Razor/2.0.0.0__31bf3856ad364e35/System.Web.Razor.dll lib/mono/4.5/System.Web.Razor.dll
try_symlink ../gac/System.Web.RegularExpressions/4.0.0.0__b03f5f7f11d50a3a/System.Web.RegularExpressions.dll lib/mono/4.5/System.Web.RegularExpressions.dll
try_symlink ../gac/System.Web.Routing/4.0.0.0__31bf3856ad364e35/System.Web.Routing.dll lib/mono/4.5/System.Web.Routing.dll
try_symlink ../gac/System.Web.Services/4.0.0.0__b03f5f7f11d50a3a/System.Web.Services.dll lib/mono/4.5/System.Web.Services.dll
try_symlink ../gac/System.Web.WebPages.Deployment/2.0.0.0__31bf3856ad364e35/System.Web.WebPages.Deployment.dll lib/mono/4.5/System.Web.WebPages.Deployment.dll
try_symlink ../gac/System.Web.WebPages.Razor/2.0.0.0__31bf3856ad364e35/System.Web.WebPages.Razor.dll lib/mono/4.5/System.Web.WebPages.Razor.dll
try_symlink ../gac/System.Web.WebPages/2.0.0.0__31bf3856ad364e35/System.Web.WebPages.dll lib/mono/4.5/System.Web.WebPages.dll
try_symlink ../gac/System.Web/4.0.0.0__b03f5f7f11d50a3a/System.Web.dll lib/mono/4.5/System.Web.dll
try_symlink ../gac/System.Windows.Forms.DataVisualization/4.0.0.0__31bf3856ad364e35/System.Windows.Forms.DataVisualization.dll lib/mono/4.5/System.Windows.Forms.DataVisualization.dll
try_symlink ../gac/System.Windows.Forms/4.0.0.0__b77a5c561934e089/System.Windows.Forms.dll lib/mono/4.5/System.Windows.Forms.dll
try_symlink ../gac/System.Windows/4.0.0.0__b03f5f7f11d50a3a/System.Windows.dll lib/mono/4.5/System.Windows.dll
try_symlink ../gac/System.Workflow.Activities/4.0.0.0__31bf3856ad364e35/System.Workflow.Activities.dll lib/mono/4.5/System.Workflow.Activities.dll
try_symlink ../gac/System.Workflow.ComponentModel/4.0.0.0__31bf3856ad364e35/System.Workflow.ComponentModel.dll lib/mono/4.5/System.Workflow.ComponentModel.dll
try_symlink ../gac/System.Workflow.Runtime/4.0.0.0__31bf3856ad364e35/System.Workflow.Runtime.dll lib/mono/4.5/System.Workflow.Runtime.dll
try_symlink ../gac/System.Xaml/4.0.0.0__b77a5c561934e089/System.Xaml.dll lib/mono/4.5/System.Xaml.dll
try_symlink ../gac/System.Xml.Linq/4.0.0.0__b77a5c561934e089/System.Xml.Linq.dll lib/mono/4.5/System.Xml.Linq.dll
try_symlink ../gac/System.Xml.Serialization/4.0.0.0__b77a5c561934e089/System.Xml.Serialization.dll lib/mono/4.5/System.Xml.Serialization.dll
try_symlink ../gac/System.Xml/4.0.0.0__b77a5c561934e089/System.Xml.dll lib/mono/4.5/System.Xml.dll
try_symlink ../gac/System/4.0.0.0__b77a5c561934e089/System.dll lib/mono/4.5/System.dll
try_symlink ../gac/WebMatrix.Data/4.0.0.0__0738eb9f132ed756/WebMatrix.Data.dll lib/mono/4.5/WebMatrix.Data.dll
try_symlink ../gac/WindowsBase/4.0.0.0__31bf3856ad364e35/WindowsBase.dll lib/mono/4.5/WindowsBase.dll
try_symlink ../gac/cscompmgd/0.0.0.0__b03f5f7f11d50a3a/cscompmgd.dll lib/mono/4.5/cscompmgd.dll
try_symlink ../gac/fastcgi-mono-server4/4.4.0.0__0738eb9f132ed756/fastcgi-mono-server4.exe lib/mono/4.5/fastcgi-mono-server4.exe
try_symlink ../gac/mod-mono-server4/4.4.0.0__0738eb9f132ed756/mod-mono-server4.exe lib/mono/4.5/mod-mono-server4.exe
try_symlink ../gac/mono-fpm/4.4.0.0__0738eb9f132ed756/mono-fpm.exe lib/mono/4.5/mono-fpm.exe
try_symlink ../gac/xsp4/4.4.0.0__0738eb9f132ed756/xsp4.exe lib/mono/4.5/xsp4.exe
try_symlink ../../../gac/Microsoft.Build.Engine/12.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Engine.dll lib/mono/xbuild/12.0/bin/Microsoft.Build.Engine.dll
try_symlink ../../../gac/Microsoft.Build.Engine/14.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Engine.dll lib/mono/xbuild/14.0/bin/Microsoft.Build.Engine.dll
try_symlink ../../../gac/Microsoft.Build.Framework/12.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Framework.dll lib/mono/xbuild/12.0/bin/Microsoft.Build.Framework.dll
try_symlink ../../../gac/Microsoft.Build.Framework/14.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Framework.dll lib/mono/xbuild/14.0/bin/Microsoft.Build.Framework.dll
try_symlink ../../../gac/Microsoft.Build.Tasks.Core/14.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Tasks.Core.dll lib/mono/xbuild/14.0/bin/Microsoft.Build.Tasks.Core.dll
try_symlink ../../../gac/Microsoft.Build.Tasks.v12.0/12.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Tasks.v12.0.dll lib/mono/xbuild/12.0/bin/Microsoft.Build.Tasks.v12.0.dll
try_symlink ../../../gac/Microsoft.Build.Utilities.Core/14.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Utilities.Core.dll lib/mono/xbuild/14.0/bin/Microsoft.Build.Utilities.Core.dll
try_symlink ../../../gac/Microsoft.Build.Utilities.v12.0/12.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.Utilities.v12.0.dll lib/mono/xbuild/12.0/bin/Microsoft.Build.Utilities.v12.0.dll
try_symlink ../../../gac/Microsoft.Build/12.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.dll lib/mono/xbuild/12.0/bin/Microsoft.Build.dll
try_symlink ../../../gac/Microsoft.Build/14.0.0.0__b03f5f7f11d50a3a/Microsoft.Build.dll lib/mono/xbuild/14.0/bin/Microsoft.Build.dll
try_symlink ../../../gac/Mono.XBuild.Tasks/12.0.0.0__0738eb9f132ed756/Mono.XBuild.Tasks.dll lib/mono/xbuild/12.0/bin/Mono.XBuild.Tasks.dll
try_symlink ../../../gac/Mono.XBuild.Tasks/14.0.0.0__0738eb9f132ed756/Mono.XBuild.Tasks.dll lib/mono/xbuild/14.0/bin/Mono.XBuild.Tasks.dll

function try_chmod {
	if [ -f $1 ]; then
		chmod +x $1
	fi
}

try_chmod bin/csc
try_chmod bin/mcs
try_chmod bin/mono-sgen64
try_chmod bin/xbuild

if [ "$UPDATE_LINKS" = true ] ; then
	echo "$SCRIPT_VERSION" > FixMonoFiles.version
fi

cd "$CUR_DIR"
