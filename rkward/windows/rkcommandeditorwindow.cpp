/***************************************************************************
                          rkcommandeditorwindow  -  description
                             -------------------
    begin                : Mon Aug 30 2004
    copyright            : (C) 2004 by Thomas Friedrichsmeier
    email                : tfry@users.sourceforge.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "rkcommandeditorwindow.h"

#include <kparts/partmanager.h>
#include <kparts/mainwindow.h>
#include <kparts/part.h>
#include <kparts/factory.h>

#include <ktexteditor/configinterface.h>
#include <ktexteditor/sessionconfiginterface.h>
#include <ktexteditor/viewcursorinterface.h>
#include <ktexteditor/printinterface.h>
#include <ktexteditor/encodinginterface.h>
#include <ktexteditor/editorchooser.h>
#include <ktexteditor/popupmenuinterface.h>

#include <qlayout.h>
#include <qpopupmenu.h>
#include <qapplication.h>
#include <qtabwidget.h>
#include <qfile.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <klocale.h>
#include <kmenubar.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kaccel.h>
#include <klibloader.h>

#include "../rbackend/rinterface.h"
#include "../rkeditormanager.h"
#include "../rkglobals.h"
#include "../rkward.h"

#include "../debug.h"

#define GET_HELP_URL 1

RKCommandEditorWindow::RKCommandEditorWindow (QWidget *parent, bool use_r_highlighting) : KMdiChildView (parent) {
	RK_TRACE (COMMANDEDITOR);
    KParts::ReadWritePart *m_katepart;

	KLibFactory *factory = KLibLoader::self()->factory( "libkatepart" );
	if (factory) {
		// Create the part
		m_katepart = (KParts::ReadWritePart *)factory->create( this,
			"katepart", "KParts::ReadWritePart" );
	}
	(RKGlobals::rkApp()->m_manager)->addPart((KParts::Part*)m_katepart,false);

	m_doc = (Kate::Document *) m_katepart;
	m_view = (Kate::View *) m_katepart->widget();

	QHBoxLayout *pLayout = new QHBoxLayout( this, 0, -1, "layout");
	pLayout->addWidget(m_view);

	m_view->setName("Kate Part View");
	if (use_r_highlighting) setRHighlighting(m_doc);
}

RKCommandEditorWindow::~RKCommandEditorWindow () {
	RK_TRACE (COMMANDEDITOR);
	delete m_doc;
}


void RKCommandEditorWindow::setRHighlighting (Kate::Document *doc) {
	// set syntax-highlighting for R
	int modes_count = highlightingInterface (doc)->hlModeCount ();
	bool found_mode = false;
	int i;
	for (i = 0; i < modes_count; ++i) {
		if (highlightingInterface (doc)->hlModeName (i).lower() == "r script") {
			found_mode = true;
			break;
		}
	}
	if (found_mode) {
		highlightingInterface (doc)->setHlMode (i);
	} else {
		RK_DO (qDebug ("No syntax highlighting definition found for r script."), COMMANDEDITOR, DL_WARNING);
	}
}






QString RKCommandEditorWindow::getSelection()
{
    	RK_TRACE (COMMANDEDITOR);
	return selectionInterface(m_doc)->selection();
}

QString RKCommandEditorWindow::getLine()
{
    	RK_TRACE (COMMANDEDITOR);
	return m_view->currentTextLine();
}

QString RKCommandEditorWindow::getText()
{
	return editInterface(m_doc)->text();
}


bool RKCommandEditorWindow::openURL (const KURL &url, bool use_r_highlighting, bool read_only){
	if (m_doc->openURL (url)){
		if (use_r_highlighting) setRHighlighting (m_doc);
		m_doc->setReadWrite (!read_only);

		updateTabCaption (url);

		return true;
	}
	return false;
}

bool RKCommandEditorWindow::getFilenameAndPath(const KURL &url,QString *fname){
	QString fullpath = url.path();
	int i,length,fnamepos;
	bool done;
	
	if ((length = (int)fullpath.length()) == 0)
		return false;

	fnamepos = 0;
	for (i = length-1,done = false ; i >= 0 && !done ; i--){
		if (fullpath[i] == '/'){
			done = true;
			fnamepos = i+1;
		}
	}

	if (!done)
		return false;
	
	if (fnamepos >= length)
		return false;

	if (fname)
		*fname = fullpath.right(length-fnamepos);

	return true;
}





KURL RKCommandEditorWindow::url(){
    return m_doc->url();
}



bool RKCommandEditorWindow::save(){
    return m_doc->save();
}

bool RKCommandEditorWindow::saveAs(const KURL &url){
	bool result = m_doc->saveAs(url);
	updateTabCaption(url); 
	return result;
}



bool RKCommandEditorWindow::isModified() {
    return m_doc->isModified();
}


void RKCommandEditorWindow::insertText(QString text)
{
	m_doc->insertText(m_view->cursorLine(),m_view->cursorColumn(),text);
	setFocus();
}



void RKCommandEditorWindow::updateTabCaption(const KURL &url)
{
	QString fname;
	if (getFilenameAndPath(url,&fname)){
		setTabCaption(fname);
	}
	else {
		setTabCaption(url.prettyURL());
	}
}


void RKCommandEditorWindow::showHelp()
{
	// I know, there must be simpler way to do it...
	// What we do here is that we look for the word under the cursor.
	

	uint para=0; uint p=0;
	m_view->cursorPosition (&para, &p);

	QString line=m_view->currentTextLine() + " ";
	if(line.isEmpty() || line.isNull())
		return;
	
	
	// We want to match any valid R name, that is, everything composed of letters, 0-9, '.'s and '_'s..
	QRegExp rx( "[^A-Za-z0-9'.''_']" );
	QRegExp rx2( "[A-Za-z0-9'.''_']" );
	
	QStringList list=QStringList::split(rx,line);
	QStringList list2=QStringList::split(rx2,line);

	QStringList::Iterator it = list.begin();
	QStringList::Iterator it2 = list2.begin();
	uint pos=0;
	QString result="";
	
	while( it != list.end() &&  it2 != list2.end() ) {
		if (pos<=p) result=*it;
		
		pos=pos+(*it).length();
		pos=pos+(*it2).length();
		
		++it;
		++it2;
	}

	
	if(result=="" && !line.isEmpty()){
		//There is only one word on this line
		result=line;
	}
		
	chain=0;
	QString s="help(\"";
	s.append(result);
	s.append("\", htmlhelp=TRUE)[1]");
	
	RKGlobals::rInterface ()->issueCommand (s, RCommand::App | RCommand::Sync | RCommand::GetStringVector, "", this, GET_HELP_URL, chain);
}

void RKCommandEditorWindow::rCommandDone (RCommand *command) {
	KURL url;
	
	if (command->getFlags () == GET_HELP_URL) {
		url.setPath(command->getStringVector ()[0]);
		if (QFile::exists( url.path() )) {
			RKGlobals::rkApp()->openHTML(url);
			return;
		}
	} else {
		RK_ASSERT (false);
	}
}

#include "rkcommandeditorwindow.moc"
