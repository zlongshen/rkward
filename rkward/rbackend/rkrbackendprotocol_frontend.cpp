/***************************************************************************
                          rkrbackendprotocol  -  description
                             -------------------
    begin                : Thu Nov 04 2010
    copyright            : (C) 2010 by Thomas Friedrichsmeier
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

#include "rkrbackendprotocol_frontend.h"

#include "rinterface.h"

#include <QThread>

#ifdef RKWARD_THREADED
#	include "rkrbackend.h"
#	include "rkrbackendprotocol_backend.h"
#else
#	include <QLocalServer>
#	include <QLocalSocket>
#	include <QProcess>
#	include <QCoreApplication>
#endif

#include "../debug.h"

#ifndef RKWARD_THREADED
	class RKFrontendTransmitter : public QThread, public RKROutputBuffer {
		Q_OBJECT
		public:
			RKFrontendTransmitter () {
				RK_TRACE (RBACKEND);

				// start server
				connection = 0;
				if (!server.listen ("rkward")) handleTransmitError ("failure to start frontend server: " + server.errorString ());
				connect (&server, SIGNAL (newConnection ()), this, SLOT (connectAndEnterLoop ()));

				// start backend
				QStringList args;
				args.append ("--debug-level " + QString::number (RK_Debug_Level));
				args.append ("--server-name " + server.fullServerName ());
				backend.setProcessChannelMode (QProcess::MergedChannels);	// at least for now. Seems difficult to get interleaving right, without this.
				connect (&backend, SIGNAL (readyReadStandardOutput ()), this, SLOT (newProcessOutput ()));
				connect (&backend, SIGNAL (finished (int, QProcess::ExitStatus)), this, SLOT (backendExit (int, QProcess::ExitStatus)));
				backend.start ("rkward_backend", args, QIODevice::ReadOnly);

				RK_ASSERT (_instance == 0);
				_instance = this;
			};

			~RKFrontendTransmitter () {
				RK_TRACE (RBACKEND);

				RK_ASSERT (!server.isListening ());
				delete connection;
			};

			static RKFrontendTransmitter* instance () { return _instance; };

			void run () {
				RK_ASSERT (connection);

				connection->moveToThread (this);
				connect (connection, SIGNAL (stateChanged (QLocalSocket::LocalSocketState)), this, SLOT (connectionStateChanged (QLocalSocket::LocalSocketState)));
				connect (connection, SIGNAL (readyRead ()), this, SLOT (newConnectionData ()));
				backend.moveToThread (this);

				exec ();
			};

			bool doMSleep (int delay) {
				msleep (delay);
				return true;
			};
		private slots:
			void connectAndEnterLoop () {
				RK_TRACE (RBACKEND);
				RK_ASSERT (server.hasPendingConnections ());

				connection = server.nextPendingConnection ();
				server.close ();

				start ();
			};

			void newProcessOutput () {
				RK_TRACE (RBACKEND);
#warning TODO: fix interleaving
				QString output = QString::fromLocal8Bit (backend.readAll ());
				handleOutput (output, output.size (), ROutput::Warning);
			};

			void newConnectionData () {
				RK_TRACE (RBACKEND);

				if (!connection->canReadLine ()) return;

				QString line = QString::fromLocal8Bit (connection->readLine ());
				bool ok;
				int expected_length = line.toInt (&ok);
				if (!ok) handleTransmitError ("Protocol header error. Last connection error was: " + connection->errorString ());

				QByteArray receive_buffer;
				while (receive_buffer.length () < expected_length) {
					if (connection->bytesAvailable ()) {
						receive_buffer.append (connection->read (expected_length - receive_buffer.length ()));
					} else {
						connection->waitForReadyRead (1000);
						if (!connection->isOpen ()) {
							handleTransmitError ("Connection closed unexepctedly. Last error: " + connection->errorString ());
							return;
						}
					}
				}

				RBackendRequest *req = RKRBackendSerializer::unserialize (receive_buffer);
				RKRBackendEvent* event = new RKRBackendEvent (req);
				qApp->postEvent (RKRBackendProtocolFrontend::instance ()->parent (), event);
			};

			void backendExit (int exitcode, QProcess::ExitStatus exitstatus) {
				RK_TRACE (RBACKEND);

				RBackendRequest* req = new RBackendRequest (false, RBackendRequest::BackendExit);
				RKRBackendEvent* event = new RKRBackendEvent (req);
				qApp->postEvent (RKRBackendProtocolFrontend::instance ()->parent (), event);
			};

			void connectionStateChanged (QLocalSocket::LocalSocketState state) {
				if (state != QLocalSocket::UnconnectedState) return;		// only interested in connection failure
				RK_TRACE (RBACKEND);

				RBackendRequest* req = new RBackendRequest (false, RBackendRequest::BackendExit);
				RKRBackendEvent* event = new RKRBackendEvent (req);
				qApp->postEvent (RKRBackendProtocolFrontend::instance ()->parent (), event);
			};

			void writeRequest (RBackendRequest *request) {
				RK_TRACE (RBACKEND);

				QByteArray buffer = RKRBackendSerializer::serialize (*request);
				connection->write (QString::number (buffer.length ()).toLocal8Bit () + "\n");
				connection->write (buffer);
				delete request;
			};

			void customEvent (QEvent *e) {
				if (((int) e->type ()) == ((int) RKRBackendEvent::RKWardEvent)) {
					RKRBackendEvent *ev = static_cast<RKRBackendEvent*> (e);
					writeRequest (ev->data ());
				} else {
					RK_ASSERT (false);
					return;
				}
			};
		private:
			void handleTransmitError (const QString &message) {
				RK_TRACE (RBACKEND);
				#warning: Show those errors to the user!

				qDebug ("%s", qPrintable (message));
			};

			int current_request_length;
			QProcess backend;
			QLocalServer server;
			QLocalSocket* connection;
			static RKFrontendTransmitter *_instance;
	};
	RKFrontendTransmitter* RKFrontendTransmitter::_instance = 0;

	#include "rkrbackendprotocol_frontend.moc"
#endif

RKRBackendProtocolFrontend* RKRBackendProtocolFrontend::_instance = 0;
RKRBackendProtocolFrontend::RKRBackendProtocolFrontend (RInterface* parent) : QObject (parent) {
	RK_TRACE (RBACKEND);

	RK_ASSERT (!_instance);
	frontend = parent;
	_instance = this;
}

RKRBackendProtocolFrontend::~RKRBackendProtocolFrontend () {
	RK_TRACE (RBACKEND);

	terminateBackend ();
#ifdef RKWARD_THREADED
	delete RKRBackendProtocolBackend::instance ();
#else
	delete RKFrontendTransmitter::instance ();
#endif
}

void RKRBackendProtocolFrontend::setupBackend (QVariantMap backend_params) {
	RK_TRACE (RBACKEND);

#ifdef RKWARD_THREADED
	new RKRBackendProtocolBackend ();
#else
	new RKFrontendTransmitter ();
#endif
}

void RKRBackendProtocolFrontend::setRequestCompleted (RBackendRequest *request) {
	RK_TRACE (RBACKEND);

	bool sync = request->synchronous;
	request->completed ();
	if (!sync) return;

#ifndef RKWARD_THREADED
	RKRBackendEvent* ev = new RKRBackendEvent (request);
	qApp->postEvent (RKFrontendTransmitter::instance (), ev);
#endif
	QThread::yieldCurrentThread ();
}

ROutputList RKRBackendProtocolFrontend::flushOutput (bool force) {
	RK_TRACE (RBACKEND);

#ifdef RKWARD_THREADED
	return (RKRBackend::this_pointer->flushOutput (force));
#else
	return RKFrontendTransmitter::instance ()->flushOutput (force);
#endif
}

void RKRBackendProtocolFrontend::interruptProcessing () {
	RK_TRACE (RBACKEND);

#ifdef RKWARD_THREADED
	RK_ASSERT (!RKRBackendProtocolBackend::inRThread ());
	RKRBackendProtocolBackend::interruptProcessing ();
#else
//	kill (SIGUSR1, pid_of_it);
#warning wont't work on windows!
#endif
}

void RKRBackendProtocolFrontend::terminateBackend () {
	RK_TRACE (RBACKEND);

#ifdef RKWARD_THREADED
	RKRBackend::this_pointer->kill ();
#else
//	kill (SIGUSR2, pid_of_it);
#warning wont't work on windows!
#endif
}

#ifdef RKWARD_THREADED
void RKRBackendProtocolFrontend::customEvent (QEvent *e) {
	if (((int) e->type ()) == ((int) RKRBackendEvent::RKWardEvent)) {
		RKRBackendEvent *ev = static_cast<RKRBackendEvent*> (e);
		frontend->handleRequest (ev->data ());
	} else {
		RK_ASSERT (false);
		return;
	}
}
#endif

