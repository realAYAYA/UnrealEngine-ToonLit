import { UserStatusData } from "../../src/robo/status-types"
import { log, updateBranchList, updateBranchesPending } from "./branches"

const branchGraphs = new Map<string, JQuery>();


// Mutex variable for disabling updateBranchList() on a global timer
let pauseRefresh = false

function refreshAll() {
	const alreadyPending = (typeof updateBranchesPending === "boolean") && updateBranchesPending
	if (!pauseRefresh && !alreadyPending) {
		pauseRefresh = true
		updateBranchList()
		pauseRefresh = false
	} else {
		//console.log('DEBUG: Not refreshing branch list due to open dropdown')
	}
}

function pauseAutoRefresh() {
	log('Pausing auto refresh of branch list')
	pauseRefresh = true
}

function resumeAutoRefresh() {
	log('Resuming auto refresh of branch list')
	pauseRefresh = false
}

(window as any).refreshAll = refreshAll;
(window as any).pauseAutoRefresh = pauseAutoRefresh;
(window as any).resumeAutoRefresh = resumeAutoRefresh;


// entry point
updateBranchList()

$('body').one("branchListUpdated", function () {
	const gRefreshTimer = setInterval(refreshAll, 10 * 1000);

})
