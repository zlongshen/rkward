/***************************************************************************
                          multistringselector  -  description
                             -------------------
    begin                : Fri Sep 10 2005
    copyright            : (C) 2005 by Thomas Friedrichsmeier
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

#include "multistringselector.h"

#include <q3listview.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qlabel.h>
//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3VBoxLayout>

#include <klocale.h>

#include "../rkglobals.h"
#include "../debug.h"

MultiStringSelector::MultiStringSelector (const QString& label, QWidget* parent) : QWidget (parent) {
	RK_TRACE (MISC);

	Q3HBoxLayout *hbox = new Q3HBoxLayout (this, RKGlobals::spacingHint ());

	Q3VBoxLayout *main_box = new Q3VBoxLayout (hbox, RKGlobals::spacingHint ());
	Q3VBoxLayout *button_box = new Q3VBoxLayout (hbox, RKGlobals::spacingHint ());

	QLabel *label_widget = new QLabel (label, this);
	main_box->addWidget (label_widget);

	list_view = new Q3ListView (this);
	list_view->addColumn (i18n ("Filename"));
	list_view->setSelectionMode (Q3ListView::Single);
	list_view->setSorting (-1);
	connect (list_view, SIGNAL (selectionChanged ()), this, SLOT (listSelectionChanged ()));
	main_box->addWidget (list_view);

	add_button = new QPushButton (i18n ("Add"), this);
	connect (add_button, SIGNAL (clicked ()), this, SLOT (addButtonClicked ()));
	button_box->addWidget (add_button);

	remove_button = new QPushButton (i18n ("Remove"), this);
	connect (remove_button, SIGNAL (clicked ()), this, SLOT (removeButtonClicked ()));
	button_box->addWidget (remove_button);

	button_box->addSpacing (10);

	up_button = new QPushButton (i18n ("Up"), this);
	connect (up_button, SIGNAL (clicked ()), this, SLOT (upButtonClicked ()));
	button_box->addWidget (up_button);

	down_button = new QPushButton (i18n ("Down"), this);
	connect (down_button, SIGNAL (clicked ()), this, SLOT (downButtonClicked ()));
	button_box->addWidget (down_button);

	listSelectionChanged ();		// causes remove, up, and down buttons to be disabled (since no item is selected)
}

MultiStringSelector::~MultiStringSelector () {
	RK_TRACE (MISC);

	// child widgets and listviewitems deleted by qt
}

QStringList MultiStringSelector::getValues () {
	RK_TRACE (MISC);

	QStringList list;
	Q3ListViewItem *item = list_view->firstChild ();
	
	while (item) {
		list.append (item->text (0));
		item = item->nextSibling ();
	}

	return list;
}

void MultiStringSelector::setValues (const QStringList& values) {
	RK_TRACE (MISC);

	list_view->clear ();
	for (QStringList::const_iterator it = values.begin (); it != values.end (); ++it) {
		Q3ListViewItem *item = new Q3ListViewItem (list_view, list_view->lastItem ());
		item->setText (0, (*it));
	}
	listSelectionChanged ();
	emit (listChanged ());
}

void MultiStringSelector::addButtonClicked () {
	RK_TRACE (MISC);

	QStringList new_strings;
	emit (getNewStrings (&new_strings));
	for (QStringList::const_iterator it = new_strings.begin (); it != new_strings.end (); ++it) {
		Q3ListViewItem *item = new Q3ListViewItem (list_view, list_view->lastItem ());
		item->setText (0, (*it));
	}
	emit (listChanged ());
	listSelectionChanged ();		// update button states
}

void MultiStringSelector::removeButtonClicked () {
	RK_TRACE (MISC);

	delete (list_view->selectedItem ());
	emit (listChanged ());
}

void MultiStringSelector::upButtonClicked () {
	RK_TRACE (MISC);

	Q3ListViewItem *sel = list_view->selectedItem ();
	RK_ASSERT (sel);

	Q3ListViewItem *above = sel->itemAbove ();
	if (above) {
		above->moveItem (sel);
	}
	emit (listChanged ());
}

void MultiStringSelector::downButtonClicked () {
	RK_TRACE (MISC);

	Q3ListViewItem *sel = list_view->selectedItem ();
	RK_ASSERT (sel);

	if (sel->nextSibling ()) sel->moveItem (sel->nextSibling ());
	emit (listChanged ());
}

void MultiStringSelector::listSelectionChanged () {
	RK_TRACE (MISC);

	if (list_view->selectedItem ()) {
		remove_button->setEnabled (true);
		up_button->setEnabled (true);
		down_button->setEnabled (true);
	} else {
		remove_button->setEnabled (false);
		up_button->setEnabled (false);
		down_button->setEnabled (false);
	}
}

#include "multistringselector.moc"
