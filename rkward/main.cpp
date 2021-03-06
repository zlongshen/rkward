/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Tue Oct 29 20:06:08 CET 2002
    copyright            : (C) 2002-2014 by Thomas Friedrichsmeier 
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



/*!
**	\mainpage	RKWard
**	\author		Thomas Friedrichsmeier and the RKWard Team
**
**	\section	website	Website
**
**	<A HREF="http://rkward.kde.org">RKWard's project page</A>
**
**	\section	description Description
**
** RKWard is meant to become an easy to use, transparent frontend to the R-language, a very powerful, yet hard-to-get-into 
** scripting-language with a strong focus on statistic functions. It will not only provide a convenient user-interface, however, but also 
** take care of seamless integration with an office-suite. Practical statistics is not just about calculating, after all, but also about 
** documenting and ultimately publishing the results.
**
** RKWard then is (will be) something like a free replacement for commercial statistical packages.
**
** \section docOverview Getting started with the documentation
**
** The following sections of the API-documentation provide useful entry-points:
** 
** - \ref UsingTheInterfaceToR
** - \ref RKComponentProperties
**
**	\section	copyright	Copyright
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
*/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kstandarddirs.h>

#include <qstring.h>
#include <QMutex>
#include <QTemporaryFile>
#include <QDir>
#include <QThread>

#include <stdio.h>
#include <stdlib.h>

#include "rkward.h"
#include "rkglobals.h"
#include "rkwardapplication.h"
#include "settings/rksettingsmoduledebug.h"
#include "windows/rkdebugmessagewindow.h"

#ifdef Q_WS_WIN
	// these are needed for the exit hack.
#	include <windows.h>
#endif

#include "debug.h"

#include "version.h"

int RK_Debug_Level = 0;
int RK_Debug_Flags = DEBUG_ALL;
int RK_Debug_CommandStep = 0;
QMutex RK_Debug_Mutex;

static KCmdLineOptions options;

void RKDebugMessageOutput (QtMsgType type, const char *msg) {
	RK_Debug_Mutex.lock ();
	if (type == QtFatalMsg) {
		fprintf (stderr, "%s\n", msg);
	}
	RKSettingsModuleDebug::debug_file->write (msg);
	RKSettingsModuleDebug::debug_file->write ("\n");
	RKSettingsModuleDebug::debug_file->flush ();
	RK_Debug_Mutex.unlock ();
}

/** The point of this redirect (to be called via the RK_DEBUG() macro) is to separate RKWard specific debug messages from
 * any other noise, coming from Qt / kdelibs. Also it allows us to retain info on flags and level, and to show messages
 * in a tool window, esp. for debugging plugins. */
void RKDebug (int flags, int level, const char *fmt, ...) {
	const int bufsize = 1024*8;
	char buffer[bufsize];

	va_list ap;
	va_start (ap, fmt);
	vsnprintf (buffer, bufsize-1, fmt, ap);
	va_end (ap);
	RKDebugMessageOutput (QtDebugMsg, buffer);
	if (QApplication::instance ()->thread () == QThread::currentThread ()) {
		// not safe to call from any other than the GUI thread
		RKDebugMessageWindow::newMessage (flags, level, QString (buffer));
	}
}

QString decodeArgument (const QString &input) {
	return (QUrl::fromPercentEncoding (input.toUtf8()));
}

int main(int argc, char *argv[]) {
	options.add ("evaluate <Rcode>", ki18n ("After starting (and after loading the specified workspace, if applicable), evaluate the given R code."), 0);
	options.add ("debug-level <level>", ki18n ("Verbosity of debug messages (0-5)"), "2");
	options.add ("debug-flags <flags>", ki18n ("Mask for components to debug (see debug.h)"), QString::number (DEBUG_ALL).toLocal8Bit ());
	options.add ("debugger <command and arguments>", ki18n ("Debugger for the frontend. Specify last, or add '--' after all debugger arguments"), "");
	options.add ("backend-debugger <command>", ki18n ("Debugger for the backend. (Enclose any debugger arguments in single quotes ('') together with the command. Make sure to re-direct stdout!)"), "");
	options.add ("r-executable <command>", ki18n ("Use specified R installation, instead of the one configured at compile time (note: rkward R library must be installed to that installation of R)"), "");
	options.add ("reuse", ki18n ("Reuse a running RKWard instance (if available). If a running instance is reused, only the file arguments will be interpreted, all other options will be ignored."), 0);
	options.add ("nowarn-external", ki18n ("When used in conjunction with rkward://runplugin/-URLs specified on the command line, suppresses the warning about application-external (untrusted) links."));
	options.add ("+[Files]", ki18n ("File or files to open, typically a workspace, or an R script file. When loading several things, you should specify the workspace, first."), 0);

	KAboutData aboutData("rkward", QByteArray (), ki18n ("RKWard"), RKWARD_VERSION, ki18n ("Frontend to the R statistics language"), KAboutData::License_GPL, ki18n ("(c) 2002, 2004 - 2016"), KLocalizedString (), "http://rkward.kde.org", "submit@bugs.kde.org");
	aboutData.addAuthor (ki18n ("Thomas Friedrichsmeier"), ki18n ("Project leader / main developer"));
	aboutData.addAuthor (ki18n ("Pierre Ecochard"), ki18n ("C++ developer between 2004 and 2007"));
	aboutData.addAuthor (ki18n ("Prasenjit Kapat"), ki18n ("Many plugins, suggestions, plot history feature"));
	aboutData.addAuthor (ki18n ("Meik Michalke"), ki18n ("Many plugins, suggestions, rkwarddev package"));
	aboutData.addAuthor (ki18n ("Stefan Roediger"), ki18n ("Many plugins, suggestions, marketing, translations"));
	aboutData.addCredit (ki18n ("Contributors in alphabetical order"));
	aboutData.addCredit (ki18n ("Björn Balazs"), ki18n ("Extensive usability feedback"));
	aboutData.addCredit (ki18n ("Aaron Batty"), ki18n ("Whealth of feedback, hardware donations"));
	aboutData.addCredit (ki18n ("Jan Dittrich"), ki18n ("Extensive usability feedback"));
	aboutData.addCredit (ki18n ("Philippe Grosjean"), ki18n ("Several helpful comments and discussions"));
	aboutData.addCredit (ki18n ("Adrien d'Hardemare"), ki18n ("Plugins and patches"));
	aboutData.addCredit (ki18n ("Yves Jacolin"), ki18n ("New website"));
	aboutData.addCredit (ki18n ("Germán Márquez Mejía"), ki18n ("HP filter plugin, spanish translation"), 0);
	aboutData.addCredit (ki18n ("Marco Martin"), ki18n ("A cool icon"));
	aboutData.addCredit (ki18n ("Daniele Medri"), ki18n ("RKWard logo, many suggestions, help on wording"));
	aboutData.addCredit (ki18n ("David Sibai"), ki18n ("Several valuable comments, hints and patches"));
	aboutData.addCredit (ki18n ("Ilias Soumpasis"), ki18n ("Translation, Suggestions, plugins"));
	aboutData.addCredit (ki18n ("Ralf Tautenhahn"), ki18n ("Many comments, useful suggestions, and bug reports"));
	aboutData.addCredit (ki18n ("Jannis Vajen"), ki18n ("German Translation, bug reports"));
	aboutData.addCredit (ki18n ("Roland Vollgraf"), ki18n ("Some patches"));
	aboutData.addCredit (ki18n ("Roy Qu"), ki18n ("patches and helpful comments"));
	aboutData.addCredit (ki18n ("Many more people on rkward-devel@kde.org"), ki18n ("Sorry, if we forgot to list you. Please contact us to get added"));

	// before initializing the commandline args, remove the ".bin" from "rkward.bin".
	// This is so it prints "Usage rkward..." instead of "Usage rkward.bin...", etc.
	// it seems safest to keep a copy, since the shell still owns argv
	char *argv_copy[argc];
	argv_copy[0] = qstrdup (QString (argv[0]).remove (".frontend").replace (".exe", ".bat").toLocal8Bit ());
	for (int i = 1; i < argc; ++i) {
		argv_copy[i] = argv[i];
	}
	KCmdLineArgs::init (argc, argv_copy, &aboutData);
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
	RK_Debug_Level = DL_FATAL - QString (args->getOption ("debug-level")).toInt ();
	RK_Debug_Flags = QString (args->getOption ("debug-flags")).toInt ();
	if (!args->getOption ("debugger").isEmpty ()) {
		RK_DEBUG (DEBUG_ALL, DL_ERROR, "--debugger option should have been handled by wrapper script. Ignoring.");
	}

	if (args->count ()) {
		QStringList urls_to_open;
		for (int i = 0; i < args->count (); ++i) {
			urls_to_open.append (KCmdLineArgs::makeURL (decodeArgument (args->arg (i)).toUtf8 ()).url ());
		}
		RKGlobals::startup_options["initial_urls"] = urls_to_open;
		RKGlobals::startup_options["warn_external"] = args->isSet ("warn-external");
	}
	RKGlobals::startup_options["evaluate"] = decodeArgument (args->getOption ("evaluate"));
	RKGlobals::startup_options["backend-debugger"] = decodeArgument (args->getOption ("backend-debugger"));

	RKWardApplication app;
	// No, I do not really understand the point of separating KDE_LANG from LANGUAGE. We do honor it in so far as not
	// forcing LANGUAGE on the backend, though. Having language as LANGUAGE makes code in RKMessageCatalog much easier compared to KCatalog.
	qputenv ("LANGUAGE", QFile::encodeName (KGlobal::locale ()->language ()));
	// install message handler *after* the componentData has been initialized
	RKSettingsModuleDebug::debug_file = new QTemporaryFile (QDir::tempPath () + "/rkward.frontend");
	RKSettingsModuleDebug::debug_file->setAutoRemove (false);
	if (RKSettingsModuleDebug::debug_file->open ()) {
		RK_DEBUG (APP, DL_INFO, "Full debug output is at %s", qPrintable (RKSettingsModuleDebug::debug_file->fileName ()));
		qInstallMsgHandler (RKDebugMessageOutput);
	}

	if (app.isSessionRestored ()) {
		RESTORE(RKWardMainWindow);	// well, whatever this is supposed to do -> TODO
	} else {
		new RKWardMainWindow ();
	}
	args->clear();

	// Usually, KDE always adds the current directory to the list of prefixes.
	// However, since RKWard 0.5.6, the main binary is in KDE's libexec dir, which defies this mechanism. Therefore, RKWARD_ENSURE_PREFIX is set from the wrapper script.
	char *add_path = getenv ("RKWARD_ENSURE_PREFIX");
	if (add_path) KGlobal::dirs ()->addPrefix (QString::fromLocal8Bit (add_path));

	// do it!
	int status = app.exec ();

	qInstallMsgHandler (0);
	RKSettingsModuleDebug::debug_file->close ();

	return status;
}
