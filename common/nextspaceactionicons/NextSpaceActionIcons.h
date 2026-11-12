/*
 * NextSpaceActionIcons.h - neutral public header of the shared NextSpace action-icon renderer
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

#include "TeacherActionIcons.h"

// The neutral name of the existing palette-aware Tabler action-icon API (target nextspace-action-icons) for consumers
// outside the Teacher Console. It is the same namespace, not a copy: the renderer, its generated path data and every
// mapping stay in master/src/TeacherActionIcons.cpp.
namespace NextSpaceActionIcons = TeacherActionIcons;
