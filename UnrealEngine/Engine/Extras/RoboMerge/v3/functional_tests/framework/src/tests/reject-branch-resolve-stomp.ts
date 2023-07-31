// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, Stream } from '../framework'
import { Perforce } from '../test-perforce'

export const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Development', streamType: 'development', parent: 'Main'}
]

const BINARY_FILENAME1 = 'fake1.uasset'
//const BINARY_FILENAME2 = 'fake2.uasset'
export class RejectBranchResolveStomp extends FunctionalTest {

	constructor(p4: Perforce) {
		super(p4)
	}

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), BINARY_FILENAME1, 'Initial content', true)

		// Populate Development stream
		await this.p4.populate(this.getStreamPath('Development'), 'Initial branch of files from Main')
	
		// Delete file in main streqm to cause branch conflict
		const mainClient = this.getClient('Main')
		await Promise.all([this.getClient('Development').sync(),
			mainClient.delete(BINARY_FILENAME1)
			.then(() => mainClient.submit('Deleting binary file from Main'))
		])
	}

	run() {
		return P4Util.editFileAndSubmit(this.getClient('Development'), BINARY_FILENAME1, 'Changed content', this.fullBranchName('Main'))
	}

	getBranches() {
		return [{
			streamDepot: this.testName,
			name: this.fullBranchName('Main'),
			streamName: 'Main',
			flowsTo: [this.fullBranchName('Development')]
		}, {
			streamDepot: this.testName,
			name: this.fullBranchName('Development'),
			streamName: 'Development',
			flowsTo: [this.fullBranchName('Main')]
		}]
	}

	async verify() {
		const edgeState = await this.getEdgeState('Development', 'Main')
		const verifyResult = await this.verifyStompRequest('Development', 'Main', edgeState)

		if (!verifyResult.validRequest && verifyResult.branchOrDeleteResolveRequired && verifyResult.files[0].resolveResult.resolveType === 'branch') {
			this.info('Success! Stomp verify detected a branch resolve requirement after the automatic merge, which is correct for this functional test.')
		}
		else {
			throw new Error(`Stomp verify returned unexpected values. Erroring...\n${JSON.stringify(verifyResult)}`)
		}

		const stompResult = await this.performStompRequest('Development', 'Main', edgeState)

		if (stompResult.statusCode === 500) {
			this.info('Robomerge successfully rejected our stomp request')
			return
		}

		throw new Error(`FAILURE!!! Robomerge did not reject our stomp request, which should have failed due to branch resolve requirement. Response was: \n${JSON.stringify(stompResult)}`)
	}
}
