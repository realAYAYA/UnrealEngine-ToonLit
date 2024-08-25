//
//  LineGraphView.swift
//  vcam
//
//  Created by TensorWorks on 17/7/2023.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation
import UIKit

class LineGraphView: UIView {
    
    // An array of data points for the graph
    private var dataPoints: [CGFloat] = []
    
    // Limit to the number of data points stored in the array
    private var sizeLimit: UInt16 = 120
    
    // Font to use in our graph title
    private var titleFont: UIFont
    
    // Font to use in our graph tick labels
    private var tickLabelFont: UIFont
    
    // Color of our font
    private var fontColor: UIColor = UIColor.white
    
    // Paragraph style for our text
    private var paragraphStyle: NSMutableParagraphStyle = NSMutableParagraphStyle()
    
    // Title text attributes
    private var titleTextAttributes: [NSAttributedString.Key: Any] = [:]
    
    // Tick label text attributes
    private var tickTextAttributes: [NSAttributedString.Key: Any] = [:]
    
    // Maximum y value we have seen
    public var maxY: CGFloat = 1
    
    // Title of graph
    public var title: String = "Title" {
        didSet {
            setNeedsDisplay() // Trigger redraw when the text is set
        }
    }
    
    // Function for formatting the numberic text labels next to the y-Axis ticks
    public var yTickLabelFormatter : (Double) -> String = {
        return String(format: "%.0f", round($0))
    }
    
    override init(frame: CGRect) {
        // Font to use in our graph title
        self.titleFont = UIFont.systemFont(ofSize: 12 / (UIScreen.main.scale - 1));
        
        // Font to use in our graph tick labels
        self.tickLabelFont = UIFont.systemFont(ofSize: 8 / (UIScreen.main.scale - 1));
        
        super.init(frame: frame)
        
        // set transparent background color
        self.backgroundColor = UIColor(red: 0.0, green: 0.0, blue: 0.0, alpha: 0.5)
        
        // fill data points with 1's
        for _ in 1...self.sizeLimit {
            addDataPoint(value: CGFloat(0));
        }
        
        // Set alignment of text in our graphs to be center
        self.paragraphStyle.alignment = .left;
        
        // Set the attributes for the text
        self.titleTextAttributes = [
            .font: self.titleFont,
            .foregroundColor: self.fontColor,
            .paragraphStyle: self.paragraphStyle
        ]
        
        self.tickTextAttributes = [
            .font: self.tickLabelFont,
            .foregroundColor: self.fontColor,
            .paragraphStyle: self.paragraphStyle
        ]
    }
    
    required init?(coder: NSCoder) {
        // Font to use in our graph title
        self.titleFont = UIFont.systemFont(ofSize: 12 / (UIScreen.main.scale - 1));
        
        // Font to use in our graph tick labels
        self.tickLabelFont = UIFont.systemFont(ofSize: 8 / (UIScreen.main.scale - 1));
        
        super.init(coder: coder)
        self.backgroundColor = UIColor.clear
    }
    
    func addDataPoint(value: CGFloat) {
        
        if value > self.maxY || value == self.maxY {
            // adds some headroom the the graph
            self.maxY = value + (self.maxY * 0.1)
        }
                
        self.dataPoints.append(value)
        
        if self.dataPoints.count > self.sizeLimit {
            self.dataPoints.removeFirst();
        }
        
        // Triggers a redraw (need to do this from main thread)
        DispatchQueue.main.async {
            if !self.isHidden && self.window != nil {
                self.setNeedsDisplay()
            }
        }
    }
    
    override func draw(_ rect: CGRect) {
        super.draw(rect)
        
        guard let context = UIGraphicsGetCurrentContext() else { return }
        
        // Draw the text in the bounding rectangle
        let titleSize = self.title.size(withAttributes: self.titleTextAttributes);
        let titleRect = CGRect(
            x: (rect.width - titleSize.width)  * 0.5,
            y: 0,
            width: rect.width,
            height: rect.height
        )
        self.title.draw(in: titleRect, withAttributes: self.titleTextAttributes)
        

        // Draw the y-axis
        let axisPadding = rect.width * 0.125;
        UIColor.white.setStroke()
        let yAxis = UIBezierPath()
        yAxis.lineWidth = 1.0
        yAxis.move(to: CGPoint(x: axisPadding, y: titleSize.height))
        yAxis.addLine(to: CGPoint(x: axisPadding, y: rect.height))
        yAxis.stroke()
        
        // Draw y-axis ticks
        let tickLength = 3.0
        let yAxisHeight = rect.height - titleSize.height
        let nTicks = 5
        
        for tickYIdx in 0...nTicks {
            let tickPath = UIBezierPath();
            tickPath.lineWidth = 1.0
            let scalar = CGFloat(tickYIdx) / CGFloat(nTicks)
            let tickY = titleSize.height + (scalar * yAxisHeight)
            tickPath.move(to: CGPoint(x: axisPadding, y: tickY))
            tickPath.addLine(to: CGPoint(x: axisPadding - tickLength, y: tickY))
            tickPath.stroke()
            // Draw tick label
            let tickValue : CGFloat = self.maxY - ( self.maxY * scalar )
            let tickLabel = self.yTickLabelFormatter(tickValue)
            
            let tickLabelSize = tickLabel.size(withAttributes: self.tickTextAttributes);
            let tickLabelX : CGFloat = axisPadding - tickLength - tickLabelSize.width
            let tickLabelYOffset = tickYIdx == nTicks ? tickLabelSize.height : tickLabelSize.height * 0.5
            let tickLabelY: CGFloat = tickY - tickLabelYOffset
            let tickRect = CGRect(x: tickLabelX, y: tickLabelY, width: rect.width, height: rect.height)
            tickLabel.draw(in: tickRect, withAttributes: self.tickTextAttributes)
        }
        
        // Draw the line graph
        let graphYSize = rect.height - titleSize.height
        let graphXSize = rect.width - axisPadding
        
        // Set the line color
        UIColor.red.setStroke()
        
        // Set up the coordinate system
        let coordinateSystem = CGRect(x: rect.origin.x, y: rect.origin.y, width: graphXSize, height: graphYSize)
        context.saveGState()
        context.translateBy(x: axisPadding, y: coordinateSystem.height + titleSize.height)
        context.scaleBy(x: 1, y: -1)
        
        // Calculate the scale factors
        let maxX = CGFloat(dataPoints.count)
        let scaleX = coordinateSystem.width / maxX
        let scaleY = coordinateSystem.height / self.maxY
        
        // Create the line path
        let path = UIBezierPath()
        path.lineWidth = 1.0
        
        for (index, dataPoint) in dataPoints.enumerated() {
            let x = CGFloat(index) * scaleX
            let y = dataPoint * scaleY
            
            if index == 0 {
                path.move(to: CGPoint(x: x, y: y))
            } else {
                path.addLine(to: CGPoint(x: x, y: y))
            }
        }
        
        // Draw the line
        path.stroke()
        
        context.restoreGState()
    }
    
}

