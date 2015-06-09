/***************************************************************************
                          rksettingsmoduler  -  description
                             -------------------
    begin                : Wed Jul 28 2004
    copyright            : (C) 2004, 2007, 2009, 2010, 2011, 2012 by Thomas Friedrichsmeier
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
#ifndef RKSETTINGSMODULER_H
#define RKSETTINGSMODULER_H

#include "rksettingsmodule.h"

#include <qstring.h>
#include <qstringlist.h>

class QCheckBox;
class QComboBox;
class QLineEdit;
class MultiStringSelector;
class KIntSpinBox;
class QTextEdit;

/**
Configure the R-backend

@author Thomas Friedrichsmeier
*/
class RKSettingsModuleR : public RKSettingsModule {
	Q_OBJECT
public:
	RKSettingsModuleR (RKSettings *gui, QWidget *parent);
	~RKSettingsModuleR ();
	
	void applyChanges ();
	void save (KConfig *config);
	
	static void saveSettings (KConfig *config);
	static void loadSettings (KConfig *config);
	
	QString caption ();
	
/** generate the commands needed to set the R run time options */
	static QStringList makeRRunTimeOptionCommands ();

/** retrieve the (probable) base url of help pages. May change across R sessions */
	static QString helpBaseUrl () { return help_base_url; };

	static int getDefaultWidth () { return options_width; };
public slots:
	void settingChanged ();
private:
	QLineEdit *outdec_input;
	KIntSpinBox *width_input;
	QComboBox *warn_input;
	KIntSpinBox *warningslength_input;
	KIntSpinBox *maxprint_input;
	QComboBox *keepsource_input;
	QComboBox *keepsourcepkgs_input;
	KIntSpinBox *expressions_input;
	KIntSpinBox *digits_input;
	QComboBox *checkbounds_input;
	QComboBox *editor_input;
	QComboBox *pager_input;
	QTextEdit *further_input;
	QCheckBox *internet2_input;

	static QString options_outdec;
	static int options_width;
	static int options_warn;
	static int options_warningslength;
	static int options_maxprint;
	static bool options_keepsource;
	static bool options_keepsourcepkgs;
	static int options_expressions;
	static int options_digits;
	static bool options_checkbounds;
	static QString options_editor;
	static QString options_pager;
	static QString options_further;
	static bool options_internet2;

// constants
	static QString builtin_editor;

// session constants
	friend class RInterface;
	static QString help_base_url;
};

#include "../rbackend/rcommandreceiver.h"

/**
Configure packages and library paths

@author Thomas Friedrichsmeier
*/
class RKSettingsModuleRPackages : public RKSettingsModule, public RCommandReceiver {
	Q_OBJECT
public:
	RKSettingsModuleRPackages (RKSettings *gui, QWidget *parent);
	~RKSettingsModuleRPackages ();
	
	void applyChanges ();
	void save (KConfig *config);
	
	static void saveSettings (KConfig *config);
	static void loadSettings (KConfig *config);

/** generate the commands needed to set the R run time options */
	static QStringList makeRRunTimeOptionCommands ();

	static bool archivePackages () { return archive_packages; }
	static QString pkgTypeOption ();
	static void addLibraryLocation (const QString& new_loc, RCommandChain *chain);
	static QStringList libraryLocations () { return (liblocs + defaultliblocs); };

/** returns the list of packages which are essential to rkward. This is hard-coded. */
	static QStringList essentialPackages () { return essential_packages.split ("\n"); };

	QString caption ();
public slots:
	void settingChanged ();
	void addLibLoc (QStringList *string_list);
	void addRepository (QStringList *string_list);
	void selectCRANMirror ();
protected:
	void rCommandDone (RCommand *command);
private:
	static QString libLocsCommand ();

	MultiStringSelector *libloc_selector;
	QCheckBox *archive_packages_box;
	QCheckBox *source_packages_box;
	MultiStringSelector *repository_selector;
	QLineEdit* cran_mirror_input;

	static QString cran_mirror_url;
	static QStringList liblocs;
	static bool archive_packages;
	static bool source_packages;
	static QStringList package_repositories;

	friend class RInterface;
	static QStringList defaultliblocs;
	static QString essential_packages;
};

#endif
