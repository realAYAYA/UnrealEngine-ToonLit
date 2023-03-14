// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

function promptFor(map) {
	var result = {};
	var success = true;
	$.each(Object.keys(map), function(_, key) {
		if (success) {
			var p = map[key];
			var dflt = "";
			if (typeof(p) === "object") {
				dflt = p.default || "";
				p = p.prompt || key;
			}

			var data = prompt(p, dflt);
			if (data === null) {
				success = false;
			}
			else {
				result[key] = data;
			}
		}
	});
	return success ? result : null;
}

class BotPips {
	constructor() {
		this.blocked = 0;
		this.paused = 0;
		this.$blocked = null;
		this.$paused = null;
	}

	createElements($container) {
		const elem = $('<span class="badge badge-pill">').css('display','none').css('position', 'relative').css('bottom','2px')
		this.$blocked = elem.clone().addClass('badge-danger').appendTo($container)
		this.$paused = elem.clone().addClass('badge-warning').appendTo($container)
	}

	show() {
		if (this.paused !== 0) {
			this.$paused.css('display', '').text(this.paused)
		}

		if (this.blocked !== 0) {
			this.$blocked.css('display', '').text(this.blocked)
		}
	}
}

let branchGraphs = new Map();

function renderBranchList(data) {
	const branches = data.branches
	branches.sort(function(a,b) {
		var comp = a.bot.localeCompare(b.bot);
		if (comp !== 0)
			return comp;
		return a.def.upperName.localeCompare(b.def.upperName);
	});

	// Create main div and a list that will serve as our naviagation bar
	var mainDiv = $('<div>')
	var navBar = $('<ul class="nav nav-tabs" role="tablist">').appendTo(mainDiv);

	// Create div to hold all our tabs (the navBar will toggle between them for us)
	var tabContent = $('<div class="tab-content">').insertAfter(navBar)

	// Go through each NodeBot and display table data
	const botStuff = new Map // name -> NodeBotTab, botPips, table

	// create tabs
	for (const [botName, botState] of data.botStates) {
		const branchRoot = $('<div class="tab-pane" role="tabpanel">')
			.attr('id', botName)
			.appendTo(tabContent)

		// Create tab for NodeBot
		let nodeBotTab = $('<li class="nav-item">').appendTo(navBar);
		// Pips control the display of the number of paused/blocked branches
		const botPips = new BotPips;
		const branchLink = $('<a class="nav-link botlink" data-toggle="tab" role="tab">')
			.attr('id', 'link-' + botName)
			.attr('href', '#' + botName)
			.append($('<b>').text(botName))
			.appendTo(nodeBotTab)

		if (!botState.isRunningBots) {
			const botNameLower = botName.toLowerCase();
			if (botState.lastError) {
				const $restartButton = $('<button>').addClass('btn btn-xs btn-danger').text('Restart').click(() => {
					$restartButton.prop('disabled', true);
					$.post('/api/control/restart-bot/' + botName);
				});

				branchLink.html($('<span>').text('\u2620 ' + botNameLower).addClass('dead-bot'));
				branchRoot.html(
					$('<div>')
					.text(botState.lastError.nodeBot + ' failed:')
					.append($('<pre>').text(botState.lastError.error))
					.append($restartButton)
				);
			}
			else {
				branchLink.text(botNameLower); // not bold
			}

			continue;
		}

		botPips.createElements(branchLink);

		// Create table and headers
		const graphTable = createGraphTableDiv(botName).appendTo(branchRoot);
		graphTable.append(createGraphTableHeaders(true))

		// Finally, append branch graph to global branchGraphs Map
		const graph = $('<div>').appendTo(branchRoot);
		let graphObj = branchGraphs.get(botName);
		const branchSpecCl = botState.lastBranchspecCl;
		if (!graphObj || graphObj.cl !== branchSpecCl) {
			graphObj = {elem: showFlowGraph(branches, {singleBotName: botName}), cl: branchSpecCl};
			branchGraphs.set(botName, graphObj);
		}
	
		graph.append(graphObj.elem);

		botStuff.set(botName, [botPips, graphTable]);
	}

	$.each(branches, function(_, branch) {
		const perBotStuff = botStuff.get(branch.bot);
		if (!perBotStuff)
			return;

		const [botPips, graphTable] = perBotStuff;

		// Append branch to table (with actions column!)
		graphTable.append(createNodeAndEdgeRowsForBranch(branch, true));

		// up the pip count
		if (branch.is_blocked) {
			botPips.blocked++
		}
		if (branch.is_paused) {
			botPips.paused++
		}
		if (branch.edges) {
			$.each(branch.edges, function(_, edge) {
				if (edge.is_blocked) {
					botPips.blocked++;
				}
				if (edge.is_paused) {
					botPips.paused++;
				}
			})
		}
		
		botPips.show();
	});

	// When a tab is displayed, the new table requires some dynamic content to be displayed
	// (canvas, striping, etc.)
	const tabHandler = function(event) {
		const prevPauseRefresh = pauseRefresh
		pauseRefresh = false

		// Add a little CSS sugar to the now active element
		$(event.target).parent('.nav-item').addClass('nav-item-active')

		// Remove the sugar from the previous element
		$(event.relatedTarget).parent('.nav-item').removeClass('nav-item-active')

		if (history.pushState) {
			history.pushState(null, null, event.target.hash)
		} else {
			window.location.hash = event.target.hash
		}

		// Redraw table elements
		onDisplayTable()

		// Restore pauseRefresh state
		pauseRefresh = prevPauseRefresh
	}

	$(document).off('shown.bs.tab.branchList')
	$(document).on('shown.bs.tab.branchList', 'a[data-toggle="tab"]', tabHandler)

	return mainDiv;
}

function setVerbose(enabled) {
	clearErrorText();
	$.ajax({
		url: '/api/control/verbose/'+(enabled?"on":"off"),
		type: 'post',
		success: updateBranchList,
		error: function(xhr, error, status) {
			setError(xhr, error);
			callback(null);
		}
	});
}

function stopBot() {
	if (confirm("Are you sure you want to stop RoboMerge?")) {
		clearErrorText();
		$.ajax({
			url: '/api/control/stop',
			type: 'post',
			success: updateBranchList,
			error: function(xhr, error, status) {
				setError(xhr, error);
				callback(null);
			}
		});
	}
}
function startBot() {
	clearErrorText();
	$.ajax({
		url: '/api/control/start',
		type: 'post',
		success: updateBranchList,
		error: function(xhr, error, status) {
			setError(xhr, error);
			callback(null);
		}
	});
}

const escapeHtml = str => str
	.replace(/&/g, '&amp;')
	.replace(/</g, '&lt;')
	.replace(/>/g, '&gt;')
	.replace(/"/g, '&quot;')
	.replace(/'/g, '&#039;');
