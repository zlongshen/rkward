/***************************************************************************
                          rkimportdialog  -  description
                             -------------------
    begin                : Tue Jan 30 2007
    copyright            : (C) 2007 by Thomas Friedrichsmeier
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

#include "rkimportdialog.h"

#include <kmessagebox.h>
#include <kfilefiltercombo.h>
#include <klocale.h>

#include <qhbox.h>
#include <qcombobox.h>
#include <qlabel.h>

#include "../plugin/rkcomponentmap.h"
#include "../plugin/rkcomponentcontext.h"

#include "../debug.h"

RKImportDialog::RKImportDialog (const QString &context_id, QWidget *parent) : KFileDialog (QString::null, QString::null, parent, 0, false) {
	RK_TRACE (DIALOGS);

	context = RKComponentMap::getContext (context_id);
	if (!context) {
		KMessageBox::sorry (this, i18n ("No plugins defined for context '%1'").arg (context_id));
		return;
	}

	component_ids = context->components ();
	QString formats = "*|" + i18n ("All Files") + " (*)\n";
	int format_count = 0;
	for (QStringList::const_iterator it = component_ids.constBegin (); it != component_ids.constEnd (); ++it) {
		if (format_count++) formats.append ('\n');

		RKComponentHandle *handle = RKComponentMap::getComponentHandle (*it);
		if (!handle) {
			RK_ASSERT (false);
			continue;
		}

		QString filter = handle->getAttributeValue ("format");
		QString label = handle->getAttributeLabel ("format");
		formats.append (filter + '|' + label + " (" + filter + ')');
		format_labels.append (label);
		filters.append (filter);
	}

	// file format selection box
	format_selection_box = new QHBox ();
	new QLabel (i18n ("File format: "), format_selection_box);
	format_combo = new QComboBox (format_selection_box);
	format_combo->insertStringList (format_labels);

	// initialize
	setMode (KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
	init (QString::null, formats, format_selection_box);
	connect (filterWidget, SIGNAL (filterChanged ()), this, SLOT (filterChanged ()));
	filterChanged ();
	show ();
}

RKImportDialog::~RKImportDialog () {
	RK_TRACE (DIALOGS);
}

void RKImportDialog::filterChanged () {
	RK_TRACE (DIALOGS);

	int index = filters.findIndex (filterWidget->currentFilter ());

	if (index < 0) {		// All files
		format_combo->setEnabled (true);
	} else {
		format_combo->setEnabled (false);
		format_combo->setCurrentItem (index);
	}
}

void RKImportDialog::slotOk () {
	RK_TRACE (DIALOGS);

	KFileDialog::slotOk ();

	int index = format_combo->currentItem ();
	QString cid = component_ids[index];
	RKComponentHandle *handle = RKComponentMap::getComponentHandle (cid);
	RKContextHandler *chandler = context->makeContextHandler (this, false);

	if (!(handle && chandler)) {
		RK_ASSERT (false);
	} else {
		RKComponentPropertyBase *filename = new RKComponentPropertyBase (chandler, false);
		filename->setValue (selectedFile ());
		chandler->addChild ("filename", filename);

		chandler->invokeComponent (handle);
	}

	deleteLater ();
}

void RKImportDialog::slotCancel () {
	RK_TRACE (DIALOGS);

	KFileDialog::slotCancel ();
	deleteLater ();
}

#include "rkimportdialog.moc"
