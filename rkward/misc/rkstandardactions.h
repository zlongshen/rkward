/***************************************************************************
                          rkstandardactions  -  description
                             -------------------
    begin                : Sun Nov 18 2007
    copyright            : (C) 2007-2016 by Thomas Friedrichsmeier
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

#ifndef RKSTANDARDACTIONS_H
#define RKSTANDARDACTIONS_H

class KAction;
class QString;
class QObject;
class RKMDIWindow;
class RKScriptContextProvider;

/** This namespace provides functions to generate some standard actions, i.e. actions which are needed at more than one place.

@author Thomas Friedrichsmeier */
namespace RKStandardActions {
	KAction *copyLinesToOutput (RKMDIWindow *window, const QObject *receiver=0, const char *member=0);
/** Allows special pasting modes for script windows.
@param member needs to have the signature void fun (const QString&). */
	KAction* pasteSpecial (RKMDIWindow *window, const QObject *receiver=0, const char *member=0);

	KAction* runCurrent (RKMDIWindow *window, const QObject *receiver=0, const char *member=0, bool current_or_line=false);
	KAction* runAll (RKMDIWindow *window, const QObject *receiver=0, const char *member=0);

	KAction* functionHelp (RKMDIWindow *window, RKScriptContextProvider *context_provider);
/** Search for current symbol / selection, online. Note that you will not have to connect this action to any slot to work. It does everything by itself.
 *  It will query the given context_provider for context. */
	KAction* onlineHelp (RKMDIWindow *window, RKScriptContextProvider *context_provider);
};

#endif
