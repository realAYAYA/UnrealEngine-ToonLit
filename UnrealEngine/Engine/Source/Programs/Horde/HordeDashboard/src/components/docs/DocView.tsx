import { Stack, mergeStyleSets } from "@fluentui/react";
import { action, makeObservable, observable } from "mobx";
import { observer } from "mobx-react-lite";
import { useRef, useState } from "react";
import { useLocation } from "react-router-dom";
import { Markdown } from "../../base/components/Markdown";
import { ISideRailLink, SideRail } from "../../base/components/SideRail";
import { useWindowSize } from "../../base/utilities/hooks";
import { hordeClasses, modeColors } from "../../styles/Styles";
import { BreadcrumbItem, Breadcrumbs } from "../Breadcrumbs";
import { TopNav } from "../TopNav";
import dashboard from "../../backend/Dashboard";

type Anchor = {
   text: string;
   anchor: string;
}

type State = {
   crumbs: BreadcrumbItem[];
   jumpLinks: ISideRailLink[];
}


export const docClasses = mergeStyleSets({
   raised: {
      backgroundColor: "#ffffff",
      boxShadow: "0 1.6px 3.6px 0 rgba(0,0,0,0.132), 0 0.3px 0.9px 0 rgba(0,0,0,0.108)",
      padding: "32px 40px"
   }
});


class LinkState {

   constructor() {
      makeObservable(this);
   }

   @observable
   state: State = { crumbs: [], jumpLinks: [] };

   @action
   setState(crumbs: BreadcrumbItem[], jumpLinks: ISideRailLink[]) {
      this.state = { crumbs: crumbs, jumpLinks: jumpLinks };
   }
}

class DocumentCache {

   constructor() {
      makeObservable(this);
   }

   get(docName: string): { markdown: string, crumbs: BreadcrumbItem[], anchors: Anchor[] } | undefined {

      // implicit subscription
      if (this.available) { }

      const text = this.documentCache.get(docName);
      const crumbs: BreadcrumbItem[] = this.crumbCache.get(docName) ?? [];
      const anchors: Anchor[] = this.anchorCache.get(docName) ?? [];

      if (text) {
         return {
            markdown: text,
            crumbs: crumbs,
            anchors: anchors
         }
      }

      // already in flight
      if (this.documentRequests.has(docName)) {
         return undefined;
      }

      this.documentRequests.add(docName);

      fetch(`/${docName}`, { cache: "no-cache" })
         .then((response) => response.text())
         .then((textContent) => {

            let crumbs: BreadcrumbItem[] = [];
            let anchors: Anchor[] = [];

            textContent = textContent.trim();

            if (textContent?.indexOf("<!doctype html>") === -1) {

               // generate anchors
               let lines = (textContent.match(/[^\r\n]+/g) ?? []) as string[];
               lines = lines.map(i => i.trim()).filter(i => !!i);
               lines.forEach(line => {
                  let anchor = "";

                  if (line.startsWith("# ")) {
                     anchor = line.split("# ")[1];
                  }

                  if (line.startsWith("## ")) {
                     anchor = line.split("## ")[1];
                  }

                  if (anchor) {
                     anchor = anchor.trim();
                     anchors.push({ text: anchor, anchor: anchor.replace(/[^a-z0-9- ]/gi, '').replace(/ /gi, '-').toLowerCase() })
                  }
               })

               // generate crumbs
               if (lines && lines.length) {
                  const line = lines[0];
                  if (line.startsWith("[Horde]")) {

                     line.split(">").map(c => c.trim()).forEach(c => {
                        const link = c.split("](");
                        if (link.length === 1) {
                           crumbs.push({ text: link[0] })
                        } else {
                           let [cname, clink] = [link[0].replace("[", ""), link[1].replace(")", "")];

                           let docPath = docName.split("/").slice(2);
                           docPath.pop();

                           const relative = (clink.match(/..\//g) || []).length;

                           if (clink.indexOf("README.md") !== -1) {
                              clink = "/docs"
                           } else {
                              docPath = docPath.slice(docPath.length - 1, -relative);
                              const elements = clink.split("/");
                              docPath.push(elements[elements.length - 1]);
                              clink = docPath.join("/");
                           }

                           if (cname !== "Horde") {
                              crumbs.push({ text: cname, link: clink });
                           }

                        }
                     });

                     textContent = textContent.replace(line, "");
                  }
               }

               this.documentCache.set(docName, textContent);
               this.crumbCache.set(docName, crumbs);
               this.anchorCache.set(docName, anchors);

            } else {
               this.documentCache.set(docName, `###Missing document ${docName}`);
               this.crumbCache.set(docName, []);
               this.anchorCache.set(docName, []);
            }

            this.setAvailable();

         });   
   }

   @observable
   private available: number = 0;

   @action
   private setAvailable() {
      this.available++;
   }

   private documentCache = new Map<string, string>();
   private documentRequests = new Set<string>();
   private crumbCache = new Map<string, BreadcrumbItem[]>();
   private anchorCache = new Map<string, Anchor[]>();

}


const linkState = new LinkState();
const documentCache = new DocumentCache();

const DocPanel: React.FC<{ docName: string }> = observer(({ docName }) => {

   const cache = documentCache.get(docName);

   if (!cache) {
      return null;
   }

   const text = cache.markdown;
   let crumbs = cache.crumbs;
   let anchors = cache.anchors;

   linkState.setState(crumbs, anchors.map(a => {
      return { text: a.text, url: a.anchor }
   }));


   return <Stack styles={{ root: { width: "100%" } }} >
      <div style={{ margin: "16px 32px" }}>
         <Markdown>{text}</Markdown>
      </div>
   </Stack>;
})

const DocRail = observer(() => {

   const state = linkState.state;

   const refLinks: ISideRailLink[] = [];

   if (dashboard.user?.dashboardFeatures?.showLandingPage === true) {
      refLinks.push({ text: "Landing", url: "/docs" });
   }

   refLinks.push({ text: "Home", url: "/docs/Home.md" });
   refLinks.push({ text: "User Guide", url: "/docs/Users.md" });
   refLinks.push({ text: "Deployment", url: "/docs/Deployment.md" });
   refLinks.push({ text: "Configuration", url: "/docs/Config.md" });
   refLinks.push({ text: "Horde Internals", url: "/docs/Internals.md" });
   refLinks.push({ text: "Release Notes", url: "/docs/ReleaseNotes.md" });

   return <SideRail jumpLinks={state.jumpLinks} relatedLinks={refLinks} />

})

const DocCrumbs = observer(() => {

   const state = linkState.state;

   const crumbs: BreadcrumbItem[] = [];
   crumbs.push({ text: "Documentation", link: "/docs" })
   crumbs.push(...state.crumbs.map(c => { return { text: c.text, link: c.link } }));

   return <Breadcrumbs items={crumbs} />

})



export const DocView = () => {

   const location = useLocation();

   // fixme
   let docName = location.pathname.replace("/docs/", "").replace("/docs", "").trim();
   if (docName.startsWith("/index")) {
      docName = docName.replace("/index", "")
   }
   if (!docName || docName.indexOf("README.md") !== -1) {

      if (dashboard.user?.dashboardFeatures?.showLandingPage === true) {
         docName = "documentation/Docs/Landing.md";
      } else {
         docName = "documentation/Docs/Home.md";
      }

   } else {

      if (!docName.startsWith("Docs/")) {
         docName = `documentation/Docs/${docName}`;
      } else {
         docName = docName.replace("Docs/", "documentation/Docs/");
      }
   }

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <DocCrumbs />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2), flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%", "position": "relative", paddingTop: "16px", paddingLeft: "32px", paddingBottom: "16px", paddingRight: 0 } }}>
            <div style={{ overflowY: 'scroll', overflowX: 'hidden', height: "calc(100vh - 162px)" }} data-is-scrollable={true}>
               <Stack horizontal>
                  <Stack style={{ width: 1240, paddingTop: 6, marginLeft: 4, height: '100%' }}>
                     <Stack className={docClasses.raised}>
                        <Stack style={{ width: "100%", height: "max-content" }} tokens={{ childrenGap: 18 }}>
                           <DocPanel docName={docName} />
                        </Stack>
                     </Stack>
                     <Stack style={{ paddingBottom: 24 }} />
                  </Stack>
                  <Stack style={{ paddingLeft: 1280, paddingTop: 12, position: "absolute", pointerEvents: "none" }}>
                     <div style={{ pointerEvents: "all" }}>
                        <DocRail />
                     </div>
                  </Stack>
               </Stack>
            </div>
         </Stack>
      </Stack>
   </Stack>

}

