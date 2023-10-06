The installer of the Datasmith Rhino exporter covers the following scenarios:
- No version of Rhino installed: A dialog informing the client of the situation will pop up and the installer will exit.
- No exporter installed yet: After a welcome dialog, the user is presented with a dialog stating what the installer is about to do for each compatible version of Rhino installed on the computer. The user can deselect the version of Rhino for which they do not want the exporter installed. If no flavor of the exporter is installed, an exit dialog is presented and the install aborts. Otherwise, the selected flavors of the exporter are installed.
- Same version of the exporter already installed: A Windows error dialog pops up inviting the user to uninstall the existing version if they want to re-install the same version.
- Older version of the exporter installed, the workflow is similar to the one where no exporter is installed. The wording changed from install to update.

If you want to generate the installer, you must install the Wix Toolset Extension for Visual Studio 2017.
You can download it from https://marketplace.visualstudio.com/items?itemName=WixToolset.WixToolsetVisualStudio2017Extension.

