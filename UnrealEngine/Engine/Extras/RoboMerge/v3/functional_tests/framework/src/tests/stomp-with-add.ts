// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Util, RobomergeBranchSpec, Stream } from '../framework'

// const jsonlint: any = require('jsonlint')

export const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'DevParent', streamType: 'development', parent: 'Main'},
	{name: 'DevChild', streamType: 'development', parent: 'DevParent'}
]

const EDIT_ASSET_FILENAME = 'edit.uasset'
const ADD_ASSET_FILENAME = 'add.uasset'

export class StompWithAdd extends FunctionalTest {

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		// Add base file
		await P4Util.addFileAndSubmit(this.getClient('Main'), EDIT_ASSET_FILENAME, 'some content', true)

		// Populate DevParent streams
		await this.p4.populate(this.getStreamPath('DevParent'), 'Initial branch of files from Main')
		await this.p4.populate(this.getStreamPath('DevChild'), 'Initial branch of files from DevParent')

		await this.getClient('DevParent').sync()

		// Create future binary file conflict in dev branch
		await P4Util.editFileAndSubmit(this.getClient('DevParent'), EDIT_ASSET_FILENAME, 'conflicting data')
	}

	async run() {
		const mainClient = this.getClient('Main')
		// Create future binary file conflict
		await Promise.all([
			P4Util.editFile(mainClient, EDIT_ASSET_FILENAME, 'Other data to conflict'),
			P4Util.addFile(mainClient, ADD_ASSET_FILENAME, 'Initial content', true)
		])

		// Submit 
		await mainClient.submit('Editing one file, adding another')

		// Now Main->DevParent should be blocked
	}

	getBranches(): RobomergeBranchSpec[] {
		return [{
			streamDepot: this.testName,
			name: this.fullBranchName('Main'),
			streamName: 'Main',
			flowsTo: [this.fullBranchName('DevParent')],
			forceAll: true,
			additionalSlackChannelForBlockages: this.testName
		}, {
			streamDepot: this.testName,
			name: this.fullBranchName('DevParent'),
			streamName: 'DevParent',
			flowsTo: [this.fullBranchName('Main')]
		}
	]}

	async verify() {
		// Test implicit target stomp
		await this.performAndVerifyStomp('DevParent')

		await this.ensureNotBlocked('Main', 'DevParent')
	}


	private async performAndVerifyStomp(branch: string) {
		await this.checkHeadRevision(branch, EDIT_ASSET_FILENAME, 2)

		// verify stomp
		let result = await this.verifyAndPerformStomp('Main', branch, this.testName)

		let files = result.verify.files

		for (const file of files) {
			const isEditAsset = file.targetFileName.indexOf(EDIT_ASSET_FILENAME) >= 0
			// file.resolved means !stomped
			if (isEditAsset === file.resolved) {
				this.error(files ? JSON.stringify(files) : 'no files')
				console.dir(result)
				throw new Error('Unexpected stomp result')
			}
		}

		await this.checkHeadRevision(branch, EDIT_ASSET_FILENAME, 3)
	}
}
