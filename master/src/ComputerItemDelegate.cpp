/*
 * ComputerItemDelegate.cpp - implementation of ComputerItemDelegate

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

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>

#include "ComputerControlListModel.h"
#include "ComputerItemDelegate.h"
#include "FeatureManager.h"


ComputerItemDelegate::ComputerItemDelegate(bool fullLengthLabels, QObject* parent) :
	QStyledItemDelegate(parent),
	m_colors(cardColors()),
	m_fullLengthLabels(fullLengthLabels)
{
	initFeaturePixmaps();
}



void ComputerItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	drawCard(painter, option);

	// the upstream thumbnail and label are drawn unchanged inside the card; selection, focus and
	// hover are shown by the card so the style must not tint the screen thumbnail
	auto contentOption = option;
	contentOption.rect = option.rect.adjusted(CardInset, CardInset, -CardInset, -CardInset);
	contentOption.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus | QStyle::State_MouseOver);
	if (option.state.testFlag(QStyle::State_Selected))
	{
		contentOption.palette.setColor(QPalette::Text, m_colors.selectedText);
	}

	QStyledItemDelegate::paint(painter, contentOption, index);

	if (index.isValid() && index.model())
	{
		const auto controlInterface = index.model()->data(index, ComputerControlListModel::ControlInterfaceRole).value<ComputerControlInterface::Pointer>();

		if (controlInterface)
		{
			if (const auto label = stateLabel(controlInterface); label.isEmpty() == false)
			{
				drawStateBadge(painter, contentOption, index, label);
			}
		}

		drawFeatureIcons(painter, contentOption.rect.topLeft(), controlInterface);
	}
}



QSize ComputerItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt = option;
	const auto style = QApplication::style();

	if (m_fullLengthLabels && index.model())
	{
		QSize maxSize;
		for (int i = 0; i < index.model()->rowCount(); ++i)
		{
			initStyleOption(&opt, index.model()->index(i, 0));
			const auto rowSize = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), nullptr);
			maxSize = maxSize.expandedTo(rowSize);
		}
		return maxSize + QSize(2 * CardInset, 2 * CardInset);
	}

	initStyleOption(&opt, index);
	const auto size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), nullptr);

	return QSize(std::min(size.width(), option.decorationSize.width()), size.height()) + QSize(2 * CardInset, 2 * CardInset);
}



ComputerItemDelegate::CardColors ComputerItemDelegate::cardColors()
{
	// visual-rules.md tokens: border, primary/selection accent, selection fill, primary text, surface
	if (VeyonCore::useDarkMode())
	{
		return {QColor(0x46586D), QColor(0x39C6C5), QColor(0x183D47), QColor(0xF1F5F9),
				QColor(0x14, 0x22, 0x35, 235), QColor(0xF1F5F9)};
	}

	return {QColor(0xCBD5E1), QColor(0x087F8C), QColor(0xE3F5F5), QColor(0x10243A),
			QColor(0xFF, 0xFF, 0xFF, 235), QColor(0x10243A)};
}



QString ComputerItemDelegate::stateLabel(const ComputerControlInterface::Pointer& controlInterface)
{
	// short text for the existing state images so state is never shown by the symbol alone
	switch (controlInterface->state())
	{
	case ComputerControlInterface::State::Connected:
		if (controlInterface->hasValidFramebuffer())
		{
			return {};
		}
		[[fallthrough]];
	case ComputerControlInterface::State::Connecting:
		return tr("Connecting…");
	case ComputerControlInterface::State::HostOffline:
		return tr("Offline");
	case ComputerControlInterface::State::HostNameResolutionFailed:
		return tr("Name not resolved");
	case ComputerControlInterface::State::ServerNotRunning:
		return tr("Student Agent not reachable");
	case ComputerControlInterface::State::AuthenticationFailed:
	case ComputerControlInterface::State::AccessControlFailed:
		return tr("Access denied");
	default:
		break;
	}

	return tr("Disconnected");
}



void ComputerItemDelegate::drawCard(QPainter* painter, const QStyleOptionViewItem& option) const
{
	const auto selected = option.state.testFlag(QStyle::State_Selected);
	const auto hovered = option.state.testFlag(QStyle::State_MouseOver);
	const auto cardRect = QRectF(option.rect).adjusted(FocusMargin, FocusMargin, -FocusMargin, -FocusMargin);

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	if (option.state.testFlag(QStyle::State_HasFocus))
	{
		// 2 px focus ring with 2 px offset outside the card
		painter->setPen(QPen(m_colors.accent, 2, Qt::DotLine));
		painter->setBrush(Qt::NoBrush);
		painter->drawRoundedRect(QRectF(option.rect).adjusted(1, 1, -1, -1), CardRadius + FocusMargin - 1, CardRadius + FocusMargin - 1);
	}

	const auto borderWidth = selected ? 2.0 : 1.0;
	painter->setPen(QPen(selected || hovered ? m_colors.accent : m_colors.border, borderWidth));
	painter->setBrush(selected ? QBrush(m_colors.selectedFill) : QBrush(Qt::NoBrush));
	painter->drawRoundedRect(cardRect.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2),
							 CardRadius, CardRadius);

	painter->restore();
}



void ComputerItemDelegate::drawStateBadge(QPainter* painter, const QStyleOptionViewItem& contentOption,
										  const QModelIndex& index, const QString& label) const
{
	// only used for state images (not live screens), so the extra style-option setup stays cheap
	auto opt = contentOption;
	initStyleOption(&opt, index);
	const auto style = opt.widget ? opt.widget->style() : QApplication::style();
	const auto decorationRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);

	const QFontMetrics fontMetrics(opt.font);
	const auto text = fontMetrics.elidedText(label, Qt::ElideRight,
											  decorationRect.width() - 2 * (BadgeMargin + BadgePaddingX));
	if (text.isEmpty() || text == QStringLiteral("…"))
	{
		return;
	}

	QRect badgeRect(0, 0, fontMetrics.horizontalAdvance(text) + 2 * BadgePaddingX, fontMetrics.height() + 2 * BadgePaddingY);
	badgeRect.moveCenter(QPoint(decorationRect.center().x(),
								decorationRect.bottom() - BadgeMargin - badgeRect.height() / 2));
	if (badgeRect.top() < decorationRect.top())
	{
		return;
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(m_colors.border);
	painter->setBrush(m_colors.badgeFill);
	painter->drawRoundedRect(QRectF(badgeRect).adjusted(0.5, 0.5, -0.5, -0.5), BadgeRadius, BadgeRadius);
	painter->setFont(opt.font);
	painter->setPen(m_colors.badgeText);
	painter->drawText(badgeRect, Qt::AlignCenter, text);
	painter->restore();
}



void ComputerItemDelegate::initFeaturePixmaps()
{
	for (const auto& feature : VeyonCore::featureManager().features() )
	{
		if (feature.testFlag(Feature::Flag::Master) && !feature.iconUrl().isEmpty())
		{
			m_featurePixmaps[feature.uid()] = QIcon(feature.iconUrl()).pixmap(QSize(OverlayIconSize, OverlayIconSize));
		}
	}
}



void ComputerItemDelegate::drawFeatureIcons(QPainter* painter, const QPoint& pos, ComputerControlInterface::Pointer controlInterface) const
{
	if (painter &&
		controlInterface &&
		controlInterface->state() == ComputerControlInterface::State::Connected)
	{
		auto count = 0;
		for (const auto& feature : controlInterface->activeFeatures())
		{
			if (m_featurePixmaps.contains(feature))
			{
				count++;
			}
		}

		if (count == 0)
		{
			return;
		}

		int x = pos.x() + OverlayIconsPadding;
		const int y = pos.y() + OverlayIconsPadding;

		painter->setRenderHint(QPainter::Antialiasing);
		painter->setBrush(QColor(255, 255, 255, 192));
		painter->setPen(m_colors.accent);
		painter->drawRoundedRect(QRect(x, y, count * (OverlayIconSize + OverlayIconSpacing), OverlayIconSize),
								 OverlayIconsRadius, OverlayIconsRadius);

		for (const auto& feature : controlInterface->activeFeatures())
		{
			const auto it = m_featurePixmaps.find(feature);
			if (it != m_featurePixmaps.constEnd())
			{
				painter->drawPixmap(QPoint(x, y), *it);
				x += OverlayIconSize + OverlayIconSpacing;
			}
		}
	}
}
