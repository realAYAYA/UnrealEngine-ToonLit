// Copyright Epic Games, Inc. All Rights Reserved.

import { Plugin, PluginMount } from "..";
import { TestReportView, ReportLinkItemPane } from "./components/SimpleReport";

class SimpleTestReportPlugin implements Plugin {

    name = "Simple Report";
    components = [
        {
            mount: PluginMount.TestReportPanel,
            component: TestReportView,
            id: "Simple Report"
        },
        {
            mount: PluginMount.TestReportLink,
            component: ReportLinkItemPane,
            id: "Simple Report"
        }
    ];
}

export default new SimpleTestReportPlugin();
