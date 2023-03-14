// Copyright Epic Games, Inc. All Rights Reserved.
import { FailureKind } from "./status-types";

export enum Resolution {
	RESOLVED = 'resolved',
	SKIPPED = 'skipped',
	CANCELLED = 'cancelled',
	DUNNO = 'fixed(?)'
}

// made to be minimal/serialisable
export interface PersistentConflict {
	// upper case branch names
	blockedBranchName: string
	targetBranchName?: string
	targetStream?: string

	cl: number
	sourceCl: number
	author: string
	owner: string

	kind: FailureKind
	time: Date

	nagged: boolean
	ugsIssue: number

	resolution?: Resolution
	resolvingCl?: number
	resolvingAuthor?: string
	timeTakenToResolveSeconds?: number
	resolvingReason?: string

	// Set when the Acknowledge button is clicked
	acknowledger?: string
	acknowledgedAt?: Date
}

export function PersistentConflictToString(conflict: PersistentConflict) {
	return `${conflict.blockedBranchName}->${conflict.targetBranchName}(${conflict.kind}) @ CL#${conflict.cl} (sourceCL: ${conflict.sourceCl})`
}