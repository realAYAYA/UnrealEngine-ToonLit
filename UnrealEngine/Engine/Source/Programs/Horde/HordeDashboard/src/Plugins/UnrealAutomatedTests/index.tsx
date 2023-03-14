// Copyright Epic Games, Inc. All Rights Reserved.

import { Plugin, PluginMount } from "..";
import { TestPassSummaryView } from "./components/UnrealAutomatedTests";

class UnrealAutomatedTestsPlugin implements Plugin {

    name = "Unreal Automated Tests";
    components = [{
        mount: PluginMount.TestReportPanel,
        component: TestPassSummaryView,
        id: "Unreal Automated Tests"
    }];

}

export default new UnrealAutomatedTestsPlugin();

