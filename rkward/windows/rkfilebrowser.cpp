/***************************************************************************
                          rkfilebrowser  -  description
                             -------------------
    begin                : Thu Apr 26 2007
    copyright            : (C) 2007, 2008, 2009, 2010, 2011 by Thomas Friedrichsmeier
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

#include "rkfilebrowser.h"

#include <kdiroperator.h>
#include <kurlcombobox.h>
#include <kurlcompletion.h>
#include <kcompletionbox.h>
#include <ktoolbar.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <kdeversion.h>
#include <KSharedConfig>
#include <kfileitemactions.h>
#include <kfileitemlistproperties.h>

#include <qdir.h>
#include <qlayout.h>
#include <QEvent>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QMenu>

#include "rkworkplace.h"
#include "../rkward.h"
#include "../misc/rkdummypart.h"

#include "../debug.h"

// static
RKFileBrowser *RKFileBrowser::main_browser = 0;

RKFileBrowser::RKFileBrowser (QWidget *parent, bool tool_window, const char *name) : RKMDIWindow (parent, FileBrowserWindow, tool_window, name) {
	RK_TRACE (APP);

	real_widget = 0;

	QVBoxLayout *layout = new QVBoxLayout (this);
	layout->setContentsMargins (0, 0, 0, 0);
	layout_widget = new KVBox (this);
	layout->addWidget (layout_widget);
	layout_widget->setFocusPolicy (Qt::StrongFocus);

	RKDummyPart *part = new RKDummyPart (this, layout_widget);
	setPart (part);
	initializeActivationSignals ();
}

RKFileBrowser::~RKFileBrowser () {
	RK_TRACE (APP);

	hide ();
}

void RKFileBrowser::showEvent (QShowEvent *e) {
	RK_TRACE (APP);

	if (!real_widget) {
		RK_DEBUG (APP, DL_INFO, "creating file browser");

		real_widget = new RKFileBrowserWidget (layout_widget);
		setFocusProxy (real_widget);
	}

	RKMDIWindow::showEvent (e);
}

void RKFileBrowser::currentWDChanged () {
	RK_TRACE (APP);
}

/////////////////// RKFileBrowserWidget ////////////////////

RKFileBrowserWidget::RKFileBrowserWidget (QWidget *parent) : KVBox (parent) {
	RK_TRACE (APP);

	KToolBar *toolbar = new KToolBar (this);
	toolbar->setIconSize (QSize (16, 16));
	toolbar->setToolButtonStyle (Qt::ToolButtonIconOnly);

	urlbox = new KUrlComboBox (KUrlComboBox::Directories, true, this);
	KUrlCompletion* cmpl = new KUrlCompletion (KUrlCompletion::DirCompletion);
	urlbox->setCompletionObject (cmpl);
	urlbox->setAutoDeleteCompletionObject (true);
	urlbox->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed));
	urlbox->completionBox (true)->installEventFilter (this);
	setFocusProxy (urlbox);

	dir = new KDirOperator (QUrl (), this);
	dir->setPreviewWidget (0);
	KConfigGroup config = KSharedConfig::openConfig ()->group ("file browser window");
	dir->readConfig (config);
	dir->setView (KFile::Default);
	connect (RKWardMainWindow::getMain (), SIGNAL (aboutToQuitRKWard()), this, SLOT (saveConfig()));

	toolbar->addAction (dir->actionCollection ()->action ("up"));
	toolbar->addAction (dir->actionCollection ()->action ("back"));
	toolbar->addAction (dir->actionCollection ()->action ("forward"));
	toolbar->addAction (dir->actionCollection ()->action ("home"));
	toolbar->addAction (dir->actionCollection ()->action ("short view"));
	toolbar->addAction (dir->actionCollection ()->action ("tree view"));
	toolbar->addAction (dir->actionCollection ()->action ("detailed view"));
//	toolbar->addAction (dir->actionCollection ()->action ("detailed tree view"));	// should we have this as well? Trying to avoid crowding in the toolbar

	fi_actions = new KFileItemActions (this);
	connect (dir, &KDirOperator::contextMenuAboutToShow, this, &RKFileBrowserWidget::contextMenuHook);

	connect (dir, &KDirOperator::urlEntered, this, &RKFileBrowserWidget::urlChangedInView);
	connect (urlbox, static_cast<void (KUrlComboBox::*)(const QString&)>(&KUrlComboBox::returnPressed), this, &RKFileBrowserWidget::stringChangedInCombo);
	connect (urlbox, &KUrlComboBox::urlActivated, this, &RKFileBrowserWidget::urlChangedInCombo);

	connect (dir, &KDirOperator::fileSelected, this, &RKFileBrowserWidget::fileActivated);

	setURL (QUrl::fromLocalFile (QDir::currentPath ()));
}

RKFileBrowserWidget::~RKFileBrowserWidget () {
	RK_TRACE (APP);
}

void RKFileBrowserWidget::contextMenuHook(const KFileItem& item, QMenu* menu) {
	RK_TRACE (APP);

	QList<KFileItem> dummy;
	dummy.append (item);
	fi_actions->setItemListProperties (KFileItemListProperties (dummy));

	// some versions of KDE appear to re-use the actions, others don't, and yet other are just plain broken (see this thread: https://mail.kde.org/pipermail/rkward-devel/2011-March/002770.html)
	// Therefore, we remove all actions, explicitly, each time the menu is shown, then add them again.
	QList<QAction*> menu_actions = menu->actions ();
	foreach (QAction* act, menu_actions) if (added_service_actions.contains (act)) menu->removeAction (act);
	added_service_actions.clear ();
	menu_actions = menu->actions ();

	fi_actions->addOpenWithActionsTo (menu, QString ());
	fi_actions->addServiceActionsTo (menu);

	QList<QAction*> menu_actions_after = menu->actions ();
	foreach (QAction* act, menu_actions_after) if (!menu_actions.contains (act)) added_service_actions.append (act);
}

// does not work in d-tor. Apparently it's too late, then
void RKFileBrowserWidget::saveConfig () {
	RK_TRACE (APP);

	KConfigGroup config = KSharedConfig::openConfig ()->group ("file browser window");
	dir->writeConfig (config);
}

void RKFileBrowserWidget::setURL (const QUrl &url) {
	RK_TRACE (APP);

	urlbox->setUrl (url);
	dir->setUrl (url, true);
}

void RKFileBrowserWidget::urlChangedInView (const QUrl &url) {
	RK_TRACE (APP);

	urlbox->setUrl (url);
}

void RKFileBrowserWidget::stringChangedInCombo (const QString &url) {
	RK_TRACE (APP);

	dir->setUrl (QUrl::fromUserInput (url, QDir::currentPath (), QUrl::AssumeLocalFile), true);
}

void RKFileBrowserWidget::urlChangedInCombo (const QUrl &url) {
	RK_TRACE (APP);

	dir->setUrl (url, true);
}

bool RKFileBrowserWidget::eventFilter (QObject* o, QEvent* e) {
	// don't trace here

	if (o == urlbox->completionBox () && e->type () == QEvent::Resize) {
		RK_TRACE (APP);
		// this hack (originally from a KDE 3 version of kate allows the completion popup to span beyond the border of the filebrowser widget itself

		KCompletionBox* box = urlbox->completionBox ();
		RK_ASSERT (box);

		int add = box->verticalScrollBar ()->isVisible () ? box->verticalScrollBar ()->width () : 0;
		box->setMinimumWidth (qMin (topLevelWidget ()->width (), box->sizeHintForColumn (0) + add));

		return false;
	}

	return (KVBox::eventFilter (o, e));
}

void RKFileBrowserWidget::fileActivated (const KFileItem& item) {
	RK_TRACE (APP);

	RKWorkplace::mainWorkplace ()->openAnyUrl (item.url ());
}

