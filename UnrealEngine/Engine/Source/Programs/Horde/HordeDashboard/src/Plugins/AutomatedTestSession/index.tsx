// Copyright Epic Games, Inc. All Rights Reserved.

import { Plugin, PluginMount } from "..";
import { TestSessionView, TestSessionViewAll, BuildHealthTestSessionView, routePath } from "./components/AutomatedTestSession";

class AutomatedTestSessionPlugin implements Plugin {

    name = "Automated Test Session";
    components = [
        {
            mount: PluginMount.TestReportPanel,
            component: TestSessionView,
            id: "Automated Test Session"
        },
        {
            mount: PluginMount.BuildHealthSummary,
            component: BuildHealthTestSessionView,
        }
    ];
    routes = [
        {
            path: routePath,
            component: TestSessionViewAll
        }
    ];
}

export default new AutomatedTestSessionPlugin();

