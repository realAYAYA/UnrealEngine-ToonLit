//
//  AppDelegate.swift
//  vcam
//
//  Created by Brian Smith on 8/8/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Override point for customization after application launch.
        AppSettings.registerDefaults()

        // log the version and build date
        if let infoDict = Bundle.main.infoDictionary,
            let buildDate = ObjCUtility.buildDate() as Date? {
            
            let df = Log.createDateFormatter()
            
            Log.info(String(format: "VCAM v%@ (build %@ / %@)", infoDict["CFBundleShortVersionString"] as! String,
                            infoDict["CFBundleVersion"] as! String,
                            df.string(from: buildDate)))
        }

        return true
    }
    
    func applicationWillTerminate(_ application: UIApplication) {
        LiveLink.shutdown()
    }

    // MARK: UISceneSession Lifecycle

    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        // Called when a new scene session is being created.
        // Use this method to select a configuration to create the new scene with.
        return UISceneConfiguration(name: "Default Configuration", sessionRole: connectingSceneSession.role)
    }

    func application(_ application: UIApplication, didDiscardSceneSessions sceneSessions: Set<UISceneSession>) {
        // Called when the user discards a scene session.
        // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
        // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
    }
}

