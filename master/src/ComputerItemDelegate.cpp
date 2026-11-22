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
#include <QStyleOptionButton>

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

	if (index.isValid() && index.model() && painter)
	{
		const auto controlInterface = index.model()->data(index, ComputerControlListModel::ControlInterfaceRole).value<ComputerControlInterface::Pointer>();
		const auto preview = previewRect(option);

		drawThumbnail(painter, preview, index);
		drawSelectionIndicator(painter, option);
		drawFooter(painter, option, preview, index, controlInterface);
		drawFeatureIcons(painter, preview.topLeft(), controlInterface);
	}
}



QSize ComputerItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto opt = option;
	initStyleOption(&opt, index);
	const auto previewSize = option.decorationSize.isValid() ? option.decorationSize : opt.decorationSize;

	auto nameFont = opt.font;
	nameFont.setPixelSize(NameTextSize);
	nameFont.setWeight(QFont::DemiBold);
	auto statusFont = opt.font;
	statusFont.setPixelSize(StatusTextSize);
	const QFontMetrics nameMetrics(nameFont);
	const QFontMetrics statusMetrics(statusFont);

	auto cardWidth = previewSize.width() + 2 * CardPadding;

	if (m_fullLengthLabels && index.model())
	{
		for (int i = 0; i < index.model()->rowCount(); ++i)
		{
			const auto rowIndex = index.model()->index(i, 0);
			const auto controlInterface = rowIndex.data(ComputerControlListModel::ControlInterfaceRole)
									  .value<ComputerControlInterface::Pointer>();
			const auto name = controlInterface ? controlInterface->computerName() : rowIndex.data(Qt::DisplayRole).toString();
			const auto status = controlInterface ? footerLabel(controlInterface) : QString{};
			cardWidth = std::max(cardWidth, std::max(nameMetrics.horizontalAdvance(name),
										 statusMetrics.horizontalAdvance(status)) + 2 * FooterPadding +
										 SelectionIndicatorSize + SelectionIndicatorGap);
		}
	}

	const auto footerHeight = 2 * FooterPadding + nameMetrics.height() + FooterLineGap + statusMetrics.height();
	const auto cardHeight = CardPadding + previewSize.height() + footerHeight;

	return QSize(cardWidth + 2 * FocusMargin, cardHeight + 2 * FocusMargin);
}



QRect ComputerItemDelegate::selectionIndicatorRect(const QStyleOptionViewItem& option) const
{
	const auto card = option.rect.adjusted(FocusMargin, FocusMargin, -FocusMargin, -FocusMargin);
	const auto preview = previewRect(option);
	auto nameFont = option.font;
	nameFont.setPixelSize(NameTextSize);
	nameFont.setWeight(QFont::DemiBold);
	const QFontMetrics nameMetrics(nameFont);
	const auto nameLineTop = preview.bottom() + 1 + FooterPadding;

	return QRect(card.left() + FooterPadding,
				 nameLineTop + (nameMetrics.height() - SelectionIndicatorSize) / 2,
				 SelectionIndicatorSize, SelectionIndicatorSize);
}



ComputerItemDelegate::CardColors ComputerItemDelegate::cardColors()
{
	// visual-rules.md tokens: Surface, Subtle surface, Border, Focus, Selection and text
	if (VeyonCore::useDarkMode())
	{
		return {QColor(0x142235), QColor(0x1C2E43), QColor(0x5A7188), QColor(0x39C6C5),
				QColor(0x183D47), QColor(0xF1F5F9), QColor(0xB2C0CF), QColor(0xF1F5F9)};
	}

	return {QColor(0xFFFFFF), QColor(0xEAF0F5), QColor(0x7F91A5), QColor(0x087F8C),
			QColor(0xE3F5F5), QColor(0x10243A), QColor(0x526477), QColor(0x10243A)};
}



QString ComputerItemDelegate::stateLabel(const ComputerControlInterface::Pointer& controlInterface)
{
	switch (controlInterface->state())
	{
	case ComputerControlInterface::State::Connected:
		return controlInterface->hasValidFramebuffer() ? tr("Online") : tr("Connecting…");
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



QString ComputerItemDelegate::userLabel(const ComputerControlInterface::Pointer& controlInterface)
{
	if (controlInterface->userFullName().isEmpty() == false)
	{
		return controlInterface->userFullName();
	}

	if (controlInterface->userLoginName().isEmpty() == false)
	{
		return VeyonCore::stripDomain(controlInterface->userLoginName());
	}

	return tr("No user logged on");
}



QString ComputerItemDelegate::footerLabel(const ComputerControlInterface::Pointer& controlInterface)
{
	if (controlInterface->state() == ComputerControlInterface::State::Connected &&
		controlInterface->hasValidFramebuffer())
	{
		return tr("Online · %1").arg(userLabel(controlInterface));
	}

	return stateLabel(controlInterface);
}



void ComputerItemDelegate::drawCard(QPainter* painter, const QStyleOptionViewItem& option) const
{
	if (painter == nullptr)
	{
		return;
	}

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
	painter->setBrush(selected ? QBrush(m_colors.selectedFill) :
					  hovered ? QBrush(m_colors.subtleSurface) : QBrush(m_colors.surface));
	painter->drawRoundedRect(cardRect.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2),
							 CardRadius, CardRadius);

	painter->restore();
}



QRect ComputerItemDelegate::previewRect(const QStyleOptionViewItem& option) const
{
	const auto card = option.rect.adjusted(FocusMargin, FocusMargin, -FocusMargin, -FocusMargin);
	QRect preview(QPoint{}, option.decorationSize);
	preview.moveTop(card.top() + CardPadding);
	preview.moveLeft(card.center().x() - preview.width() / 2);
	return preview;
}



void ComputerItemDelegate::drawThumbnail(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
	const auto image = index.data(Qt::DecorationRole).value<QImage>();
	if (painter == nullptr || image.isNull() || rect.isEmpty())
	{
		return;
	}

	painter->save();
	painter->drawImage(rect, image, image.rect());
	painter->restore();
}



void ComputerItemDelegate::drawSelectionIndicator(QPainter* painter, const QStyleOptionViewItem& option) const
{
	if (painter == nullptr)
	{
		return;
	}

	QStyleOptionButton checkBoxOption;
	if (option.widget)
	{
		checkBoxOption.initFrom(option.widget);
	}
	checkBoxOption.palette = option.palette;
	checkBoxOption.direction = option.direction;
	checkBoxOption.rect = selectionIndicatorRect(option);
	checkBoxOption.state = QStyle::State_Enabled |
		(option.state.testFlag(QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off);

	const auto style = option.widget ? option.widget->style() : QApplication::style();
	style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkBoxOption, painter, option.widget);
}



void ComputerItemDelegate::drawFooter(QPainter* painter, const QStyleOptionViewItem& option, const QRect& preview,
									  const QModelIndex& index,
									  const ComputerControlInterface::Pointer& controlInterface) const
{
	if (painter == nullptr)
	{
		return;
	}

	const auto card = option.rect.adjusted(FocusMargin, FocusMargin, -FocusMargin, -FocusMargin);
	const QRect footer(card.left() + FooterPadding, preview.bottom() + 1 + FooterPadding,
					   card.width() - 2 * FooterPadding, card.bottom() - preview.bottom() - 2 * FooterPadding);
	if (footer.isEmpty())
	{
		return;
	}

	auto nameFont = option.font;
	nameFont.setPixelSize(NameTextSize);
	nameFont.setWeight(QFont::DemiBold);
	auto statusFont = option.font;
	statusFont.setPixelSize(StatusTextSize);
	const QFontMetrics nameMetrics(nameFont);
	const QFontMetrics statusMetrics(statusFont);
	const auto textInset = SelectionIndicatorSize + SelectionIndicatorGap;
	const QRect textRect(footer.left() + textInset, footer.top(), footer.width() - textInset, footer.height());

	const auto computerName = controlInterface ? controlInterface->computerName() : index.data(Qt::DisplayRole).toString();
	const auto status = controlInterface ? footerLabel(controlInterface) : QString{};
	const auto nameText = m_fullLengthLabels ? computerName : nameMetrics.elidedText(computerName, Qt::ElideRight, textRect.width());
	const auto statusText = m_fullLengthLabels ? status : statusMetrics.elidedText(status, Qt::ElideRight, textRect.width());
	const auto selected = option.state.testFlag(QStyle::State_Selected);

	painter->save();
	painter->setFont(nameFont);
	painter->setPen(selected ? m_colors.selectedText : m_colors.primaryText);
	painter->drawText(QRect(textRect.left(), textRect.top(), textRect.width(), nameMetrics.height()),
					  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, nameText);
	painter->setFont(statusFont);
	painter->setPen(selected ? m_colors.selectedText : m_colors.secondaryText);
	painter->drawText(QRect(textRect.left(), textRect.top() + nameMetrics.height() + FooterLineGap,
							textRect.width(), statusMetrics.height()),
					  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, statusText);
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
