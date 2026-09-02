// Copyright (c) 2022 and onwards The McBopomofo Authors.
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

import Cocoa

/// The window controller for showing tooltops.
public class TooltipController: NSWindowController {
    private var backgroundColor: NSColor
    private var textColor: NSColor
    private let animationDuration: TimeInterval
    private var visibilityGeneration: UInt = 0
    private var messageTextField: NSTextField
    private var tooltip: String = "" {
        didSet {
            messageTextField.stringValue = tooltip
            adjustSize()
        }
    }

    /// Creates a new instance.
    public init(
        backgroundColor: NSColor = NSColor(
            calibratedHue: 0.16, saturation: 0.22, brightness: 0.97, alpha: 1.0),
        textColor: NSColor = .black,
        font: NSFont = .systemFont(ofSize: NSFont.systemFontSize(for: .small)),
        cornerRadius: CGFloat = 0,
        animationDuration: TimeInterval = 0
    ) {
        self.backgroundColor = backgroundColor
        self.textColor = textColor
        self.animationDuration = animationDuration
        let contentRect = NSRect(x: 128.0, y: 128.0, width: 300.0, height: 20.0)
        let styleMask: NSWindow.StyleMask = [.borderless, .nonactivatingPanel]
        let panel = NSPanel(
            contentRect: contentRect, styleMask: styleMask, backing: .buffered, defer: false)
        panel.level = NSWindow.Level(Int(kCGPopUpMenuWindowLevel) + 1)
        panel.hasShadow = true
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.contentView?.wantsLayer = true
        panel.contentView?.layer?.cornerRadius = cornerRadius
        panel.contentView?.layer?.masksToBounds = cornerRadius > 0

        messageTextField = NSTextField()
        messageTextField.isEditable = false
        messageTextField.isSelectable = false
        messageTextField.isBezeled = false
        messageTextField.textColor = textColor
        messageTextField.drawsBackground = true
        messageTextField.backgroundColor = backgroundColor
        messageTextField.font = font
        panel.contentView?.addSubview(messageTextField)

        super.init(window: panel)
    }

    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    /// Present the tooltip window with a given text and a location.
    ///
    /// - Parameters:
    ///   - tooltip: The text to display.
    ///   - point: The origin of the window.
    @objc(showTooltip:atPoint:)
    public func show(tooltip: String, at point: NSPoint) {
        self.tooltip = tooltip
        set(windowLocation: point)
        fadeIn()
    }

    /// Presents the tooltip near an input caret, preferring its lower-right
    /// side and automatically flipping above or to the left near screen edges.
    public func show(
        tooltip: String,
        near anchorRect: NSRect,
        backgroundColor: NSColor? = nil,
        textColor: NSColor? = nil
    ) {
        updateAppearance(backgroundColor: backgroundColor, textColor: textColor)
        self.tooltip = tooltip
        set(windowNear: anchorRect)
        fadeIn()
    }

    /// Hide the tooltip window.
    @objc
    public func hide() {
        hide(after: 0)
    }

    /// Hides the tooltip after a short delay. A subsequent show cancels it.
    public func hide(after delay: TimeInterval) {
        visibilityGeneration &+= 1
        let generation = visibilityGeneration
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self, self.visibilityGeneration == generation else {
                return
            }
            self.fadeOut(generation: generation)
        }
    }

    private func updateAppearance(backgroundColor: NSColor?, textColor: NSColor?) {
        if let backgroundColor {
            self.backgroundColor = backgroundColor
            messageTextField.backgroundColor = backgroundColor
        }
        if let textColor {
            self.textColor = textColor
            messageTextField.textColor = textColor
        }
    }

    private func fadeIn() {
        visibilityGeneration &+= 1
        guard let window else { return }
        if !window.isVisible && animationDuration > 0 {
            window.alphaValue = 0
        }
        window.orderFront(nil)
        guard animationDuration > 0 else {
            window.alphaValue = 1
            return
        }
        NSAnimationContext.runAnimationGroup { context in
            context.duration = animationDuration
            window.animator().alphaValue = 1
        }
    }

    private func fadeOut(generation: UInt) {
        guard let window, window.isVisible else { return }
        guard animationDuration > 0 else {
            window.orderOut(nil)
            return
        }
        NSAnimationContext.runAnimationGroup { context in
            context.duration = animationDuration
            window.animator().alphaValue = 0
        } completionHandler: { [weak self, weak window] in
            guard let self, self.visibilityGeneration == generation else {
                return
            }
            window?.orderOut(nil)
            window?.alphaValue = 1
        }
    }

    /// Set the location of the tooltip window.
    ///
    /// - Parameter windowTopLeftPoint: The givin origin.
    private func set(windowLocation windowTopLeftPoint: NSPoint) {

        var adjustedPoint = windowTopLeftPoint
        adjustedPoint.y -= 5

        var screenFrame = NSScreen.main?.visibleFrame ?? NSRect.zero
        for screen in NSScreen.screens {
            let frame = screen.visibleFrame
            if windowTopLeftPoint.x >= frame.minX && windowTopLeftPoint.x <= frame.maxX
                && windowTopLeftPoint.y >= frame.minY && windowTopLeftPoint.y <= frame.maxY
            {
                screenFrame = frame
                break
            }
        }

        let windowSize = window?.frame.size ?? NSSize.zero

        // bottom beneath the screen?
        if adjustedPoint.y - windowSize.height < screenFrame.minY {
            adjustedPoint.y = screenFrame.minY + windowSize.height
        }

        // top over the screen?
        if adjustedPoint.y >= screenFrame.maxY {
            adjustedPoint.y = screenFrame.maxY - 1.0
        }

        // right
        if adjustedPoint.x + windowSize.width >= screenFrame.maxX {
            adjustedPoint.x = screenFrame.maxX - windowSize.width
        }

        // left
        if adjustedPoint.x < screenFrame.minX {
            adjustedPoint.x = screenFrame.minX
        }

        window?.setFrameTopLeftPoint(adjustedPoint)

    }

    private func set(windowNear anchorRect: NSRect) {
        let screenFrame = NSScreen.screens.first(where: {
            $0.visibleFrame.intersects(anchorRect)
        })?.visibleFrame ?? NSScreen.main?.visibleFrame ?? .zero
        let windowSize = window?.frame.size ?? .zero
        let spacing: CGFloat = 6

        var x = anchorRect.maxX + spacing
        if x + windowSize.width > screenFrame.maxX {
            x = anchorRect.minX - windowSize.width - spacing
        }
        x = min(max(x, screenFrame.minX), screenFrame.maxX - windowSize.width)

        var top = anchorRect.minY - spacing
        if top - windowSize.height < screenFrame.minY {
            top = anchorRect.maxY + spacing + windowSize.height
        }
        top = min(max(top, screenFrame.minY + windowSize.height), screenFrame.maxY)
        window?.setFrameTopLeftPoint(NSPoint(x: x, y: top))
    }

    private func adjustSize() {
        let attrString = messageTextField.attributedStringValue
        var rect = attrString.boundingRect(
            with: NSSize(width: 1600.0, height: 1600.0), options: .usesLineFragmentOrigin)
        rect.size.width += 10
        messageTextField.frame = rect
        window?.setFrame(rect, display: true)
    }

}
