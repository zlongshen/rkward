/***************************************************************************
                          rksettingsmodule  -  description
                             -------------------
    begin                : Fri Apr 22 2005
    copyright            : (C) 2005, 2015 by Thomas Friedrichsmeier
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

#ifndef RKSETTINGSMODULEOBJECTBROWSER_H
#define RKSETTINGSMODULEOBJECTBROWSER_H

#include "rksettingsmodule.h"

#include "../misc/rkobjectlistview.h"

class QCheckBox;
class MultiStringSelector;

/** Configuration module for instances of RObjectListView
@see RKSettingsModule
@author Thomas Friedrichsmeier
*/
class RKSettingsModuleObjectBrowser : public RKSettingsModule {
	Q_OBJECT
public:
	RKSettingsModuleObjectBrowser (RKSettings *gui, QWidget *parent);

	~RKSettingsModuleObjectBrowser ();

/** applies current settings in this RKSettingsModule. This will only be called, if hasChanges () is true */
	void applyChanges () override;
/** saves current changes to the given KConfig
@param config probably always RKGlobals::rkApp ()->config. But passing this as an argument is both more flexible and saves #including files.*/
	void save (KConfig *config) override;

/** @returns the caption ("Workspace Browser") */
	QString caption () override;

	static void saveSettings (KConfig *config);
	static void loadSettings (KConfig *config);

	static bool isDefaultForWorkspace (RKObjectListViewSettings::PersistentSettings setting) { return workspace_settings[setting]; };
	static bool isDefaultForVarselector (RKObjectListViewSettings::PersistentSettings setting) { return varselector_settings[setting]; };
	static void setDefaultForWorkspace (RKObjectListViewSettings::PersistentSettings setting, bool state);
	static void setDefaultForVarselector (RKObjectListViewSettings::PersistentSettings setting, bool state);

	static bool isPackageBlacklisted (const QString &package_name);

	QString helpURL () override { return ("rkward://page/rkward_workspace_browser#settings"); };
public slots:
/** called when a checkbox has been changed. Signals change to RKSettings dialog to enable apply button */
	void boxChanged (int);
	void listChanged ();
	void addBlackList (QStringList *string_list);
private:
	MultiStringSelector *blacklist_choser;
	static QStringList getstructure_blacklist;

	static bool workspace_settings[RKObjectListViewSettings::SettingsCount];
	static bool varselector_settings[RKObjectListViewSettings::SettingsCount];
};

#endif
