import maya.cmds as cmds
import maya.mel as mel
import os, cPickle
from functools import partial

def customMayaMenu():
    gMainWindow = mel.eval('$temp1=$gMainWindow')
    
    
    menus = cmds.window(gMainWindow, q = True, menuArray = True)
    found = False
    for menu in menus:
        label = cmds.menu(menu, q = True, label = True)
        if label == "Epic Games":
            found = True
    
    if found == False:
        customMenu = cmds.menu(parent=gMainWindow, label = 'Epic Games')
        
        #tools path
	toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
	if os.path.exists(toolsPath):
	    
	    f = open(toolsPath, 'r')
	    mayaToolsDir = f.readline()
	    f.close()	
	    
        #ART
	cmds.menuItem(parent = customMenu, label = "Animation Rigging Toolset", bld = True, enable = False)
	cmds.menuItem(parent = customMenu, divider = True)
	cmds.menuItem(parent = customMenu, label = "Character Rig Creator", c = launchSkeletonBuilder)
	cmds.menuItem(parent = customMenu, label = "Edit Existing Character", c = editCharacter)
	cmds.menuItem(parent = customMenu, label = "Add Character For Animation", c = launchAddCharacter)
	cmds.menuItem(parent = customMenu, label = "Animation Interface", c = launchAnimUI)
	
	cmds.menuItem(parent = customMenu, label = "Settings", c = launchARTSettings)
        artHelp = cmds.menuItem(parent = customMenu, label = "Help", subMenu=True)
	cmds.menuItem(parent = artHelp, label = "Learning Videos", c = launchLearningVideos)
	cmds.menuItem(parent = artHelp, label = "Help Documentation", c = launchRigHelp)
        cmds.menuItem(parent = artHelp, label = "About", c = aboutARTTools)
	cmds.menuItem(parent = customMenu, divider = True)
	cmds.menuItem(parent = customMenu, label = "Misc. Tools", bld = True, enable = False)
	cmds.menuItem(parent = customMenu, divider = True)
	


	#PERFORCE
	p4Menu = cmds.menuItem(parent = customMenu, label = "Perforce", subMenu=True, to = True)
	cmds.menuItem("perforceSubmitMenuItem", parent = p4Menu, label = "Submit Current File", enable = False, c = p4Submit)
	cmds.menuItem("perforceAddAndSubmitMenuItem", parent = p4Menu, label = "Add and Submit Current File", enable = False, c = p4AddSubmit)
	cmds.menuItem("perforceCheckOutMenuItem", parent = p4Menu, label = "Check Out Current File", enable = False, c = p4CheckOut)
	cmds.menuItem("perforceFileHistoryMenuItem", parent = p4Menu, label = "Current File History", enable = False, c = p4GetHistory)
	cmds.menuItem("perforceGetLatestMenuItem", parent = p4Menu, label = "Get Latest Revision of Current File", enable = False, c = p4GetLatest)
	cmds.menuItem("perforceProjectList", parent = p4Menu, label = "Set Project", enable = False, subMenu = True)
	cmds.menuItem("perforceProject_New", parent = "perforceProjectList", label = "New Project", c = createNewProj)
	cmds.radioMenuItemCollection("perforceProjectRadioMenuCollection", parent = "perforceProjectList")
	cmds.menuItem(parent = customMenu, divider = True)
	
	

	
	
	
	
	#check settings to see if use source control is turned on
	toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
	if os.path.exists(toolsPath):
	    
	    f = open(toolsPath, 'r')
	    mayaToolsDir = f.readline()
	    f.close()	

	    settingsLocation = mayaToolsDir + "/General/Scripts/projectSettings.txt"
	    
	    if os.path.exists(settingsLocation):
		f = open(settingsLocation, 'r')
		settings = cPickle.load(f)
		f.close()	
		
		#find use source control value
		sourceControl = settings.get("UseSourceControl")
		
		if sourceControl:
		    cmds.menuItem("perforceSubmitMenuItem", edit = True, enable = True)
		    cmds.menuItem("perforceAddAndSubmitMenuItem", edit = True, enable = True)
		    cmds.menuItem("perforceCheckOutMenuItem", edit = True, enable = True)
		    cmds.menuItem("perforceFileHistoryMenuItem", edit = True, enable = True)
		    cmds.menuItem("perforceGetLatestMenuItem", edit = True, enable = True)
		    cmds.menuItem("perforceProjectList", edit = True, enable = True)
		    
		    #launch script job for checking Maya Tools
		    cmds.scriptJob(event = ["NewSceneOpened", autoUpdateTools])
		    

	    

    
#############################################################################################
#############################################################################################
#############################################################################################
def p4ProjectMenu(*args):
    
    #clear any projects that are in the collection first
    items = cmds.lsUI(menuItems = True)
    for i in items:
	data = cmds.menuItem(i, q = True, docTag = True)
	if data == "P4Proj":
	    cmds.deleteUI(i)
	    
    #find projects
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
	
	f = open(toolsPath, 'r')
	mayaToolsDir = f.readline()
	f.close()
	
    projects = os.listdir(mayaToolsDir + "/General/Scripts/")
    p4Projects = []
    #Test_Project_Settings
    for proj in projects:
	if proj.rpartition(".")[2] == "txt":
	    if proj.partition("_")[2].partition("_")[0] == "Project":
		p4Projects.append(proj)


    #set the current project
    try:
	f = open(mayaToolsDir + "/General/Scripts/projectSettings.txt", 'r')
	settings = cPickle.load(f)
	f.close()
	currentProj = settings.get("CurrentProject")
    except:
	pass
    

    
    #add the projects to the menu	
    for proj in p4Projects:
	projectName = proj.partition("_")[0]
	
	if projectName == currentProj:
	    val = True
	else:
	    val = False
	    
	menuItem = cmds.menuItem(label = projectName, parent = "perforceProjectList", cl = "perforceProjectRadioMenuCollection", rb = val, docTag = "P4Proj", c = partial(setProj, projectName))
	cmds.menuItem(parent = "perforceProjectList", optionBox = True, c = partial(editProj, projectName))	
	
#############################################################################################
#############################################################################################
#############################################################################################
def setProj(projectName, *args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.setCurrentProject(projectName)
    
#############################################################################################
#############################################################################################
#############################################################################################
def editProj(projectName, *args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.editProject(projectName)
    
#############################################################################################
#############################################################################################
#############################################################################################
def createNewProj(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.createNewProject()
    
#############################################################################################
#############################################################################################
#############################################################################################
def autoUpdateTools(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_checkForUpdates()
    

#############################################################################################
#############################################################################################
#############################################################################################
def launchARTSettings(*args):
    import ART_Settings
    reload(ART_Settings)
    ART_Settings.ART_Settings()
    
#############################################################################################
#############################################################################################
#############################################################################################
def aboutARTTools(*args):
    
    cmds.confirmDialog(title = "About", message = "Copyright Epic Games, Inc.\nCreated by: Jeremy Ernst\njeremy.ernst@epicgames.com\nVisit www.epicgames.com", icon = "information")
    
#############################################################################################
#############################################################################################
#############################################################################################
def editCharacter(*args):
    
    if cmds.window("artEditCharacterUI", exists = True):
        cmds.deleteUI("artEditCharacterUI")
    
    window = cmds.window("artEditCharacterUI", w = 300, h = 400, title = "Edit Character", mxb = False, mnb = False, sizeable = False)
    mainLayout = cmds.columnLayout(w = 300, h = 400, rs = 5, co = ["both", 5])
    
    #banner image
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
	
    cmds.image(w = 300, h = 50, image = mayaToolsDir + "/General/Icons/ART/artBanner300px.bmp", parent = mainLayout)
    
    cmds.text(label = "", h = 1, parent = mainLayout)
    optionMenu = cmds.optionMenu("artProjOptionMenu", label = "Project:", w =290, h = 40, cc = getProjCharacters, parent = mainLayout)
    textScrollList = cmds.textScrollList("artProjCharacterList", w = 290, h = 300, parent = mainLayout)
    button = cmds.button(w = 290, h = 40, label = "Edit Export File", c = editSelectedCharacter, ann = "Edit the character's skeleton settings, joint positions, or skin weights.", parent = mainLayout)
    button2 = cmds.button(w = 290, h = 40, label = "Edit Rig File", c = editSelectedCharacterRig, ann = "Edit the character's control rig that will be referenced in by animation.", parent = mainLayout)
    
    cmds.text(label = "", h = 1)
    
    
    cmds.showWindow(window)
    getProjects()
    getProjCharacters()
    
#############################################################################################
#############################################################################################
#############################################################################################
def getProjects(*args):
    
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
        
    projects = os.listdir(mayaToolsDir + "/General/ART/Projects/")
    for project in projects:
        cmds.menuItem(label = project, parent = "artProjOptionMenu")

#############################################################################################
#############################################################################################
#############################################################################################        
def getProjCharacters(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
        
    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)
    
    cmds.textScrollList("artProjCharacterList", edit = True, removeAll = True)
    characters = os.listdir(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/")
    
    for character in characters:
	if os.path.isfile(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/" + character):
	    if character.rpartition(".")[2] == "mb":
		niceName = character.rpartition(".")[0]
		niceName = niceName.partition("_Export")[0]
		cmds.textScrollList("artProjCharacterList", edit = True, append = niceName)
        
#############################################################################################
#############################################################################################
#############################################################################################
def editSelectedCharacter(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
        
    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)
    character = cmds.textScrollList("artProjCharacterList", q = True, si = True)[0]
    
    cmds.file(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/" + character + "_Export.mb", open = True, force = True)
    cmds.deleteUI("artEditCharacterUI")
    launchSkeletonBuilder()
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def editSelectedCharacterRig(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
        
    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)
    character = cmds.textScrollList("artProjCharacterList", q = True, si = True)[0]
    
    cmds.file(mayaToolsDir + "/General/ART/Projects/" + proj + "/AnimRigs/" + character + ".mb", open = True, force = True)
    cmds.deleteUI("artEditCharacterUI")
    launchSkeletonBuilder()
    

    
#############################################################################################
#############################################################################################
#############################################################################################    
def changeMayaToolsLoc(*args):
    path = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(path):
        os.remove(path)
        cmds.confirmDialog(title = "Change Location", message = "Once you have chosen your new tools location, it is recommended that you restart Maya.", button = "OK")
        cmds.file(new = True, force = True)
        
#############################################################################################
#############################################################################################
#############################################################################################
def launchSkeletonBuilder(*args):
    
    import ART_skeletonBuilder_UI
    reload(ART_skeletonBuilder_UI)
    UI = ART_skeletonBuilder_UI.SkeletonBuilder_UI()
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def launchAddCharacter(*args):
    
    import ART_addCharacter_UI
    reload(ART_addCharacter_UI)
    UI = ART_addCharacter_UI.AddCharacter_UI()
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def launchAnimUI(*args):
    
    import ART_animationUI
    reload(ART_animationUI)
    UI = ART_animationUI.AnimationUI()

#############################################################################################
#############################################################################################
#############################################################################################
def launchEpic(*args):
	
    cmds.launch(web="http://www.epicgames.com")
    
#############################################################################################
#############################################################################################
#############################################################################################
def launchUnreal(*args):
	
    cmds.launch(web="http://www.unrealengine.com")
    
#############################################################################################
#############################################################################################
#############################################################################################
def launchAnimHelp(*args):
	
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):
        
        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()
	
	
    if os.path.exists(mayaToolsDir + "/General/ART/Help/ART_AnimHelp.pdf"):
	cmds.launch(pdfFile = mayaToolsDir + "/General/ART/Help/ART_AnimHelp.pdf")

#############################################################################################
#############################################################################################
#############################################################################################
def launchRigHelp(self, *args):
	
    cmds.launch(web = "https://docs.unrealengine.com/latest/INT/Engine/Content/Tools/MayaRiggingTool/index.html")
	    

#############################################################################################
#############################################################################################
#############################################################################################
def launchLearningVideos(self, *args):
	
    import ART_Help
    reload(ART_Help)
    ART_Help.ART_LearningVideos()
	
	
#############################################################################################
#############################################################################################
#############################################################################################    
def setupScene(*args):
    
    cmds.currentUnit(time = 'ntsc')
    cmds.playbackOptions(min = 0, max = 100, animationStartTime = 0, animationEndTime = 100)
    cmds.currentTime(0)
    
    
    #check for skeleton builder or animation UIs
    if cmds.dockControl("skeletonBuilder_dock", exists = True):
	print "Custom Maya Menu: SetupScene"
	channelBox = cmds.formLayout("SkelBuilder_channelBoxFormLayout", q = True, childArray = True)
	if channelBox != None:
	    channelBox = channelBox[0]
	
	    #reparent the channelBox Layout back to maya's window
	    cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
	    channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
	    channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')
	
	    #edit the channel box pane's attachment to the formLayout
	    cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))
	    
	    
	#print "deleting dock and window and shit"
	cmds.deleteUI("skeletonBuilder_dock")
	if cmds.window("SkelBuilder_window", exists = True):
	    cmds.deleteUI("SkelBuilder_window")	




	
    if cmds.dockControl("artAnimUIDock", exists = True):
	
	channelBox = cmds.formLayout("ART_cbFormLayout", q = True, childArray = True)
	if channelBox != None:
	    channelBox = channelBox[0]
	
	    #reparent the channelBox Layout back to maya's window
	    cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
	    channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
	    channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')
	
	    #edit the channel box pane's attachment to the formLayout
	    cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))
	    
	    
	#print "deleting dock and window and shit"
	cmds.deleteUI("artAnimUIDock")
	if cmds.window("artAnimUI", exists = True):
	    cmds.deleteUI("artAnimUI")
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def autoOpenAnimUI():
    if cmds.objExists("*:master_anim_space_switcher_follow"):
	launchAnimUI()
	
	
#############################################################################################
#############################################################################################
#############################################################################################
def p4GetLatest(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_getLatestRevision(None)
    
    
#############################################################################################
#############################################################################################
#############################################################################################    
def p4CheckOut(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_checkOutCurrentFile(None)
    
#############################################################################################
#############################################################################################
#############################################################################################
def p4GetHistory(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_getRevisionHistory()
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def p4Submit(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_submitCurrentFile(None, None)
    
    
#############################################################################################
#############################################################################################
#############################################################################################
def p4AddSubmit(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_addAndSubmitCurrentFile(None, None)
    
    
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

#LAUNCH SCRIPT JOBS
scriptJobNum2 = cmds.scriptJob(event = ["PostSceneRead", autoOpenAnimUI])
scriptJobNum = cmds.scriptJob(event = ["NewSceneOpened", setupScene])
p4ScriptJob = cmds.scriptJob(event = ["NewSceneOpened", p4ProjectMenu])

    
    
    
    
    
