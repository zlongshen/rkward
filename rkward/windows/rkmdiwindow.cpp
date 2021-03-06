/***************************************************************************
                          rkmdiwindow  -  description
                             -------------------
    begin                : Tue Sep 26 2006
    copyright            : (C) 2006, 2007, 2008, 2009, 2010, 2011 by Thomas Friedrichsmeier
    email                : thomas.friedrichsmeier@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "rkmdiwindow.h"

#include <qapplication.h>
#include <qpainter.h>
#include <qtimer.h>
#include <QEvent>
#include <QPaintEvent>

#include <kparts/event.h>
#include <kxmlguifactory.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kaction.h>
#include <kpassivepopup.h>

#include "rkworkplace.h"
#include "rkworkplaceview.h"
#include "rktoolwindowbar.h"
#include "rktoolwindowlist.h"
#include "../settings/rksettingsmodulegeneral.h"
#include "../misc/rkstandardicons.h"
#include "../misc/rkxmlguisyncer.h"
#include "../rbackend/rcommand.h"

#include "../debug.h"

RKMDIStandardActionClient::RKMDIStandardActionClient () : KXMLGUIClient () {
	RK_TRACE (APP);

	setComponentData (KGlobal::mainComponent ());
	setXMLFile ("rkstandardactions.rc", true);
}

RKMDIStandardActionClient::~RKMDIStandardActionClient () {
	RK_TRACE (APP);
}


// TODO: remove name parameter
RKMDIWindow::RKMDIWindow (QWidget *parent, int type, bool tool_window, const char *) : QFrame (parent) {
	RK_TRACE (APP);

	if (tool_window) {
		type |= ToolWindow;
	} else {
		type |= DocumentWindow;
	}
	RKMDIWindow::type = type;
	state = Attached;
	tool_window_bar = 0;
	part = 0;
	active = false;
	no_border_when_active = false;
	standard_client = 0;
	status_popup = 0;

	setWindowIcon (RKStandardIcons::iconForWindow (this));
}

RKMDIWindow::~RKMDIWindow () {
	RK_TRACE (APP);

	if (isToolWindow ()) RKToolWindowList::unregisterToolWindow (this);
	delete standard_client;
	delete status_popup;
}

KActionCollection *RKMDIWindow::standardActionCollection () {
	if (!standard_client) {
		RK_TRACE (APP);
		standard_client = new RKMDIStandardActionClient ();
		RK_ASSERT (part);	// call setPart () first!
		part->insertChildClient (standard_client);
	}
	return standard_client->actionCollection ();
}

//virtual
QString RKMDIWindow::fullCaption () {
	RK_TRACE (APP);
	return shortCaption ();
}

//virtual
QString RKMDIWindow::shortCaption () {
	RK_TRACE (APP);
	return windowTitle ();
}

void RKMDIWindow::setCaption (const QString &caption) {
	RK_TRACE (APP);
	QWidget::setWindowTitle (caption);
	emit (captionChanged (this));
}

bool RKMDIWindow::isActive () {
	// don't trace, called pretty often

	if (!topLevelWidget ()->isActiveWindow ()) return false;
	return (active || (!isAttached ()));
}

void RKMDIWindow::activate (bool with_focus) {
	RK_TRACE (APP);

	QWidget *old_focus = qApp->focusWidget ();

	if (isToolWindow ()) {
		if (tool_window_bar) tool_window_bar->showWidget (this);
		else if (!isVisible ()) RKWorkplace::mainWorkplace ()->detachWindow (this, true);
		else {
			topLevelWidget ()->show ();
			topLevelWidget ()->raise ();
		}
	} else {
		if (isAttached ()) RKWorkplace::mainWorkplace ()->view ()->setActivePage (this);
		else {
			topLevelWidget ()->show ();
			topLevelWidget ()->raise ();
		}
	}

	emit (windowActivated (this));
	if (with_focus) {
		if (old_focus) old_focus->clearFocus ();
		topLevelWidget ()->activateWindow ();
		setFocus();
	} else {
		if (old_focus) {
			old_focus->setFocus ();
			active = false;
		}
	}
}

bool RKMDIWindow::close (bool also_delete) {
	RK_TRACE (APP);

	if (isToolWindow ()) {
		if (!isAttached ()) {
			topLevelWidget ()->deleteLater ();
			// flee the dying DetachedWindowContainer
			if (tool_window_bar) RKWorkplace::mainWorkplace ()->attachWindow (this);
			else {
				state = Attached;
				hide ();
				setParent (0);
			}
		}

		if (tool_window_bar) tool_window_bar->hideWidget (this);
		else hide ();
		return true;
	}

	if (also_delete) {
		bool closed = QWidget::close ();
		if (closed) {
			// WORKAROUND for https://bugs.kde.org/show_bug.cgi?id=170806
			// NOTE: can't move this to the d'tor, since the part is already partially deleted, then
			// TODO: use version check / remove once fixed in kdelibs
			if (part && part->factory ()) {
				part->factory ()->removeClient (part);
			}
			// WORKAROUND end

			delete this;	// Note: using deleteLater(), here does not work well while restoring workplaces (window is not fully removed from workplace before restoring)
		}
		return closed;
	} else {
		RK_ASSERT (!testAttribute (Qt::WA_DeleteOnClose));
		return QWidget::close ();
	}
}

void RKMDIWindow::prepareToBeAttached () {
	RK_TRACE (APP);
}

void RKMDIWindow::prepareToBeDetached () {
	RK_TRACE (APP);

	if (isToolWindow ()) {
		if (tool_window_bar) tool_window_bar->hideWidget (this);
	}
}

bool RKMDIWindow::eventFilter (QObject *watched, QEvent *e) {
	// WARNING: The derived object and the part may both the destroyed at this point of time!
	// Make sure not to call any virtual function on this object!
	RK_ASSERT (acceptsEventsFor (watched));

	if (watched == getPart ()) {
		if (KParts::PartActivateEvent::test (e)) {
			RK_TRACE (APP);		// trace only the "interesting" calls to this function

			KParts::PartActivateEvent *ev = static_cast<KParts::PartActivateEvent *> (e);
			if (ev->activated ()) {
				emit (windowActivated (this));
				setFocus ();		// focus doesn't always work correctly for the kate part
				active = true;
			} else {
				active = false;
			}
			if (layout()->margin () < 1) {
				layout()->setMargin (1);
			}
			update ();
		}
	}
	return false;
}

bool RKMDIWindow::acceptsEventsFor (QObject *object) {
	// called very often. Don't trace

	if (object == getPart ()) return true;
	return false;
}

void RKMDIWindow::initializeActivationSignals () {
	RK_TRACE (APP);

	RK_ASSERT (getPart ());
	getPart ()->installEventFilter (this);

	RKXMLGUISyncer::self ()->watchXMLGUIClientUIrc (getPart ());
}

void RKMDIWindow::paintEvent (QPaintEvent *e) {
	// RK_TRACE (APP); Do not trace!

	QFrame::paintEvent (e);

	if (isActive () && !no_border_when_active) {
		QPainter paint (this);
		paint.setPen (QColor (255, 0, 0));
		paint.drawLine (0, 0, 0, height ()-1);
		paint.drawLine (0, height ()-1, width ()-1, height ()-1);
		paint.drawLine (0, 0, width ()-1, 0);
		paint.drawLine (width ()-1, 0, width ()-1, height ()-1);
	}
}

void RKMDIWindow::windowActivationChange (bool) {
	RK_TRACE (APP);

	// NOTE: active is NOT the same as isActive(). Active just means that this window *would* be active, if its toplevel window is active.
	if (active || (!isAttached ())) update ();
}

void RKMDIWindow::slotActivateForFocusFollowsMouse () {
	RK_TRACE (APP);

	if (!underMouse ()) return;

	// we can't do without activateWindow(), below. Unfortunately, this has the side effect of raising the window (in some cases). This is not always what we want, e.g. if a 
	// plot window is stacked above this window. (And since this is activation by mouse hover, this window is already visible, by definition!)
	// So we try a heuristic (imperfect) to find, if there are any other windows stacked above this one, in order to re-raise them above this.
	QWidgetList toplevels = qApp->topLevelWidgets ();
	QWidgetList overlappers;
	QWidget *window = topLevelWidget ();
	QRect rect = window->frameGeometry ();
	for (int i = toplevels.size () - 1; i >= 0; --i) {
		QWidget *tl = toplevels[i];
		if (!tl->isWindow ()) continue;
		if (tl == window) continue;
		if (tl->isHidden ()) continue;

		QRect tlrect = tl->geometry ();
		QRect intersected = tlrect.intersected (rect);
		if (!intersected.isEmpty ()) {
			QWidget *above = qApp->topLevelAt ((intersected.left () +intersected.right ()) / 2, (intersected.top () +intersected.bottom ()) / 2);
			if (above && (above != window) && (above->isWindow ()) && (!above->isHidden ()) && (overlappers.indexOf (above) < 0)) overlappers.append (above);
		}
	}

	activate (true);

	for (int i = 0; i < overlappers.size (); ++i) {
		overlappers[i]->raise ();
	}
}

void RKMDIWindow::enterEvent (QEvent *event) {
	RK_TRACE (APP);

	if (!isActive ()) {
		if (RKSettingsModuleGeneral::mdiFocusPolicy () == RKSettingsModuleGeneral::RKMDIFocusFollowsMouse) {
			if (!QApplication::activePopupWidget ()) {
				// see http://sourceforge.net/p/rkward/bugs/90/
				// enter events may be delivered while a popup-menu (in a different window) is executing. If we activate in this case, the popup-menu might get deleted
				// while still handling events.
				//
				// Similar problems seem to occur, when the popup menu has just finished (by the user selecting an action) and this results
				// in the mouse entering this widget. To prevent crashes in this second case, we delay the activation until the next iteration of the event loop.
				//
				// Finally, in some cases (such as when a new script window was created), we need a short delay, as we may be catching an enter event on a window that is in the same place,
				// where the newly created window goes. This would cause activation to switch back, immediately.
				QTimer::singleShot (50, this, SLOT (slotActivateForFocusFollowsMouse()));
			}
		}
	}

	QFrame::enterEvent (event);
}

void RKMDIWindow::setStatusMessage (const QString& message, RCommand *command) {
	RK_TRACE (MISC);

	if (!status_popup) {
		status_popup = new KPassivePopup (this);
		disconnect (status_popup, SIGNAL (clicked()), status_popup, SLOT (hide()));   // no auto-hiding, please
	}

	if (command) connect (command->notifier (), SIGNAL (commandFinished (RCommand*)), this, SLOT (clearStatusMessage()));
	if (!message.isEmpty ()) {
		status_popup->setView (QString (), message);
		status_popup->show (this->mapToGlobal (QPoint (20, 20)));
		status_popup->setTimeout (0);
	} else {
		status_popup->hide ();
		status_popup->setTimeout (10);  // this is a lame way to keep track of whether the popup is empty. See showEvent()
	}
}

void RKMDIWindow::clearStatusMessage () {
	RK_TRACE (APP);

	setStatusMessage (QString ());
}

void RKMDIWindow::hideEvent (QHideEvent* ev) {
	if (status_popup) {
		status_popup->hide ();
	}
	QWidget::hideEvent (ev);
}

void RKMDIWindow::showEvent (QShowEvent* ev) {
	if (status_popup && (status_popup->timeout () == 0)) status_popup->show (this->mapToGlobal (QPoint (20, 20)));
	QWidget::showEvent (ev);
}


void RKMDIWindow::setWindowStyleHint (const QString& hint) {
	RK_TRACE (APP);

	if (hint == "preview") {
		if (standard_client) {
			QAction *act = standardActionCollection ()->action ("window_help");
			if (act) act->setVisible (false);
			act = standardActionCollection ()->action ("window_configure");
			if (act) act->setVisible (false);
		}
		no_border_when_active = true;
	}
}

void RKMDIWindow::setMetaInfo (const QString& _generic_window_name, const QString& _help_url, RKSettings::SettingsPage _settings_page) {
	RK_TRACE (APP);

	// only meant to be called once
	RK_ASSERT (generic_window_name.isEmpty() && help_url.isEmpty ());
	generic_window_name = _generic_window_name;
	help_url = _help_url;
	settings_page = _settings_page;

	if (!help_url.isEmpty ()) {
		KAction *action = standardActionCollection ()->addAction ("window_help", this, SLOT (showWindowHelp()));
		action->setText (i18n ("Help on %1", generic_window_name));
	}
	if (settings_page != RKSettings::NoPage) {
		KAction *action = standardActionCollection ()->addAction ("window_configure", this, SLOT (showWindowSettings()));
		action->setText (i18n ("Configure %1", generic_window_name));
	}
}

void RKMDIWindow::showWindowHelp () {
	RK_TRACE (APP);

	RK_ASSERT (!help_url.isEmpty ());
	RKWorkplace::mainWorkplace()->openHelpWindow (help_url, true);
}

void RKMDIWindow::showWindowSettings () {
	RK_TRACE (APP);

	RK_ASSERT (settings_page != RKSettings::NoPage);
	RKSettings::configureSettings (settings_page, this);
}


#include "rkmdiwindow.moc"
