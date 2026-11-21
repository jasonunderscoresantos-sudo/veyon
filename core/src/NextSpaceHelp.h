/*
 * NextSpaceHelp.h - NextSpace offline help launcher
 *
 * This file is part of NextSpace Classroom Management, a modified version of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include "VeyonCore.h"

class QWidget;

// Opens the English offline help installed with the product. Only the two fixed files help/index.html and
// help/quick-reference.txt below QCoreApplication::applicationDirPath() are used; no caller-supplied path or URL,
// working directory, environment variable or online location is consulted. The local HTML is handed to the default
// viewer first; when it is missing or the request is rejected, the plain text is shown in a read-only selectable
// dialog; when neither can be shown, one local error names the support address.
class VEYON_CORE_EXPORT NextSpaceHelp
{
public:
	static void open( QWidget* parent );

} ;
