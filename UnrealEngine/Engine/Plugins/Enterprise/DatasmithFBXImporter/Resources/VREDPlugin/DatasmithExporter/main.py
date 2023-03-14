from PySide2 import QtCore, QtGui, QtWidgets

import sys
import os
import logging
import xml.etree.ElementTree as ET
from collections import defaultdict

import vrFileIO
import vrFileDialog
import vrVariantSets
import vrMaterialPtr
import vrScenegraph
import vrNodeUtils
import vrFieldAccess
import vrController
import vrOptimize
import vrNodePtr

# Adds the ScriptPlugins folder to path so that we can find the other files
# in our module
scriptPluginsFolder = os.path.dirname(os.path.realpath(os.curdir))
if scriptPluginsFolder not in sys.path:
    sys.path.append(scriptPluginsFolder)

from DatasmithExporter.exporter import DatasmithFBXExporter
from DatasmithExporter.exporter import exporterVersion

def vredMainWindow(id):
    from shiboken2 import wrapInstance
    return wrapInstance(id, QtWidgets.QMainWindow)

class DatasmithExporterPlugin():
    def __init__(self, mw):
        # Store this since it's only globally available when the plugin first loads
        self.mainWindow = mw
        mb = self.mainWindow.menuBar()
        actions = mb.actions()

        self.exporter = DatasmithFBXExporter(mw)

        # Create an action
        self.exportSceneAction = QtWidgets.QAction(mw.tr("Export to Datasmith..."), mw)
        self.exportSceneAction.triggered.connect(self.exporter.exportSceneDialog)

        # Iterate over the main menu bar
        for action in actions:

            # Iterate over the file menu
            if action.text() == mw.tr("&File"):
                fileMenuItems = action.menu().actions()
                for fileItem in fileMenuItems:

                    # Add our action after a separator at the end of the Export menu
                    if fileItem.text() == mw.tr("Export Scene Data") or fileItem.text() == mw.tr("Export"):
                        exportMenuItems = fileItem.menu().actions()

                        # Get indices of all entries that have "Datasmith" in them
                        indices = [pair[0] for pair in enumerate(exportMenuItems) if "Export to Datasmith" in pair[1].text()]

                        # Append indices of the previous elements too as we always add
                        # a separator before our entries
                        for index in reversed(indices):
                            indices.append(index-1)

                        # Remove old entries as we need new ones to point to our reloaded
                        # modules and classes
                        for index in reversed(sorted(indices)):
                            itemToDelete = exportMenuItems[index]
                            if "Export to Datasmith" in itemToDelete.text() or itemToDelete.isSeparator():
                                fileItem.menu().removeAction(itemToDelete)

                        fileItem.menu().addSeparator()
                        fileItem.menu().addAction(self.exportSceneAction)

        logging.info('Finished loading DatasmithExporter plugin version ' + str(exporterVersion))

exporter = DatasmithExporterPlugin(vredMainWindow(VREDMainWindowId))

