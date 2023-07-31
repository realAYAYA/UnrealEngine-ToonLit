// Copyright Epic Games, Inc. All Rights Reserved.
// boilerplate.js -- This script should be loaded for every Robomerge page to provide a consistent UI and dataset for each page,
//                   as well as provide common, shared functions to our client-side Javascript.


/********************
 * COMMON FUNCTIONS *
 ********************/

// Global user information map
let robomergeUser

window.showNodeLastChanges = false
window.allowNodeReconsiders = false

// Common mutex variable around displaying error messages
let clearer = null
function clearErrorText() {
	const $err = $('#error')
	$err.addClass('_hidden');
	if (!clearer) {
		clearer = setTimeout(() => {
			if (clearer) {
				$err.empty();
			}
		}, 1000);
	}
}
function setErrorText(text) {
	if (clearer) {
		clearTimeout(clearer);
		clearer = null;
	}
	$('#error').text(text).removeClass('_hidden');
}
function setError(xhr, error) {
	setErrorText(createErrorMessage(xhr, error))
}
function createErrorMessage(xhr, error) {
	if (xhr.responseText)
		return `${xhr.status ? xhr.status + ": " : ""}${xhr.responseText}`
	else if (xhr.status == 0) {
		document.title = "ROBOMERGE (error)";
		return "Connection error";
	}
	else
		return `HTTP ${xhr.status}: ${error}`;
}

// Add way to prompt the user on leave/reload before the perform an action
function addOnBeforeUnload(message) {
	$( window ).on("beforeunload", function() {
		return message
	 })
}
function removeOnBeforeUnload() {
	$( window ).off("beforeunload")
}

function makeClLink(cl, alias) {
	if (typeof(cl) !== "number") {
		return `<span style="font-weight:bolder">${cl}</span>`
	}
	return `<a href="https://p4-swarm.companyname.net/changes/${cl}" target="_blank">${alias ? alias : cl}</a>`
}

function makeSwarmFileLink(depotPath, alias) {
	return `<a href="https://p4-swarm.companyname.net/files/${
		encodeURI(depotPath.endsWith('/') ? depotPath.slice(2, -1) : depotPath.slice(2)) // Remove '//' (and any trailing slash) to ensure the URL is valid
	}#commits" target="_blank">${alias ? alias : depotPath}</a>`
}

function debug(message) {
	log('DEBUG: ' + message)
}

function error(message) {
	log('ERROR!: ' + message + '\n' + (new Error()).stack)
}

function log(message) {
	const now = new Date(Date.now())

	console.log(
		now.toLocaleString(undefined, {dateStyle: 'short', timeStyle: 'medium', hour12: false}) +
		"." + String(now.getMilliseconds()).padStart(3, '0') +
		": " + message
	)
}

// Reload the branchlist div
let updateBranchesPending = false;
function updateBranchList(graphBotName=null) {
	if (updateBranchesPending)
		return;
	updateBranchesPending = true;
	let expandedDivIds = []
	$('#branchList .elist-row>.collapse.show').each( (_index, div) => {
		expandedDivIds.push(div.id)
	})

	// Keep the height value pre-refresh
	const beginningBLHeight = $('#branchList').height()

	// Capture the current tab (if any) so we can swap back to it post-refresh if it still exists
	const currentBotNavLinkId = $('.nav-item.nav-item-active>a.nav-link.botlink.active.show').attr("id")

	// Prevent window resizing and scrolling
	$('#branchList').css('min-height', beginningBLHeight + 'px')
	getBranchList(function(data) {
		if (data && data.started) {
			preprocessData(data)

			// clear 'error' or 'stopped' from title if present
			document.title = 'ROBOMERGE';

			$('div#currentlyRunning').empty()
			let newBranchList = $('<div id="branchList">').append(renderBranchList(data))
			expandedDivIds.forEach( (divId) => {
				const escapedDivId = divId.replace(/\./g, '\\.')
				// Show the div again
				const div = newBranchList.find(`#${escapedDivId}`)
				if (div.hasClass('collapse') && !div.hasClass('show')) {
					div.addClass('show')
				}
				
				// Ensure the caret icon in the link is correct
				const collapseControlIcon = newBranchList.find(`.nrow .namecell a[data-target="#${escapedDivId}"]>i`)
				if (collapseControlIcon.length === 1 && collapseControlIcon.hasClass("fa-caret-right")) {
					collapseControlIcon.removeClass("fa-caret-right")
					collapseControlIcon.addClass("fa-caret-down")
				}
			})

			// Prevent window resizing and scrolling
			newBranchList.css('min-height', beginningBLHeight + 'px')
			$('#branchList').replaceWith(newBranchList)
			$('body').trigger('branchListUpdated')
		}
		else {
			const bl = $('#branchList').empty()

			$('<div>')
				.text('Cannot communicate with Robomerge.')
				.appendTo(bl)
			
			$('<button>')
				.addClass('btn btn-xs robomerge-crash-buttons')
				.text("View Logs")
				.click(function() {
					window.location.href = "/api/last_crash";
				})
				.appendTo(bl)
		}

		// highlight requested link
		if (graphBotName) {
			const botlinkhash = `#link-${graphBotName.toUpperCase()}`
			//debug(`Received request to see ${botlinkhash} on refresh`)
			$(botlinkhash).click();
		}
		// If we found an active bot prior to this refresh, click back to it now
		else if (currentBotNavLinkId && $('#' + currentBotNavLinkId).length === 1) {
			//debug('Returning to bot tab with ID ' + currentBotNavLinkId)
			$('#' + currentBotNavLinkId).click()
		}
		// Fallback to window hash if it exists
		else if (window.location.hash) {
			const botlinkhash = `#link-${window.location.hash.substr(1).toUpperCase()}`
			//debug(`Using window.location.hash to click ${botlinkhash}`)
			let bot = $(botlinkhash)
			if (bot.length !== 0) {
				bot.click();
			}
			else {
				//debug(`Couldn't find a graph bot tab link for ${botlinkhash}, defaulting to first bot.`)
				$('.botlink').first().click();
			}
		}
		else {
			//debug(`Defaulting to first bot.`)
			$('.botlink').first().click();
		}

		$('#branchList').css('min-height', '')
		updateBranchesPending = false;
	});
}

let opPending = false;
function nodeAPIOp(botname, nodeName, op, successCallback, failureCallback) {
	return genericAPIOp(`/api/op/bot/${encodeURIComponent(botname)}/node/${encodeURIComponent(nodeName)}/op${op}`, successCallback, failureCallback)
}
function edgeAPIOp(botname, nodeName, edgeName, op, successCallback, failureCallback) {
	return genericAPIOp(`/api/op/bot/${encodeURIComponent(botname)}/node/${encodeURIComponent(nodeName)}/edge/${encodeURIComponent(edgeName)}/op${op}`, successCallback, failureCallback)
}

function genericAPIOp(url, successCallback, failureCallback) {
	if (opPending) {
		alert("Operation already pending");
		return;
	}
	opPending = true;
	clearErrorText();

	return $.ajax({
		url,
		type: 'post',
		contentType: 'application/json',
		success: function(data) {
			opPending = false;
			if (successCallback) {
				successCallback(data);
			}
		},
		error: function(xhr, error, status) {
			opPending = false;
			const errMsg = createErrorMessage(xhr, error)
			setErrorText(errMsg);
			if (failureCallback) {
				failureCallback(errMsg);
			} else if (successCallback) {
				// Older behavior when old code doesn't specify a failureCallback
				successCallback(null);
			}
		}
	});
}

function toQuery(queryMap) {
	let queryString = "";
	for (let key in queryMap) {
		if (queryString.length > 0) {
			queryString += "&";
		}
		queryString += encodeURIComponent(key) + '=' + encodeURIComponent(queryMap[key]);
	}
	return queryString;
}

function getBranchList(callback) {
	clearErrorText();
	$.ajax({
		url: '/api/branches',
		type: 'get',
		dataType: 'json',
		success: [callback, function(data) {
			robomergeUser = data.user
		}],
		error: function(xhr, error, status) {
			robomergeUser = undefined;
			setError(xhr, error);
			callback(null);
		}
	});
}

function getBranch(botname, branchname, callback) {
	clearErrorText();
	$.ajax({
		url: `/api/bot/${botname}/branch/${branchname}`,
		type: 'get',
		dataType: 'json',
		success: callback,
		error: function(xhr, error, status) {
			setError(xhr, error);
			callback(null);
		}
	});
}

function getUserWorkspaces(callback) {
	clearErrorText();
	$.ajax({
		url: `/api/user/workspaces`,
		type: 'get',
		dataType: 'json',
		success: callback,
		error: function(xhr, error, status) {
			setError(xhr, error);
			callback(null);
		}
	});
}

function handleUserPermissions(user) {
	if (!user) {
		return;
	}

	let isFTE = false;
	if (user.privileges && Array.isArray(user.privileges)) {
		for (const tag of user.privileges) {
			if (tag === 'fte') {
				isFTE = true;
				break;
			}
		}
	}

	// Fulltime employees should see the log buttons.
	if (isFTE) {
		$('#fteButtons').show();
	}
}

// Adds a message along with bootstrap alert styles
// https://www.w3schools.com/bootstrap/bootstrap_ref_js_alert.asp
function displayMessage(message, alertStyle, closable = true) {
	// Focus user attention
	window.scrollTo(0, 0);

	let messageDiv = $(`<div class="alert ${alertStyle} fade in show" role="alert">`)

	// Add closable button
	if (closable) {
		messageDiv.addClass("alert-dismissible")

		// Display 'X' button
		let button = $('<button type="button" class="close" data-dismiss="alert" aria-label="Close">')
		button.html('&times;')
		messageDiv.append(button)

		// Fade out after 10 seconds
		setTimeout(function() {
			messageDiv.slideUp().alert('close')
		}, 10000)
	}

	messageDiv.append(message)

	$('#status_message').append(messageDiv)
}
// Helper functions wrapping displayMessage()
function displaySuccessfulMessage(message, closable = true) {
	displayMessage(`<strong>Success!</strong> ${message}`, "alert-success", closable)
}
function displayInfoMessage(message, closable = true) {
	displayMessage(`${message}`, "alert-info", closable)
}
function displayWarningMessage(message, closable = true) {
	displayMessage(`<strong>Warning:</strong> ${message}`, "alert-warning", closable)
}
function displayErrorMessage(message, closable = true) {
	displayMessage(`<strong>Error:</strong> ${message}`, "alert-danger", closable)
}

// This function takes string arrays of key names (requiredKeys, optionalKeys) and attempts to get them from the query parameters.
// If successful, returns an object of { key: value, ... }
// If failure, displays an error message and returns null
function processQueryParameters(requiredKeys, optionalKeys) {
	let urlParams = new URLSearchParams(window.location.search);
	let returnObject = {}

	requiredKeys.forEach(keyName => {
		// These should have been vetted by roboserver.ts prior to getting here, but shouldn't assume
		if (!urlParams.has(keyName)) {
			displayErrorMessage(`Query parameter '${keyName}' required for operation. Aborting...`)
			return null
		} else {
			returnObject[keyName] = urlParams.get(keyName)
		}
	});

	if (optionalKeys) {
		optionalKeys.forEach(keyName => {
			if (urlParams.has(keyName)) {
				returnObject[keyName] = urlParams.get(keyName)
			}
		})
	}

	return returnObject
}

// Creates a consistent UI for our pages
function generateRobomergeHeader(createSignedInUserDiv = true) {
	// Create top-right div which contains logged-in user information and Robomerge uptime/version info
	let topright = $('<div id="top-right">')
	if (createSignedInUserDiv) {
		let loggedInUser = $('<div id="signed-in-user" class="initiallyHidden">')
		loggedInUser.append('<span><i class="fas fa-lg fa-user"></i></span>')
		loggedInUser.append('<span class="user-name"></span><span class="tags"></span>')
		
		let logOutButton = $('<button id="log-out" class="btn btn-xs btn-warning">Sign out</button>')
		logOutButton.click(function() {
			document.cookie = 'auth=; path=; redirect_to=;';
			window.location.href = '/login';
		})
		loggedInUser.append(logOutButton)

		topright.append(loggedInUser)
		topright.append('<div id="uptime"></div>')
		topright.append('<div id="version"></div>')
	}

	const bigHelpButton = $('<button>')
		.addClass('btn btn-outline-dark big-help')
		.click(function() { window.location.href='/help' })
		.html('<strong>Help Page</strong>')
	topright.append(bigHelpButton)

	// Show Robomerge logo
	let logo = $('<img id="logoimg" src="/img/logo.png">')
		.click(function() {
			// Provide some secret functionality to the homepage
			if (window.location.pathname === "/") {
				if (event.ctrlKey) {
					stopBot();
				} else {
					updateBranchList();
				}

				return false;
			} else {
				window.location.href = '/';
			}
		})
	
	const logoDiv = $('<div id="logodiv">')

	// Provide some secret functionality to the homepage
	if (window.location.pathname === "/") {
		logo.click(function() {
			if (event.ctrlKey) {
				stopBot();
			} else {
				updateBranchList();
			}

			return false;
		}).appendTo(logoDiv)
	} 
	// If we're not on the homepage, make the robomerge icon a link to the homepage
	else {
		$('<a href="/">').append(logo).appendTo(logoDiv)
	}

    // Empty div for displaying messages to the user
    let statusMessageDiv = $('<div id="status_message">')

	// Create header if one does not exist
	let header
	if ($("header").length == 0) {
		$('head').after($('<header>'))
	}
	
	$('header').replaceWith($('<header>').append(topright, [logoDiv, $('<hr>'), statusMessageDiv]))
}
function generateRobomergeFooter() {
	let fixedFooterContents = $('<div class="button-bar">')

	let fteButtonDiv = $('<div id="fteButtons">')
	fteButtonDiv.hide()
	fixedFooterContents.append(fteButtonDiv)

	let logButton = $('<button id="logButton">')
	logButton.addClass("btn btn-sm btn-outline-dark")
	logButton.click(function() { window.open('/api/logs', '_blank') })
	logButton.text("Logs")
	fteButtonDiv.append(logButton)

	let lastCrashButton = $('<button id="lastCrashButton">')
	lastCrashButton.addClass("btn btn-sm btn-outline-dark")
	lastCrashButton.click(function() { window.open('/api/last_crash', '_blank') })
	lastCrashButton.text("Last Crash")
	fteButtonDiv.append(lastCrashButton)

	let p4TasksButton = $('<button id="p4TasksButton">')
	p4TasksButton.addClass("btn btn-sm btn-outline-dark")
	p4TasksButton.click(function() { window.open('/api/p4tasks', '_blank') })
	p4TasksButton.text("P4 Tasks")
	fteButtonDiv.append(p4TasksButton)

	let p4AllBotsButton = $('<button id="p4AllBotsButton">')
	p4AllBotsButton.addClass("btn btn-sm btn-outline-dark")
	p4AllBotsButton.click(function() { window.open('/allbots', '_blank') })
	p4AllBotsButton.text("All bots graph")
	fteButtonDiv.append(p4AllBotsButton)

	let branchesButton = $('<button id="branchesButton">')
	branchesButton.addClass("btn btn-sm btn-outline-dark")
	branchesButton.click(function() { window.open('/api/branches', '_blank') })
	branchesButton.text("Branch Data")
	fteButtonDiv.append(branchesButton)

	let currentlyRunningDiv = $('<div id="currentlyRunning">')
	fixedFooterContents.append(currentlyRunningDiv)

	let rightButtonDiv = $('<div class="button-bar-right">')
	fixedFooterContents.append(rightButtonDiv)

	let helpButton = $('<button id="helpButton">')
	helpButton.addClass("btn btn-sm btn-outline-dark")
	helpButton.click(function() { window.location.href='/help' })
	helpButton.html('<strong>Help Page</strong>')
	rightButtonDiv.append(helpButton)

	let contactButton = $('<button id="contactButton">')
	contactButton.addClass("btn btn-sm btn-outline-dark")
	contactButton.click(function() { window.location.href='/contact' })
	contactButton.html('Contact Us')
	rightButtonDiv.append(contactButton)
	
	if ($('footer#fixed-footer').length == 0) {
		$('html').append($('<footer id="fixed-footer">'))
	}
	
	$('footer#fixed-footer').replaceWith($('<footer id="fixed-footer">').append(fixedFooterContents))
}

function checkDate(beginning, end, check) {
	return beginning <= check && check <= end
}

function processHolidays() {
	const now = new Date()
	const currentYear = now.getFullYear()

	// Valentine's Day
	if (checkDate(
		new Date(currentYear, 1, 8, 0, 0, 0),
		new Date(currentYear, 1, 15, 0, 0, 0),
		now)) {
		$('img#logoimg').first().attr('src', '/img/logo-valentines.png').attr('title', "Happy Valentine's Day!")
		$('body').first().addClass('valentines')
	}
	// St. Patrick's Day
	else if (checkDate(
		new Date(currentYear, 2, 11, 0, 0, 0),
		new Date(currentYear, 2, 18, 0, 0, 0),
		now)) {
		$('img#logoimg').first().attr('src', '/img/logo-stpatricks.png').attr('title', "Happy St. Paddy's Day!")
		$('body').first().addClass('stpatricks')
	}
	// Hallowe'en
	else if (checkDate(
		new Date(currentYear, 9, 29, 0, 0, 0),
		new Date(currentYear, 10, 2, 0, 0, 0),
		now)) {
		$('img#logoimg').first().attr('src', '/img/logo-halloween.png').attr('title', "Happy Halloween!")
		$('body').first().addClass('halloween')
	}
	// Christmas
	else if (checkDate(
		new Date(currentYear, 11, 23, 0, 0, 0),
		new Date(currentYear, 11, 28, 0, 0, 0),
		now)) {
		const $logo = $('img#logoimg')
		$logo.first().attr('src', '/img/logo-holiday.png').attr('title', "Happy Holidays!")
		$('body').first().add($logo).addClass('holiday')
	}
	// Star Wars Day
	else if (checkDate(
		new Date(currentYear, 4, 3, 0, 0, 0),
		new Date(currentYear, 4, 5, 0, 0, 0),
		now)) {
		const $logo = $('img#logoimg')
		$logo.first().attr('src', '/img/logo-starwars.png').attr('title', "May the 4th Be With You!")
		$('body').first().add($logo).addClass('starwars')
	}
}

/*********************************
 * BRANCH TABLE HELPER FUNCTIONS *
 *********************************/

 // Helper functions for consistency's sake
function getGraphTableDivID(graphName) {
	return `${graphName}-Table`
}
function getNodeRowId(botName, nodeName) {
	return `${botName}-${nodeName}`
}
function getNodeEdgesRowId(botName, nodeName) {
	return `${getNodeRowId(botName, nodeName)}-Edges`
}
function getEdgeRowId(botName, nodeName, targetBranch) {
	return `${getNodeRowId(botName, nodeName)}-${targetBranch}`
}
function getAPIName(data) {
	const name = data.def ? data.def.name :
		data.target ? data.target :
		data.name

	return name.toUpperCase()
}
// If we have an explicitly set display name, use it
function getDisplayName(data) {
	return data.display_name ?  data.display_name :
		data.def ? 
			(data.def.display_name ? data.def.display_name : data.def.name) :
		data.name
}

function getConflict(nodeConflictArray, targetBranch=null, cl=null) {
	if (!nodeConflictArray || !Array.isArray(nodeConflictArray)) {
		return null
	}

	return nodeConflictArray.find(function (conflict) {
		const ctarget = conflict.target ? conflict.target.toUpperCase() : null
		return (!targetBranch || ctarget === targetBranch.toUpperCase()) &&
		 	(!cl || conflict.cl === cl)
	})
}

// Helper function to help use format duration status text
function printDurationInfo(stopString, millis) {
	let time = millis / 1000; // starting off in seconds
	if (time < 60) {
		return [`${stopString} less than a minute ago`, 'gray'];
	}
	time = Math.round(time / 60);
	if (time < 120) {
		return [`${stopString} ${time} minute${time < 2 ? '' : 's'} ago`, time < 10 ? 'gray' : time < 20 ? 'orange' : 'red'];
	}

	time = Math.round(time / 60);
	if (time < 36) {
		return [`${stopString} ${time} hours ago`, 'red'];
	}

	return [`${stopString} for more than a day`, 'red'];
}

// Helper function to create action buttons
function createActionButton(buttonText, onClick, title) {
	let button = $('<button class="btn btn-primary-alt">') // 'btn-primary-alt' - This is our own take on the primary button
		.append(buttonText)

	if (onClick) {
		button.click(onClick)
	}

	if (title) {
		button.attr('title', title)
		button.attr('data-title', title)
		button.attr('data-toggle', 'tooltip')
		button.attr('data-placement', 'left')
		button.tooltip({
			trigger: 'hover',
			delay: {show: 300, hide: 100} 
		})
		button.click(function () {
			$(this).tooltip('hide')
		})
	}
	return button

}
// Helper function to create action dropdown menu items
function createActionOption(optionText, onClick, title) {
	let optionLink = $('<a class="dropdown-item" href="#">').append(optionText)

	if (onClick) {
		optionLink.click(onClick)
	}

	if (title) {
		optionLink.attr('title', title)
		optionLink.attr('data-toggle', 'tooltip')
		optionLink.attr('data-placement', 'left')
		// Show slower, Hide faster -- so users can select other options without waiting for the tooltip to disappear
		optionLink.tooltip({ 
			trigger: 'hover', 
			delay: {show: 500, hide: 0} 
		})
		optionLink.click(function () {
			$(this).tooltip('hide')
		})
	}

	return optionLink
}

/**************************
 * BRANCH TABLE FUNCTIONS *
 **************************/

let createdListeners = false
function createGraphTableDiv(graphName=null) {
	let graphTableDiv = $(`<div class="gtable">`)
		
	if (graphName) {
		graphTableDiv.attr('id', getGraphTableDivID(graphName))
	}

	// Event listeners to show the expand and collapsed caret icon
	graphTableDiv.on("show.bs.collapse hide.bs.collapse", ".collapse", function() {
		const divId = $(this).attr("id")
		if (!divId) {
			log("ERROR: Div has no id?")
			return;
		}
		const divLinkIconArray = $(`a[data-target="#${divId}"] i`)
		if (divLinkIconArray.length === 0) {
			log("ERROR: Div has no toggle link?")
			return
		}
		
		const divLink = divLinkIconArray[0]

		// When these events are triggered, the classes haven't been updated yet. So we check the inverse
		if ($(this).hasClass("show")) {
			$(divLink).removeClass("fa-caret-down")
			$(divLink).addClass("fa-caret-right")
		} 
		else {
			$(divLink).removeClass("fa-caret-right")
			$(divLink).addClass("fa-caret-down")
		}		
	})

	graphTableDiv.find('.nrow:nth-child(even)').addClass("nrow-zebra")
	graphTableDiv.find('.erow').find(':nth-child(even)').addClass("erow-zebra")

	if (!createdListeners) {
		$(onDisplayTable)
		window.addEventListener("resize", drawAllCanvases);
		createdListeners = true
	}

	return graphTableDiv
}

function onDisplayTable() {

	/*
	 * NOTE:
	 * Technically we don't need to stripe everytime we display a table --
	 * but this saves us an additional function call when finishing creating 
	 * table rows.
	 */

	// CSS3 can't do nth-of-type with a class selector
	$('.gtable').each(function (_index, table) {
		$(table).find('.nrow:even').addClass("nrow-zebra")
	})
	// We want zebra striping to match the nrow it's attached to
	$(".gtable .etable").each(function(index, element) {
		$(element)
			.find(index % 2 === 0 ? ".erow:even" : ".erow:odd")
			.addClass("erow-zebra");
	})


	// Always redraw the canvases on display because drawing them while they are hidden
	// by bootstrap nav produces buggy results
	drawAllCanvases()
}


// Create a header scheme for graph table data
function createGraphTableHeaders(includeActions) {
	let headerRow = $('<div class="gthead-row">')

	headerRow.append($('<div class="namecell">').text('Branch Name'))
	headerRow.append($('<div class="statuscell">').text('Status'))

	if (includeActions) {
		headerRow.append($('<div class="actionscell">').text('Actions'))
	}

	headerRow.append($('<div class="lastchangecell">').text('Last Change'))

	return headerRow
}
/*
 * For use in conjunction with createBranchGraphHeaders(), render a table row element for a given NodeBot and branch.
 * branchData should be the specific branch object from getBranchList() (i.e. def= {...}, bot = "...", etc.)
*/
function createNodeAndEdgeRowsForBranch(branchData, includeActions, singleEdgeData=null) {
	let returnArray = []

	// Create node row
	let nodeRow = $(`<div id="${getNodeRowId(branchData.bot, branchData.def.name)}" class="nrow">`)

	// Create node row -- if we're single edge, we shouldn't include collapse controls
	nodeRow.append(createNodeRow(branchData, includeActions, !!!singleEdgeData))
	returnArray.push(nodeRow)

	let edgeListingRow = $(`<div class="elist-row">`)
	let collapseDiv = $(`<div id="${getNodeEdgesRowId(branchData.bot, branchData.def.name)}" class="${!!!singleEdgeData ? 'collapse' : '' }">`).appendTo(edgeListingRow)
	let cardDiv = $('<div class="card card-body">').appendTo(collapseDiv)
	// Add empty canvas to card div
	$('<div class="elist-canvas">').appendTo(cardDiv)
	// Create edge table
	let etableDiv = $('<div class="etable">').appendTo(cardDiv)
	

	// If we only have one edge, we should only render that edge
	if (singleEdgeData) {
		let singleEdgeRow = $(`<div class="erow" id="${getEdgeRowId(branchData.bot, branchData.def.name, singleEdgeData.target)}">`)
			.appendTo(etableDiv)
		singleEdgeRow.append(createEdgeRow(branchData, singleEdgeData, includeActions))
	}
	else {
		Object.keys(branchData.edges).forEach( (edgeKey) => {
			const edgeData = branchData.edges[edgeKey]
			// If one of our edges needs attention, ensure we start out expanded
			if (edgeData.blockage && !collapseDiv.hasClass('show')) {
				collapseDiv.addClass('show')
			}
	
			// make a row containing:
			let edgeRow = $(`<div class="erow" id="${getEdgeRowId(branchData.bot, branchData.def.name, edgeData.target)}">`).appendTo(etableDiv)
			edgeRow.append(createEdgeRow(branchData, edgeData, includeActions))
		})
	}

	// Append the finished row to the supplied table body
	returnArray.push(edgeListingRow)
	return returnArray
}


// Helper function to create consistent columns between nodes
function createNodeRow(nodeData, includeActions, includeCollapseControls=true) {
	let columnArray = []
	let conflict = null
	if (nodeData.is_blocked) {
		conflict = getConflict(nodeData.conflicts, nodeData.blockage.targetBranchName, nodeData.blockage.change)
	}

	// Branch Name Column 
	columnArray.push(renderNameCell_Common(nodeData, nodeData.bot))
	postRenderNameCell_Node(columnArray[0], nodeData, includeCollapseControls, conflict)
	
	// Status Column 
	columnArray.push(preRenderStatusCell_Node(nodeData))
	renderStatusCell_Common(columnArray[1], nodeData)

	// Actions
	const operationArgs = [nodeData.bot, getAPIName(nodeData)]
	if (includeActions) {
		columnArray.push(preRenderActionsCell_Node(nodeData))
		renderActionsCell_Common(columnArray[2], nodeData, nodeAPIOp, operationArgs, conflict)
	}

	// Last Change Column 
	if (window.showNodeLastChanges) {
		columnArray.push(renderLastChangeCell_Common(nodeData.bot, nodeData.def.name, nodeData.last_cl, nodeAPIOp, operationArgs))
	}
	else {
		columnArray.push($('<div>').addClass('lastchangecell'))
	}

	return columnArray

}

// Helper function to create consistent columns between edges
function createEdgeRow(nodeData, edgeData, includeActions) {
	let columnArray = []

	// Branch Name Column 
	columnArray.push(renderNameCell_Common(edgeData, nodeData.bot))
	let conflict = getConflict(nodeData.conflicts, edgeData.target, edgeData.blockage ? edgeData.blockage.change : null)
	postRenderNameCell_Edge(columnArray[0], edgeData, conflict)
	
	// Status Column 
	columnArray.push(preRenderStatusCell_Edge(nodeData, edgeData))
	renderStatusCell_Common(columnArray[1], edgeData)

	// Actions
	const operationArgs = [nodeData.bot, getAPIName(nodeData), getAPIName(edgeData)]
	if (includeActions) {
		columnArray.push(preRenderActionsCell_Edge(edgeData))

		if (!conflict && edgeData.is_blocked) {
			// Our edge is blocked but our node has no conflict record for it. (This is a larger issue with RM)
			// Create a mock conflict to get appropriate UI options.
			conflict = {
				cl: edgeData.blockage.change,
				kind: 'Mock Conflict',
				target: edgeData.target,
				targetStream: edgeData.targetStream
			}
		}

		renderActionsCell_Common(columnArray[2], edgeData, edgeAPIOp, operationArgs, conflict)
		renderActionsCell_Edge(columnArray[2], nodeData, edgeData, conflict)
	}

	// Last Change Column 
	let catchupText = null
	if (edgeData.num_changes_remaining > 1) {
		catchupText = `pending: ${edgeData.num_changes_remaining}`
	}
	columnArray.push(renderLastChangeCell_Common(nodeData.bot, nodeData.def.name, edgeData.last_cl, edgeAPIOp,
														operationArgs, catchupText, edgeData.display_name))
	postRenderLastChangeCell_Edge(columnArray[columnArray.length - 1], edgeData)

	return columnArray
}

// Helper function to create branch name column
function renderNameCell_Common(data, botname) {
	let entireNameDiv = $('<div class="namecell">')
	const name = getDisplayName(data)

	// This 'cell' (really just a <div>) is going to contain at least one, possibly two div containers:
	// 1. "topInternalDiv" - The name, status, root, and info button
	// 2. If the node is blocked, it will also have a pause info div below the previous
	let topInternalDiv = $('<div>').appendTo(entireNameDiv)
	$('<div class="element-name">').appendTo(topInternalDiv).text(name)

	// if the branch is active, display status
	if (data.status_msg)
	{
		const statusSince = new Date(data.status_since)
		$('<div class="status-msg">')
			.text(data.status_msg)
			.attr('title', `Since ${statusSince}`)
			.appendTo(topInternalDiv)

		const currentlyRunningDiv = $('div#currentlyRunning')
		if (currentlyRunningDiv.length > 0) {
			currentlyRunningDiv.empty()

			currentlyRunningDiv.html(`<strong>${botname}:${name} Currently Running:</strong> ${data.status_msg}`)
			currentlyRunningDiv.attr('title', `Since ${statusSince}`)
		}
	}

	// Display Reconsidering text if applicable
	if (data.queue && data.queue.length > 0)
	{
		let queueDiv = $('<div class="status-msg">').appendTo(topInternalDiv).append("Reconsidering: ");
		data.queue.forEach( (queueData) => {
			$('<span class="reconsidering badge badge-primary">')
				.appendTo(queueDiv)
				.text(`${queueData.cl}`);
		})
	}

	if (data.is_available) {
		if (data.windowBypass) {
			$('<div>').addClass('status-msg warning')
				.appendTo(topInternalDiv)
				.text('Bypassing window!');
		}
	}
	else {
		const unpauseTime = data.blockage && data.blockage.endsAt ? new Date(data.blockage.endsAt) : 'never';
		let msg = '';

		if (data.is_paused) {
			msg += `Paused by ${data.manual_pause.owner}, please contact before unpausing.<br />`;
		}
		if (data.is_blocked) {
			if (data.blockage.acknowledger) {
				msg += `Acknowledged by ${data.blockage.acknowledger}, please contact for more information.<br />`;
			} else {
				msg += `Blocked but not acknowledged. Possible owner: ${data.blockage.owner}<br />`;
			}
		}

		if (data.is_paused || data.is_blocked) {
			msg += `Will unpause and retry on: ${unpauseTime}.<br />`;
		}

		if (msg) {
			$('<div class="status-msg">')
				.appendTo(topInternalDiv)
				.html($('<p>').html(msg));
		}
	}
	
	// Appending the P4 Rootpath floating to the right side
	let rootPathStr = data.def ? data.def.rootPath :
				   data.rootPath ? data.rootPath :
				   null

	if (rootPathStr) {
		let rootPath = $('<div class="rootpath">')
			.text(rootPathStr)
			.appendTo(topInternalDiv)

		// Append info button to branch name
		//if (data.def.isMonitored) {
			$('<button class="btn btn-xs btn-primary-alt configinfo"><i class="fas fa-info-circle"></i></button>')
				.appendTo(rootPath)
				.click(function() {
					alert(JSON.stringify(data, null, '  '));
				});
		//}
	}

	return entireNameDiv
}
function createPauseDivs(data, conflict) {
	const divs = []

	// Display pause data if applicable
	if (data.is_paused) {
		divs.push($('<div class="info-block pause">')
			.append(
				$('<p>').html(`<h6>Paused by ${data.manual_pause.owner}</h6><span class="pause-div-label">On:</span> ${new Date(data.manual_pause.startedAt).toString()}<br><span class="pause-div-label">Reason:</span> ${data.manual_pause.message}`)
			)
		)
	}

	// Display blockage data if applicable
	if (data.is_blocked) {
		const changelist = conflict && conflict.cl ? conflict.cl :
			data.blockage && data.blockage.change ? data.blockage.change :
			'unknown'
		
		const info = conflict && conflict.kind ? `<span class="pause-div-label">Cause:</span> <strong>${conflict.kind.toLowerCase()}</strong>` :
			data.blockage ? `<span class="pause-div-label">Blocked.</span> Type: ${data.blockage.type}<br /> Message: ${data.blockage.message}` :
			`No info can be provided. Please contact Robomerge help.`

		divs.push($('<div class="info-block conflict">')
			.append(
				$('<p>').html('<h6>Blocked on change ' + makeClLink(changelist) + `</h6> ${info}`)
			)
		)
	}

	return divs
}
function postRenderNameCell_Node(nameCell, nodeData, includeCollapseControls, conflict=null) {
	// For nodes, go through the edges to display a badge next to the name for blocked and paused edges
	let edgesInConflictCount = 0
	let edgesPausedCount = 0
	const edgeNames = Object.keys(nodeData.edges)
	if (nodeData.edges) {
		edgeNames.forEach( (edgeKey) => {
			const edgeData = nodeData.edges[edgeKey]
			if (edgeData.is_blocked || edgeData.retry_cl > 0) {
				edgesInConflictCount++
			}
			else if (edgeData.is_paused) {
				edgesPausedCount++
			}
		})
	}

	const totalBlockages = edgesInConflictCount + !!nodeData.is_blocked
	
	let badges = []
	if (totalBlockages > 0) {

		if (edgesInConflictCount === edgeNames.length) {
			badges.push($('<div>')
				.html('<i class="fas fa-exclamation-triangle"></i>')
				.attr('title', 'ALL EDGES BLOCKED')
				.css('color', 'DARKRED')
			)
		}

		badges.push($('<span class="badge badge-pill badge-danger" title="Edges Conflicted">').text(totalBlockages))
	}
	if (edgesPausedCount > 0) {
		badges.push($('<span class="badge badge-pill badge-warning" title="Edges Paused">').text(edgesPausedCount))
	}

	const nameHeader = nameCell.find('div.element-name').first()
	let collapseLink = null

	if (includeCollapseControls && nodeData.edges && Object.keys(nodeData.edges).length !== 0) {
		// Replace the context with collapse controls
		nameHeader.empty()
		
		const name = getDisplayName(nodeData)
		collapseLink = $('<a role="button" data-toggle="collapse" title="Show/Hide Edges">').appendTo(nameHeader)
		collapseLink.attr('data-target', '#' + getNodeEdgesRowId(nodeData.bot, name))

		let icon = $('<i class="fas fa-lg">').appendTo(collapseLink)
		
		// If we have any edges in conflict, we default to showing the edge table. Display the caret down
		if (edgesInConflictCount > 0) {
			icon.addClass('fa-caret-down')
		} else {
			icon.addClass('fa-caret-right') // Collapsed
		}

		collapseLink.append(`&nbsp;${name}`, badges)
	}
	else {
		nameHeader.append(badges)
	}

	// Append Pause Info
	if (!nodeData.is_available) {
		nameCell.append(createPauseDivs(nodeData, conflict))
	}

	// We need to represent if edges need attention
	if (nodeData.conflicts && nodeData.edges 
		&& Array.isArray(nodeData.conflicts) && Array.isArray(nodeData.edges)
		&& nodeData.conflicts.length >= Object.keys(nodeData.edges).length) {

		// Alert if all edges are blocked by putting a danger icon beside the name
		if (collapseLink) {
			collapseLink.append(dangerSymbol)
		}
		else {
			nameHeader
		}
	}
}
function postRenderNameCell_Edge(nameCell, edgeData, conflict=null) {
	// Append Pause Info
	if (!edgeData.is_available) {
		nameCell.append(createPauseDivs(edgeData, conflict))
	}
}

// Helper function to create status data cell
function preRenderStatusCell_Node(nodeData) {
	// Create holder for the stylized status text
	let statusCell = $('<div class="statuscell">')

	if (!nodeData.def.isMonitored) {
		statusCell.addClass("unmonitored-text")
		statusCell.text('Unmonitored')
	}

	if (nodeData.is_blocked) {
		$('<div class="blockageinfo">')
			.text('ENTIRELY BLOCKED')
			.addClass('important-status')
			.css('color', 'DARKRED')
			.appendTo(statusCell)
	}

	return statusCell
}
function preRenderStatusCell_Edge(nodeData, edgeData) {
	// Create holder for the stylized status text
	let statusCell = $('<div>').addClass('statuscell')

	if (edgeData.is_blocked) {
		$('<div class="blockageinfo">')
			.text('BLOCKED')
			.addClass('important-status')
			.css('color', 'DARKRED')
			.appendTo(statusCell)
	}
	else if (!edgeData.is_paused) {
		// If we're not paused, but the node is paused or blocked, display a message informing users
		// that we'll not be receiving any work.
		if (nodeData.is_paused || nodeData.is_blocked || nodeData.retry_cl) {
			$('<div>')
				.text('NODE ' + (nodeData.is_paused ? 'PAUSED' : 'BLOCKED'))
				.addClass('important-status')
				.css('color', 'ORANGE')
				.prependTo(statusCell)
		}
		else if (edgeData.gateClosedMessage) {
			let msg = edgeData.gateClosedMessage
			if (edgeData.nextWindowOpenTime) {
				const timestamp = Date.parse(edgeData.nextWindowOpenTime)
				if (!isNaN(timestamp)) {
					msg += ` (opens: ${(new Date(timestamp)).toLocaleString()})`
				}
			}
			$('<div>').addClass('status-msg').text(msg).appendTo(statusCell)
		}
	}

	return statusCell
}
function renderStatusCell_Common(statusCell, data) {
	// Don't bother with unmonitored nodes
	if (statusCell.hasClass('unmonitored-text')) {
		return
	}

	if (data.is_paused) {
		let pauseInfo = $('<div>')
			.text('PAUSED')
			.addClass('important-status')
			.css('color', 'ORANGE')
			.prependTo(statusCell)

		let [pausedDurationStr, pausedDurationColor] = printDurationInfo('Paused', 
			Date.now() - new Date(data.manual_pause.startedAt).getTime())
		$('<div class="pause-details">')
			.html(`${pausedDurationStr} by <strong>${data.manual_pause.owner}</strong>`)
			.css('color', pausedDurationColor)
			.insertAfter(pauseInfo)
	}

	if (data.is_blocked) {
		const blockageinfoDiv = statusCell.find('.blockageinfo')
		// If we have an acknowledged date, print acknowledged text
		if (data.blockage.acknowledgedAt) {
			let acknowledgedSince = new Date(data.blockage.acknowledgedAt);
			let [ackDurationStr, ackDurationColor] = printDurationInfo("Acknowledged", Date.now() - acknowledgedSince.getTime())
			$('<div class="blockage-details">')
				.css('color', ackDurationColor)
				.html(`${ackDurationStr} by <strong>${data.blockage.acknowledger}</strong>`)
				.insertAfter(blockageinfoDiv)
		}
		// Determine who is responsible for resolving this.
		else {
			let blockedSince = new Date(data.blockage.startedAt)
			let [blockedDurationStr, blockedDurationColor] = printDurationInfo("Blocked", Date.now() - blockedSince.getTime())
			const pauseCulprit = data.blockage.owner || data.blockage.author || "unknown culprit";

			const blockageDetailsDiv = $('<div class="blockage-details">').insertAfter(blockageinfoDiv)

			$('<div>')
				.css('color', blockedDurationColor)
				.html(blockedDurationStr)
				.appendTo(blockageDetailsDiv)

			$('<div>')
				.html(`Resolver: <strong>${pauseCulprit} (unacknowledged)</strong>`)
				.appendTo(blockageDetailsDiv)
		}
	}

	let text = null, color = 'GREEN'
	// If the node is currently processing (active) display as such
	if (data.retry_cl) {
		text = 'RETRYING'
		color = 'ORANGE'
	}
	else if (data.is_active) {
		text = 'ACTIVE'
	} 
	// Otherwise, if we're available and we haven't populated any data inside statusCell, print that we're running
	else if (data.is_available && statusCell.children().length === 0) {
		text = 'RUNNING'
	}

	if (text) {
		$('<div>')
			.text(text)
			.css('color', color)
			.prependTo(statusCell)
	}

	return statusCell
}

// Helper function to create actions cell
function preRenderActionsCell_Node(nodeData) {
	let actionCell = $('<div class="actionscell">')

	// If the branch is not monitored, we can't provide actions. Simply provide text response and return.
	if (!nodeData.def.isMonitored) {
		actionCell.append($('<div class="unactionable-text">').text("No Actions Available"))
		actionCell.addClass('unactionable')
	}
	else if (!nodeData.edges) {
		actionCell.append($('<div class="unactionable-text">').text("No Edges Configured"))
		actionCell.addClass('unactionable')
	}

	return actionCell
}
function preRenderActionsCell_Edge(_edgeData) {
	return $('<div class="actionscell">')
}
// operationArgs should be a string array with these elements:
// 0 - bot name
// 1 - branch/node name
// 2 - edge name (if applicable)
function renderActionsCell_Common(actionCell, data, operationFunction, operationArgs, conflict=null) {
	// Don't bother with unmonitored nodes
	if (actionCell.hasClass('unactionable-text')) {
		return
	}
	const botname = operationArgs[0]
	const branchNameForAPI = operationArgs[1]
	const optEdgeName = operationArgs[2]
	const dataDisplayName = getDisplayName(data)
	const dataFullName = `${botname}:${dataDisplayName}`

	// Start collecting available actions in our button group



	// Keep track if we have an individual action button forming our default option for a given NodeBot ---
	// this controls how we render the dropdown menu
	let specificActionButton = null
	// If manually paused, the default option should be to unpause it
	if (data.is_paused) {
		specificActionButton = createActionButton(
			`Unpause`,
			function() {
				operationFunction(...operationArgs, '/unpause', function(success) {
					if (success) {
						displaySuccessfulMessage(`Unpaused ${dataDisplayName}.`)
					} else {
						displayErrorMessage(`Error unpausing ${dataFullName}, please check logs.`)
						$('#helpButton').trigger('click')
					}
					updateBranchList(botname)
				})
			},
			`Resume Robomerge monitoring of ${dataDisplayName}`
		)
	}
	// If the whole node is paused on a blocker, the acknowledge functions should be the default options
	else if (data.is_blocked) {
		let ackQueryData = toQuery({
			cl: data.blockage.change
		})

		let ackFunc = function() {
			operationFunction(...operationArgs, '/acknowledge?' + ackQueryData, 
				function(success) {
					displaySuccessfulMessage(`Acknowledged blockage in ${dataFullName}.`)
					updateBranchList(botname)
				},
				function(failure) {
					displayErrorMessage(`Error acknowledging blockage in ${dataFullName}, please contact Robomerge support. Error message: ${failure}`)
					$('#helpButton').trigger('click')
					updateBranchList(botname)
				}
			)
		}
		let unackFunc = function() {
			operationFunction(...operationArgs, '/unacknowledge?' + ackQueryData, 
				function(success) {
					displaySuccessfulMessage(`Unacknowledged blockage in ${dataFullName}.`)
					updateBranchList(botname)
				},
				function(failure) {
					displayErrorMessage(`Error unacknowledging blockage in ${dataFullName}, please contact Robomerge support.  Error message: ${failure}`)
					$('#helpButton').trigger('click')
					updateBranchList(botname)
				}
			)
		}

		// Acknowledge
		if (!data.blockage.acknowledger) {
			specificActionButton = createActionButton(
				[$('<i class="fas fa-exclamation-triangle" style="color:yellow">'), '&nbsp;', "Acknowledge"],
				ackFunc,
				'Take ownership of this blockage'
			)
		} 
		// Unacknowledge
		else if (robomergeUser && data.blockage.acknowledger === robomergeUser.userName) {
			specificActionButton = createActionButton(
				[$('<i class="fas fa-eject" style="color:red">'), '&nbsp;', "Unacknowledge"],
				unackFunc,
				'Back-pedal: someone else should fix this'
			)
		}
		// Take ownership
		else {
			specificActionButton = createActionButton(
				[$('<i class="fas fa-hands-helping" style="color:white">'), '&nbsp;', "Take Ownership"],
				ackFunc,
				`Take ownership of this blockage over ${data.blockage.acknowledger}`
			)
		}
	}

	// collect actions that should be added to a drop-down, if any
	const dropdownEntries = []
	if (data.is_blocked) {
		// If this object has a conflict, report it
		if (conflict) {
			const queryParams = toQuery({
				bot: botname,
				branch: branchNameForAPI,
				target: conflict.target,
				cl: conflict.cl
			})

			// Create Shelf
			let shelfRequest = '/op/createshelf?' + queryParams
			if (conflict.targetStream) {
				shelfRequest += '&' + toQuery({ targetStream: conflict.targetStream })
			}
			shelfRequest += location.hash

			const toTargetText = conflict.target ? ' -> ' + conflict.target : '';
			const shelfOption = createActionOption('Create Shelf for ' + conflict.cl + toTargetText, function() {
				window.location.href = shelfRequest;
			}, `Retry merge of ${conflict.cl} and create a shelf in a specified P4 workspace`)
			
			// Stomp
			const stompRequest = `/op/stomp?` + queryParams + location.hash
			const stompOption = createActionOption('Stomp Changes using ' + conflict.cl + toTargetText, function() {
				window.location.href = stompRequest;
			}, `Use ${conflict.cl} to stomp binary changes in ${conflict.target}`);

			// Can only create shelves or perform stomps for merge conflicts
			if (conflict.kind !== 'Merge conflict') {
				shelfOption.addClass("disabled")
				shelfOption.off('click')
				// Bootstrap magic converts 'title' to 'data-original-title'
				shelfOption.attr('data-original-title', `Shelving not available for ${conflict.kind.toLowerCase()}.`)

				stompOption.addClass("disabled")
				stompOption.off('click')
				stompOption.attr('data-original-title', `Stomp not available for ${conflict.kind.toLowerCase()}.`)
			}

			dropdownEntries.push(shelfOption)
			dropdownEntries.push(stompOption)
		}

		// Exposing this section in the case of a blockage but no conflict
		// Retry CL #
		const retryOption = createActionOption(`Retry ${data.blockage.change}`, function() {
			operationFunction(...operationArgs, '/retry', function(success) {
				if (success) {
					displaySuccessfulMessage(`Resuming ${dataDisplayName} and retrying ${data.blockage.change}`)
				} else {
					displayErrorMessage(`Error resuming ${dataDisplayName} and retrying ${data.blockage.change}, please check logs.`)
					$('#helpButton').trigger('click')
				}
				updateBranchList(botname)
			})
		},  `Resume Robomerge monitoring of ${dataDisplayName} and retry ${data.blockage.change}`)
		dropdownEntries.push(retryOption)
		
		dropdownEntries.push('<div class="dropdown-divider">')
	}

	if (!data.is_paused) {
		// Pause Option
		const pauseOption = createActionOption(`Pause ${dataDisplayName}`, function() {
			let pauseReasonPrompt = promptFor({
				msg: `Why are you pausing ${dataDisplayName}?`}
			);
			if (pauseReasonPrompt) {
				pauseReason = pauseReasonPrompt.msg.toString().trim()
				// Request a reason.
				if (pauseReason === "") {
					displayErrorMessage(`Please provide reason for pausing ${dataFullName}. Cancelling pause request.`)
					return
				}

				operationFunction(...operationArgs, "/pause?" + toQuery(pauseReasonPrompt), function(success) {
					if (success) {
						displaySuccessfulMessage(`Paused ${dataDisplayName}: "${pauseReasonPrompt.msg}"`)
					} else {
						displayErrorMessage(`Error pausing ${dataDisplayName}, please check logs.`)
						$('#helpButton').trigger('click')
					}
					updateBranchList(botname)
				})
			}
		},  `Pause Robomerge monitoring of ${dataDisplayName}`)	
		dropdownEntries.push(pauseOption)
	}

	if (optEdgeName || window.allowNodeReconsiders) {
		// Add Reconsider action
		const reconsiderOption = createActionOption(`Reconsider...`, function() {
			let data = promptFor({
				cl: `Enter the CL to reconsider (should be a CL from ${dataDisplayName}):`
			});
			if (data) {
				// Ensure they entered a CL
				if (isNaN(parseInt(data.cl))) {
					displayErrorMessage("Please provide a valid changelist number to reconsider.")
					return
				}

				operationFunction(...operationArgs, "/reconsider?" + toQuery(data), function(success) {
					if (success) {
						displaySuccessfulMessage(`Reconsidering ${data.cl} in ${dataFullName}...`)
					} else {
						displayErrorMessage(`Error reconsidering ${data.cl}, please check logs.`)
						$('#helpButton').trigger('click')
					}
					updateBranchList(botname)
				});
			}
		}, `Manually submit a CL to Robomerge to process (should be a CL from ${dataDisplayName})`)
		dropdownEntries.push(reconsiderOption)
	}

	// if no available actions, we're done
	if (!specificActionButton && dropdownEntries.length === 0) {
		return
	}

	const buttonGroup = $('<div class="btn-group">').appendTo(actionCell)
	// Ensure we don't do a refresh of the branch list while displaying a dropdown
	buttonGroup.on('show.bs.dropdown', function() {
		// Check to see if we have our mutex function
		if (typeof pauseAutoRefresh === 'function') {
			pauseAutoRefresh()
		}
	})
	buttonGroup.on('hidden.bs.dropdown', function() {
		// Check to see if we have our mutex function
		if (typeof resumeAutoRefresh === 'function') {
			resumeAutoRefresh()
		}
	})

	if (specificActionButton) {
		buttonGroup.append(specificActionButton)
	}

	if (dropdownEntries.length > 0) {
		// Create dropdown menu button
		const actionsDropdownButton = $('<button class="btn dropdown-toggle" data-toggle="dropdown">')
		// If there is already a button, we will append this no-text button to the end of it

		if (specificActionButton) {
			actionsDropdownButton.addClass('btn-primary-alt') // This is our own take on the primary button
			actionsDropdownButton.addClass('dropdown-toggle-split')
		}
		else {
			// If we have no conflicts, simply label our non-emergency actions
			actionsDropdownButton.text('Actions\n')
			actionsDropdownButton.addClass('btn btn-secondary-alt') // This is our own take on the secondary  button
		}
		actionsDropdownButton.appendTo(buttonGroup)

		const optionsList = $('<div class="dropdown-menu">').appendTo(buttonGroup)
		for (const entry of dropdownEntries) {
			optionsList.append(entry)
		}
	}

	return
}

function renderActionsCell_Edge(actionCell, nodeData, edgeData, conflict=null) {
	const edgeName = getDisplayName(edgeData)
	const optionsList = $(actionCell).find(".btn-group>.dropdown-menu")

	if (optionsList.length !== 1) {
		error('Could not complete rendering action cell for edge ' + edgeName + '.')
		return
	}

	const insertFunction = function(element) {
		// Try to insert the skip option as the last contextual operation, signified by the dropdown divider
		let divider = optionsList.find('.dropdown-divider')
		if (divider.length !== 1) {
			// If we for some reason can't find the divider, just append
			optionsList.append(element)
		}
		else {
			element.insertBefore(divider)
		}
	}

	let tooManyFilesBlockage = conflict && conflict.kind === 'Too many files'

	if (edgeData.is_blocked && edgeData.blockage && edgeData.blockage.targetBranchName) {
		// Skip CL
		const skipEnabled = tooManyFilesBlockage || !edgeData.disallowSkip
		
		const skipChangelistText = `Skip Changelist ${edgeData.blockage.change}`
		if (skipEnabled) {
			let skipRequest = '/op/skip?' + toQuery({
				bot: nodeData.bot,
				branch: getAPIName(nodeData),
				cl: edgeData.blockage.change,
				edge: edgeData.blockage.targetBranchName
			}) + location.hash

			let tooltip = `Skip past the blockage caused by changelist ${edgeData.blockage.change}. `
			tooltip += 
				tooManyFilesBlockage ?  'Please ensure this large changelist has been integrated before skipping.' :
				"This option should only be selected if the work does not need to be merged or you will merge this work youself."
			const skipOption = createActionOption(skipChangelistText, function() {
				window.location.href = skipRequest;
			}, tooltip)
			insertFunction(skipOption)
		}
		else {

			insertFunction(createActionOption(skipChangelistText, null,
				`Skip has been disabled.`).addClass("disabled").off('click')
			)
		}
	}

	if (edgeData.nextWindowOpenTime) {
		const sense = edgeData.windowBypass ? 'false' : 'true'

		const text = edgeData.windowBypass
			? 'Re-enable gate window'
			: 'Bypass gate window';

		const tooltip = edgeData.windowBypass
			? 'Integrations will be delayed until window is open'
			: 'Integrations will proceed, disregarding specified window';

		const bypassRequest = () => 
			edgeAPIOp(nodeData.bot, getAPIName(nodeData), edgeData.target, '/bypassgatewindow?sense=' + sense, () => {}, () => {});

		insertFunction(createActionOption(text, bypassRequest, tooltip));
	}
}

// Helper function to create last change cell
function renderLastChangeCell_Common(botname, nodename, lastCl, operationFunction, operationArgs, catchupText=null, edgename=null) {
	const lastChangeCell = $('<div>').addClass('lastchangecell')

	const swarmLink = $(makeClLink(lastCl)).appendTo(lastChangeCell)
	if (catchupText) {
		lastChangeCell.append($('<div>').addClass('catchup').text(catchupText))
	}

	// On shift+click, we can set the CL instead
	swarmLink.click(function(evt) {
		if (evt.shiftKey)
		{
			let data = promptFor({
				cl: {prompt: 'Enter CL', default: lastCl},
			})
			if (data) {
				data.reason = "manually set through Robomerge homepage"

				operationFunction(...operationArgs, "/set_last_cl?" + toQuery(data), function(success) {
					if (success) {
						updateBranchList(botname)
						displaySuccessfulMessage(`Successfully set ${edgename || nodename} to changelist ${data.cl}`)
					} else {
						displayErrorMessage(`Error setting ${edgename || nodename} to changelist ${data.cl}, please check logs.`)
						$('#helpButton').trigger('click')
					}
				})
				
			}
			if (evt.preventDefault) {
				evt.preventDefault()
			}
			return false
		}
		return true
	})
	// shift+click end

	return lastChangeCell
}

function prettyDate(date) {
	try {
		/*
			Warning: toLocaleDateString seems to have very different behavior between browsers.
			{dateStyle: "medium",timeStyle: "long"} works fine in Chrome, but not in Firefox.
			When changing this, please test browser compatibility.
		 */
		return date.toLocaleDateString(undefined, {
			month: "short",
			day: "2-digit",
			year: "numeric",
			hour: "2-digit",
			minute: "2-digit",
			second: "2-digit",
			timeZoneName: "short"
		})
	}
	catch (e) {
		return date.toString()
	}
}

function postRenderLastChangeCell_Edge(lastChangeCell, edgeData) {
	if (edgeData.lastGoodCL) {
		let tooltip = 'CL approved by CIS'
		if (edgeData.lastGoodCLDate) {
			tooltip += ` on ${prettyDate(new Date(edgeData.lastGoodCLDate))}`
		}
		if (edgeData.headCL) {
			tooltip += ` (head changelist ${edgeData.headCL})`
		}
		
		let goodCL = edgeData.lastGoodCLJobLink ? $(`<a href="${edgeData.lastGoodCLJobLink}">`).prop('target', '_blank') : $('<div>');
		goodCL.html('\u{2713} ' + edgeData.lastGoodCL).addClass('last-good-cl').prop('title', tooltip).appendTo(lastChangeCell)
	}
}


// Helper functions to wrap createBranchGraphHeaders() and appendBranchGraphRow() and construct a table
function renderSingleBranchTable(branchData, singleEdgeData) {
	let graphTable = createGraphTableDiv()
	graphTable.append(createGraphTableHeaders(false))
	graphTable.append(createNodeAndEdgeRowsForBranch(branchData, false, singleEdgeData))
	return graphTable
}

function drawCanvas(edgeTableRow) {
	const canvasDiv = $(edgeTableRow).find(".elist-canvas");
	const canvasWidth = canvasDiv.outerWidth(true);
  
	// If the div is already collapsed, briefly expand the div so we can accurately draw the canvas
	const collapsingDiv = $(edgeTableRow).children(":first");
	let startExpanded = $(collapsingDiv).hasClass("show");
	let collapsingDivClassesBefore = $(collapsingDiv).attr('class')
	if (!startExpanded) {
	  $(collapsingDiv).addClass('show');
	}
	// Try to find an existing canvas element first. Otherwise create a new one
	let canvas = canvasDiv.find("canvas");
	if (canvas.length === 0) {
		canvas = $("<canvas>").appendTo(canvasDiv);
	}

	// Reset the canvas dimensions on re-draw before we get the div's height.
	// This is to ensure zooms don't ever increase the canvas size
	canvas.attr("width", 0);
	canvas.attr("height", 0);
	const canvasHeight = $(collapsingDiv).find("div.etable").first().outerHeight(true);

	canvas.attr("width", canvasWidth);
	canvas.attr("height", canvasHeight);
	const context = canvas[0].getContext("2d");
  
  	if (canvasHeight > 0) {
  		// set element to be same size as canvas to prevent scaling
		canvas.height(canvasHeight)
	}

	// First begin by clearing out the canvas. This comes in handy when we need to redraw the element
	context.clearRect(0, 0, canvasWidth, canvasHeight);
	context.fillStyle = $(edgeTableRow).find('.erow').css('background-color')
	context.fillRect(0, 0, canvasWidth, canvasHeight)
  
	// Configure line here
	context.lineWidth = 2;
	context.lineJoin = "miter";
	context.shadowBlur = 1;
	context.shadowColor = "DimGray";
  
	// For each edge, we want to draw a line between the top of the parent row (bottom of the node row) to the edge
	let previousY = 0;
	const edgeRows = $(edgeTableRow).find(".erow");
	edgeRows.each(function(_eRowIndex, edgeRow) {
	  const edgeRowHeight = $(edgeRow).outerHeight(true);
	  const edgeRowPos = $(edgeRow).position();
	  const targetY = edgeRowPos.top + edgeRowHeight / 2;
  
	  // Move to the center of the div on the X access, and start where we last left off
	  context.moveTo(canvasWidth / 2, previousY);
  
	  // Draw to the center of the edge row
	  context.lineTo(canvasWidth / 2, targetY);
  
	  // Draw over to the row
	  context.lineTo(canvasWidth - 5, targetY);
  
	  // Draw a short end to the line
	  context.moveTo(canvasWidth - 10, targetY - 5);
	  context.lineTo(canvasWidth - 5, targetY);
	  context.lineTo(canvasWidth - 10, targetY + 5);
  
	  // Finally set the previous Y to our completed line
	  previousY = targetY;
	});
  
	// Draw
	context.stroke();
  
	// If the div was initially collapsed, restore the state
	if (!startExpanded) {
	  $(collapsingDiv).attr("class", collapsingDivClassesBefore);
	}
}
  
function drawAllCanvases(graphTable) {
	// Process each canvas by first finding the Edge Table Row parent
	$('.gtable .elist-row').each(function(_index, edgeTableRow) {
		drawCanvas(edgeTableRow);
	})
}

/******************************
 * END BRANCH TABLE FUNCTIONS *
 ******************************/

function preprocessData(data) {

	for (const node of data.branches) {
		const conflicts = node.conflicts
		if (conflicts.length === 0) {
			continue
		}

		const edges = node.edges
		for (const edgeName of Object.keys(edges)) {
			const edge = edges[edgeName]
			if (!edge.is_blocked && edge.lastBlockage) {
				if (conflicts.find(c => c.cl === edge.lastBlockage && c.target === edge.target.toUpperCase())) {
					edge.retry_cl = edge.lastBlockage
				}
			}
		}

		if (!node.is_blocked && node.lastBlockage) {
			if (conflicts.find(c => c.cl === node.lastBlockage && !c.target)) {
				node.retry_cl = node.lastBlockage
			}
		}
	}
}



/***********************
 * MAIN EXECUTION CODE *
 ***********************/

const onLoginPage = window.location.pathname === "/login"

// Define some standard header and footer for our pages
generateRobomergeHeader(!onLoginPage)
generateRobomergeFooter()

// Festive fun! (process after page load with jQuery ready())
$(function() {
	processHolidays()
})

// Since we are on the login page, do not show login information
if (!onLoginPage) {
	// This call populates user information and uptime statistics UI elements.
	getBranchList(function(data) {
		let uptime = $('#uptime').empty();
		if (data) {
			clearErrorText();
			handleUserPermissions(data.user);

			if (data.started){
				uptime.append($('<b>').text("Running since: ")).append(new Date(data.started).toString());
			}
			else {
				document.title = "ROBOMERGE (stopped)" + document.title
				setErrorText('Not running');
			}

			if (data.user) {
				const $container = $('#signed-in-user').removeClass('hidden');
				$('.user-name', $container).text(data.user.displayName);
				$('.tags', $container).text(data.user.privileges && Array.isArray(data.user.privileges) ? ` (${data.user.privileges.join(', ')})` : '');

				if (data.insufficientPrivelege) {
					setErrorText('There are bots running but logged in user does not have admin access');
				}

			}
			else {
				$('#signed-in-user').addClass('hidden');
			}

			if (data.version) {
				$('#version').text(data.version);
			}
		}
	});
}
