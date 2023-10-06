//
//  WebRTCStatsView.swift
//  vcam
//
//  Created by TensorWorks on 17/7/2023.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

// Draws WebRTC stats as line graphs
class WebRTCStatsView: UIView {
    
    private var statsGraphs : [String: LineGraphView] = [:]
    private var graphWidth : Int = 175
    private var graphHeight : Int = 175
    private var graphPadding : Int = 10
    private var graphHeaderYPadding : Int = 30
    private var graphLeftMargin : Int = 100;
    private var graphRightMargin : Int = 100;

    // Graphs will be displayed in a grid layout, rowsxcols, where nRows and nCols are determined by frameWidth/graphWidth
    // When graphs exceed nCols on a single row then a new row will begin
    // There is no maximum number of rows, but some graphs may be drawn offscreen if there is too many rows
    private var nCols : Int = 5
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        
        // Disable user interaction so stats do not block underlying video
        self.isUserInteractionEnabled = false
        
        // set transparent background color
        self.backgroundColor = UIColor.clear
        
        // Calculate nRows and nCols
        let frameWidth : CGFloat = frame.width - CGFloat(self.graphLeftMargin) - CGFloat(self.graphRightMargin)
        self.nCols = Int( floor( frameWidth / CGFloat(self.graphWidth) ))

    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        
    }
    
    func addGraph(graphName: String) -> LineGraphView {
        
        // Calculate the rect in which the graph is drawn
        let nGraphs : Int = self.statsGraphs.count
        let colIdx : Int = nGraphs % self.nCols
        let rowIdx : Int = abs(nGraphs /  self.nCols)
        let graphX : Int = colIdx * (self.graphWidth + self.graphPadding) + self.graphLeftMargin
        let graphY : Int = rowIdx * (self.graphHeight + self.graphPadding) + self.graphHeaderYPadding
        let graphFrame : CGRect = CGRect(x: graphX, y: graphY, width: self.graphWidth, height: self.graphHeight)
        let graph : LineGraphView = LineGraphView(frame: graphFrame)

        // Add the graph the be drawn under this view
        self.addSubview(graph)
        
        // Given it the passed in key as its title
        graph.title = graphName
        
        // Add the graph to our map of graphs
        self.statsGraphs[graphName] = graph
        
        return graph
    }
    
}
