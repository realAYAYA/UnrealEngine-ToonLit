from PySide2 import QtCore, QtGui, QtWidgets

# This has to be run after everything is imported, so that we can remove all entries
# that our scripts created. For some reason VRED likes parsing this file last, so
# we placed the menu cleaning code in here

def vredMainWindow(id):
    from shiboken2 import wrapInstance
    return wrapInstance(id, QtWidgets.QMainWindow)

# Only run this when VRED is parsing this file
if 'VREDModule' in __name__ or 'PythonQt_module' in __name__:
    # Strings to remove from the Scripts menu. Could do something with the os module but
    # this is safer, and shouldn't change that often
    fileNames = ['anim_exporter', 'log', 'main', 'sanitizer', 'utils', 'exporter', 'mat', '__init__']

    mainWindow = vredMainWindow(VREDMainWindowId)
    mb = mainWindow.menuBar()
    actions = mb.actions()

    # Iterate over the main menu bar
    for action in actions:
        # Iterate over the Scripts menu and remove entries in fileNames
        if action.text() == mainWindow.tr("Scripts"):
            scriptsMenuItems = action.menu().actions()
            for scriptsItem in scriptsMenuItems:
                if scriptsItem.text() in fileNames:
                    scriptsItem.setVisible(False)
