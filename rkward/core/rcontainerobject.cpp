/***************************************************************************
                          rcontainerobject  -  description
                             -------------------
    begin                : Thu Aug 19 2004
    copyright            : (C) 2004 by Thomas Friedrichsmeier
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
#include "rcontainerobject.h"

#include "../rbackend/rinterface.h"
#include "robjectlist.h"

#include "../rkglobals.h"

#include "../debug.h"

#define UPDATE_DIM_COMMAND 1
#define UPDATE_CLASS_COMMAND 2
#define UPDATE_CHILD_LIST_COMMAND 3
#define UPDATE_TYPE_COMMAND 4
#define FIND_META_COMMAND 5
#define FIND_CHILD_META_COMMAND 6

RContainerObject::RContainerObject (RContainerObject *parent, const QString &name) : RObject (parent, name) {
	RK_TRACE (OBJECTS);
	classname = 0;
	dimension = 0;
	num_classes = num_dimensions = 0;
	num_children_updating = 0;
}

RContainerObject::~RContainerObject () {
	RK_TRACE (OBJECTS);
}

void RContainerObject::updateFromR () {
	RK_TRACE (OBJECTS);
	RCommand *command = new RCommand ("is.list (" + getMetaObjectName () + "$data)", RCommand::App | RCommand::Sync | RCommand::GetIntVector, "", this, FIND_META_COMMAND);
	RKGlobals::rInterface ()->issueCommand (command, RKGlobals::rObjectList()->getUpdateCommandChain ());

	command = new RCommand ("is.list (" + getMetaObjectName () + "$children)", RCommand::App | RCommand::Sync | RCommand::GetIntVector, "", this, FIND_CHILD_META_COMMAND);
	RKGlobals::rInterface ()->issueCommand (command, RKGlobals::rObjectList()->getUpdateCommandChain ());
}

void RContainerObject::rCommandDone (RCommand *command) {
	RK_TRACE (OBJECTS);
	RK_DO (qDebug ("command: %s", command->command ().latin1 ()), OBJECTS, DL_DEBUG);
	RObject::rCommandDone (command);

	bool changed = false;
	if (command->getFlags () == FIND_META_COMMAND) {
		if ((command->intVectorLength () == 1) && command->getIntVector ()[0]) {
			type |= HasMetaObject;
			getMetaData (RKGlobals::rObjectList()->getUpdateCommandChain ());
		} else {
			type -= (type & HasMetaObject);
		}
	} else if (command->getFlags () == FIND_CHILD_META_COMMAND) {
		if ((command->intVectorLength () == 1) && command->getIntVector ()[0]) {
			type |= HasChildMetaObject;
		} else {
			type -= (type & HasChildMetaObject);
		}
		
		RCommand *ncommand = new RCommand ("class (" + getFullName () + ")", RCommand::App | RCommand::Sync | RCommand::GetStringVector, "", this, UPDATE_CLASS_COMMAND);
		RKGlobals::rInterface ()->issueCommand (ncommand, RKGlobals::rObjectList()->getUpdateCommandChain ());
	
		ncommand = new RCommand ("dim (" + getFullName () + ")", RCommand::App | RCommand::Sync | RCommand::GetIntVector, "", this, UPDATE_DIM_COMMAND);
		RKGlobals::rInterface ()->issueCommand (ncommand, RKGlobals::rObjectList()->getUpdateCommandChain ());

		ncommand = new RCommand ("names (" + getFullName () + ")", RCommand::App | RCommand::Sync | RCommand::GetStringVector, "", this, UPDATE_CHILD_LIST_COMMAND);
		RKGlobals::rInterface ()->issueCommand (ncommand, RKGlobals::rObjectList()->getUpdateCommandChain ());

		// this command might result in a type mismatch and then a deletion of this object. Hence we run it last.
		ncommand = new RCommand ("c (is.data.frame (" + getFullName () + "), is.matrix (" + getFullName () + "), is.array (" + getFullName () + "), is.list (" + getFullName () + "))", RCommand::App | RCommand::Sync | RCommand::GetIntVector, "", this, UPDATE_TYPE_COMMAND);
		RKGlobals::rInterface ()->issueCommand (ncommand, RKGlobals::rObjectList()->getUpdateCommandChain ());
	} else if (command->getFlags () == UPDATE_CHILD_LIST_COMMAND) {
		num_children_updating = command->stringVectorLength ();
		// empty object?
		if (!num_children_updating) {
			parent->childUpdateComplete ();
		}
		for (int i = 0; i < command->stringVectorLength (); ++i) {
			QString cname = command->getStringVector ()[i];
			if (childmap.find (cname) != childmap.end ()) {
				RK_DO (qDebug ("updating existing child: %s", cname.latin1 ()), APP, DL_DEBUG);
				childmap[cname]->updateFromR ();
			} else {
				RK_DO (qDebug ("creating new child: %s", cname.latin1 ()), APP, DL_DEBUG);
				RKGlobals::rObjectList()->createFromR (this, cname);
				changed = true;
			}
		}
	} else if (command->getFlags () == UPDATE_CLASS_COMMAND) {

		if (num_classes != command->stringVectorLength ()) {
			num_classes = command->stringVectorLength ();
			delete classname;
			classname = new QString [num_classes];
			changed = true;
		}
		for (int cn=0; cn < num_classes; ++cn) {
			if (classname[cn] != command->getStringVector ()[cn]) changed = true;
			classname[cn] = command->getStringVector ()[cn];
		}
	
	} else if (command->getFlags () == UPDATE_TYPE_COMMAND) {

		int new_type = 0;
		if (command->intVectorLength () != 4) {
			RK_ASSERT (false);
		} else {
			if (command->getIntVector ()[0]) {
				new_type = RObject::DataFrame | RObject::Matrix | RObject::Array | RObject::List | RObject::Container;
			} else if (command->getIntVector ()[1]) {
				new_type = RObject::Matrix | RObject::Array | RObject::List | RObject::Container;
			} else if (command->getIntVector ()[2]) {
				new_type = RObject::Array | RObject::List | RObject::Container;
			} else if (command->getIntVector ()[3]) {
				new_type = RObject::List | RObject::Container;
			}
		}
		if ((RObject::type) && (new_type != RObject::type)) {
			if (!(new_type & RObject::Container)) {
				RK_DO (qDebug ("name: %s, old_type: %d, new_type: %d", RObject::name.latin1 (), type, new_type), OBJECTS, DL_DEBUG);
				RObject::parent->typeMismatch (this, RObject::name);
				return;	// will be deleted!
			}
		}
		if (new_type != RObject::type) {
			changed = true;
			RObject::type = new_type;
		}

	} else if (command->getFlags () == UPDATE_DIM_COMMAND) {

		if (num_dimensions != command->intVectorLength ()) {
			num_dimensions = command->intVectorLength ();
			changed = true;
			delete dimension;
			dimension = new int [num_dimensions];
		}

		for (int d=0; d < num_dimensions; ++d) {
			if (dimension[d] != command->getIntVector ()[d]) changed=true;
			dimension[d] = command->getIntVector ()[d];
		}
	}
	
	// TODO: signal change if any
}

void RContainerObject::typeMismatch (RObject *child, QString childname) {
	RK_TRACE (OBJECTS);
	delete child;
	childmap.remove (childname);
	
	RKGlobals::rObjectList()->createFromR (this, childname);
}

void RContainerObject::childUpdateComplete () {
	RK_TRACE (OBJECTS);
	RK_ASSERT (num_children_updating);
	if ((--num_children_updating) <= 0) parent->childUpdateComplete ();
}

void RContainerObject::addChild (RObject *child, QString childname) {
	RK_TRACE (OBJECTS);
	RK_ASSERT (child);

	childmap.insert (childname, child);
	child->updateFromR ();
}

int RContainerObject::numChildren () {
	RK_TRACE (OBJECTS);
	return childmap.size ();
}

RObject **RContainerObject::children () {
	RK_TRACE (OBJECTS);
	RObject **ret = new RObject *[childmap.size ()];

	int i = 0;
	for (RObjectMap::iterator it = childmap.begin (); it != childmap.end (); ++it) {
		ret[i++] = it.data ();
	}
	return ret;
}

QString RContainerObject::makeClassString (const QString &sep) {
	RK_TRACE (OBJECTS);
	QString ret;
	for (int i=0; i < num_classes; ++i) {
		ret.append (classname[i]);
		if (i < (num_classes - 1)) {
			ret.append (sep);
		}
	}
	return ret;
}

void RContainerObject::createMetaObject (RCommandChain *chain) {
	RK_TRACE (OBJECTS);
	if (!hasMetaObject ()) {
		parent->createMetaObject (chain);
		RCommand *command = new RCommand ("if (!is.list (" + getMetaObjectName () + ")) " + getMetaObjectName () + " <- list ()", RCommand::App | RCommand::Sync);
		RKGlobals::rInterface ()->issueCommand (command, chain);
		command = new RCommand ("if (!is.data.frame (" + getMetaObjectName () + "$data)) " + getMetaObjectName () + "$data <- data.frame ()", RCommand::App | RCommand::Sync);
		RKGlobals::rInterface ()->issueCommand (command, chain);
		command = new RCommand ("if (!is.list (" + getMetaObjectName () + "$children)) " + getMetaObjectName () + "$children <- list ()", RCommand::App | RCommand::Sync);
		RKGlobals::rInterface ()->issueCommand (command, chain);
	}
	type |= HasMetaObject;
	type |= HasChildMetaObject;
}

void RContainerObject::writeMetaData (RCommandChain *chain) {
	RK_TRACE (OBJECTS);
	RObject::writeMetaData (chain);
	
	for (RObjectMap::iterator it = childmap.begin (); it != childmap.end (); ++it) {
		it.data ()->writeMetaData (chain);;
	}
}

void RContainerObject::setChildModified () {
	RK_TRACE (OBJECTS);
	RObject::state |= ChildrenModified;
	parent->setChildModified ();
}

RObject *RContainerObject::findChild (const QString &name) {
	RK_TRACE (OBJECTS);
	RObjectMap::iterator it = childmap.find (name);
	RK_ASSERT (it != childmap.end ());
	return (it.data ());
}
