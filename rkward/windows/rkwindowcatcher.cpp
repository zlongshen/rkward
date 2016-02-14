/***************************************************************************
                          rwindowcatcher.cpp  -  description
                             -------------------
    begin                : Wed May 4 2005
    copyright            : (C) 2005 - 2013 by Thomas Friedrichsmeier
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

#include "rkwindowcatcher.h"

#ifndef DISABLE_RKWINDOWCATCHER

#include <qlayout.h>
#include <qapplication.h>
#include <QDesktopWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDialog>
#include <QWindow>

#include <kmessagebox.h>
#include <klocale.h>
#include <KWindowSystem>
#include <KWindowInfo>

#include "../settings/rksettingsmodulegraphics.h"
#include "../dialogs/rkerrordialog.h"
#include "rkworkplace.h"
#include "../misc/rkstandardicons.h"
#include "../debug.h"

RKWindowCatcher *RKWindowCatcher::_instance = 0;
RKWindowCatcher* RKWindowCatcher::instance () {
	if (!_instance) {
		RK_TRACE (MISC);
		_instance = new RKWindowCatcher ();
	}
	return _instance;
}

RKWindowCatcher::RKWindowCatcher () : QObject () {
	RK_TRACE (MISC);
}

RKWindowCatcher::~RKWindowCatcher () {
	RK_TRACE (MISC);
}

void RKWindowCatcher::start (int prev_cur_device) {
	RK_TRACE (MISC);
	RK_DEBUG (RBACKEND, DL_DEBUG, "Window Catcher activated");

	last_cur_device = prev_cur_device;
	created_window = 0;
	connect (KWindowSystem::self(), &KWindowSystem::windowAdded, this, &RKWindowCatcher::windowAdded);
}

void RKWindowCatcher::stop (int new_cur_device) {
	RK_TRACE (MISC);
	RK_DEBUG (RBACKEND, DL_DEBUG, "Window Catcher deactivated");

	if (!created_window) {
		// we did not see the window, yet? Maybe the event simply hasn't been processed, yet.
		qApp->sync ();
		qApp->processEvents ();
	}
	disconnect (KWindowSystem::self(), &KWindowSystem::windowAdded, this, &RKWindowCatcher::windowAdded);

	if (new_cur_device != last_cur_device) {
		if (created_window) {
			qDebug ("using %x", created_window);
			//qApp->sync ();
			// this appears to have the side-effect of forcing the captured window to sync with X, which is exactly, what we're trying to achieve.
			KWindowInfo wininfo = KWindowSystem::windowInfo (created_window, NET::WMName | NET::WMGeometry);
			QWindow *window = QWindow::fromWinId (created_window);
			RKWorkplace::mainWorkplace ()->newX11Window (window, new_cur_device);
			// All this syncing looks like a bloody hack? Absolutely. It appears to work around the occasional error "figure margins too large" from R, though.
			//qApp->processEvents ();
			//qApp->processEvents ();
		} else {
#if defined Q_OS_MAC
			KMessageBox::information (0, i18n ("You have tried to embed a new R graphics device window in RKWard. However, this is not currently supported in this build of RKWard on Mac OS X. See http://rkward.kde.org/mac for more information."), i18n ("Could not embed R X11 window"), "embed_x11_device_not_supported");
#else
			RKErrorDialog::reportableErrorMessage (0, i18n ("You have tried to embed a new R graphics device window in RKWard. However, either no window was created, or RKWard failed to detect the new window. If you think RKWard should have done better, consider reporting this as a bug. Alternatively, you may want to adjust Settings->Configure RKWard->Onscreen Graphics."), QString (), i18n ("Could not embed R X11 window"), "failure_to_detect_x11_device");
#endif
		}
	}
	last_cur_device = new_cur_device;
}

void RKWindowCatcher::windowAdded (WId id) {
	RK_TRACE (APP);

	// KF5 TODO: Note: Previously, on windows we checked for IsWindow (hwnd) and IsWindowVisible (hwnd), as sometimes invisible ghost windows
	// would be created in addition to the real device window. Is this still needed?
	// A whole lot of windows appear to get created, but it does look like the last one is the one we need.
	created_window = id;
}

void RKWindowCatcher::windowChanged (WId id, NET::Properties properties, NET::Properties2 properties2) {
	Q_UNUSED (properties2);
	if (!(properties & NET::WMName)) return;
	RK_TRACE (APP);
	RKMDIWindow *watcher = name_watchers_list.value (id);
	if (!watcher) return;
	watcher->setCaption (KWindowInfo (id, NET::WMName).name ());
}

// KF5 TODO: should no longer be needed. See QWindow::windowTitleChanged()
void RKWindowCatcher::registerNameWatcher (WId watched, RKMDIWindow *watcher) {
	RK_TRACE (APP);
	RK_ASSERT (!name_watchers_list.contains (watched));

	if (name_watchers_list.isEmpty ()) {
		connect (KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged), this, &RKWindowCatcher::windowChanged);
	}
	name_watchers_list.insert (watched, watcher);
}

void RKWindowCatcher::unregisterNameWatcher (WId watched) {
	RK_TRACE (APP);
	RK_ASSERT (name_watchers_list.contains (watched));

	name_watchers_list.remove (watched);
	if (name_watchers_list.isEmpty ()) {
		disconnect (KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged), this, &RKWindowCatcher::windowChanged);
	}
}

void RKWindowCatcher::updateHistory (QStringList params) {
	RK_TRACE (MISC);
	RK_ASSERT (params.count () >= 1);

	int history_length = params[0].toInt ();
	QStringList labels = params.mid (1, history_length);
	RK_ASSERT (((params.count () - history_length) % 2) == 1)
	for (int i = history_length + 1; i < (params.count () - 1); i += 2) {
		RKCaughtX11Window* window = RKCaughtX11Window::getWindow (params[i].toInt ());
		if (window) {
			int position = params[i+1].toInt ();
			window->updateHistoryActions (history_length, position, labels);
		} else {
			RK_DEBUG (RBACKEND, DL_DEBUG, "Device %d is not managed, while trying to update history", params[i].toInt ());
		}
	}
}

void RKWindowCatcher::killDevice (int device_number) {
	RK_TRACE (MISC);

	RKCaughtX11Window* window = RKCaughtX11Window::getWindow (device_number);
	if (window) {
		window->setKilledInR ();
		window->close (true);
		// KF5 TODO: Still needed?
		QApplication::sync ();
	}
}

///////////////////////////////// END RKWindowCatcher //////////////////////////////////
/**************************************************************************************/
//////////////////////////////// BEGIN RKCaughtX11Window //////////////////////////////


#include <QScrollArea>
#include <qlabel.h>
#ifdef Q_OS_WIN
// KF5 TODO: needed?
#	include "../qwinhost/qwinhost.h"
#	include <windows.h>
#endif
#include <QTimer>
#include <QCloseEvent>
#include <QSpinBox>

#include <ktoggleaction.h>
#include <kselectaction.h>
#include <kactioncollection.h>

#include "../rkglobals.h"
#include "../rbackend/rinterface.h"
#include "../rbackend/rkwarddevice/rkgraphicsdevice.h"
#include "../core/robject.h"
#include "../misc/rkprogresscontrol.h"
#include "../misc/rksaveobjectchooser.h"
#include "../plugin/rkcomponentcontext.h"

// static
QHash<int, RKCaughtX11Window*> RKCaughtX11Window::device_windows;

RKCaughtX11Window::RKCaughtX11Window (QWindow* window_to_embed, int device_number) : RKMDIWindow (0, X11Window) {
	RK_TRACE (MISC);
// TODO: Actually, the WindowCatcher should pass a QWindow*, not WId.
	commonInit (device_number);
	embedded = window_to_embed;

#if !defined Q_OS_MAC
	KWindowInfo wininfo = KWindowSystem::windowInfo (embedded->winId (), NET::WMName | NET::WMGeometry);
	RK_ASSERT (wininfo.valid ());

	// set a fixed size until the window is shown
	xembed_container->setFixedSize (wininfo.geometry ().width (), wininfo.geometry ().height ());
	setGeometry (wininfo.geometry ());	// it's important to set a size, even while not visible. Else DetachedWindowContainer will assign a default size of 640*480, and then size upwards, if necessary.
	setCaption (wininfo.name ());
#else
	RK_ASSERT (false);
#endif

	// KF5 TODO: Still needed?
	// somehow in Qt 4.4.3, when the RKCaughtWindow is reparented the first time, the QX11EmbedContainer may kill its client. Hence we delay the actual embedding until after the window was shown.
	// In some previous version of Qt, this was not an issue, but I did not track the versions.
	QTimer::singleShot (2000, this, SLOT (doEmbed()));
	//doEmbed ();
}

RKCaughtX11Window::RKCaughtX11Window (RKGraphicsDevice* rkward_device, int device_number) : RKMDIWindow (0, X11Window) {
	RK_TRACE (MISC);

	commonInit (device_number);
	rk_native_device = rkward_device;
	xembed_container->setFixedSize (rk_native_device->viewPort ()->size ());
	resize (xembed_container->size ());
	rk_native_device->viewPort ()->setParent (xembed_container);
	xembed_container->layout ()->addWidget (rk_native_device->viewPort ());
	connect (rkward_device, &RKGraphicsDevice::captionChanged, this, &RKCaughtX11Window::setCaption);
	connect (rkward_device, &RKGraphicsDevice::goingInteractive, this, &RKCaughtX11Window::deviceInteractive);
	stop_interaction->setVisible (true);
	stop_interaction->setEnabled (false);
	setCaption (rkward_device->viewPort ()->windowTitle ());

	QTimer::singleShot (0, this, SLOT (doEmbed()));
}

void RKCaughtX11Window::commonInit (int device_number) {
	RK_TRACE (MISC);

	capture = 0;
	embedded = 0;
	rk_native_device = 0;
	killed_in_r = close_attempted = false;
	RKCaughtX11Window::device_number = device_number;
	RK_ASSERT (!device_windows.contains (device_number));
	device_windows.insert (device_number, this);

	error_dialog = new RKProgressControl (0, i18n ("An error occurred"), i18n ("An error occurred"), RKProgressControl::DetailedError);
	setPart (new RKCaughtX11WindowPart (this));
	setMetaInfo (i18n ("Graphics Device Window"), QUrl ("rkward://page/rkward_plot_history"), RKSettings::PageX11);
	initializeActivationSignals ();
	setFocusPolicy (Qt::ClickFocus);
	updateHistoryActions (0, 0, QStringList ());

	QVBoxLayout *layout = new QVBoxLayout (this);
	layout->setContentsMargins (0, 0, 0, 0);
	scroll_widget = new QScrollArea (this);
	layout->addWidget (scroll_widget);

	xembed_container = new QWidget (this);	// QX11EmbedContainer can not be reparented (between the this, and the scroll_widget) directly. Therefore we place it into a container, and reparent that instead.
	// Also, this makes it easier to handle the various different devices
	QVBoxLayout *xembed_layout = new QVBoxLayout (xembed_container);
	xembed_layout->setContentsMargins (0, 0, 0, 0);
	//layout->addWidget (xembed_container);
	scroll_widget->setWidget (xembed_container);
	xembed_container->hide ();

	dynamic_size = false;
	dynamic_size_action->setChecked (false);
}

void RKCaughtX11Window::doEmbed () {
	RK_TRACE (MISC);

	if (embedded) {
/*		if (capture) {
			embedded->setParent (0);
			capture->deleteLater ();
		} */
		qApp->sync ();
		KWindowInfo wininfo = KWindowSystem::windowInfo (embedded->winId (), NET::WMName | NET::WMGeometry);
		capture = QWidget::createWindowContainer (embedded, xembed_container);
		xembed_container->layout ()->addWidget (capture);
		QTimer::singleShot (100, xembed_container, SLOT (show ()));
		// KF5 TODO: Will this detect closed device, correclty? No, will probably need assistance from KWindowSystem.
		connect (embedded, &QObject::destroyed, this, &RKCaughtX11Window::deleteLater);
		// KF5 TODO: Window caption?
	}

	if (!isAttached ()) {
		// make xembed_container resizable, again, now that it actually has a content
		dynamic_size_action->setChecked (true);
		QTimer::singleShot (1000, this, SLOT (fixedSizeToggled ()));
	}

	// try to be helpful when the window is too large to fit on screen
	QRect dims = window ()->frameGeometry ();
	QRect avail = QApplication::desktop ()->availableGeometry (window ());
	if ((dims.width () > avail.width ()) || (dims.height () > avail.height ())) {
		KMessageBox::information (this, i18n ("The current window appears too large to fit on the screen. If this happens regularly, you may want to adjust the default graphics window size in Settings->Configure RKWard->Onscreen Graphics."), i18n ("Large window"), "dont_ask_again_large_x11_window");
	}
}

RKCaughtX11Window::~RKCaughtX11Window () {
	RK_TRACE (MISC);
	RK_ASSERT (device_windows.contains (device_number));
	device_windows.remove (device_number);

	close (false);
	if (capture) RKWindowCatcher::instance ()->unregisterNameWatcher (capture->winId ());
	error_dialog->autoDeleteWhenDone ();
}

void RKCaughtX11Window::setWindowStyleHint (const QString& hint) {
	RK_TRACE (MISC);

	if (hint == "preview") {
		for (int i = actions_not_for_preview.count () - 1; i >= 0; --i) {
			actions_not_for_preview[i]->setVisible (false);
		}
	}
	RKMDIWindow::setWindowStyleHint (hint);
}

void RKCaughtX11Window::forceClose () {
	killed_in_r = true;
	if (embedded) {
		// HACK: Somehow (R 3.0.0alpha), the X11() window is surpisingly die-hard, if it is not closed "the regular way".
		// So we expurge it, and leave the rest to the user.
		embedded->setParent (0);
		qApp->processEvents ();
	}
	RKMDIWindow::close (true);
}

bool RKCaughtX11Window::close (bool also_delete) {
	RK_TRACE (MISC);

	if (killed_in_r || RKGlobals::rInterface ()->backendIsDead ()) {
		return RKMDIWindow::close (also_delete);
	}

	if (rk_native_device) rk_native_device->stopInteraction ();

	QString status = i18n ("Closing device (saving history)");
	if (!close_attempted) {
		RCommand* c = new RCommand ("dev.off (" + QString::number (device_number) + ')', RCommand::App, i18n ("Shutting down device number %1", device_number));
		setStatusMessage (status, c);
		RKGlobals::rInterface ()->issueCommand (c);
		close_attempted = true;
	} else {
		if (KMessageBox::questionYesNo (this, i18n ("<p>The graphics device is being closed, saving the last plot to the plot history. This may take a while, if the R backend is still busy. You can close the graphics device immediately, in case it is stuck. However, the last plot may be missing from the plot history, if you do this.</p>")
#ifdef Q_WS_X11
		+ i18n ("<p>Note: On X11, the embedded window may be expurged, and you will have to close it manually in this case.</p>")
#endif
		, status, KGuiItem (i18n ("Close immediately")), KGuiItem (i18n ("Keep waiting"))) == KMessageBox::Yes) forceClose ();
	}

	return false;
}

void RKCaughtX11Window::prepareToBeAttached () {
	RK_TRACE (MISC);

	dynamic_size_action->setChecked (false);
	fixedSizeToggled ();
	dynamic_size_action->setEnabled (false);
}

void RKCaughtX11Window::prepareToBeDetached () {
	RK_TRACE (MISC);

	dynamic_size_action->setEnabled (true);
	dynamic_size_action->setChecked (true);
	fixedSizeToggled ();
}

void RKCaughtX11Window::deviceInteractive (bool interactive, const QString& prompt) {
	RK_TRACE (MISC);

	stop_interaction->setToolTip (prompt);
	stop_interaction->setEnabled (interactive);

	if (interactive) {
		activate (true);
		// it is necessary to do this also in the wrapper widget. Otherwise, for some reason, the view cannot be expanded, but can be shrunk.
		setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
	} else {
		setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Preferred);
	}
}

void RKCaughtX11Window::stopInteraction () {
	RK_TRACE (MISC);

	RK_ASSERT (rk_native_device);
	rk_native_device->stopInteraction ();
}

void RKCaughtX11Window::fixedSizeToggled () {
	RK_TRACE (MISC);

	if (embedded && !capture) return;  // while in the middle of embedding, don't mess with any of this, it seems to cause trouble
	if (dynamic_size == dynamic_size_action->isChecked ()) return;
	dynamic_size = dynamic_size_action->isChecked ();

	if (dynamic_size_action->isChecked ()) {
		scroll_widget->takeWidget ();
		layout ()->addWidget (xembed_container);
		xembed_container->show ();
		scroll_widget->hide ();
		xembed_container->setMinimumSize (5, 5);
		xembed_container->setMaximumSize (32767, 32767);
	} else {
		xembed_container->setFixedSize (xembed_container->size ());
		layout ()->removeWidget (xembed_container);
		scroll_widget->setWidget (xembed_container);
		scroll_widget->show ();
	}
}

void RKCaughtX11Window::setFixedSize1 () {
	RK_TRACE (MISC);

	dynamic_size_action->setChecked (false);
	fixedSizeToggled ();		// apparently KToggleAction::setChecked () does not invoke the slot!
	xembed_container->setFixedSize (500, 500);
}

void RKCaughtX11Window::setFixedSize2 () {
	RK_TRACE (MISC);

	dynamic_size_action->setChecked (false);
	fixedSizeToggled ();		// see setFixedSize1 () above
	xembed_container->setFixedSize (1000, 1000);
}

void RKCaughtX11Window::setFixedSize3 () {
	RK_TRACE (MISC);

	dynamic_size_action->setChecked (false);
	fixedSizeToggled ();		// see setFixedSize1 () above
	xembed_container->setFixedSize (2000, 2000);
}

void RKCaughtX11Window::setFixedSizeManual () {
	RK_TRACE (MISC);

// TODO: not very pretty, yet
	QDialog *dialog = new QDialog (this);
	dialog->setWindowTitle (i18n ("Specify fixed size"));
	dialog->setModal (true);

	QVBoxLayout *dlayout = new QVBoxLayout (dialog);

	dlayout->addWidget (new QLabel (i18n ("Width"), dialog));
	QSpinBox *width = new QSpinBox (dialog);
	width->setMaximum (32767);
	width->setMinimum (5);
	width->setSingleStep (1);
	width->setValue (xembed_container->width ());
	width->setFocus ();
	width->selectAll ();
	dlayout->addWidget (width);

	dlayout->addWidget (new QLabel (i18n ("Height"), dialog));
	QSpinBox *height = new QSpinBox (dialog);
	height->setMaximum (32767);
	height->setMinimum (5);
	height->setSingleStep (1);
	height->setValue (xembed_container->height ());
	dlayout->addWidget (height);

	QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect (box->button (QDialogButtonBox::Ok), &QPushButton::clicked, dialog, &QDialog::accept);
	connect (box->button (QDialogButtonBox::Cancel), &QPushButton::clicked, dialog, &QDialog::reject);
	box->button (QDialogButtonBox::Ok)->setShortcut (Qt::CTRL | Qt::Key_Return);
	dlayout->addWidget (box);

	dialog->exec ();

	if (dialog->result () == QDialog::Accepted) {
		dynamic_size_action->setChecked (false);
		fixedSizeToggled ();		// see setFixedSize1 () above

		xembed_container->setFixedSize (width->value (), height->value ());
	}

	delete dialog;
}

void RKCaughtX11Window::activateDevice () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("dev.set (" + QString::number (device_number) + ')', RCommand::App, i18n ("Activate graphics device number %1", device_number), error_dialog);
}

void RKCaughtX11Window::copyDeviceToOutput () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("dev.set (" + QString::number (device_number) + ")\ndev.copy (device=rk.graph.on)\nrk.graph.off ()", RCommand::App | RCommand::CCOutput, i18n ("Copy contents of graphics device number %1 to output", device_number), error_dialog);
}

void RKCaughtX11Window::printDevice () {
	RK_TRACE (MISC);

	QString printer_device;
	if (RKSettingsModuleGraphics::kdePrintingEnabled ()) printer_device = "rk.printer.device";
	RKGlobals::rInterface ()->issueCommand ("dev.set (" + QString::number (device_number) + ")\ndev.print (" + printer_device + ')', RCommand::App, i18n ("Print contents of graphics device number %1", device_number), error_dialog);
}

void RKCaughtX11Window::copyDeviceToRObject () {
	RK_TRACE (MISC);

// TODO: not very pretty, yet
	QDialog *dialog = new QDialog (this);
	dialog->setWindowTitle (i18n ("Specify R object"));
	dialog->setModal (true);
	QVBoxLayout *layout = new QVBoxLayout (dialog);

	layout->addWidget (new QLabel (i18n ("Specify the R object name, you want to save the graph to"), dialog));
	RKSaveObjectChooser *chooser = new RKSaveObjectChooser (dialog, "my.plot");
	layout->addWidget (chooser);

	QDialogButtonBox *box = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect (box->button (QDialogButtonBox::Ok), &QPushButton::clicked, dialog, &QDialog::accept);
	connect (box->button (QDialogButtonBox::Cancel), &QPushButton::clicked, dialog, &QDialog::reject);
	box->button (QDialogButtonBox::Ok)->setShortcut (Qt::CTRL | Qt::Key_Return);
	layout->addWidget (box);

	connect (chooser, &RKSaveObjectChooser::changed, box->button (QDialogButtonBox::Ok), &QPushButton::setEnabled);
	if (!chooser->isOk ()) box->button (QDialogButtonBox::Ok)->setEnabled (false);

	dialog->exec ();

	if (dialog->result () == QDialog::Accepted) {
		RK_ASSERT (chooser->isOk ());

		QString name = chooser->currentFullName ();

		RKGlobals::rInterface ()->issueCommand ("dev.set (" + QString::number (device_number) + ")\n" + name + " <- recordPlot ()", RCommand::App | RCommand::ObjectListUpdate, i18n ("Save contents of graphics device number %1 to object '%2'", device_number, name), error_dialog);
	}

	delete dialog;
}

void RKCaughtX11Window::duplicateDevice () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("rk.duplicate.device (" + QString::number (device_number) + ')', RCommand::App, i18n ("Duplicate graphics device number %1", device_number), error_dialog);
}

void RKCaughtX11Window::nextPlot () {
	RK_TRACE (MISC);

	RCommand* c = new RCommand ("rk.next.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Load next plot in device number %1", device_number), error_dialog);
	setStatusMessage (i18n ("Loading plot from history"), c);
	RKGlobals::rInterface ()->issueCommand (c);
}

void RKCaughtX11Window::previousPlot () {
	RK_TRACE (MISC);

	RCommand* c = new RCommand ("rk.previous.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Load previous plot in device number %1", device_number), error_dialog);
	setStatusMessage (i18n ("Loading plot from history"), c);
	RKGlobals::rInterface ()->issueCommand (c);
}

void RKCaughtX11Window::firstPlot () {
	RK_TRACE (MISC);

	RCommand* c = new RCommand ("rk.first.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Load first plot in device number %1", device_number), error_dialog);
	setStatusMessage (i18n ("Loading plot from history"), c);
	RKGlobals::rInterface ()->issueCommand (c);
}

void RKCaughtX11Window::lastPlot () {
	RK_TRACE (MISC);

	RCommand* c = new RCommand ("rk.last.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Load last plot in device number %1", device_number), error_dialog);
	setStatusMessage (i18n ("Loading plot from history"), c);
	RKGlobals::rInterface ()->issueCommand (c);
}

void RKCaughtX11Window::gotoPlot (int index) {
	RK_TRACE (MISC);

	RCommand* c = new RCommand ("rk.goto.plot (" + QString::number (device_number) + ", " + QString::number (index+1) + ')', RCommand::App, i18n ("Load plot %1 in device number %2", index, device_number), error_dialog);
	setStatusMessage (i18n ("Loading plot from history"), c);
	RKGlobals::rInterface ()->issueCommand (c);
}

void RKCaughtX11Window::forceAppendCurrentPlot () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("rk.force.append.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Append this plot to history (device number %1)", device_number), error_dialog);
}

void RKCaughtX11Window::removeCurrentPlot () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("rk.removethis.plot (" + QString::number (device_number) + ')', RCommand::App, i18n ("Remove current plot from history (device number %1)", device_number), error_dialog);
}

void RKCaughtX11Window::clearHistory () {
	RK_TRACE (MISC);

	if (KMessageBox::warningContinueCancel (this, i18n ("This will clear the plot history for all device windows, not just this one. If this is not your intent, press cancel, below.")) != KMessageBox::Continue) return;

	RKGlobals::rInterface ()->issueCommand ("rk.clear.plot.history ()", RCommand::App, i18n ("Clear plot history"), error_dialog);
}

void RKCaughtX11Window::showPlotInfo () {
	RK_TRACE (MISC);

	RKGlobals::rInterface ()->issueCommand ("rk.show.plot.info (" + QString::number (device_number) + ')', RCommand::App, i18n ("Plot properties (device number %1)", device_number), error_dialog);
}

void RKCaughtX11Window::updateHistoryActions (int history_length, int position, const QStringList &labels) {
	RK_TRACE (MISC);

	RKCaughtX11Window::history_length = history_length;
	RKCaughtX11Window::history_position = position;

	plot_first_action->setEnabled ((history_length > 0) && (position > 1));
	plot_prev_action->setEnabled ((history_length > 0) && (position > 1));
	plot_next_action->setEnabled ((history_length > 0) && (position < history_length));
	plot_last_action->setEnabled ((history_length > 0) && (position < history_length));
	QStringList _labels = labels;
	if (position > history_length) _labels.append (i18n ("<Unsaved plot>"));
	plot_list_action->setItems (_labels);
	plot_list_action->setCurrentItem (history_position - 1);
	plot_list_action->setEnabled (history_length > 0);

	plot_force_append_action->setEnabled ((history_length > 0) && (RKSettingsModuleGraphics::plotHistoryEnabled ()));
	plot_remove_action->setEnabled (history_length > 0);

	plot_clear_history_action->setEnabled (history_length > 0);
	plot_properties_action->setEnabled (RKSettingsModuleGraphics::plotHistoryEnabled ());
}

///////////////////////////////// END RKCaughtX11Window ///////////////////////////////
/**************************************************************************************/
//////////////////////////////// BEGIN RKCaughtX11WindowPart //////////////////////////


RKCaughtX11WindowPart::RKCaughtX11WindowPart (RKCaughtX11Window *window) : KParts::Part (0) {
	RK_TRACE (MISC);

	setComponentName (QCoreApplication::applicationName (), QGuiApplication::applicationDisplayName ());

	setWidget (window);
	RKCaughtX11WindowPart::window = window;

	setXMLFile ("rkcatchedx11windowpart.rc");

	window->dynamic_size_action = new KToggleAction (i18n ("Draw area follows size of window"), window);
	connect (window->dynamic_size_action, &KToggleAction::triggered, window, &RKCaughtX11Window::fixedSizeToggled);
	actionCollection ()->addAction ("toggle_fixed_size", window->dynamic_size_action);
	window->actions_not_for_preview.append (window->dynamic_size_action);

	QAction *action;
	action = actionCollection ()->addAction ("set_fixed_size_1", window, SLOT (setFixedSize1()));
	action->setText (i18n ("Set fixed size 500x500"));
	window->actions_not_for_preview.append (action);
	action = actionCollection ()->addAction ("set_fixed_size_2", window, SLOT (setFixedSize2()));
	action->setText (i18n ("Set fixed size 1000x1000"));
	window->actions_not_for_preview.append (action);
	action = actionCollection ()->addAction ("set_fixed_size_3", window, SLOT (setFixedSize3()));
	action->setText (i18n ("Set fixed size 2000x2000"));
	window->actions_not_for_preview.append (action);
	action = actionCollection ()->addAction ("set_fixed_size_manual", window, SLOT (setFixedSizeManual()));
	action->setText (i18n ("Set specified fixed size..."));
	window->actions_not_for_preview.append (action);

	action = actionCollection ()->addAction ("plot_prev", window, SLOT (previousPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("Previous plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionMoveLeft));
	window->plot_prev_action = (QAction *) action;
	action = actionCollection ()->addAction ("plot_first", window, SLOT (firstPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("First plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionMoveFirst));
	window->plot_first_action = (QAction *) action;
	action = actionCollection ()->addAction ("plot_next", window, SLOT (nextPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("Next plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionMoveRight));
	window->plot_next_action = (QAction *) action;
	action = actionCollection ()->addAction ("plot_last", window, SLOT (lastPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("Last plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionMoveLast));
	window->plot_last_action = (QAction *) action;
	action = window->plot_list_action = new KSelectAction (i18n ("Go to plot"), 0);
	window->actions_not_for_preview.append (action);
	window->plot_list_action->setToolBarMode (KSelectAction::MenuMode);
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionListPlots));
	actionCollection ()->addAction ("plot_list", action);
	connect (action, &QAction::triggered, window, &RKCaughtX11Window::gotoPlot);

	action = actionCollection ()->addAction ("plot_force_append", window, SLOT (forceAppendCurrentPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("Append this plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionSnapshot));
	window->plot_force_append_action = (QAction *) action;
	action = actionCollection ()->addAction ("plot_remove", window, SLOT (removeCurrentPlot()));
	window->actions_not_for_preview.append (action);
 	action->setText (i18n ("Remove this plot"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionRemovePlot));
	window->plot_remove_action = (QAction *) action;

	action = actionCollection ()->addAction ("plot_clear_history", window, SLOT (clearHistory()));
	window->plot_clear_history_action = (QAction *) action;
 	action->setText (i18n ("Clear history"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionClear));
	window->actions_not_for_preview.append (action);

	action = actionCollection ()->addAction ("plot_properties", window, SLOT (showPlotInfo()));
	window->plot_properties_action = (QAction *) action;
	action->setText (i18n ("Plot properties"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionDocumentInfo));
	window->actions_not_for_preview.append (action);

	action = actionCollection ()->addAction ("device_activate", window, SLOT (activateDevice()));
	action->setText (i18n ("Make active"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionFlagGreen));
	window->actions_not_for_preview.append (action);
	action = actionCollection ()->addAction ("device_copy_to_output", window, SLOT (copyDeviceToOutput()));
	action->setText (i18n ("Copy to output"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::WindowOutput));
	action = actionCollection ()->addAction (KStandardAction::Print, "device_print", window, SLOT (printDevice()));
	action = actionCollection ()->addAction ("device_copy_to_r_object", window, SLOT (copyDeviceToRObject()));
	action->setText (i18n ("Store as R object..."));
	action = actionCollection ()->addAction ("device_duplicate", window, SLOT (duplicateDevice()));
	action->setText (i18n ("Duplicate"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionWindowDuplicate));

	action = window->stop_interaction = actionCollection ()->addAction ("stop_interaction", window, SLOT (stopInteraction()));
	action->setText (i18n ("Stop interaction"));
	action->setIcon (RKStandardIcons::getIcon (RKStandardIcons::ActionInterrupt));
	action->setVisible (false);

	// initialize context for plugins
	RKComponentGUIXML *context = RKComponentMap::getContext ("x11");
	if (!context) return;
	RKContextHandler *context_handler = context->makeContextHandler (this);
	insertChildClient (context_handler);
	RKComponentPropertyInt *devnum_property = new RKComponentPropertyInt (this, false, 0);
	devnum_property->setIntValue (window->device_number);
	context_handler->addChild ("devnum", devnum_property);
}

RKCaughtX11WindowPart::~RKCaughtX11WindowPart () {
	RK_TRACE (MISC);
}


#endif // DISABLE_RKWINDOWCATCHER
