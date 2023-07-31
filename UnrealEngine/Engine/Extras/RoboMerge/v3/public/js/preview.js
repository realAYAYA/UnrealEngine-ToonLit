// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

function doit(query) {
	$.get(query)
	.then(data => {
		const allBranches = JSON.parse(data).allBranches
		if (allBranches.length === 0) {
			throw new Error('no branches found in shelf')
		}
		const botNames = new Set(allBranches.map(b => b.bot))

		$('#graph').append(showFlowGraph(allBranches, {
			botNames: [], ...parseOptions(location.search)
		}));
		$('.bots').html([...botNames].map(s => `<tt>${s.toLowerCase()}</tt>`).join(', '))
		$('#success-panel').show();
	})
	.catch(error => {

		const $errorPanel = $('#error-panel');
		const errorMsg = error.responseText
			? error.responseText.replace(/\t/g, '    ')
			: 'Internal error: ' + error.message;
		$('pre', $errorPanel).text(errorMsg);
		$errorPanel.show();
	});
}

window.doPreview = doit;
