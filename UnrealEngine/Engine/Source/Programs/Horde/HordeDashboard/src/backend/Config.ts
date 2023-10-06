// Copyright Epic Games, Inc. All Rights Reserved.

// import { initDatadog} from './Datadog';

export type SiteConfig = {
    environment: "dev" | "production" | "dev-local" | "";
    plugins?: string[];
    datadogClient?: string;
}

const dev = {
	"environment": "dev",
	"datadogClient" : "",
	"plugins": [
		"AutomatedTestSession",
		"UnrealAutomatedTests",
		"SimpleTestReport"
	]
} 

const prod = {
	"environment": "production",
	"datadogClient" : "",
	"plugins": [
		"AutomatedTestSession",
		"UnrealAutomatedTests",
		"SimpleTestReport"
	]
} 

export function getSiteConfig() {

    // @todo: fix me
    const isProd = window.location.hostname === "horde.devtools.epicgames.com";
    
    return isProd ? prod : dev;
}
