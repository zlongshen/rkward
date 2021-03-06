/***************************************************************************
                          rkhtmlwindow  -  description
                             -------------------
    begin                : Wed Oct 12 2005
    copyright            : (C) 2005-2015 by Thomas Friedrichsmeier
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
#include "rkhtmlwindow.h"

#include <khtmlview.h>
#include <khtml_part.h>
#include <klibloader.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kparts/plugin.h>
#include <kactioncollection.h>
#include <kdirwatch.h>
#include <kmimetype.h>
#include <kio/job.h>
#include <kservice.h>
#include <ktemporaryfile.h>
#include <kwebview.h>
#include <kcodecaction.h>
#include <kglobalsettings.h>

#include <qfileinfo.h>
#include <qwidget.h>
#include <qlayout.h>
#include <qtimer.h>
#include <qdir.h>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QNetworkRequest>
#include <QWebFrame>
#include <QPrintDialog>
#include <QMenu>
#include <QTextCodec>

#include "../rkglobals.h"
#include "../rbackend/rinterface.h"
#include "rkhelpsearchwindow.h"
#include "../rkward.h"
#include "../rkconsole.h"
#include "../settings/rksettingsmodulegeneral.h"
#include "../settings/rksettingsmoduler.h"
#include "../settings/rksettingsmoduleoutput.h"
#include "../misc/rkcommonfunctions.h"
#include "../misc/rkstandardactions.h"
#include "../misc/rkstandardicons.h"
#include "../misc/xmlhelper.h"
#include "../misc/rkxmlguisyncer.h"
#include "../misc/rkprogresscontrol.h"
#include "../misc/rkmessagecatalog.h"
#include "../misc/rkfindbar.h"
#include "../plugin/rkcomponentmap.h"
#include "../windows/rkworkplace.h"
#include "../windows/rkworkplaceview.h"
#include "../debug.h"

RKWebPage::RKWebPage (RKHTMLWindow* window): KWebPage (window, KIOIntegration | KPartsIntegration) {
	RK_TRACE (APP);
	RKWebPage::window = window;
	new_window = false;
	direct_load = false;
	settings ()->setFontFamily (QWebSettings::StandardFont, KGlobalSettings::generalFont ().family ());
	settings ()->setFontFamily (QWebSettings::FixedFont, KGlobalSettings::fixedFont ().family ());
}

bool RKWebPage::acceptNavigationRequest (QWebFrame* frame, const QNetworkRequest& request, QWebPage::NavigationType type) {
	Q_UNUSED (type);

	RK_TRACE (APP);
	RK_DEBUG (APP, DL_DEBUG, "Navigation request to %s", qPrintable (request.url ().toString ()));
	if (direct_load && (frame == mainFrame ())) {
		direct_load = false;
		return true;
	}

	if (new_window) {
		frame = 0;
		new_window = false;
	}
	if (!frame) {
		RKWorkplace::mainWorkplace ()->openAnyUrl (request.url ());
		return false;
	}

	if (frame != mainFrame ()) {
		if (request.url ().isLocalFile () && (KMimeType::findByUrl (request.url ())->is ("text/html"))) return true;
	}

	if (KUrl (mainFrame ()->url ()).equals (request.url (), KUrl::CompareWithoutFragment | KUrl::CompareWithoutTrailingSlash)) {
		RK_DEBUG (APP, DL_DEBUG, "Page internal navigation request from %s to %s", qPrintable (mainFrame ()->url ().toString ()), qPrintable (request.url ().toString ()));
		emit (pageInternalNavigation (request.url ()));
		return true;
	}

	window->openURL (request.url ());
	return false;
}

void RKWebPage::load (const QUrl& url) {
	RK_TRACE (APP);
	direct_load = true;
	mainFrame ()->load (url);
}

QWebPage* RKWebPage::createWindow (QWebPage::WebWindowType) {
	RK_TRACE (APP);
	new_window = true;         // Don't actually create the window, until we know which URL we're talking about.
	return (this);
}

RKHTMLWindow::RKHTMLWindow (QWidget *parent, WindowMode mode) : RKMDIWindow (parent, RKMDIWindow::HelpWindow) {
	RK_TRACE (APP);

	current_cache_file = 0;

	QVBoxLayout* layout = new QVBoxLayout (this);
	layout->setContentsMargins (0, 0, 0, 0);
	view = new KWebView (this, false);
	page = new RKWebPage (this);
	view->setPage (page);
	view->setContextMenuPolicy (Qt::CustomContextMenu);
	layout->addWidget (view, 1);
	findbar = new RKFindBar (this);
	layout->addWidget (findbar);
	findbar->hide ();
	connect (findbar, SIGNAL(findRequest(QString,bool,const RKFindBar*,bool*)), this, SLOT(findRequest(QString,bool,const RKFindBar*,bool*)));
	have_highlight = false;

	part = new RKHTMLWindowPart (this);
	setPart (part);
	part->initActions ();
	initializeActivationSignals ();
	part->setSelectable (true);
	setFocusPolicy (Qt::StrongFocus);
	setFocusProxy (view);

	// We have to connect this in order to allow browsing.
	connect (page, SIGNAL (pageInternalNavigation(QUrl)), this, SLOT (internalNavigation(QUrl)));
	connect (page, SIGNAL (downloadRequested(QNetworkRequest)), this, SLOT (saveRequested(QNetworkRequest)));
	connect (page, SIGNAL (printRequested(QWebFrame*)), this, SLOT(slotPrint()));
	connect (view, SIGNAL (customContextMenuRequested(QPoint)), this, SLOT(makeContextMenu(QPoint)));

	current_history_position = -1;
	url_change_is_from_history = false;

	window_mode = Undefined;
	useMode (mode);

	// needed to enable / disable the run selection action
	connect (view, SIGNAL (selectionChanged()), this, SLOT (selectionChanged()));
	selectionChanged ();
}

RKHTMLWindow::~RKHTMLWindow () {
	RK_TRACE (APP);

	delete current_cache_file;
}

KUrl RKHTMLWindow::restorableUrl () {
	RK_TRACE (APP);

	return (current_url.url ().replace (RKSettingsModuleR::helpBaseUrl(), "rkward://RHELPBASE"));
}

bool RKHTMLWindow::isModified () {
	RK_TRACE (APP);
	return false;
}

void RKHTMLWindow::makeContextMenu (const QPoint& pos) {
	RK_TRACE (APP);

	QMenu *menu = page->createStandardContextMenu ();
	menu->addAction (part->run_selection);
	menu->exec (view->mapToGlobal (pos));
	delete (menu);
}

void RKHTMLWindow::selectionChanged () {
	RK_TRACE (APP);

	if (!(part && part->run_selection)) {
		RK_ASSERT (false);
		return;
	}

#if QT_VERSION >= 0x040800
	part->run_selection->setEnabled (view->hasSelection ());
#else
	part->run_selection->setEnabled (!view->selectedText ().isEmpty ());
#endif
}

void RKHTMLWindow::runSelection () {
	RK_TRACE (APP);

	RKConsole::pipeUserCommand (view->selectedText ());
}

void RKHTMLWindow::findRequest (const QString& text, bool backwards, const RKFindBar* findbar, bool* found) {
	RK_TRACE (APP);

	QWebPage::FindFlags flags = QWebPage::FindWrapsAroundDocument;
	if (backwards) flags |= QWebPage::FindBackward;
	bool highlight = findbar->isOptionSet (RKFindBar::HighlightAll);
	if (highlight) flags |= QWebPage::HighlightAllOccurrences;
	if (findbar->isOptionSet (RKFindBar::MatchCase)) flags |= QWebPage::FindCaseSensitively;

	// clear previous highlight, if any
	if (have_highlight) page->findText (QString (), QWebPage::HighlightAllOccurrences);

	*found = page->findText (text, flags);
	have_highlight = found && highlight;
}

void RKHTMLWindow::slotPrint () {
	RK_TRACE (APP);

	// NOTE: taken from kwebkitpart, with small mods
	// Make it non-modal, in case a redirection deletes the part
	QPointer<QPrintDialog> dlg (new QPrintDialog (view));
	if (dlg->exec () == QPrintDialog::Accepted) {
		view->print (dlg->printer ());
	}
	delete dlg;
}

void RKHTMLWindow::slotSave () {
	RK_TRACE (APP);

	page->downloadUrl (page->mainFrame ()->url ());
}

void RKHTMLWindow::saveRequested (const QNetworkRequest& request) {
	RK_TRACE (APP);

	page->downloadUrl (request.url ());
}

void RKHTMLWindow::openLocationFromHistory (VisitedLocation &loc) {
	RK_TRACE (APP);
	RK_ASSERT (window_mode == HTMLHelpWindow);

	int history_last = url_history.count () - 1;
	RK_ASSERT (current_history_position >= 0);
	RK_ASSERT (current_history_position <= history_last);
	if (loc.url == current_url) {
		restoreBrowserState (&loc);
	} else {
		url_change_is_from_history = true;
		openURL (loc.url);            // TODO: merge into restoreBrowserState()?
		restoreBrowserState (&loc);
		url_change_is_from_history = false;
	}

	part->back->setEnabled (current_history_position > 0);
	part->forward->setEnabled (current_history_position < history_last);
}

void RKHTMLWindow::slotForward () {
	RK_TRACE (APP);

	++current_history_position;
	openLocationFromHistory (url_history[current_history_position]);
}

void RKHTMLWindow::slotBack () {
	RK_TRACE (APP);

	// if going back from the end of the history, save that position, first.
	if (current_history_position >= (url_history.count () - 1)) {
		changeURL (current_url);
		--current_history_position;
	}
	--current_history_position;
	openLocationFromHistory (url_history[current_history_position]);
}

void RKHTMLWindow::openRKHPage (const KUrl& url) {
	RK_TRACE (APP);

	RK_ASSERT (url.protocol () == "rkward");
	changeURL (url);
	bool ok = false;
	if ((url.host () == "component") || (url.host () == "page")) {
		useMode (HTMLHelpWindow);

		startNewCacheFile ();
		RKHelpRenderer render (current_cache_file);
		ok = render.renderRKHelp (url);
		current_cache_file->close ();

		KUrl cache_url = KUrl::fromLocalFile (current_cache_file->fileName ());
		cache_url.setFragment (url.fragment ());
		page->load (cache_url);
	} else if (url.host ().toUpper () == "RHELPBASE") {	// NOTE: QUrl () may lowercase the host part, internally
		KUrl fixed_url = KUrl (RKSettingsModuleR::helpBaseUrl ());
		fixed_url.setPath (url.path ());
		if (url.hasQuery ()) fixed_url.setQuery (url.query ());
		if (url.hasFragment ()) fixed_url.setFragment (url.fragment ());
		ok = openURL (fixed_url);
	}
	if (!ok) {
		fileDoesNotExistMessage ();
	}
}

// static
bool RKHTMLWindow::handleRKWardURL (const KUrl &url, RKHTMLWindow *window) {
	RK_TRACE (APP);

	if (url.protocol () == "rkward") {
		if (url.host () == "runplugin") {
			QString path = url.path ();
			if (path.startsWith ('/')) path = path.mid (1);
			int sep = path.indexOf ('/');
			// NOTE: These links may originate externally, even from untrusted sources. The submit mode *must* remain "ManualSubmit" for this reason!
			RKComponentMap::invokeComponent (path.left (sep), path.mid (sep+1).split ('\n', QString::SkipEmptyParts), RKComponentMap::ManualSubmit);
			return true;
		} else {
			if (url.host () == "rhelp") {
				// TODO: find a nice solution to render this in the current window
				QStringList spec = url.path ().mid (1).split ('/');
				QString function, package, type;
				if (!spec.isEmpty ()) function = spec.takeLast ();
				if (!spec.isEmpty ()) package = spec.takeLast ();
				if (!spec.isEmpty ()) type = spec.takeLast ();
				RKHelpSearchWindow::mainHelpSearch ()->getFunctionHelp (function, package, type);
				return true;
			}

			if (window) window->openRKHPage (url);
			else RKWorkplace::mainWorkplace ()->openHelpWindow (url);	// will recurse with window set, via openURL()
			return true;
		}
	}
	return false;
}

bool RKHTMLWindow::openURL (const KUrl &url) {
	RK_TRACE (APP);

	if (handleRKWardURL (url, this)) return true;

	if (window_mode == HTMLOutputWindow) {
		if (url != current_url) {
			// output window should not change url after initialization open any links in new windows
			if (!current_url.isEmpty ()) {
				RKWorkplace::mainWorkplace ()->openAnyUrl (url);
				return false;
			}

			current_url = url;	// needs to be set before registering
			RKOutputWindowManager::self ()->registerWindow (this);
		}
	}

	if (url.isLocalFile () && (KMimeType::findByUrl (url)->is ("text/html") || window_mode == HTMLOutputWindow)) {
		changeURL (url);
		QFileInfo out_file (url.toLocalFile ());
		bool ok = out_file.exists();
		if (ok)  {
			page->load (url);
		} else {
			fileDoesNotExistMessage ();
		}
		return ok;
	}

	if (url_change_is_from_history || url.protocol ().toLower ().startsWith ("help")) {	// handle help pages, and any page that we have previously handled (from history)
		changeURL (url);
		page->load (url);
		return true;
	}

	// special casing for R's dynamic help pages. These should be considered local, even though they are served through http
	if (url.protocol ().toLower ().startsWith ("http")) {
		QString host = url.host ();
		if ((host == "127.0.0.1") || (host == "localhost") || host == QHostInfo::localHostName ()) {
			KIO::TransferJob *job = KIO::get (url, KIO::Reload);
			connect (job, SIGNAL (mimetype(KIO::Job*,QString)), this, SLOT (mimeTypeDetermined(KIO::Job*,QString)));
			return true;
		}
	}

	RKWorkplace::mainWorkplace ()->openAnyUrl (url, QString (), KMimeType::findByUrl (url)->is ("text/html"));	// NOTE: text/html type urls, which we have not handled, above, are forced to be opened externally, to avoid recursion. E.g. help:// protocol urls.
	return true;
}

KUrl RKHTMLWindow::url () {
	return current_url;
}

void RKHTMLWindow::mimeTypeDetermined (KIO::Job* job, const QString& type) {
	RK_TRACE (APP);

	KIO::TransferJob* tj = static_cast<KIO::TransferJob*> (job);
	KUrl url = tj->url ();
	tj->putOnHold ();
	if (type == "text/html") {
		changeURL (url);
		page->load (url);
	} else {
		RKWorkplace::mainWorkplace ()->openAnyUrl (url, type);
	}
}

void RKHTMLWindow::internalNavigation (const QUrl& new_url) {
	RK_TRACE (APP);

	KUrl real_url = current_url;    // Note: This could be something quite different from new_url: a temp file for rkward://-urls. We know the base part of the URL has not actually changed, when this gets called, though.
	real_url.setFragment (new_url.fragment ());

	changeURL (real_url);
}

void RKHTMLWindow::changeURL (const KUrl &url) {
	KUrl prev_url = current_url;
	current_url = url;
	updateCaption (url);

	if (!url_change_is_from_history) {
		if (window_mode == HTMLHelpWindow) {
			if (current_history_position >= 0) {	// skip initial blank page
				url_history = url_history.mid (0, current_history_position);

				VisitedLocation loc;
				loc.url = prev_url;
				saveBrowserState (&loc);
				url_history.append (loc);
			}

			++current_history_position;
 			part->back->setEnabled (current_history_position > 0);
			part->forward->setEnabled (false);
		}
	}
}

void RKHTMLWindow::updateCaption (const KUrl &url) {
	RK_TRACE (APP);

	if (window_mode == HTMLOutputWindow) setCaption (i18n ("Output %1").arg (url.fileName ()));
	else setCaption (url.fileName ());
}

void RKHTMLWindow::refresh () {
	RK_TRACE (APP);

	view->reload ();
}

void RKHTMLWindow::scrollToBottom () {
	RK_TRACE (APP);

	RK_ASSERT (window_mode == HTMLOutputWindow);
	view->page ()->mainFrame ()->setScrollBarValue (Qt::Vertical, view->page ()->mainFrame ()->scrollBarMaximum (Qt::Vertical));
}

void RKHTMLWindow::zoomIn () {
	RK_TRACE (APP);
	view->setZoomFactor (view->zoomFactor () * 1.1);
}

void RKHTMLWindow::zoomOut () {
	RK_TRACE (APP);
	view->setZoomFactor (view->zoomFactor () / 1.1);
}

void RKHTMLWindow::setTextEncoding (QTextCodec* encoding) {
	RK_TRACE (APP);

	page->settings ()->setDefaultTextEncoding (encoding->name ());
	view->reload ();
}

void RKHTMLWindow::useMode (WindowMode new_mode) {
	RK_TRACE (APP);

	if (window_mode == new_mode) return;

	if (new_mode == HTMLOutputWindow) {
		type = RKMDIWindow::OutputWindow | RKMDIWindow::DocumentWindow;
		setWindowIcon (RKStandardIcons::getIcon (RKStandardIcons::WindowOutput));
		part->setOutputWindowSkin ();
		setMetaInfo (i18n ("Output Window"), "rkward://page/rkward_output", RKSettings::PageOutput);
		connect (page, SIGNAL(loadFinished(bool)), this, SLOT(scrollToBottom()));
//	TODO: This would be an interesting extension, but how to deal with concurrent edits?
//		page->setContentEditable (true);
	} else {
		RK_ASSERT (new_mode == HTMLHelpWindow);

		type = RKMDIWindow::HelpWindow | RKMDIWindow::DocumentWindow;
		setWindowIcon (RKStandardIcons::getIcon (RKStandardIcons::WindowHelp));
		part->setHelpWindowSkin ();
		disconnect (page, SIGNAL(loadFinished(bool)), this, SLOT(scrollToBottom()));
	}

	updateCaption (current_url);
	window_mode = new_mode;
}

void RKHTMLWindow::startNewCacheFile () {
	delete current_cache_file;
	current_cache_file = new KTemporaryFile ();
	current_cache_file->setSuffix (".html");
	current_cache_file->open ();
}

void RKHTMLWindow::fileDoesNotExistMessage () {
	RK_TRACE (APP);

	startNewCacheFile ();
	if (window_mode == HTMLOutputWindow) {
		current_cache_file->write (i18n ("<HTML><BODY><H1>RKWard output file could not be found</H1>\n</BODY></HTML>").toUtf8 ());
	} else {
		current_cache_file->write (QString ("<html><body><h1>" + i18n ("Page does not exist or is broken") + "</h1></body></html>").toUtf8 ());
	}
	current_cache_file->close ();

	KUrl cache_url = KUrl::fromLocalFile (current_cache_file->fileName ());
	page->load (cache_url);
}

void RKHTMLWindow::flushOutput () {
	RK_TRACE (APP);

	int res = KMessageBox::questionYesNo (this, i18n ("Do you really want to clear the output? This will also remove all image files used in the output. It will not be possible to restore it."), i18n ("Flush output?"));
	if (res==KMessageBox::Yes) {
		QFile out_file (current_url.toLocalFile ());
		RCommand *c = new RCommand ("rk.flush.output (" + RCommand::rQuote (out_file.fileName ()) + ", ask=FALSE)\n", RCommand::App);
		connect (c->notifier (), SIGNAL (commandFinished(RCommand*)), this, SLOT (refresh()));
		RKProgressControl *status = new RKProgressControl (this, i18n ("Flushing output"), i18n ("Flushing output"), RKProgressControl::CancellableNoProgress);
		status->addRCommand (c, true);
		status->doNonModal (true);
		RKGlobals::rInterface ()->issueCommand (c);
	}
}

void RKHTMLWindow::saveBrowserState (VisitedLocation* state) {
	RK_TRACE (APP);

	if (view && view->page () && view->page ()->mainFrame ()) {
		state->scroll_position = view->page ()->mainFrame ()->scrollPosition ();
	} else {
		state->scroll_position = QPoint ();
	}
}

void RKHTMLWindow::restoreBrowserState (VisitedLocation* state) {
	RK_TRACE (APP);

	if (state->scroll_position.isNull ()) return;
	RK_ASSERT (view && view->page () && view->page ()->mainFrame ());
	view->page ()->mainFrame ()->setScrollPosition (state->scroll_position);
}

RKHTMLWindowPart::RKHTMLWindowPart (RKHTMLWindow* window) : KParts::Part (window) {
	RK_TRACE (APP);
	setComponentData (KGlobal::mainComponent ());
	RKHTMLWindowPart::window = window;
	setWidget (window);
}

void RKHTMLWindowPart::initActions () {
	RK_TRACE (APP);

	// We keep our own history.
	window->page->action (QWebPage::Back)->setVisible (false);
	window->page->action (QWebPage::Forward)->setVisible (false);
	// For now we won't bother with this one: Does not behave well, in particular (but not only) WRT to rkward://-links
	window->page->action (QWebPage::DownloadLinkToDisk)->setVisible (false);

	// common actions
	actionCollection ()->addAction (KStandardAction::Copy, "copy", window->view->pageAction (QWebPage::Copy), SLOT (trigger()));
	QAction* zoom_in = actionCollection ()->addAction ("zoom_in", new KAction (KIcon ("zoom-in"), i18n ("Zoom In"), this));
	connect (zoom_in, SIGNAL(triggered(bool)), window, SLOT (zoomIn()));
	QAction* zoom_out = actionCollection ()->addAction ("zoom_out", new KAction (KIcon ("zoom-out"), i18n ("Zoom Out"), this));
	connect (zoom_out, SIGNAL(triggered(bool)), window, SLOT (zoomOut()));
	actionCollection ()->addAction (KStandardAction::SelectAll, "select_all", window->view->pageAction (QWebPage::SelectAll), SLOT (trigger()));
	// unfortunately, this will only affect the default encoding, not necessarily the "real" encoding
	KCodecAction *encoding = new KCodecAction (KIcon ("character-set"), i18n ("Default &Encoding"), this, true);
	encoding->setStatusTip (i18n ("Set the encoding to assume in case no explicit encoding has been set in the page or in the HTTP headers."));
	actionCollection ()->addAction ("view_encoding", encoding);
	connect (encoding, SIGNAL (triggered(QTextCodec*)), window, SLOT (setTextEncoding(QTextCodec*)));

	print = actionCollection ()->addAction (KStandardAction::Print, "print_html", window, SLOT (slotPrint()));
	save_page = actionCollection ()->addAction (KStandardAction::Save, "save_html", window, SLOT (slotSave()));

	run_selection = RKStandardActions::runCurrent (window, window, SLOT (runSelection()));

	// help window actions
	back = actionCollection ()->addAction (KStandardAction::Back, "help_back", window, SLOT (slotBack()));
	back->setEnabled (false);

	forward = actionCollection ()->addAction (KStandardAction::Forward, "help_forward", window, SLOT (slotForward()));
	forward->setEnabled (false);

	// output window actions
	outputFlush = actionCollection ()->addAction ("output_flush", window, SLOT (flushOutput()));
	outputFlush->setText (i18n ("&Flush Output"));
	outputFlush->setIcon (KIcon ("edit-delete"));

	outputRefresh = actionCollection ()->addAction ("output_refresh", window, SLOT (refresh()));
	outputRefresh->setText (i18n ("&Refresh Output"));
	outputRefresh->setIcon (KIcon ("view-refresh"));

	actionCollection ()->addAction (KStandardAction::Find, "find", window->findbar, SLOT (activate()));
	KAction* findAhead = actionCollection ()->addAction ("find_ahead", new KAction (i18n ("Find as you type"), this));
	findAhead->setShortcut ('/');
	connect (findAhead, SIGNAL (triggered(bool)), window->findbar, SLOT (activate()));
	actionCollection ()->addAction (KStandardAction::FindNext, "find_next", window->findbar, SLOT (forward()));;
	actionCollection ()->addAction (KStandardAction::FindPrev, "find_previous", window->findbar, SLOT (backward()));;;
}

void RKHTMLWindowPart::setOutputWindowSkin () {
	RK_TRACE (APP);

	print->setText (i18n ("Print output"));
	save_page->setText (i18n ("Save Output as HTML"));
	setXMLFile ("rkoutputwindow.rc");
	run_selection->setVisible (false);
}

void RKHTMLWindowPart::setHelpWindowSkin () {
	RK_TRACE (APP);

	print->setText (i18n ("Print page"));
	save_page->setText (i18n ("Export page as HTML"));
	setXMLFile ("rkhelpwindow.rc");
	run_selection->setVisible (true);
}

//////////////////////////////////////////
//////////////////////////////////////////

bool RKHelpRenderer::renderRKHelp (const KUrl &url) {
	RK_TRACE (APP);

	if (url.protocol () != "rkward") {
		RK_ASSERT (false);
		return (false);
	}

	bool for_component = false;		// is this a help page for a component, or a top-level help page?
	if (url.host () == "component") for_component = true;

	QStringList anchors, anchornames;

	RKComponentHandle *chandle = 0;
	if (for_component) {
		chandle = componentPathToHandle (url.path ());
		if (!chandle) return false;
	}

	component_xml = new XMLHelper (for_component ? chandle->getFilename () : QString (), for_component ? chandle->messageCatalog () : 0);
	QString help_file_name;
	QDomElement element;
	QString help_base_dir = RKCommonFunctions::getRKWardDataDir () + "pages/";
	QString css_filename = QUrl::fromLocalFile (help_base_dir + "rkward_help.css").toString ();

	// determine help file, and prepare
	if (for_component) {
		component_doc_element = component_xml->openXMLFile (DL_ERROR);
		if (component_doc_element.isNull ()) return false;
		element = component_xml->getChildElement (component_doc_element, "help", DL_ERROR);
		if (!element.isNull ()) {
			help_file_name = component_xml->getStringAttribute (element, "file", QString (), DL_ERROR);
			if (!help_file_name.isEmpty ()) help_file_name = QFileInfo (chandle->getFilename ()).absoluteDir ().filePath (help_file_name);
		}
	} else {
		help_file_name = help_base_dir + url.path () + ".rkh";
	}
	RK_DEBUG (APP, DL_DEBUG, "rendering help page for local file %s", help_file_name.toLatin1().data());

	// open help file
	const RKMessageCatalog *catalog = component_xml->messageCatalog ();
	if (!for_component) catalog = RKMessageCatalog::getCatalog ("rkward__pages", RKCommonFunctions::getRKWardDataDir () + "po/");
	help_xml = new XMLHelper (help_file_name, catalog);
	help_doc_element = help_xml->openXMLFile (DL_ERROR);
	if (help_doc_element.isNull () && (!for_component)) return false;

	// initialize output, and set title
	QString page_title (i18n ("No Title"));
	if (for_component) {
		page_title = chandle->getLabel ();
	} else {
		element = help_xml->getChildElement (help_doc_element, "title", DL_WARNING);
		page_title = help_xml->i18nElementText (element, false, DL_WARNING);
	}
	writeHTML ("<html><head><title>" + page_title + "</title><link rel=\"stylesheet\" type=\"text/css\" href=\"" + css_filename + "\">"
	           "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></head>\n<body><div id=\"main\">\n<h1>" + page_title + "</h1>\n");

	if (help_doc_element.isNull ()) {
		RK_ASSERT (for_component);
		writeHTML (i18n ("<h1>Help page missing</h1>\n<p>The help page for this component has not yet been written (or is broken). Please consider contributing it.</p>\n"));
	}
	if (for_component) {
		QString component_id = componentPathToId (url.path());
		RKComponentHandle *handle = componentPathToHandle (url.path());
		if (handle && handle->isAccessible ()) writeHTML ("<a href=\"rkward://runplugin/" + component_id + "/\">" + i18n ("Use %1 now", page_title) + "</a>");
	}

	// fix all elements containing an "src" attribute
	QDir base_path (QFileInfo (help_file_name).absolutePath());
	XMLChildList src_elements = help_xml->findElementsWithAttribute (help_doc_element, "src", QString (), true, DL_DEBUG);
	for (XMLChildList::iterator it = src_elements.begin (); it != src_elements.end (); ++it) {
		QString src = (*it).attribute ("src");
		if (KUrl::isRelativeUrl (src)) {
			src = "file://" + QDir::cleanPath (base_path.filePath (src));
			(*it).setAttribute ("src", src);
		}
	}

	// render the sections
	element = help_xml->getChildElement (help_doc_element, "summary", DL_INFO);
	if (!element.isNull ()) {
		writeHTML (startSection ("summary", i18n ("Summary"), QString (), &anchors, &anchornames));
		writeHTML (renderHelpFragment (element));
	}

	element = help_xml->getChildElement (help_doc_element, "usage", DL_INFO);
	if (!element.isNull ()) {
		writeHTML (startSection ("usage", i18n ("Usage"), QString (), &anchors, &anchornames));
		writeHTML (renderHelpFragment (element));
	}

	XMLChildList section_elements = help_xml->getChildElements (help_doc_element, "section", DL_INFO);
	for (XMLChildList::iterator it = section_elements.begin (); it != section_elements.end (); ++it) {
		QString title = help_xml->i18nStringAttribute (*it, "title", QString (), DL_WARNING);
		QString shorttitle = help_xml->i18nStringAttribute (*it, "shorttitle", QString (), DL_DEBUG);
		QString id = help_xml->getStringAttribute (*it, "id", QString (), DL_WARNING);
		writeHTML (startSection (id, title, shorttitle, &anchors, &anchornames));
		writeHTML (renderHelpFragment (*it));
	}

	// the section "settings" is the most complicated, as the labels of the individual GUI items has to be fetched from the component description. Of course it is only meaningful for component help, and not rendered for top level help pages.
	if (for_component) {
		element = help_xml->getChildElement (help_doc_element, "settings", DL_INFO);
		if (!element.isNull ()) {
			writeHTML (startSection ("settings", i18n ("GUI settings"), QString (), &anchors, &anchornames));
			XMLChildList setting_elements = help_xml->getChildElements (element, QString (), DL_WARNING);
			for (XMLChildList::iterator it = setting_elements.begin (); it != setting_elements.end (); ++it) {
				if ((*it).tagName () == "setting") {
					QString id = help_xml->getStringAttribute (*it, "id", QString (), DL_WARNING);
					QString title = help_xml->i18nStringAttribute (*it, "title", QString (), DL_INFO);
					if (title.isEmpty ()) title = resolveLabel (id);
					writeHTML ("<h4>" + title + "</h4>");
					writeHTML (renderHelpFragment (*it));
				} else if ((*it).tagName () == "caption") {
					QString id = help_xml->getStringAttribute (*it, "id", QString (), DL_WARNING);
					QString title = help_xml->i18nStringAttribute (*it, "title", QString (), DL_INFO);
					if (title.isEmpty ()) title = resolveLabel (id);
					writeHTML ("<h3>" + title + "</h3>");
				} else {
					help_xml->displayError (&(*it), "Tag not allowed, here", DL_WARNING);
				}
			}
		}
	}

	// "related" section
	element = help_xml->getChildElement (help_doc_element, "related", DL_INFO);
	if (!element.isNull ()) {
		writeHTML (startSection ("related", i18n ("Related functions and pages"), QString (), &anchors, &anchornames));
		writeHTML (renderHelpFragment (element));
	}

	// "technical" section
	element = help_xml->getChildElement (help_doc_element, "technical", DL_INFO);
	if (!element.isNull ()) {
		writeHTML (startSection ("technical", i18n ("Technical details"), QString (), &anchors, &anchornames));
		writeHTML (renderHelpFragment (element));
	}

	if (for_component) {
		// "dependencies" section
		QList<RKComponentDependency> deps = chandle->getDependencies ();
		if (!deps.isEmpty ()) {
			writeHTML (startSection ("dependencies", i18n ("Dependencies"), QString (), &anchors, &anchornames));
			writeHTML (RKComponentDependency::depsToHtml (deps));
		}
	}

	// "about" section
	RKComponentAboutData about;
	if (for_component) {
		about = chandle->getAboutData ();
	} else {
		about = RKComponentAboutData (help_xml->getChildElement (help_doc_element, "about", DL_INFO), *help_xml);
	}
	if (about.valid) {
		writeHTML (startSection ("about", i18n ("About"), QString (), &anchors, &anchornames));
		writeHTML (about.toHtml ());
	}

	// create a navigation bar
	KUrl url_copy = url;
	QString navigation = i18n ("<h1>On this page:</h1>");
	RK_ASSERT (anchornames.size () == anchors.size ());
	for (int i = 0; i < anchors.size (); ++i) {
		QString anchor = anchors[i];
		QString anchorname = anchornames[i];
		if (!(anchor.isEmpty () || anchorname.isEmpty ())) {
			url_copy.setRef (anchor);
			navigation.append ("<p><a href=\"" + url_copy.url () + "\">" + anchorname + "</a></p>\n");
		}
	}
	writeHTML ("</div><div id=\"navigation\">" + navigation + "</div>");
	writeHTML ("</body></html>\n");

	return (true);
}

QString RKHelpRenderer::resolveLabel (const QString& id) const {
	RK_TRACE (APP);

	QDomElement source_element = component_xml->findElementWithAttribute (component_doc_element, "id", id, true, DL_WARNING);
	if (source_element.isNull ()) {
		RK_DEBUG (PLUGIN, DL_ERROR, "No such UI element: %s", qPrintable (id));
	}
	return (component_xml->i18nStringAttribute (source_element, "label", i18n ("Unnamed GUI element"), DL_WARNING));
}

QString RKHelpRenderer::renderHelpFragment (QDomElement &fragment) {
	RK_TRACE (APP);

	QString text = help_xml->i18nElementText (fragment, true, DL_WARNING);

	// Can't resolve links and references based on the already parsed dom-tree, because they can be inside string to be translated.
	// I.e. resolving links before doing i18n will cause i18n-lookup to fail
	int pos = 0;
	int npos;
	QString ret;
	while ((npos = text.indexOf ("<link", pos)) >= 0) {
		ret += text.mid (pos, npos - pos);

		QString href;
		int href_start = text.indexOf (" href=\"", npos + 5);
		if (href_start >= 0) {
			href_start += 7;
			int href_end = text.indexOf ("\"", href_start);
			href = text.mid (href_start, href_end - href_start);
		}
		QString linktext;
		int end = text.indexOf (">", npos) + 1;
		if (text[end-2] != QChar ('/')) {
			int nend = text.indexOf ("</link>", end);
			linktext = text.mid (end, nend - end);
			end = nend + 7;
		}

		ret += prepareHelpLink (href, linktext);
		pos = end;
	}
	ret += text.mid (pos);

	if (component_xml) {
		text = ret;
		ret.clear ();
		pos = 0;
		while ((npos = text.indexOf ("<label ", pos)) >= 0) {
			ret += text.mid (pos, npos - pos);

			QString id;
			int id_start = text.indexOf ("id=\"", npos + 6);
			if (id_start >= 0) {
				id_start += 4;
				int id_end = text.indexOf ("\"", id_start);
				id = text.mid (id_start, id_end - id_start);
				pos = text.indexOf ("/>", id_end) + 2;
			}
			ret += resolveLabel (id);
		}
		ret += text.mid (pos);
	}

	RK_DEBUG (APP, DL_DEBUG, "%s", qPrintable (ret));
	return ret;
}

QString RKHelpRenderer::prepareHelpLink (const QString &href, const QString &text) {
	RK_TRACE (APP);

	QString ret = "<a href=\"" + href + "\">";
	if (!text.isEmpty ()) {
		ret += text;
	} else {
		QString ltext;
		KUrl url (href);
		if (url.protocol () == "rkward") {
			if (url.host () == "component") {
				RKComponentHandle *chandle = componentPathToHandle (url.path ());
				if (chandle) ltext = chandle->getLabel ();
			} else if (url.host () == "rhelp") {
				ltext = i18n ("R Reference on '%1'", url.path ().mid (1));
			} else if (url.host () == "page") {
				QString help_base_dir = RKCommonFunctions::getRKWardDataDir () + "pages/";
		
				XMLHelper xml (help_base_dir + url.path () + ".rkh", RKMessageCatalog::getCatalog ("rkward__pages", RKCommonFunctions::getRKWardDataDir () + "po/"));
				QDomElement doc_element = xml.openXMLFile (DL_WARNING);
				QDomElement title_element = xml.getChildElement (doc_element, "title", DL_WARNING);
				ltext = xml.i18nElementText (title_element, false, DL_WARNING);
			}

			if (ltext.isEmpty ()) {
				ltext = i18n ("BROKEN REFERENCE");
				RK_DEBUG (APP, DL_WARNING, "Broken reference to %s", url.path ().toLatin1 ().data ());
			}
		}
		ret.append (ltext);
	}
	return (ret + "</a>");
}

QString RKHelpRenderer::componentPathToId (QString path) {
	RK_TRACE (APP);

	QStringList path_segments = path.split ('/', QString::SkipEmptyParts);
	if (path_segments.count () > 2) return 0;
	if (path_segments.count () < 1) return 0;
	if (path_segments.count () == 1) path_segments.push_front ("rkward");
	RK_ASSERT (path_segments.count () == 2);

	return (path_segments.join ("::"));
}

RKComponentHandle *RKHelpRenderer::componentPathToHandle (QString path) {
	RK_TRACE (APP);

	return (RKComponentMap::getComponentHandle (componentPathToId (path)));
}

QString RKHelpRenderer::startSection (const QString &name, const QString &title, const QString &shorttitle, QStringList *anchors, QStringList *anchor_names) {
	QString ret = "<a name=\"" + name + "\">";
	ret.append ("<h2>" + title + "</h2>\n");
	anchors->append (name);
	if (!shorttitle.isNull ()) anchor_names->append (shorttitle);
	else anchor_names->append (title);
	return (ret);
}

void RKHelpRenderer::writeHTML (const QString& string) {
	RK_TRACE (APP);

	device->write (string.toUtf8 ());
}

/////////////////////////////////////
/////////////////////////////////////


// static
RKOutputWindowManager* RKOutputWindowManager::_self = 0;

RKOutputWindowManager* RKOutputWindowManager::self () {
	if (!_self) {
		RK_TRACE (APP);

		_self = new RKOutputWindowManager ();
	}
	return _self;
}

RKOutputWindowManager::RKOutputWindowManager () : QObject () {
	RK_TRACE (APP);

	file_watcher = new KDirWatch (this);
	connect (file_watcher, SIGNAL (dirty(QString)), this, SLOT (fileChanged(QString)));
	connect (file_watcher, SIGNAL (created(QString)), this, SLOT (fileChanged(QString)));
}

RKOutputWindowManager::~RKOutputWindowManager () {
	RK_TRACE (APP);

	file_watcher->removeFile (current_default_path);
	delete (file_watcher);
}

void RKOutputWindowManager::registerWindow (RKHTMLWindow *window) {
	RK_TRACE (APP);

	RK_ASSERT (window->mode () == RKHTMLWindow::HTMLOutputWindow);
	KUrl url = window->url ();

	if (!url.isLocalFile ()) {
		RK_ASSERT (false);		// should not happen right now, but might be an ok condition in the future. We can't monitor non-local files, though.
		return;
	}

	url.cleanPath ();
	QString file = url.toLocalFile ();
	if (!windows.contains (file, window)) {
		if (!windows.contains (file)) {
			if (file != current_default_path) file_watcher->addFile (file);
		}
	
		windows.insertMulti (file, window);
		connect (window, SIGNAL (destroyed(QObject*)), this, SLOT (windowDestroyed(QObject*)));
	} else {
		RK_ASSERT (false);
	}
}

void RKOutputWindowManager::setCurrentOutputPath (const QString &_path) {
	RK_TRACE (APP);

	KUrl url = KUrl::fromLocalFile (_path);
	url.cleanPath ();
	QString path = url.toLocalFile ();

#ifdef Q_OS_WIN
	// On windows, when flushing the output (i.e. deleting, re-creating it), KDirWatch seems to purge the file from the
	// watch list (KDE 4.10.2; always?), so we need to re-add it. To make things complex, however, this may happen
	// asynchronously, with this function called (via rk.set.output.html.file()), _before_ KDirWatch purges the file.
	// To hack around the race condition, we re-watch the output file after a short delay.
	QTimer::singleShot (100, this, SLOT (rewatchOutput ()));
#endif
	if (path == current_default_path) return;

	if (!windows.contains (path)) {
		file_watcher->addFile (path);
	}
	if (!windows.contains (current_default_path)) {
		file_watcher->removeFile (current_default_path);
	}

	current_default_path = path;
}

void RKOutputWindowManager::rewatchOutput () {
	RK_TRACE (APP);
	// called on Windows, only
	file_watcher->removeFile (current_default_path);
	file_watcher->addFile (current_default_path);
}

RKHTMLWindow* RKOutputWindowManager::getCurrentOutputWindow () {
	RK_TRACE (APP);

	RKHTMLWindow *current_output = windows.value (current_default_path);

	if (!current_output) {
		current_output = new RKHTMLWindow (RKWorkplace::mainWorkplace ()->view (), RKHTMLWindow::HTMLOutputWindow);

		current_output->openURL (KUrl::fromLocalFile (current_default_path));

		RK_ASSERT (current_output->url ().toLocalFile () == current_default_path);
	}

	return current_output;
}

void RKOutputWindowManager::fileChanged (const QString &path) {
	RK_TRACE (APP);

	RKHTMLWindow *w = 0;
	QList<RKHTMLWindow *> window_list = windows.values (path);
	for (int i = 0; i < window_list.size (); ++i) {
		window_list[i]->refresh ();
		w = window_list[i];
	}
	RK_DEBUG (APP, DL_DEBUG, "changed %s, window %p", qPrintable (path), w);

	if (w) {
		if (RKSettingsModuleOutput::autoRaise ()) w->activate ();
	} else {
		RK_ASSERT (path == current_default_path);
		if (RKSettingsModuleOutput::autoShow ()) RKWorkplace::mainWorkplace ()->openOutputWindow (path);
	}
}

void RKOutputWindowManager::windowDestroyed (QObject *window) {
	RK_TRACE (APP);

	// warning: Do not call any methods on the window. It is half-destroyed, already.
	RKHTMLWindow *w = static_cast<RKHTMLWindow*> (window);

	QString path = windows.key (w);
	windows.remove (path, w);

	// if there are no further windows for this file, stop listening
	if ((path != current_default_path) && (!windows.contains (path))) {
		file_watcher->removeFile (path);
	}
}

#include "rkhtmlwindow.moc"
