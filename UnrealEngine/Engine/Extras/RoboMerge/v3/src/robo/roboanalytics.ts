// Copyright Epic Games, Inc. All Rights Reserved.
import { Analytics } from '../common/analytics'

export let roboAnalytics: Analytics | null = null
export function setGlobalAnalytics(analytics: Analytics) {
	roboAnalytics = analytics
}
