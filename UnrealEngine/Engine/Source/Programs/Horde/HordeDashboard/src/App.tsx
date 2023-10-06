// Copyright Epic Games, Inc. All Rights Reserved.

import { Image, Spinner, SpinnerSize, Stack, Text } from '@fluentui/react';
import React, { useState } from 'react';
import { useDarkreader } from 'react-darkreader';
import { createBrowserRouter, Navigate, Outlet, RouteObject, RouterProvider } from 'react-router-dom';
import backend from './backend';
import { DashboardPreference } from './backend/Api';
import { getSiteConfig } from './backend/Config';
import dashboard from './backend/Dashboard';
import { AdminToken } from './components/AdminToken';
import { AgentView } from './components/AgentView';
import { AuditLogView } from './components/AuditLog';
import { AutomationView } from './components/AutomationView';
import { DashboardView } from './components/DashboardView';
import { DebugView } from './components/DebugView';
import { DeviceView } from './components/DeviceView';
import { DocView } from './components/docs/DocView';
import { ErrorDialog, ErrorHandler } from './components/ErrorHandler';
import { JobDetailViewV2 } from './components/jobDetailsV2/JobDetailViewV2';
import { LogView } from './components/LogView';
import { NoticeView } from './components/NoticeView';
import { PerforceServerView } from './components/PerforceView';
import { PoolView } from './components/PoolView';
import { PreflightRedirector } from './components/Preflight';
import { ProjectHome } from './components/ProjectHome';
import { StreamView } from './components/StreamView';
import { TestReportView } from './components/TestReportView';
import { ToolView } from './components/ToolView';
import { UserHomeView } from './components/UserHome';
import { UtilizationReportView } from './components/UtilizationReportView';
import hordePlugins from './Plugins';
import { modeColors, preloadFonts } from './styles/Styles';
import { StepIssueReportTest } from './components/test/IssueStepReport';
import { JobRedirector } from './components/JobRedirector';


let router: any;

const RouteError: React.FC = () => {
   return <Navigate to="/index" replace={true} />
}

const Main: React.FC = () => {

   const [init, setInit] = useState(false);
   const [pluginsLoaded, setPluginsLoaded] = useState(false);

   const config = getSiteConfig();

   if (!init) {

      backend.init().then(() => {

         backend.getCurrentUser().then(user => {

            let darktheme = user.dashboardSettings?.preferences?.get(DashboardPreference.Darktheme);

            // We need to initialize default theme
            if (darktheme !== "true" && darktheme !== "false") {
               console.error("Invalid dark theme setting ", darktheme);
               darktheme = "true";
            }

            let local = localStorage.getItem("horde_darktheme");

            if (!local) {

               console.log("Setting local theme to ", darktheme);

               localStorage.setItem("horde_darktheme", darktheme);

               if (darktheme === "false") {
                  // need to reload for light mode as dark is default
                  window.location.reload();
               } else {
                  setInit(true);
                  return null;
               }

            } else if (local !== darktheme) {

               console.log(`Setting local theme to ${darktheme} and reloading for change`);
               localStorage.setItem("horde_darktheme", darktheme);
               window.location.reload();

            } else {
               setInit(true);
               return null;
            }
         }).catch(reason => {
            ErrorHandler.set({ title: "Error initializing site, unable to get user", reason: reason }, true);
         })


      }).catch((reason) => {
         ErrorHandler.set({ title: "Error initializing site", reason: reason }, true);
      });

      return (<div style={{ position: 'absolute', left: '50%', top: '50%', transform: 'translate(-50%, -50%)' }}>
         <Stack horizontalAlign="center" styles={{ root: { padding: 20, minWidth: 200, minHeight: 100 } }}>
            <Stack horizontal>
               <Stack styles={{ root: { paddingTop: 2, paddingRight: 6 } }}>
                  <Image shouldFadeIn={false} shouldStartVisible={true} width={48} src="/images/horde.svg" />
               </Stack>
               <Stack styles={{ root: { paddingTop: 12 } }}>
                  <Text styles={{ root: { fontFamily: "Horde Raleway Bold", fontSize: 24 } }}>HORDE</Text>
               </Stack>
            </Stack>
            <Stack>
               {preloadFonts.map(font => {
                  // preload fonts to avoid FOUT
                  return <Text key={`font_preload_${font}`} styles={{ root: { fontFamily: font, fontSize: 10 } }} />
               })}
            </Stack>
            <Spinner styles={{ root: { paddingTop: 8, paddingLeft: 4 } }} size={SpinnerSize.large} />
         </Stack>
      </div>);
   }

   if (!pluginsLoaded) {
      hordePlugins.loadPlugins(config.plugins).finally(() => {
         setPluginsLoaded(true);
      })
      return null;
   }

   if (!router) {

      const routes: RouteObject[] = [
         {
            path: "/", element: <Root />, errorElement: <RouteError />, children: [
               { path: "index", element: (dashboard.user?.dashboardFeatures?.showLandingPage === true) ? <DocView /> : <UserHomeView /> },
               { path: "project/:projectId", element: <ProjectHome /> },
               { path: "pools", element: <PoolView /> },
               { path: "job/:jobId", element: <JobDetailViewV2 /> },               
               { path: "job", element: <JobRedirector /> },
               { path: "log/:logId", element: <LogView /> },
               { path: "testreport/:testdataId", element: <TestReportView /> },
               { path: "stream/:streamId", element: <StreamView /> },
               { path: "agents", element: <AgentView /> },
               { path: "admin/token", element: <AdminToken /> },
               { path: "reports/utilization", element: <UtilizationReportView /> },
               { path: "preflight", element: <PreflightRedirector /> },
               { path: "dashboard", element: <DashboardView /> },
               { path: "perforce/servers", element: <PerforceServerView /> },
               { path: "notices", element: <NoticeView /> },
               { path: "devices", element: <DeviceView /> },
               { path: "audit/agent/:agentId", element: <AuditLogView /> },
               { path: "audit/issue/:issueId", element: <AuditLogView /> },
               { path: "automation", element: <AutomationView /> },
               { path: "tools", element: <ToolView /> },
               { path: "debug/lease/:leaseId", element: <DebugView /> },
               { path: "docs", element: <DocView /> },
               { path: "docs/*", element: <DocView /> },
               { path: "test/stepissuereport", element: <StepIssueReportTest /> }
            ]
         }
      ];

      // mount plugins
      const pluginRoutes = hordePlugins.routes.map((route) => {
         return { path: route.path, element: <route.component /> };
      })

      routes[0].children!.push(...pluginRoutes);

      router = createBrowserRouter(routes);
   }

   return (
      <RouterProvider router={router} />
   );
};

const Darkmode: React.FC = () => {

   const additionalCSS = `
      .ms-Toggle-thumb {background-color: ${modeColors.text};}}
      .ms-Toggle-thumb:hover {background-color: ${modeColors.text};}}
      .ms-Toggle-thumb:hover {background-color: ${modeColors.text};}}
   `;

   // NOTE: if an Stack child isn't respecting className="horde-no-darktheme", check that you are using style: {} instead of styles:{root:{}}!
   /*const [isDark, { toggle }] = */useDarkreader(dashboard.darktheme, { brightness: 100, contrast: 100, sepia: 0, grayscale: 0, darkSchemeTextColor: "#FFFFFFFF" }, { disableStyleSheetsProxy: false, invert: [], ignoreInlineStyle: ['.horde-no-darktheme *'], css: additionalCSS, ignoreImageAnalysis: [] });

   return null;
};

const App: React.FC = () => {

   return (
      <React.Fragment>
         <Darkmode />
         <ErrorDialog />
         <Main />
      </React.Fragment>
   );
};

export default App;

const HomeRedirect: React.FC = () => {
   if (window.location.pathname === "/" || !window.location.pathname) {
      return <Navigate to="/index" replace={true} />
   }
   return null;
}

const Root: React.FC = () => {
   return <div>
      <Outlet />
      <HomeRedirect />
   </div>
}

