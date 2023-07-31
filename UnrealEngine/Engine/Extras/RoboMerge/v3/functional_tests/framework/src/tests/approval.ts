import { EdgeProperties, P4Util } from '../framework'
import { SimpleMainAndReleaseTestBase, streams } from '../SimpleMainAndReleaseTestBase'

export class Approval extends SimpleMainAndReleaseTestBase {

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)
		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')
		await this.initialPopulate()
	}

	async run() {
		const releaseClient = this.getClient('Release')
		await releaseClient.sync()

		// await this.p4.lock(this.getClient('Main').workspace, this.getStreamPath('Main'))
		await P4Util.editFileAndSubmit(releaseClient, 'test.txt', 'Initial content\n\nmergeable')
	}

	verify() {
		return Promise.all([
				this.ensureNotBlocked('Main'),
				this.ensureBlocked('Release', 'Main'),
				this.ensureConflictMessagePostedToSlack('Release', 'Main')
			])
	}

	getEdges() : EdgeProperties[] {
		return [
		  { from: this.fullBranchName('Release'), to: this.fullBranchName('Main')
		  , approval: {
		      description: 'hey there!',
		      channelName: 'approvers',
		      channelId: 'abc123'
		    }
		  }
		]
	}
}