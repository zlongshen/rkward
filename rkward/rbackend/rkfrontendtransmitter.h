/***************************************************************************
                          rkfrontendtransmitter  -  description
                             -------------------
    begin                : Thu Nov 04 2010
    copyright            : (C) 2010 by Thomas Friedrichsmeier
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

#ifndef RKFRONTENDTRANSMITTER_H
#define RKFRONTENDTRANSMITTER_H

#include "rktransmitter.h"

class QProcess;
class QLocalServer;
class RKGraphicsDeviceFrontendTransmitter;

class RKFrontendTransmitter : public RKAbstractTransmitter, public RKROutputBuffer {
Q_OBJECT
public:
	RKFrontendTransmitter ();
	~RKFrontendTransmitter ();

	void run ();

	bool doMSleep (int delay) {
		msleep (delay);
		return true;
	};
	void writeRequest (RBackendRequest *request);
	void requestReceived (RBackendRequest *request);
private slots:
	void connectAndEnterLoop ();
	void backendExit (int exitcode);
private:
	void handleTransmissionError (const QString &message);

	QProcess* backend;
	QLocalServer* server;
	RKGraphicsDeviceFrontendTransmitter* rkd_transmitter;
};

#endif

