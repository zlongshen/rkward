/***************************************************************************
                          rksettingsmoduledebug -  description
                             -------------------
    begin                : Tue Oct 23 2007
    copyright            : (C) 2007, 2009 by Thomas Friedrichsmeier
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
#ifndef RKSETTINGSMODULEDEBUG_H
#define RKSETTINGSMODULEDEBUG_H

#include "rksettingsmodule.h"

class RKSpinBox;
class QButtonGroup;
class QTemporaryFile;

/**
configuration for the Command Editor windows

@author Thomas Friedrichsmeier
*/
class RKSettingsModuleDebug : public RKSettingsModule {
	Q_OBJECT
public:
	RKSettingsModuleDebug (RKSettings *gui, QWidget *parent);

	~RKSettingsModuleDebug ();
	
	void applyChanges () override;
	void save (KConfig *config) override;
	
	static void saveSettings (KConfig *config);
	static void loadSettings (KConfig *config);
	
	QString caption () override;

	// static members are declared in debug.h and defined in main.cpp
public slots:
	void settingChanged (int);
public:
	// public for internal reason, only! Do not mess with this!
	static QTemporaryFile* debug_file;
private:
	RKSpinBox* command_timeout_box;
	RKSpinBox* debug_level_box;
	QButtonGroup* debug_flags_group;
};

#endif
