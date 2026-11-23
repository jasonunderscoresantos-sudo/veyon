/*
 * ComputerItemDelegate.h - header file for ComputerItemDelegate
 *
 * Copyright (c) 2025-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
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

#include <QPalette>
#include <QPixmapCache>
#include <QStyledItemDelegate>

#include "ComputerControlInterface.h"

class TeacherConsoleUiConvergenceTest;

class ComputerItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	ComputerItemDelegate(bool fullLengthLabels, QObject* parent = nullptr);

	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QRect selectionIndicatorRect(const QStyleOptionViewItem& option) const;

private:
	friend class TeacherConsoleUiConvergenceTest;

	struct CardColors
	{
		QColor surface;
		QColor subtleSurface;
		QColor border;
		QColor accent;
		QColor selectedFill;
		QColor primaryText;
		QColor secondaryText;
		QColor selectedText;
	};

	static CardColors cardColors(const QPalette& palette);
	static CardColors cardColors(const QPalette& palette, bool highContrast);
	static QString stateLabel(const ComputerControlInterface::Pointer& controlInterface);
	static QString userLabel(const ComputerControlInterface::Pointer& controlInterface);
	static QString footerLabel(const ComputerControlInterface::Pointer& controlInterface);

	void initFeaturePixmaps();
	void drawCard(QPainter* painter, const QStyleOptionViewItem& option) const;
	QRect previewRect(const QStyleOptionViewItem& option) const;
	void drawThumbnail(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
	void drawSelectionIndicator(QPainter* painter, const QStyleOptionViewItem& option) const;
	void drawFooter(QPainter* painter, const QStyleOptionViewItem& option, const QRect& preview,
					const QModelIndex& index, const ComputerControlInterface::Pointer& controlInterface) const;
	void drawFeatureIcons(QPainter* painter, const QPoint& pos, const QPalette& palette,
						  ComputerControlInterface::Pointer controlInterface) const;

	static constexpr int OverlayIconSize = 32;
	static constexpr int OverlayIconSpacing = 4;
	static constexpr int OverlayIconsPadding = 8;
	static constexpr int OverlayIconsRadius = 6;

	// NextSpace card geometry (logical pixels)
	static constexpr int FocusMargin = 4;
	static constexpr int CardPadding = 6;
	static constexpr int CardRadius = 10;
	static constexpr int FooterPadding = 12;
	static constexpr int FooterLineGap = 4;
	static constexpr int NameTextSize = 14;
	static constexpr int StatusTextSize = 12;
	static constexpr int SelectionIndicatorSize = 16;
	static constexpr int SelectionIndicatorGap = 8;

	QMap<QUuid, QPixmap> m_featurePixmaps;
	bool m_fullLengthLabels;

};
