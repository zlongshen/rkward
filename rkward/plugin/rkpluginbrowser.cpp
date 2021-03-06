/***************************************************************************
                          rkpluginbrowser  -  description
                             -------------------
    begin                : Sat Mar 10 2005
    copyright            : (C) 2005, 2006, 2007, 2009, 2010, 2012, 2014 by Thomas Friedrichsmeier
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

#include "rkpluginbrowser.h"

#include <QVBoxLayout>

#include <klocale.h>

#include "../misc/xmlhelper.h"
#include "../misc/getfilenamewidget.h"
#include "../rkglobals.h"
#include "../debug.h"

RKPluginBrowser::RKPluginBrowser (const QDomElement &element, RKComponent *parent_component, QWidget *parent_widget) : RKComponent (parent_component, parent_widget) {
	RK_TRACE (PLUGIN);

	// get xml-helper
	XMLHelper *xml = parent_component->xmlHelper ();

	// create and add property
	addChild ("selection", selection = new RKComponentPropertyBase (this, true));
	connect (selection, SIGNAL (valueChanged(RKComponentPropertyBase*)), this, SLOT (textChanged(RKComponentPropertyBase*)));

	setRequired (xml->getBoolAttribute (element, "required", true, DL_INFO));
	connect (requirednessProperty (), SIGNAL (valueChanged(RKComponentPropertyBase*)), this, SLOT (requirednessChanged(RKComponentPropertyBase*)));

	QVBoxLayout *vbox = new QVBoxLayout (this);
	vbox->setContentsMargins (0, 0, 0, 0);

	int intmode = xml->getMultiChoiceAttribute (element, "type", "file;dir;savefile", 0, DL_INFO);
	GetFileNameWidget::FileType mode;
	if (intmode == 0) mode = GetFileNameWidget::ExistingFile;
	else if (intmode == 1) mode = GetFileNameWidget::ExistingDirectory;
	else mode = GetFileNameWidget::SaveFile;

	bool only_local = !xml->getBoolAttribute (element, "allow_urls", false, DL_INFO);

	label_string = xml->i18nStringAttribute (element, "label", i18n ("Enter filename"), DL_INFO);
	selector = new GetFileNameWidget (this, mode, only_local, label_string, i18n ("Select"), xml->getStringAttribute (element, "initial", QString (), DL_INFO));
	QString filter = xml->getStringAttribute (element, "filter", QString (), DL_INFO);
	if (!filter.isEmpty ()) {
		filter.append ("\n*|All files");
		selector->setFilter (filter);
	}
	connect (selector, SIGNAL (locationChanged()), SLOT (textChanged()));

	vbox->addWidget (selector);

	// initialize
	updating = false;
	textChanged ();
}

RKPluginBrowser::~RKPluginBrowser () {
	RK_TRACE (PLUGIN);
}

void RKPluginBrowser::textChanged (RKComponentPropertyBase *) {
	RK_TRACE (PLUGIN);

	if (updating) return;
	updating = true;

	selector->setLocation (fetchStringValue (selection));
	updateColor ();

	updating = false;
	changed ();
}

void RKPluginBrowser::textChanged () {
	RK_TRACE (PLUGIN);

	selection->setValue (selector->getLocation ());
}

bool RKPluginBrowser::isValid () {
	return (!(fetchStringValue (selection).isEmpty ()));
}

void RKPluginBrowser::requirednessChanged (RKComponentPropertyBase *) {
	RK_TRACE (PLUGIN);

	updateColor ();
}

void RKPluginBrowser::updateColor () {
	RK_TRACE (PLUGIN);

	if (isEnabled ()) {
		if (isSatisfied ()) {
			selector->setBackgroundColor (QColor (255, 255, 255));
		} else {
			selector->setBackgroundColor (QColor (255, 0, 0));
		}
	} else {
		selector->setBackgroundColor (QColor (200, 200, 200));
	}
}

QStringList RKPluginBrowser::getUiLabelPair () const {
	RK_TRACE (PLUGIN);

	QStringList ret (label_string);
	ret.append (selection->value ().toString ());
	return ret;
}

#include "rkpluginbrowser.moc"
