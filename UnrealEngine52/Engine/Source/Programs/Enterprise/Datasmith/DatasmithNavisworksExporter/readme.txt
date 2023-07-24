- Exporter plugins 

Setup environment variables to point to appropriate SDK path, e.g.(for Navisowrks 2019):
	Navisworks_2019_API=C:\Program Files\Autodesk\Navisworks Manage 2019\

Built from DatasmithNavisworksPlugin/DatasmithNavisworks.sln
Choose appropriate solution config, e.g. 'Release2019' for Navisworks 2019

- Installer 

Is build with Installer\Installer.sln 
It bundles plugins for all versions(so these versions need to be built prior to building the installer) 

- How to test build with BuildGraph locally

Engine\Build\BatchFiles\RunUAT.bat BuildGraph -NoCompile -Script=Engine/Restricted/NotForLicensees/Build/InternalEngineBuild.xml -SingleNode="Compile Datasmith Navisworks Exporter" -Target="Package Enterprise"
Engine\Build\BatchFiles\RunUAT.bat BuildGraph -NoCompile -Script=Engine/Restricted/NotForLicensees/Build/InternalEngineBuild.xml -SingleNode="Build Datasmith Navisworks Installer" -Target="Package Enterprise"