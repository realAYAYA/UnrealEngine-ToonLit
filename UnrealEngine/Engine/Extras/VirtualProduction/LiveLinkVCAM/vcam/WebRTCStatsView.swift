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
    
    override var isHidden: Bool {
        set {
            super.isHidden = newValue
            for graphKV in statsGraphs {
                graphKV.value.isHidden = newValue
            }
        }
        get {
            return super.isHidden
        }
    }

    // Graphs will be displayed in a grid layout, rowsxcols, where nRows and nCols are determined by frameWidth/graphWidth
    // When graphs exceed nCols on a single row then a new row will begin
    // There is no maximum number of rows, but some graphs may be drawn offscreen if there is too many rows
    private var nCols : Int = 6
    private var nRows : Int = 3
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        
        self.graphLeftMargin = Int(frame.width / 100)
        self.graphRightMargin = Int(frame.width / 100)
        
        let scale = abs(2 - UIScreen.main.scale) + 1
        
        self.graphWidth = Int(frame.width / (scale * CGFloat(self.nCols + 1)))
        self.graphHeight = Int(frame.height / (scale * CGFloat(self.nRows + 1)))
        
        // Disable user interaction so stats do not block underlying video
        self.isUserInteractionEnabled = false
        
        // set transparent background color
        self.backgroundColor = UIColor.clear
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

        // Graph inherits parents hidden property
        graph.isHidden = self.isHidden

        // Add the graph the be drawn under this view
        self.addSubview(graph)
        
        // Given it the passed in key as its title
        graph.title = graphName
        
        // Add the graph to our map of graphs
        self.statsGraphs[graphName] = graph
        
        return graph
    }
    
}
