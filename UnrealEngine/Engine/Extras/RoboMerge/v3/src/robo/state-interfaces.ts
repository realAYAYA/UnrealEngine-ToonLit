// Copyright Epic Games, Inc. All Rights Reserved.
import { Context } from "./settings";
import { AnyStateInfo, AvailableInfo, AvailableTypeEnum, BlockagePauseInfo, ManualPauseInfo, PauseStatusFields } from "./status-types"
import { BlockagePauseInfoMinimal, ManualPauseTypeEnum } from "./status-types"
import { BlockagePauseTypeEnum } from "./status-types"

export function isBlockagePauseInfo(info: AnyStateInfo | undefined | null): info is BlockagePauseInfo {
	return !!info && info.type in BlockagePauseTypeEnum
}
export function isMoreThanBlockagePauseInfoMinimal(info: BlockagePauseInfo | BlockagePauseInfoMinimal | undefined | null): info is BlockagePauseInfo {
	return !!info && info.type in BlockagePauseTypeEnum && "startedAt" in info
}


export function isManualPauseInfo(info: AnyStateInfo | undefined | null): info is ManualPauseInfo {
	return !!info && info.type in ManualPauseTypeEnum
}

export function newAvailableInfo(date?: Date) : AvailableInfo {
	return {
		type: 'available',
		startedAt: date ? date : new Date
	}
}

// Class resolvers -- calling the following two functions will inform the Typescript parser
// which class BotStateInfo is
export function isAvailableInfo(info: AnyStateInfo | undefined | null): info is AvailableInfo {
	return !!info && info.type in AvailableTypeEnum
}

const MAX_RETRY_TIMEOUT_HOURS = 48 // want to abandon auto retry for any integrations that take say >30 minutes, then bring this way down
export class PauseState {
	private _manualPauseInfo: ManualPauseInfo | null = null
	private _blockagePauseInfo: BlockagePauseInfo | null = null

	private lastActionDate: Date

	private _infoStr: string

	private blockagePauseTimeout: NodeJS.Timer | null = null;

	constructor(private parentUnblock: (reason: string) => void, private readonly context: Context) {
		this._restore()
	}

	get infoStr() {
		return this._infoStr
	}
	set infoStr(str: string) {
		this._infoStr = str
	}

	get manualPauseInfo() {
		return this._manualPauseInfo
	}
	set manualPauseInfo(info: ManualPauseInfo | null) {
		this._manualPauseInfo = info
		this._refreshInfoString()
		this.persist()
	}

	get blockagePauseInfo() {
		return this._blockagePauseInfo
	}
	set blockagePauseInfo(info: BlockagePauseInfo | null) {
		this._blockagePauseInfo = info
		this._refreshInfoString()
		this.persist()
	}

	get availableInfo(): AvailableInfo | null {
		if (!this.isBlocked() && !this.isManuallyPaused()) {
			return newAvailableInfo(this.lastActionDate)
		}
		return null
	}

	isManuallyPaused() {
		return !!this._manualPauseInfo
	}

	isBlocked() {
		return !!this._blockagePauseInfo
	}

	isAvailable() {
		return !!this.availableInfo
	}

	block(blockageInfo: BlockagePauseInfo | BlockagePauseInfoMinimal, pauseDurationSeconds?: number) {
		if (!isMoreThanBlockagePauseInfoMinimal(blockageInfo)) {
			blockageInfo = {
				...blockageInfo,
				startedAt: new Date()
			}
		}
		
		const now = new Date

		if (pauseDurationSeconds) {
			// quantise to units of total time paused rounded to half hours, so kind of doubling, up to a maximum of 2 days
			// round down so it doesn't kick in too early, e.g. 
			// e.g. intervals:	25, 25, 30,		60,		120,	240
			//		total:		25, 50, 1h20,	2h20,	4h20,	8h20
			const minDurationMinutes = pauseDurationSeconds / 60
			const pausedSoFarHalfHours = (now.getTime() - blockageInfo.startedAt.getTime()) / (30*60*1000)

			const actualPauseDurationMinutes = Math.min(MAX_RETRY_TIMEOUT_HOURS * 60, Math.max(Math.floor(pausedSoFarHalfHours) * 30, minDurationMinutes))

			const ms = actualPauseDurationMinutes * 60 * 1000
			this.blockagePauseTimeout = setTimeout(() => {
				this.parentUnblock('timeout')
			}, ms)

			// note: this also has the effect of updating the info argument passed in
			blockageInfo.endsAt = new Date(now.getTime() + ms)
		}

		this.blockagePauseInfo = blockageInfo
	}

	unblock(): BlockagePauseInfo | null {
		const oldBlockage = this.blockagePauseInfo
		this.blockagePauseInfo = null

		this.cancelBlockagePauseTimeout()
		return oldBlockage
	}

	manuallyPause(pauseInfo: ManualPauseInfo): void;
	manuallyPause(reason: string, pauseOwner: string, date?: Date): void;
	manuallyPause(arg1: ManualPauseInfo | string, pauseOwner?: string, date?: Date) {
		if (this.isManuallyPaused()) {
			return
		}

		if (typeof arg1 !== "string") {
			this.manualPauseInfo = arg1
		} else {
			this.manualPauseInfo = {
				type: 'manual-lock',
				owner: pauseOwner!,
				message: arg1,
				startedAt: date ? date : new Date
			}
		}


	}

	unpause(): ManualPauseInfo | null {
		const oldPause = this.manualPauseInfo
		this.manualPauseInfo = null
		return oldPause
	}

	cancelBlockagePauseTimeout() {
		if (this.blockagePauseTimeout) {
			clearTimeout(this.blockagePauseTimeout)
			this.blockagePauseTimeout = null
		}
	}

	acknowledge(acknowledger : string) : void {
		if (!this.isBlocked) {
			return
		}

		const newInfo = this.blockagePauseInfo!

		newInfo.acknowledger = acknowledger
		newInfo.acknowledgedAt = new Date

		this.blockagePauseInfo = newInfo

		// Remove the timeout once acknowledged -- 
		// we do not need to retry ourselves once they take volunteer to care of it
		//this.cancelAutoUnpause()
	}

	unacknowledge() : void {
		if (!this.isBlocked()) {
			return;
		}
		const newInfo = this.blockagePauseInfo!

		newInfo.acknowledger = undefined
		newInfo.acknowledgedAt = undefined

		this.blockagePauseInfo = newInfo
	}

	secondsSincePause() {
		return this.isManuallyPaused() ? (Date.now() - this.manualPauseInfo!.startedAt.getTime()) / 1000 : NaN
	}

	secondsSinceBlockage() {
		return this.isBlocked() ? (Date.now() - this.blockagePauseInfo!.startedAt.getTime()) / 1000 : NaN
	}

	applyStatus(status: Partial<PauseStatusFields>) {
		if (this.availableInfo) {
			status.available = this.availableInfo
		}
		if (this.blockagePauseInfo) {
			status.blockage = this.blockagePauseInfo
		}
		if (this.manualPauseInfo) {
			status.manual_pause = this.manualPauseInfo
		}
	}

	private persist() {
		let json: any = {}
		
		json.blockage = this.blockagePauseInfo
		json.manual_pause = this.manualPauseInfo
		json.infoStr = this.infoStr

		this.context.set('pause', json)
	}

	private _restore() {
		this.lastActionDate = new Date
		
        const state = this.context.get('pause')
		if (!state) { // No state found -- by default we'll be available
			return
		}
		
		/* Handle Pause Infos */
        const pauseInfo: ManualPauseInfo | undefined = (state.manual_pause) as ManualPauseInfo // Including state.info for legacy compat
		if (isManualPauseInfo(pauseInfo)) { // If we aren't paused, just create a new AvailableInfo in unpause()
			pauseInfo.startedAt = pauseInfo.startedAt ? new Date(pauseInfo.startedAt) : new Date()
			this._manualPauseInfo = pauseInfo
		}
		
		const blockageInfo : BlockagePauseInfo | undefined = state.blockage as BlockagePauseInfo
		if (isBlockagePauseInfo(blockageInfo)) {
			if (blockageInfo.endsAt) {
				const endsAtDate = new Date(blockageInfo.endsAt)
				const untilEndMs = endsAtDate.getTime() - Date.now()

				if (untilEndMs > 0) {
					// reset unpause timer
					this.blockagePauseTimeout = setTimeout(() => {
						this.parentUnblock('timeout')
					}, untilEndMs)

					blockageInfo.endsAt = endsAtDate

					if (blockageInfo.startedAt) {
						blockageInfo.startedAt = new Date(blockageInfo.startedAt)
					} 
					else {
						blockageInfo.startedAt = new Date()
					}

					this._blockagePauseInfo = blockageInfo
				} 
				// else forget blocking - we would have timed out by now
			} else {
				// This doesn't have an ending date for whatever reason
				this._blockagePauseInfo = blockageInfo
			}
		}

		this._refreshInfoString()
		this.persist()
	}

	private _refreshInfoString() {
		const availableInfo = this.availableInfo
		if (availableInfo) {
			this._infoStr = "Available Since: " + availableInfo.startedAt.toString()
			return
		}

		const lines: string[] = []
		if (isManualPauseInfo(this._manualPauseInfo)) {
			lines.push(`Manually paused by ${this._manualPauseInfo.owner} on ${this._manualPauseInfo.startedAt}:`)
			lines.push(this._manualPauseInfo.message)
		}

		if (isBlockagePauseInfo(this._blockagePauseInfo)) {
			lines.push(`Encountered blockage:`)
			if (this._blockagePauseInfo.source) {
				lines.push('Source=' + this._blockagePauseInfo.source)
			}
	
			/* We want to list these three values, in order of importance:
			 * 1. Acknowledger -- this person has acknowledge the blockage and is taking ownership
			 * 2. Owner (if different from Author) - Usually when a branch has an explict conflict resolver or a reconsider instigator
			 * 3. Author - Original author of commit
			 */
			if (this._blockagePauseInfo.acknowledger) {
				let ackStr = `Acknowledger=${this._blockagePauseInfo.acknowledger}`
				if (this._blockagePauseInfo.acknowledgedAt) {
					ackStr += ` (${this._blockagePauseInfo.acknowledgedAt.toString()})`
				} else {
					// Realistically we should not get here since 'acknowledgedAt' is defined at the same time as 'acknowledger'
					ackStr += " (unknown acknowledge time, ping Robomerge devs)"
				}
				lines.push(ackStr)
			} else if (this._blockagePauseInfo.owner) {
				// Check if owner is different than author
				if (!this._blockagePauseInfo.author || this._blockagePauseInfo.owner !== this._blockagePauseInfo.author) {
					lines.push('Owner=' + this._blockagePauseInfo.owner)
				}
			} else if (this._blockagePauseInfo.author) {
				// Finally we'll list author if present
				lines.push('Author=' + this._blockagePauseInfo.author)
			} else {
				// If we have none of the three values, this is most likely an error. We can call that out here to investigate.
				lines.push('Unknown owner/author. Contact RoboMerge team.')
			}
	
			if (this._blockagePauseInfo.targetStream) {
				lines.push('Target Stream=' + this._blockagePauseInfo.targetStream)
			}
			if (this._blockagePauseInfo.targetBranchName) {
				lines.push('Target Branch Name=' + this._blockagePauseInfo.targetBranchName)
			}
	
			if (this._blockagePauseInfo.message) {
				lines.push(this._blockagePauseInfo.message)
			}
		}
		
		this._infoStr = lines.join('\n')
	}
}
