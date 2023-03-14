import { BotStatusFields, BranchStatus, ConflictStatusFields, EdgeStatusFields, NodeStatusFields } from "../../src/robo/status-types"
// import { drawCanvas, drawAllCanvases } from "./canvas"
import { clearErrorText, setErrorText, setError, createErrorMessage } from "./ui"
import { nodeAPIOp } from "./ops"
import { BranchDefForStatus, User, UserStatusData } from "../../src/robo/status-types"

type GraphvizElement = {
	elem: JQuery
	cl: number
}

const branchGraphs = new Map<string, GraphvizElement>();


/********************
 * COMMON FUNCTIONS *
 ********************/

// Global user information map
let robomergeUser: User | null = null


function toQuery(queryMap: { [key: string]: string | number }) {
	let queryString = "";
	for (let key in queryMap) {
		if (queryString.length > 0) {
			queryString += "&";
		}
		queryString += encodeURIComponent(key) + '=' + encodeURIComponent(queryMap[key]);
	}
	return queryString;
}

function getBranchList(callback: (data: UserStatusData | null) => void) {
	clearErrorText();
	$.ajax({
		url: '/api/branches',
		type: 'get',
		dataType: 'json',
		success: [callback, function(data) {
			robomergeUser = data.user
		}],
		error: function(xhr, error, status) {
			robomergeUser = null;
			setError(xhr, error);
			callback(null);
		}
	});
}

// Adds a message along with bootstrap alert styles
// https://www.w3schools.com/bootstrap/bootstrap_ref_js_alert.asp
function displayMessage(message: string, alertStyle: string, closable = true) {
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
function displaySuccessfulMessage(message: string, closable = true) {
	displayMessage(`<strong>Success!</strong> ${message}`, "alert-success", closable)
}
function displayInfoMessage(message: string, closable = true) {
	displayMessage(`${message}`, "alert-info", closable)
}
function displayWarningMessage(message: string, closable = true) {
	displayMessage(`<strong>Warning:</strong> ${message}`, "alert-warning", closable)
}
function displayErrorMessage(message: string, closable = true) {
	displayMessage(`<strong>Error:</strong> ${message}`, "alert-danger", closable)
}

function debug(message: string) {
	log('DEBUG: ' + message)
}

function error(message: string) {
	log('ERROR!: ' + message + '\n' + (new Error()).stack)
}

export function log(message: string) {
	const now = new Date(Date.now())

// DateTimeFormatOptions missing dateStyle and typeStyle currently
	console.log(
		now.toLocaleString(undefined, {dateStyle: 'short', timeStyle: 'medium', hour12: false} as any) +
		"." + String(now.getMilliseconds()).padStart(3, '0') +
		": " + message
	)
}

// Reload the branchlist div
export let updateBranchesPending = false;
export function updateBranchList(graphBotName: string | null = null) {
	if (updateBranchesPending)
		return;
	updateBranchesPending = true;
	log(`Updating Branch List...`)
	let expandedDivIds: string[] = []
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
					window.location.href = "/api/logs";
				})
				.appendTo(bl)

			$('<button>')
				.addClass('btn btn-xs btn-danger robomerge-crash-buttons')
				.text("Restart Robomerge")
				.click(startBot)
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

/*********************************
 * BRANCH TABLE HELPER FUNCTIONS *
 *********************************/

function getGraphTableDivID(graphName: string) {
	return `${graphName}-Table`
}
function getNodeRowId(bot: string, node: string) {
	return `${bot}-${node}`
}
function getNodeEdgesRowId(bot: string, node: string) {
	return getNodeRowId(bot, node) + '-Edges'
}

// why sending BranchDefForStatus or BranchStatus? let's try always BranchDefForStatus


function getAPIName(data: EdgeStatusFields | BranchDefForStatus) {
	let name: string

	const target = (data as EdgeStatusFields).target
	if (target) {
		name = target
	}
	else {
		name = (data as BranchDefForStatus).name
	}

	return name.toUpperCase()
}


function makeClLink(cl: string | number, alias?: string) {
	if (typeof(cl) !== "number") {
		return `<span style="font-weight:bolder">${cl}</span>`
	}
	return `<a href="https://p4-swarm.epicgames.net/changes/${cl}" target="_blank">${alias ? alias : cl}</a>`
}

// Helper function to help use format duration status text
function printDurationInfo(stopString: string, millis: number) {
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
function createActionButton(buttonText: any, onClick: () => void, title: string) {
	let button = $('<button>').addClass('btn btn-primary-alt') // 'btn-primary-alt' - This is our own take on the primary button
		.append(buttonText)

	if (onClick) {
		button.click(onClick)
	}

	if (title) {
		button.attr('title', title)
		button.attr('data-title', title)
		button.attr('data-toggle', 'tooltip')
		button.attr('data-placement', 'left')
		button.tooltip({ // fight with TS to define bootstrap plugin funcs later
			trigger: 'hover',
			delay: {show: 300, hide: 100} 
		})
		button.click(function () {
			$(this).tooltip('hide')
		})
	}
	return button

}

function createActionOption(optionText: any, onClick: () => void, title: string) {
	let optionLink = $('<a class="dropdown-item" href="#">').append(optionText)

	if (onClick) {
		optionLink.click(onClick)
	}

	if (title) {
		optionLink.attr('title', title)
		optionLink.attr('data-toggle', 'tooltip')
		optionLink.attr('data-placement', 'left')
		// Show slower, Hide faster -- so users can select other options without waiting for the tooltip to disappear
		;(optionLink as any).tooltip({ 
			trigger: 'hover', 
			delay: {show: 500, hide: 0} 
		})
		optionLink.click(function () {
			($(this) as any).tooltip('hide')
		})
	}

	return optionLink
}

/**************************
 * BRANCH TABLE FUNCTIONS *
 **************************/

let createdListeners = false
function createGraphTableDiv(graphName: string | null = null) {
	const graphTableDiv = $('<div>').addClass('gtable')
		
	if (graphName) {
		graphTableDiv.prop('id', getGraphTableDivID(graphName))
	}

	// Event listeners to show the expand and collapsed caret icon
	graphTableDiv.on('show.bs.collapse hide.bs.collapse', '.collapse', function() {
		const divId = $(this).prop('id')
		if (!divId) {
			log('ERROR: Div has no id?')
			return
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
function createGraphTableHeaders(includeActions: boolean) {
	const headerRow = $('<div>').addClass('gthead-row')

	headerRow.append($('<div>').addClass('namecell').text('Branch Name'))
	headerRow.append($('<div>').addClass('statuscell').text('Status'))

	if (includeActions) {
		headerRow.append($('<div>').addClass('actionscell').text('Actions'))
	}

	headerRow.append($('<div>').addClass('lastchangecell').text('Last Change'))

	return headerRow
}

// create branch name column
function renderNameCell_Common(data: Partial<BotStatusFields>, displayName: string, rootPath: string, botname: string) {

	const newStatusDiv = () => $('<div>').addClass('status-msg')

	const entireNameDiv = $('div').addClass('namecell')

	// This 'cell' (really just a <div>) is going to contain at least one, possibly two div containers:
	// 1. "topInternalDiv" - The name, status, root, and info button
	// 2. If the node is blocked, it will also have a pause info div below the previous
	const topInternalDiv = $('<div>').appendTo(entireNameDiv)
	$('<div>').addClass('element-name').appendTo(topInternalDiv).text(displayName)

	// if the branch is active, display status
	if (data.status_msg) {
		const statusSince = new Date(data.status_since!)
		newStatusDiv()
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
	const queue = (data as Partial<NodeStatusFields>).queue
	if (queue && queue.length > 0) {
		const queueDiv = newStatusDiv().appendTo(topInternalDiv).append('Reconsidering: ')
		for (const element of queue) {
			$('<span>').addClass('reconsidering badge badge-primary')
				.appendTo(queueDiv)
				.text(element.cl)
		}
	}

	if (!data.is_available) {
		const unpauseTime = data.blockage && data.blockage.endsAt ? new Date(data.blockage.endsAt) : 'never'
		let msg = ''

		if (data.is_paused) {
			msg += `Paused by ${data.manual_pause!.owner}, please contact before unpausing.<br />`
		}
		if (data.is_blocked) {
			const blockage = data.blockage!
			if (blockage.acknowledger) {
				msg += `Acknowledged by ${blockage.acknowledger}, please contact for more information.<br />`
			} else {
				msg += `Blocked but not acknowledged. Possible owner: ${blockage.owner}<br />`
			}
		}

		const lastGoodCL = (data as Partial<EdgeStatusFields>).lastGoodCL
		if (data.is_paused || data.is_blocked) {
			msg += `Will unpause and retry on: ${unpauseTime}.<br />`
		}
		else if (lastGoodCL && data.headCL && lastGoodCL === data.last_cl && data.last_cl < data.headCL) {
			msg = 'Waiting on CIS gate'
		}

		if (msg) {
			newStatusDiv()
				.appendTo(topInternalDiv)
				.append($('<p>').html(msg))
		}
	}

	// Appending the P4 Rootpath floating to the right side
	if (rootPath) {
		const rootPathElement = $('<div class="rootpath">')
			.text(rootPath)
			.appendTo(topInternalDiv)

		// Append info button to branch name
		$('<button>').addClass('btn btn-xs btn-primary-alt configinfo')
			.append('<i>').addClass('fas fa-info-circle')
			.appendTo(rootPathElement)
			.click(function() {
				alert(JSON.stringify(data, null, '  '));
			});
	}

	return entireNameDiv
}

function createPauseDivs(data: Partial<BotStatusFields>, conflict: ConflictStatusFields | null) {
	const divs: JQuery[] = []

	// Display pause data if applicable
	if (data.is_paused) {
		const pauseStatus = data.manual_pause!
		divs.push($('<div class="info-block pause">')
			.append(
				$('<p>')
					.append($('<h6>').text('Paused by ' + pauseStatus.owner))
					.append($('<span>').addClass('pause-div-label').text('On:'))
					.append(` ${new Date(pauseStatus.startedAt)}<br>`)
					.append($('<span>').addClass('pause-div-label').text('Reason:'))
					.append(' ' + pauseStatus.message)
		))
	}

	// Display blockage data if applicable
	if (data.is_blocked) {
		const changelist = (conflict && conflict.cl) ||
			(data.blockage && data.blockage.change) ||
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


// function postRenderNameCell_Edge(nameCell, edgeData, conflict=null) {
// 	// Append Pause Info
// 	if (!edgeData.is_available) {
// 		nameCell.append(createPauseDivs(edgeData, conflict))
// 	}
// }

function renderStatusCell_Common(statusCell: JQuery, data: Partial<BotStatusFields>) {
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

		const pauseStatus = data.manual_pause!

		let [pausedDurationStr, pausedDurationColor] = printDurationInfo('Paused', 
			Date.now() - new Date(pauseStatus.startedAt).getTime())
		$('<div class="pause-details">')
			.html(`${pausedDurationStr} by <strong>${pauseStatus.owner}</strong>`)
			.css('color', pausedDurationColor)
			.insertAfter(pauseInfo)
	}

	if (data.is_blocked) {
		const blockageinfoDiv = statusCell.find('.blockageinfo')
		// If we have an acknowledged date, print acknowledged text
		const blockage = data.blockage!
		if (blockage.acknowledgedAt) {
			let acknowledgedSince = new Date(blockage.acknowledgedAt);
			let [ackDurationStr, ackDurationColor] = printDurationInfo("Acknowledged", Date.now() - acknowledgedSince.getTime())
			$('<div class="blockage-details">')
				.css('color', ackDurationColor)
				.html(`${ackDurationStr} by <strong>${blockage.acknowledger}</strong>`)
				.insertAfter(blockageinfoDiv)
		}
		// Determine who is responsible for resolving this.
		else {
			let blockedSince = new Date(blockage.startedAt)
			let [blockedDurationStr, blockedDurationColor] = printDurationInfo("Blocked", Date.now() - blockedSince.getTime())
			const pauseCulprit = blockage.owner || blockage.author || "unknown culprit";

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

// operationArgs should be a string array with these elements:
// 0 - bot name
// 1 - branch/node name
// 2 - edge name (if applicable)
function renderActionsCell_Common(actionCell: JQuery, data: Partial<BotStatusFields>, displayName: string, operationFunction: Function,
										operationArgs: string[], conflict: ConflictStatusFields | null = null) {
	// Don't bother with unmonitored nodes
	if (actionCell.hasClass('unactionable-text')) {
		return
	}
	const botname = operationArgs[0]
	const branchNameForAPI = operationArgs[1]
	const dataFullName = `${botname}:${displayName}`

	// Start collecting available actions in our button group
	let buttonGroup = $('<div class="btn-group">').appendTo(actionCell)
	// Ensure we don't do a refresh of the branch list while displaying a dropdown
	buttonGroup.on('show.bs.dropdown', function() {
		// Check to see if we have our mutex function
		if (typeof (window as any).pauseAutoRefresh === 'function') {
			(window as any).pauseAutoRefresh()
		}
	})
	buttonGroup.on('hidden.bs.dropdown', function() {
		// Check to see if we have our mutex function
		if (typeof (window as any).resumeAutoRefresh === 'function') {
			(window as any).resumeAutoRefresh()
		}
	})


	// Keep track if we have an individual action button forming our default option for a given NodeBot ---
	// this controls how we render the dropdown menu
	let actionButton = null
	// If manually paused, the default option should be to unpause it
	if (data.is_paused) {
		actionButton = createActionButton(
			`Unpause`,
			function() {
				operationFunction(...operationArgs, '/unpause', (success: any | null) => {
					if (success) {
						displaySuccessfulMessage(`Unpaused ${displayName}.`)
					} else {
						displayErrorMessage(`Error unpausing ${dataFullName}, please check logs.`)
						$('#helpButton').trigger('click')
					}
					updateBranchList(botname)
				})
			},
			`Resume Robomerge monitoring of ${displayName}`
		)
	}
	// If the whole node is paused on a blocker, the acknowledge functions should be the default options
	else if (data.is_blocked && data.blockage!.change) {
		const blockage = data.blockage!
		const ackQueryData = toQuery({
			cl: data.blockage!.change
		})

		const ackFunc = () => {
			operationFunction(...operationArgs, '/acknowledge?' + ackQueryData, 
				(success: any | null) => {
					displaySuccessfulMessage(`Acknowledged blockage in ${dataFullName}.`)
					updateBranchList(botname)
				},
				(failure: string) => {
					displayErrorMessage(`Error acknowledging blockage in ${dataFullName}, please contact Robomerge support. Error message: ${failure}`)
					$('#helpButton').trigger('click')
					updateBranchList(botname)
				}
			)
		}
		const unackFunc = () => {
			operationFunction(...operationArgs, '/unacknowledge?' + ackQueryData, 
				(success: any | null) => {
					displaySuccessfulMessage(`Unacknowledged blockage in ${dataFullName}.`)
					updateBranchList(botname)
				},
				(failure: string) => {
					displayErrorMessage(`Error unacknowledging blockage in ${dataFullName}, please contact Robomerge support.  Error message: ${failure}`)
					$('#helpButton').trigger('click')
					updateBranchList(botname)
				}
			)
		}

		// Acknowledge
		if (!blockage.acknowledger) {
			actionButton = createActionButton(
				[$('<i class="fas fa-exclamation-triangle" style="color:yellow">'), '&nbsp;', "Acknowledge"],
				ackFunc,
				'Take ownership of this blockage'
			)
		} 
		// Unackowledge
		else if (robomergeUser && blockage.acknowledger === robomergeUser.userName) {
			actionButton = createActionButton(
				[$('<i class="fas fa-eject" style="color:red">'), '&nbsp;', "Unacknowledge"],
				unackFunc,
				'Relinguish control over this conflict'
			)
		}
		// Take ownership
		else {
			actionButton = createActionButton(
				[$('<i class="fas fa-hands-helping" style="color:white">'), '&nbsp;', "Take Ownership"],
				ackFunc,
				`Take ownership of this blockage over ${blockage.acknowledger}`
			)
		}
	}
	// If the node isn't Paused or Blocked, our action button needn't have an actual function, just provide the dropdown
	
	// Create dropdown menu button
	let actionsDropdownButton = $('<button class="btn dropdown-toggle" data-toggle="dropdown">')
	// If there is already a button, we will append this no-text button to the end of it
	if (actionButton) {
		buttonGroup.append(actionButton)
		actionsDropdownButton.addClass('btn-primary-alt') // This is our own take on the primary button
		actionsDropdownButton.addClass('dropdown-toggle-split')
	}
	// If we have no conflicts, simply label our non-emergency actions
	else {
		// If we have no conflicts, simply label our non-emergency actions
		actionsDropdownButton.text("Actions\n")
		actionsDropdownButton.addClass('btn btn-secondary-alt') // This is our own take on the secondary  button
	}
	actionsDropdownButton.appendTo(buttonGroup)

	// Start collecting dropdown options
	let optionsList = $('<div class="dropdown-menu">').appendTo(buttonGroup)

	
	if (data.is_blocked) {
		const blockage = data.blockage!
		// If this object has a conflict, report it
		if (conflict) {
			const params: any = {
				bot: botname,
				branch: branchNameForAPI,
				cl: conflict.cl
			}
			if (conflict.target) {
				params.target = conflict.target
			}
			const queryParams = toQuery(params)

			// Create Shelf
			let shelfRequest = '/op/createshelf?' + queryParams
			if (conflict.targetStream) {
				shelfRequest += '&' + toQuery({ targetStream: conflict.targetStream })
			}

			const toTargetText = conflict.target ? ' -> ' + conflict.target : '';
			const shelfOption = createActionOption('Create Shelf for ' + conflict.cl + toTargetText, function() {
				window.location.href = shelfRequest;
			}, `Retry merge of ${conflict.cl} and create a shelf in a specified P4 workspace`)
			
			// Stomp
			const stompRequest = `/op/stomp?` + queryParams
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

			optionsList.append(shelfOption)
			optionsList.append(stompOption)
		}

		// Exposing this section in the case of a blockage but no conflict
		// Retry CL #
		const retryOption = createActionOption(`Retry ${blockage.change}`, function() {
			operationFunction(...operationArgs, '/retry', (success: any | null) => {
				if (success) {
					displaySuccessfulMessage(`Resuming ${displayName} and retrying ${blockage.change}`)
				} else {
					displayErrorMessage(`Error resuming ${displayName} and retrying ${blockage.change}, please check logs.`)
					$('#helpButton').trigger('click')
				}
				updateBranchList(botname)
			})
		},  `Resume Robomerge monitoring of ${displayName} and retry ${blockage.change}`)
		optionsList.append(retryOption)
		
		optionsList.append('<div class="dropdown-divider">')
	}

	if (!data.is_paused) {
		// Pause Option
		const pauseOption = createActionOption(`Pause ${displayName}`, function() {
			const pauseReasonPrompt = promptFor({
				msg: `Why are you pausing ${displayName}?`
			})
			if (pauseReasonPrompt) {
				const pauseReason = pauseReasonPrompt.msg.toString().trim()
				// Request a reason.
				if (pauseReason === "") {
					displayErrorMessage(`Please provide reason for pausing ${dataFullName}. Cancelling pause request.`)
					return
				}

				operationFunction(...operationArgs, "/pause?" + toQuery(pauseReasonPrompt), (success: any | null) => {
					if (success) {
						displaySuccessfulMessage(`Paused ${displayName}: "${pauseReasonPrompt!.msg}"`)
					} else {
						displayErrorMessage(`Error pausing ${displayName}, please check logs.`)
						$('#helpButton').trigger('click')
					}
					updateBranchList(botname)
				})
			}
		},  `Pause Robomerge monitoring of ${displayName}`)	
		optionsList.append(pauseOption)
	}

	// Add Reconsider action
	const reconsiderOption = createActionOption(`Reconsider...`, function() {
		let data = promptFor({
			cl: `Enter the CL to reconsider (should be a CL from ${displayName}):`
		})
		if (data) {
			// Ensure they entered a CL
			if (isNaN(parseInt(data.cl))) {
				displayErrorMessage("Please provide a valid changelist number to reconsider.")
				return
			}

			operationFunction(...operationArgs, "/reconsider?" + toQuery(data), (success: any | null) => {
				if (success) {
					displaySuccessfulMessage(`Reconsidering ${data!.cl} in ${dataFullName}...`)
				} else {
					displayErrorMessage(`Error reconsidering ${data!.cl}, please check logs.`)
					$('#helpButton').trigger('click')
				}
				updateBranchList(botname)
			});
		}
	}, `Manually submit a CL to Robomerge to process (should be a CL from ${displayName})`)
	optionsList.append(reconsiderOption)

	return actionCell
}




class NodeRenderer {
	private displayName: string

	constructor(private node: BranchStatus, private includeActions?: boolean, private singleEdgeData?: EdgeStatusFields) {
		this.displayName = this.node.display_name || this.node.def.name
	}

	createNodeAndEdgeRowsForBranch() {
		const rows: JQuery[] = []

		const nodeRow = $('<div>')
			.prop('id', getNodeRowId(this.node.bot, this.node.def.name))
			.addClass('nrow')

		nodeRow.append(this.createNodeRow())
		rows.push(nodeRow)

		const edgeListingRow = $('<div>').addClass('elist-row')
		rows.push(edgeListingRow)

		return rows
	}

	private createNodeRow() {
		const columns: JQuery[] = []
		let conflict = this.node.is_blocked ? this.getConflict(
			this.node.blockage!.targetBranchName,
			this.node.blockage!.change!) : null

		const branchNameColumn = renderNameCell_Common(this.node, this.displayName, this.node.def.rootPath, this.node.bot)
		this.postRenderNameCell_Node(branchNameColumn, conflict)
		columns.push(branchNameColumn)

		const statusColumn = this.preRenderStatusCell_Node()
		renderStatusCell_Common(statusColumn, this.node)
		columns.push(statusColumn)

		if (this.includeActions) {
			const operationArgs = [this.node.bot, getAPIName(this.node.def)]
			const actionsColumn = this.preRenderActionsCell_Node()
			renderActionsCell_Common(actionsColumn, this.node, this.displayName, nodeAPIOp, operationArgs, conflict)
			columns.push(actionsColumn)
		}

		return columns
	}

	private getConflict(targetBranch?: string, cl?: number) {
		const conflicts = this.node.conflicts

		return conflicts && conflicts.find(conflict => {
			const ctarget = conflict.target ? conflict.target.toUpperCase() : null
			return (!targetBranch || ctarget === targetBranch.toUpperCase()) &&
			 	(!cl || conflict.cl === cl)
		})
	}

	private postRenderNameCell_Node(nameCell: JQuery, conflict: ConflictStatusFields | null = null) {
		// For nodes, go through the edges to display a badge next to the name for blocked and paused edges
		let edgesInConflictCount = 0
		let edgesPausedCount = 0

		let numEdges = 0
		const edges = this.node.edges
		if (edges) {
			const edgeNames = Object.keys(edges)
			numEdges = edgeNames.length
			for (const edgeName of edgeNames) {
				const edgeData = edges[edgeName]!
				if (edgeData.is_blocked || (edgeData.retry_cl && edgeData.retry_cl > 0)) {
					++edgesInConflictCount
				}
				else if (edgeData.is_paused) {
					++edgesPausedCount
				}
			}
		}

		const totalBlockages = edgesInConflictCount + (this.node.is_blocked ? 1 : 0)

		const badges: JQuery[] = []
		if (totalBlockages > 0) {

			if (edgesInConflictCount === numEdges) {
				badges.push($('<div>')
					.append($('<i>').addClass('fas fa-exclamation-triangle'))
					.attr('title', 'ALL EDGES BLOCKED')
					.css('color', 'DARKRED')
				)
			}

			badges.push($('<span>').addClass('badge badge-pill badge-danger').prop('title', 'Edges Conflicted').text(totalBlockages))
		}
		if (edgesPausedCount > 0) {
			badges.push($('<span>').addClass('badge badge-pill badge-warning').prop('title', 'Edges Paused').text(edgesPausedCount))
		}

		const nameHeader = nameCell.find('div.element-name').first()
		let collapseLink = null

		// collapse controls
		if (!this.singleEdgeData && numEdges > 0) {
			// Replace the context with collapse controls
			nameHeader.empty()

			collapseLink = $('<a role="button" data-toggle="collapse" title="Show/Hide Edges">').appendTo(nameHeader)
			collapseLink.attr('data-target', '#' + getNodeEdgesRowId(this.node.bot, this.displayName))

			let icon = $('<i>').addClass('fas fa-lg').appendTo(collapseLink)
			
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
		if (!this.node.is_available) {
			nameCell.append(createPauseDivs(this.node, conflict))
		}
	}

	private preRenderStatusCell_Node() {
		// Create holder for the stylized status text
		const statusCell = $('<div>').addClass('statuscell')

		if (!this.node.def.isMonitored) {
			statusCell.addClass("unmonitored-text")
			statusCell.text('Unmonitored')
		}

		if (this.node.is_blocked) {
			$('<div>').addClass('blockageinfo important-status')
				.text('ENTIRELY BLOCKED')
				.css('color', 'DARKRED')
				.appendTo(statusCell)
		}

		return statusCell
	}

	private preRenderActionsCell_Node() {
		const actionCell = $('<div>').addClass('actionscell')

		// If the branch is not monitored, we can't provide actions. Simply provide text response and return.
		if (!this.node.def.isMonitored) {
			actionCell.append($('<div>').addClass('unactionable-text').text('No Actions Available'))
			actionCell.addClass('unactionable')
		}
		else if (!this.node.edges) {
			actionCell.append($('<div>').addClass('unactionable-text').text('No Edges Configured'))
			actionCell.addClass('unactionable')
		}

		return actionCell
	}
}

/******************************
 * END BRANCH TABLE FUNCTIONS *
 ******************************/



// from main.js
type PromptEntry = {
	prompt: string
	default?: string
}

function promptFor(map: { [key: string]: PromptEntry | string }) {
	let result: { [key: string]: string } = {}
	for (const key of Object.getOwnPropertyNames(map)) {
		const entry = map[key]
		let p: string
		let dflt = ''

		if (typeof entry === 'object') {
			dflt = entry.default || ''
			p = entry.prompt || key
		}
		else {
			p = entry as string
		}

		const data = prompt(p, dflt)
		if (data === null) {
			return null
		}
		result[key] = data
	}
	return result
}


function renderBranchList(data: UserStatusData) {
	const branches = data.branches
	branches.sort((lhs, rhs) =>
		lhs.bot.localeCompare(rhs.bot) || 
			lhs.def.upperName.localeCompare(rhs.def.upperName)
	)

	// Create main div and a list that will serve as our naviagation bar
	const mainDiv = $('<div>')
	const navBar = $('<ul>').addClass('nav nav-tabs').prop('role', 'tablist').appendTo(mainDiv)

	// Create div to hold all our tabs (the navBar will toggle between them for us)
	const tabContent = $('<div>').addClass('tab-content').insertAfter(navBar)

	const botStuff = new Map<string, [JQuery, JQuery]>()

	// create tabs
	for (const [botName, botState] of data.botStates) {
		const branchRoot = $('<div>').addClass('tab-pane')
			.prop('role', 'tabpanel')
			.prop('id', botName)
			.appendTo(tabContent)

		// Create tab for NodeBot
		const nodeBotTab = $('<li>').addClass('nav-item').appendTo(navBar)

		const branchLink = $('<a>').addClass('nav-link botlink')
		// does prop work here or do I need attr?
			.prop('data-toggle', 'tab')
			.prop('role', 'tab')
			.prop('id', 'link-' + botName)
			.prop('href', '#' + botName)
			.append($('<b>').text(botName))
			.appendTo(nodeBotTab)

		if (!botState.isRunningBots) {
			const botNameLower = botName.toLowerCase();
			if (botState.lastError) {
				const $restartButton = $('<button>').addClass('btn btn-xs btn-danger').text('Restart').click(() => {
					$restartButton.prop('disabled', true);
					$.post('/api/control/restart-bot/' + botName)
				})

				branchLink.append($('<span>').text('\u2620 ' + botNameLower).addClass('dead-bot'));
				branchRoot.append(
					$('<div>')
					.text(botState.lastError.nodeBot + ' failed:')
					.append($('<pre>').text(botState.lastError.error))
					.append($restartButton)
				)
			}
			else {
				branchLink.text(botNameLower) // not bold
			}

			continue
		}

		// Pips control the display of the number of paused/blocked branches
		// const botPips = new BotPips;
		// botPips.createElements(branchLink);

		// Create table and headers
		const graphTable = createGraphTableDiv(botName).appendTo(branchRoot);
		graphTable.append(createGraphTableHeaders(true))

		// Finally, append branch graph to global branchGraphs Map
		// const graph = $('<div>').appendTo(branchRoot);
		// let graphObj = branchGraphs.get(botName);
		// const branchSpecCl = botState.lastBranchspecCl;
		// if (!graphObj || graphObj.cl !== branchSpecCl) {
		// 	graphObj = {elem: showFlowGraph(branches, botName), cl: branchSpecCl};
		// 	branchGraphs.set(botName, graphObj);
		// }
	
		// graph.append(graphObj.elem);

		botStuff.set(botName, [$('<div>') /*botPips*/, graphTable]);
	}

	for (const branch of branches) {
		const perBotStuff = botStuff.get(branch.bot)
		if (!perBotStuff)
			continue

		const [botPips, graphTable] = perBotStuff;

		const renderer = new NodeRenderer(branch, true /* with actions */)
		graphTable.append(renderer.createNodeAndEdgeRowsForBranch());
	}

	return mainDiv
}

function setVerbose(enabled: boolean) {
	clearErrorText();
	$.ajax({
		url: '/api/control/verbose/'+(enabled?"on":"off"),
		type: 'post',
		success: updateBranchList,
		error: setError
	});
}

function stopBot() {
	if (confirm("Are you sure you want to stop RoboMerge?")) {
		clearErrorText();
		$.ajax({
			url: '/api/control/stop',
			type: 'post',
			success: updateBranchList,
			error: setError
		});
	}
}
function startBot() {
	clearErrorText();
	$.ajax({
		url: '/api/control/start',
		type: 'post',
		success: updateBranchList,
		error: setError
	});
}

function preprocessData(data: UserStatusData) {

	for (const node of data.branches) {
		const conflicts = node.conflicts
		if (!node.edges || !conflicts || conflicts.length === 0) {
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