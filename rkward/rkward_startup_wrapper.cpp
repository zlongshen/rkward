/***************************************************************************
                          rkward_startup_wrapper  -  description
                             -------------------
    begin                : Sun Mar 10 2013
    copyright            : (C) 2013, 2014, 2015 by Thomas Friedrichsmeier
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

/** This simple helper executable is responsible for reading basic configuration
 * settings, checking for some services and starting RKWard. These things used
 * to be done using a script, and there was nothing wrong with that, in principle.
 * However, the binary allows more flexibility, and more consistency across
 * platforms. */

#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QUrl>
#include <QFile>
#include <QtDBus>
#include "misc/rkdbusapi.h"

#ifndef RKWARD_FRONTEND_LOCATION
#	define RKWARD_FRONTEND_LOCATION ""
#endif

#ifndef R_EXECUTABLE
#	define R_EXECUTABLE ""
#endif

QString findExeAtPath (const QString appname, const QString &path) {
	QDir dir (path);
	dir.makeAbsolute ();
	if (QFileInfo (dir.filePath (appname)).isExecutable ()) return dir.filePath (appname);
#ifdef Q_WS_WIN
	if (QFileInfo (dir.filePath (appname + ".exe")).isExecutable ()) return dir.filePath (appname + ".exe");
	if (QFileInfo (dir.filePath (appname + ".com")).isExecutable ()) return dir.filePath (appname + ".com");
	if (QFileInfo (dir.filePath (appname + ".bat")).isExecutable ()) return dir.filePath (appname + ".bat");
#endif
	return QString ();
}

QString findRKWardAtPath (const QString &path) {
	return findExeAtPath ("rkward.frontend", path);
}

#ifdef Q_WS_WIN
#include <windows.h>
#include <QTemporaryFile>
#endif
QString quoteCommand (const QString &orig) {
#ifdef Q_WS_WIN
// Get short path name as a safe way to pass all sort of commands on the Windows shell
// credits to http://erasmusjam.wordpress.com/2012/10/01/get-8-3-windows-short-path-names-in-a-qt-application/
	wchar_t input[orig.size()+1];
	orig.toWCharArray (input);
	input[orig.size ()] = L'\0'; // terminate string
	long length = GetShortPathName (input, NULL, 0);
	wchar_t output[length];
	GetShortPathName (input, output, length);
	return QString::fromWCharArray (output, length-1);
#else
	return orig;
#endif
}

#ifndef Q_WS_WIN
// see http://blog.qt.digia.com/blog/2006/03/16/starting-interactive-processes-with-qprocess/
// Need an interactive process e.g. for running through gdb
#	include <unistd.h>
class InteractiveProcess : public QProcess {
    static int stdinClone;
public:
    InteractiveProcess (QObject *parent = 0) : QProcess (parent) {
        if (stdinClone == -1) stdinClone = ::dup (fileno(stdin));
    }
protected:
    void setupChildProcess () {
        ::dup2 (stdinClone, fileno(stdin));
    }
};
int InteractiveProcess::stdinClone = -1;
#else
// no easy solution for Windows. But ain't Windows the world of graphical debuggers, anyway...
#	define InteractiveProcess QProcess
#endif

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);
	QStringList args = app.arguments ();
	if (!args.isEmpty ()) args.pop_front ();	// The command itself
	qputenv ("DESKTOP_STARTUP_ID", qgetenv ("STARTUP_ID_COPY"));	// for startup notifications (set via rkward.desktop)
	qputenv ("STARTUP_ID_COPY", "");

	// Parse arguments that need handling in the wrapper
	bool usage = false;
	QStringList debugger_args;
	QStringList file_args;
	bool reuse = false;
	bool warn_external = true;
	QString r_exe_arg;
	int debug_level = 2;

	for (int i=0; i < args.size (); ++i) {
		if (args[i] == "--debugger") {
			args.removeAt (i);
			while (i < args.size ()) {
				QString arg = args.takeAt (i);
				if (arg == "--") break;
				debugger_args.append (arg);
			}
			if (debugger_args.isEmpty ()) usage = true;
		} else if (args[i] == "--r-executable") {
			if ((i+1) < args.size ()) {
				r_exe_arg = args.takeAt (i + 1);
			} else usage = true;
			args.removeAt (i);
			--i;
		} else if (args[i] == "--debug-level") {
			if ((i+1) < args.size ()) {
				debug_level = args[i+1].toInt ();
			}
		} else if (args[i] == "--reuse") {
			reuse = true;
		} else if (args[i] == "--nowarn-external") {
			warn_external = false;
		} else if (args[i].startsWith ("--")) {
			// all RKWard and KDE options (other than --reuse) are of the for --option <value>. So skip over the <value>
			i++;
		} else {
			QUrl url (args[i]);
			if (url.isRelative ()) {
				file_args.append (QDir::current ().absoluteFilePath (url.toLocalFile ()));
			} else {
				file_args.append (args[i]);
			}
		}
	}

	if (reuse) {
		if (!QDBusConnection::sessionBus ().isConnected ()) {
			if (debug_level > 2) qDebug ("Could not connect to session dbus");
		} else {
			QDBusInterface iface (RKDBUS_SERVICENAME, "/", "", QDBusConnection::sessionBus ());
			if (iface.isValid ()) {
				QDBusReply<void> reply = iface.call ("openAnyUrl", file_args, warn_external);
				if (!reply.isValid ()) {
					if (debug_level > 2) qDebug ("Error while placing dbus call: %s", qPrintable (reply.error ().message ()));
					return 1;
				}
				return 0;
			}
		}
	}

	// MacOS may need some path adjustments, first
#ifdef Q_WS_MAC
	QString oldpath = qgetenv ("PATH");
	if (!oldpath.contains (INSTALL_PATH)) {
		//ensure that PATH is set to include what we deliver with the bundle
		qputenv ("PATH", QString ("%1/bin:%1/sbin:%2").arg (INSTALL_PATH).arg (oldpath).toLocal8Bit ());
		if (debug_level > 3) qDebug ("Adjusting system path to %s", qPrintable (qgetenv ("PATH")));
	}
	// ensure that RKWard finds its own packages
	qputenv ("R_LIBS", R_LIBS);
	QProcess::execute ("launchctl", QStringList () << "load" << "-w" << INSTALL_PATH "/Library/LaunchAgents/org.freedesktop.dbus-session.plist");
#endif

	// Locate KDE and RKWard installations
	QString kde4_config_exe = findExeAtPath ("kde4-config", QDir::currentPath ());
	if (kde4_config_exe.isNull ()) kde4_config_exe = findExeAtPath ("kde4-config", app.applicationDirPath ());
	if (kde4_config_exe.isNull ()) kde4_config_exe = findExeAtPath ("kde4-config", QDir (app.applicationDirPath ()).filePath ("KDE/bin"));
	if (kde4_config_exe.isNull ()) {
#ifdef Q_WS_WIN
	QStringList syspath = QString (qgetenv ("PATH")).split (';');
#else
	QStringList syspath = QString (qgetenv ("PATH")).split (':');
#endif
		for (int i = 0; i < syspath.size (); ++i) {
			kde4_config_exe = findExeAtPath ("kde4-config", syspath[i]);
			if (!kde4_config_exe.isNull ()) break;
		}
	}

	if (kde4_config_exe.isNull ()) {
		QMessageBox::critical (0, "Could not find KDE installation", "The KDE installation could not be found (kde4-config). When moving / copying RKWard, make sure to copy the whole application folder, or create a shorcut / link, instead.");
		exit (1);
	}

	QDir kde_dir (QFileInfo (kde4_config_exe).absolutePath ());
	kde_dir.makeAbsolute ();
	QString kde_dir_safe_path = quoteCommand (kde_dir.path ());
#ifdef Q_WS_WIN
	QString kdeinit4_exe = findExeAtPath ("kdeinit4", kde_dir.path ());
	qputenv ("PATH", QString (kde_dir_safe_path + ';' + qgetenv ("PATH")).toLocal8Bit ());
	if (debug_level > 3) qDebug ("Adding %s to the system path", qPrintable (kde_dir_safe_path));
#endif
	// important if RKWard is not in KDEPREFIX/bin but e.g. KDEPREFIX/lib/libexec
	qputenv ("RKWARD_ENSURE_PREFIX", kde_dir_safe_path.toLocal8Bit ());
	if (debug_level > 3) qDebug ("Setting environment variable RKWARD_ENSURE_PREFIX=%s", qPrintable (kde_dir_safe_path));

	QString rkward_frontend_exe = findRKWardAtPath (app.applicationDirPath ());	// this is for running directly from a build tree
#ifdef Q_WS_MAC
	if (rkward_frontend_exe.isNull ()) rkward_frontend_exe = findRKWardAtPath (app.applicationDirPath () + "/rkward.frontend.app/Contents/MacOS"); 	// this is for running directly from a build tree
#endif
	if (rkward_frontend_exe.isNull ()) rkward_frontend_exe = findRKWardAtPath (RKWARD_FRONTEND_LOCATION);
	if (rkward_frontend_exe.isNull ()) rkward_frontend_exe = findRKWardAtPath (kde_dir.absoluteFilePath ("bin"));
	if (rkward_frontend_exe.isNull ()) rkward_frontend_exe = findRKWardAtPath (kde_dir.absoluteFilePath ("../lib/libexec"));

	if (rkward_frontend_exe.isNull ()) {
		QMessageBox::critical (0, "RKWard frontend binary missing", "RKWard frontend binary could not be found. When moving / copying RKWard, make sure to copy the whole application folder, or create a shorcut / link, instead.");
		exit (1);
	}

	if (usage) {
		QProcess::execute (rkward_frontend_exe, QStringList ("--help"));
		exit (1);
	}

#ifdef Q_WS_WIN
	// Explicit initialization of KDE, in case Windows 7 asks for admin privileges
	if (kdeinit4_exe.isNull ()) {
		kdeinit4_exe = findExeAtPath ("kdeinit4", QFileInfo (rkward_frontend_exe).absolutePath ());
	}
	if (!kdeinit4_exe.isNull ()) QProcess::execute (kdeinit4_exe, QStringList ());
#endif

	// Look for R:
	//- command line parameter
	//- Specified in cfg file next to rkward executable
	//- compile-time default
	QString r_exe = r_exe_arg;
	if (!r_exe.isNull ()) {
		if (!QFileInfo (r_exe).isExecutable ()) {
			QMessageBox::critical (0, "Specified R executable does not exist", QString ("The R executable specified on the command line (%1) does not exist or is not executable.").arg (r_exe));
			exit (1);
		}
		if (debug_level > 3) qDebug ("Using R specified on command line");
	} else {
		QFileInfo frontend_info (rkward_frontend_exe);
		QDir frontend_path = frontend_info.absoluteDir ();
		QFileInfo rkward_ini_file (frontend_path.absoluteFilePath ("rkward.ini"));
		if (rkward_ini_file.isReadable ()) {
			QSettings rkward_ini (rkward_ini_file.absoluteFilePath (), QSettings::IniFormat);
			r_exe = rkward_ini.value ("R executable").toString ();
			if (!r_exe.isNull ()) {
				if (QDir::isRelativePath (r_exe)) {
					r_exe = frontend_path.absoluteFilePath (r_exe);
				}
				if (!QFileInfo (r_exe).isExecutable ()) {
					QMessageBox::critical (0, "Specified R executable does not exist", QString ("The R executable specified in the rkward.ini file (%1) does not exist or is not executable.").arg (rkward_ini_file.absoluteFilePath ()));
					exit (1);
				}
			}
			if (debug_level > 3) qDebug ("Using R as configured in config file %s", qPrintable (rkward_ini_file.absoluteFilePath ()));
		}
		if (r_exe.isNull ()) {
			r_exe = R_EXECUTABLE;
			if (!QFileInfo (r_exe).isExecutable ()) {
				QMessageBox::critical (0, "Specified R executable does not exist", QString ("The R executable specified at compile time (%1) does not exist or is not executable. Probably the installation of R has moved. You can use the command line parameter '--R', or supply an rkward.ini file to specify the new location.").arg (r_exe));
				exit (1);
			}
			if (debug_level > 3) qDebug ("Using R as configured at compile time");
		}
	}

	qputenv ("R_BINARY", r_exe.toLocal8Bit ());
	QStringList call_args ("CMD");
	call_args.append (debugger_args);
	call_args.append (quoteCommand (rkward_frontend_exe));

	if (!args.isEmpty ()) {
		// NOTE: QProcess quotes its arguments, *but* properly passing all spaces and quotes through the R CMD wrapper, seems near(?) impossible on Windows. Instead, we use percent encoding, internally.
		for (int i = 0; i < args.size (); ++i) {
			call_args.append (QString::fromUtf8 (QUrl::toPercentEncoding (args[i], QByteArray (), " \"")));
		}
	}

	if (debug_level > 2) qDebug ("Starting frontend: %s %s", qPrintable (r_exe), qPrintable (call_args.join (" ")));

	InteractiveProcess proc;
#ifdef Q_WS_WIN
	if (debugger_args.isEmpty ()) {
		// start _without_ opening an annoying console window
		QTemporaryFile *vbsf = new QTemporaryFile (QDir::tempPath () + "/rkwardlaunchXXXXXX.vbs");
		vbsf->setAutoRemove (false);
		if (vbsf->open ()) {
			QTextStream vbs (vbsf);
			vbs << "Dim WinScriptHost\r\nSet WinScriptHost = CreateObject(\"WScript.Shell\")\r\nWinScriptHost.Run \"" << quoteCommand (r_exe);
			for (int i = 0;  i < call_args.length (); ++i) {
				vbs << " " << call_args[i];
			}
			vbs << "\", 0\r\nSet WomScriptHost = Nothing\r\n";
			vbsf->close ();
			QString filename = vbsf->fileName ();
			delete (vbsf);  // somehow, if creating vbsf on the stack, we cannot launch it, because "file is in use by another process", despite we have closed it.
			proc.start ("WScript.exe", QStringList (filename));
			bool ok = proc.waitForFinished (-1);
			if (proc.exitCode () || !ok) {
				QMessageBox::critical (0, "Error starting RKWard", QString ("Starting RKWard failed with error \"%1\"").arg (proc.errorString ()));
			}
			QFile (filename).remove ();
			return (0);
		}
	}
	// if that did not work or not on windows:
#endif
	proc.setProcessChannelMode (QProcess::ForwardedChannels);
	proc.start (quoteCommand (r_exe), call_args);
	bool ok = proc.waitForFinished (-1);
	if (proc.exitCode () || !ok) {
		QMessageBox::critical (0, "Error starting RKWard", QString ("Starting RKWard failed with error \"%1\"").arg (proc.errorString ()));
	}

	return (0);
}
