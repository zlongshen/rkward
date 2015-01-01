/***************************************************************************
                          rkcomponentmeta  -  description
                             -------------------
    begin                : Wed Jan 09 2013
    copyright            : (C) 2013, 2014 by Thomas Friedrichsmeier
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

#include "rkcomponentmeta.h"

#include "../misc/xmlhelper.h"
#include "../misc/rkmessagecatalog.h"
#include "../rbackend/rksessionvars.h"

#include <klocale.h>

#include "../debug.h"

QLatin1String rkward_min_version_tag ("rkward_min_version");
QLatin1String rkward_max_version_tag ("rkward_max_version");
QLatin1String R_min_version_tag ("R_min_version");
QLatin1String R_max_version_tag ("R_max_version");
QLatin1String any_min_version_tag ("min_version");
QLatin1String any_max_version_tag ("max_version");

RKComponentAboutData::RKComponentAboutData (const QDomElement& e, XMLHelper &xml) {
	RK_TRACE (PLUGIN);
	if (e.isNull ()) {
		valid = false;
		return;
	}

	valid = true;
	name = xml.i18nStringAttribute (e, "name", QString (), DL_INFO);
	version = xml.getStringAttribute (e, "version", QString (), DL_INFO);
	releasedate = xml.getStringAttribute (e, "releasedate", QString (), DL_INFO);
	shortinfo = xml.i18nStringAttribute (e, "shortinfo", QString (), DL_INFO);
	copyright = xml.getStringAttribute (e, "copyright", QString (), DL_INFO);
	license = xml.getStringAttribute (e, "license", QString (), DL_INFO);
	url = xml.getStringAttribute (e, "url", QString (), DL_INFO);
	category = xml.i18nStringAttribute (e, "category", i18n ("Unspecified"), DL_INFO);

	XMLChildList aes = xml.getChildElements (e, "author", DL_INFO);
	for (int i = 0; i < aes.size (); ++i) {
		QDomElement ae = aes[i];
		RKComponentAuthor author;
		author.name = xml.i18nStringAttribute (ae, "name", QString (), DL_INFO);
		if (author.name.isEmpty ()) {
			author.name = xml.i18nStringAttribute (ae, "given", QString (), DL_WARNING) + " " + xml.i18nStringAttribute (ae, "family", QString (), DL_WARNING);
			
		}
		if (author.name.isEmpty ()) xml.displayError (&ae, "No author name specified", DL_WARNING);
		author.roles = xml.getStringAttribute (ae, "role", QString (), DL_INFO);
		author.email = xml.getStringAttribute (ae, "email", QString (), DL_WARNING);
		author.url = xml.getStringAttribute (ae, "url", QString (), DL_INFO);
		authors.append (author);
	}

	const QString translator_names_id ("Your names");
	const QString translator_emails_id ("Your emails");
	translator_names = xml.messageCatalog ()->translate ("NAME OF TRANSLATORS", translator_names_id);
	translator_emails = xml.messageCatalog ()->translate ("EMAIL OF TRANSLATORS", translator_emails_id);
	if (translator_names == translator_names_id) translator_names.clear ();
	if (translator_emails == translator_emails_id) translator_emails.clear ();
}

RKComponentAboutData::~RKComponentAboutData () {
	RK_TRACE (PLUGIN);
}

QString RKComponentAboutData::toHtml () const {
	RK_TRACE (PLUGIN);

	QString ret = "<p><b>" + name + "</b>";
	if (!version.isEmpty ()) ret.append (" " + version);
	if (!releasedate.isEmpty ()) ret.append (" (" + releasedate + ")");
	if (!shortinfo.isEmpty ()) ret.append (":</p>\n<p>" + shortinfo);
	ret.append ("</p>\n");
	if (!url.isEmpty ()) ret.append ("URL: <a href=\"" + url + "\">" + url + "</a></p>\n<p>");
	if (!copyright.isEmpty ()) ret.append (i18n ("Copyright (c): %1", copyright) + "</p>\n<p>");
	if (!license.isEmpty ()) ret.append (i18n ("License: %1", license) + "</p>");

	if (!authors.isEmpty ()) {
		ret.append ("\n<p><b>" + i18n ("Authors:") + "</b></p>\n<p><ul>");
		for (int i = 0; i < authors.size (); ++i) {
			RKComponentAuthor a = authors[i];
			ret.append ("<li>" + a.name);
			if (!a.email.isEmpty ()) ret.append (" (" + a.email + ")");
			if (!a.url.isEmpty ()) ret.append (" (" + a.url + ")");
			if (!a.roles.isEmpty ()) ret.append ("<br/><i>" + i18nc ("Author roles (contributor, etc.)", "Roles") + "</i>: " + a.roles);
		}
		ret.append ("</ul></p>");
	}

	if (!translator_names.isNull ()) {
		QStringList tns = translator_names.split (QLatin1Char(','), QString::KeepEmptyParts);
		QStringList tes = translator_emails.split (QLatin1Char(','), QString::KeepEmptyParts);
		ret.append ("\n<p><b>" + i18n ("Translators:") + "</b></p>\n<p><ul>");
		for (int i = 0; i < tns.size (); ++i) {
			QString tn = tns.value (i);
			QString te = tes.value (i);
			if (tn.isEmpty () && te.isEmpty ()) continue;
			ret.append ("<li>" + tn + " <" + te + "></li>\n");
		}
		ret.append ("</ul></p>");
	}

	return ret;
}


bool RKComponentDependency::isRKWardVersionCompatible (const QDomElement& e) {
	RK_TRACE (PLUGIN);

	if (e.hasAttribute (rkward_min_version_tag)) {
		if (RKSessionVars::compareRKWardVersion (e.attribute (rkward_min_version_tag)) > 0) return false;
	}
	if (e.hasAttribute (rkward_max_version_tag)) {
		if (RKSessionVars::compareRKWardVersion (e.attribute (rkward_max_version_tag)) < 0) return false;
	}

	return true;
}

bool RKComponentDependency::isRVersionCompatible (const QDomElement& e) {
	RK_TRACE (PLUGIN);

	if (e.hasAttribute (R_min_version_tag)) {
		if (RKSessionVars::compareRVersion (e.attribute (R_min_version_tag)) > 0) return false;
	}
	if (e.hasAttribute (R_max_version_tag)) {
		if (RKSessionVars::compareRVersion (e.attribute (R_max_version_tag)) < 0) return false;
	}

	return true;
}

QList <RKComponentDependency> RKComponentDependency::parseDependencies (const QDomElement& e, XMLHelper &xml) {
	RK_TRACE (PLUGIN);

	QList<RKComponentDependency> ret;
	if (e.isNull ()) return ret;
	RKComponentDependency dep;

	// Check for R dependency, first.
	dep.type = RKComponentDependency::RBaseInstallation;
	if (e.hasAttribute (R_min_version_tag)) dep.min_version = RKSessionVars::parseVersionString (e.attribute (R_min_version_tag), 0);
	if (e.hasAttribute (R_max_version_tag)) dep.max_version = RKSessionVars::parseVersionString (e.attribute (R_max_version_tag), 0);
	if ((dep.min_version > 0) || (dep.max_version < 0xFFFFFFFF)) ret.append (dep);

	XMLChildList deps = xml.getChildElements (e, QString (), DL_INFO);
	for (int i = 0; i < deps.size (); ++i) {
		QDomElement dep_e = deps[i];
		if (dep_e.tagName () == "package") {
			dep.type = RKComponentDependency::RPackage;
			dep.source_info = xml.getStringAttribute (e, "repository", QString (), DL_INFO);
		} else if (dep_e.tagName () == "pluginmap") {
			dep.type = RKComponentDependency::RKWardPluginmap;
			dep.source_info = xml.getStringAttribute (e, "url", QString ("http://rkward.kde.org"), DL_WARNING);
		} else {
			RK_DEBUG (PLUGIN, DL_ERROR, "Tag <%s> is not allowed, here.", qPrintable (dep_e.tagName ()));
			continue;
		}
		dep.package = xml.getStringAttribute (dep_e, "name", QString (), DL_ERROR);

		dep.min_version = 0;
		dep.max_version = 0xFFFFFFFF;
		if (e.hasAttribute (any_min_version_tag)) dep.min_version = RKSessionVars::parseVersionString (e.attribute (any_min_version_tag), 0);
		if (e.hasAttribute (any_max_version_tag)) dep.max_version = RKSessionVars::parseVersionString (e.attribute (any_max_version_tag), 0);

		ret.append (dep);
	}

	// Add RKWard dependency, last
	dep.type = RKComponentDependency::RKWardVersion;
	dep.min_version = 0;
	dep.max_version = 0xFFFFFFFF;
	dep.source_info.clear ();
	// Although we ignore it, here, RKWard dependencies may come with a non-numeric suffix
	QString suffix_dummy;
	if (e.hasAttribute (rkward_min_version_tag)) dep.min_version = RKSessionVars::parseVersionString (e.attribute (rkward_min_version_tag), &suffix_dummy);
	if (e.hasAttribute (rkward_max_version_tag)) dep.max_version = RKSessionVars::parseVersionString (e.attribute (rkward_max_version_tag), &suffix_dummy);
	if ((dep.min_version > 0) || (dep.max_version < 0xFFFFFFFF)) ret.append (dep);

	return ret;
}

QString numericVersionToString (quint32 numeric) {
	QString ret;
	for (int i = 3; i >= 0; --i) {
		int ver_part = (numeric >> (i * 8)) & 0x000000FF;
		ret.append (QString::number (ver_part));
		if (i > 0) ret.append ('.');
	}
	if (ret.endsWith (".0")) ret.chop (2);	// HACK: Don't print more than three verison parts, unless the fourth is non-zero
	return ret;
}

QString RKComponentDependency::depsToHtml (const QList <RKComponentDependency>& deps) {
	RK_TRACE (PLUGIN);

	QString ret;
	if (deps.isEmpty ()) return ret;

	ret.append ("<ul>");
	for (int i = 0;  i < deps.size (); ++i) {
		ret.append ("<li>");
		const RKComponentDependency &dep = deps[i];
		if (dep.type == RBaseInstallation) {
			ret.append ("R");
		} else if (dep.type == RKWardVersion) {
			ret.append ("RKWard");
		} else {
			if (dep.type == RKWardPluginmap) ret.append (i18n ("RKWard plugin map"));
			else ret.append (i18n ("R package"));
			ret.append (" \"" + dep.package + "\"");
			if (!dep.source_info.isEmpty ()) ret.append (" (" + dep.source_info + ")");
		}
		if (dep.min_version > 0) ret.append (" &gt;= " + numericVersionToString (dep.min_version));
		if (dep.max_version < 0xFFFFFFFF) ret.append (" &lt;= " + numericVersionToString (dep.max_version));
		ret.append ("</li>");
	}
	ret.append ("</ul>");
	return ret;
}

