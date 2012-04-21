/**
 * @file
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_UI_UI_NODE_ABSTRACTNODE_H
#define CLIENT_UI_UI_NODE_ABSTRACTNODE_H

#include "../../../shared/cxx.h"
#include "../ui_nodes.h"
#include "../ui_node.h"

struct uiBehaviour_s;

class uiNode {
public:
	/* system allocation */

	/** Called before script initialization, initialized default values */
	virtual void loading(uiNode_t* node) {}
	/** only called one time, when node parsing was finished */
	virtual void loaded(uiNode_t* node) {}
	/** call to initialize a cloned node */
	virtual void clone(uiNode_t const* source, uiNode_t* clone) {}
	/** call to initialize a dynamic node */
	virtual void newNode(uiNode_t* node) {}
	/** call to delete a dynamic node */
	virtual void deleteNode(uiNode_t* node) {}

	/* system callback */

	/** Invoked when the window is added to the rendering stack */
	virtual void windowOpened(uiNode_t* node, linkedList_t *params);
	/** Invoked when the window is removed from the rendering stack */
	virtual void windowClosed(uiNode_t* node);
	/** Activate the node. Can be used without the mouse (ie. a button will execute onClick) */
	virtual void activate(uiNode_t* node);
	/** Called when a property change */
	virtual void propertyChanged(uiNode_t* node, const value_t *property);

	virtual ~uiNode() {}
};

class uiLocatedNode : public uiNode {
public:
	/** How to draw a node */
	virtual void draw(uiNode_t* node) {}
	/** Allow to draw a custom tooltip */
	virtual void drawTooltip(uiNode_t* node, int x, int y);
	/** Callback to draw content over the window @sa UI_CaptureDrawOver */
	virtual void drawOverWindow(uiNode_t* node) {}

	/** Called to update node layout */
	virtual void doLayout(uiNode_t* node);
	/** Called when the node size change */
	virtual void sizeChanged(uiNode_t* node);

	/* mouse events */

	/** Left mouse click event in the node */
	virtual void leftClick(uiNode_t* node, int x, int y);
	/** Right mouse button click event in the node */
	virtual void rightClick(uiNode_t* node, int x, int y);
	/** Middle mouse button click event in the node */
	virtual void middleClick(uiNode_t* node, int x, int y);
	/** Mouse wheel event in the node */
	virtual bool scroll(uiNode_t* node, int deltaX, int deltaY);
	/* Planned */
#if 0
	/* mouse move event */
	virtual void mouseEnter(uiNode_t* node);
	virtual void mouseLeave(uiNode_t* node);
#endif

	/** Mouse move event in the node */
	virtual void mouseMove(uiNode_t* node, int x, int y) {}
	/** Mouse button down event in the node */
	virtual void mouseDown(uiNode_t* node, int x, int y, int button) {}
	/** Mouse button up event in the node */
	virtual void mouseUp(uiNode_t* node, int x, int y, int button) {}
	/** Mouse entered on the node (a child node is part of the node) */
	virtual void mouseEnter(uiNode_t* node);
	/** Mouse left the node */
	virtual void mouseLeave(uiNode_t* node);
	/** Mouse move event in the node when captured */
	virtual void capturedMouseMove(uiNode_t* node, int x, int y) {}
	/** Capture is finished */
	virtual void capturedMouseLost(uiNode_t* node) {}
	/*
	 * Send mouse event when a pressed mouse button is dragged
	 * @return True if the event is used
	 */
	virtual bool startDragging(uiNode_t* node, int startX, int startY, int currentX, int currentY, int button) {
		return false;
	}

	/* drag and drop callback */

	/** Send to the target when we enter first, return true if we can drop the DND somewhere on the node */
	virtual bool dndEnter(uiNode_t* node);
	/** Send to the target when we enter first, return true if we can drop the DND here */
	virtual bool dndMove(uiNode_t* node, int x, int y);
	/** Send to the target when the DND is canceled */
	virtual void dndLeave(uiNode_t* node);
	/** Send to the target to finalize the drop */
	virtual bool dndDrop(uiNode_t* node, int x, int y);
	/** Sent to the source to finalize the drop */
	virtual bool dndFinished(uiNode_t* node, bool isDroped);

	/* focus and keyboard events */
	virtual void focusGained(uiNode_t* node) {}
	virtual void focusLost(uiNode_t* node) {}
	virtual bool keyPressed(uiNode_t* node, unsigned int key, unsigned short unicode) {return false;}
	virtual bool keyReleased(uiNode_t* node, unsigned int key, unsigned short unicode) {return false;}

	/** Return the position of the client zone into the node */
	virtual void getClientPosition(uiNode_t const* node, vec2_t position) {}
	/** cell size */
	virtual int getCellWidth (uiNode_t *node) {return 1;}
	/** cell size */
	virtual int getCellHeight (uiNode_t *node) {return 1;}

};

void UI_RegisterAbstractNode(struct uiBehaviour_s *);

#endif
