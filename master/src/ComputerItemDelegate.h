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

#include <QPixmapCache>
#include <QStyledItemDelegate>

#include "ComputerControlInterface.h"

class ComputerItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	ComputerItemDelegate(bool fullLengthLabels, QObject* parent = nullptr);

	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
	struct CardColors
	{
		QColor border;
		QColor accent;
		QColor selectedFill;
		QColor selectedText;
		QColor badgeFill;
		QColor badgeText;
	};

	static CardColors cardColors();
	static QString stateLabel(const ComputerControlInterface::Pointer& controlInterface);

	void initFeaturePixmaps();
	void drawCard(QPainter* painter, const QStyleOptionViewItem& option) const;
	void drawStateBadge(QPainter* painter, const QStyleOptionViewItem& contentOption, const QModelIndex& index,
						const QString& label) const;
	void drawFeatureIcons(QPainter* painter, const QPoint& pos, ComputerControlInterface::Pointer controlInterface) const;

	static constexpr int OverlayIconSize = 32;
	static constexpr int OverlayIconSpacing = 4;
	static constexpr int OverlayIconsPadding = 8;
	static constexpr int OverlayIconsRadius = 6;

	// NextSpace card geometry (logical pixels): focus ring margin + card padding around the upstream content
	static constexpr int FocusMargin = 4;
	static constexpr int CardPadding = 6;
	static constexpr int CardInset = FocusMargin + CardPadding;
	static constexpr int CardRadius = 10;
	static constexpr int BadgePaddingX = 8;
	static constexpr int BadgePaddingY = 3;
	static constexpr int BadgeMargin = 6;
	static constexpr int BadgeRadius = 6;

	const CardColors m_colors;
	QMap<QUuid, QPixmap> m_featurePixmaps;
	bool m_fullLengthLabels;

};
