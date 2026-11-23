/*
 * TeacherConsoleUiConvergenceTest.cpp - focused V3cc card presentation checks
 *
 * This file is part of NextSpace Classroom Management, a modified version of Veyon.
 */

#include <QPalette>
#include <QPainter>
#include <QPixmapCache>
#include <QApplication>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QtTest>

#include "ComputerControlListModel.h"
#include "ComputerControlInterface.h"
#include "VeyonCore.h"

// Test the production helper results directly.  Access is widened only in this
// translation unit; the shipped class remains unchanged.
#define private public
#include "ComputerItemDelegate.h"
#undef private


class TeacherConsoleUiConvergenceTest : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void cardColorsFollowReceivingPalette()
	{
		QPalette palette;
		palette.setCurrentColorGroup( QPalette::Active );
		// ComputerMonitoringWidget intentionally overrides Base/Text with the
		// configured canvas colors. Cards must use unaffected receiving-palette
		// roles so those legacy overrides cannot replace the Surface/Text tokens.
		palette.setColor( QPalette::Active, QPalette::Base, QColor( 101, 102, 103 ) );
		palette.setColor( QPalette::Active, QPalette::Text, QColor( 104, 105, 106 ) );
		palette.setColor( QPalette::Active, QPalette::Button, QColor( 1, 2, 3 ) );
		palette.setColor( QPalette::Active, QPalette::ButtonText, QColor( 16, 17, 18 ) );
		palette.setColor( QPalette::Active, QPalette::AlternateBase, QColor( 4, 5, 6 ) );
		palette.setColor( QPalette::Active, QPalette::Mid, QColor( 7, 8, 9 ) );
		palette.setColor( QPalette::Active, QPalette::Link, QColor( 10, 11, 12 ) );
		palette.setColor( QPalette::Active, QPalette::Highlight, QColor( 13, 14, 15 ) );
		palette.setColor( QPalette::Active, QPalette::PlaceholderText, QColor( 19, 20, 21 ) );
		palette.setColor( QPalette::Active, QPalette::HighlightedText, QColor( 22, 23, 24 ) );

		const auto colors = ComputerItemDelegate::cardColors( palette );
		QCOMPARE( colors.surface, QColor( 1, 2, 3 ) );
		QCOMPARE( colors.subtleSurface, QColor( 4, 5, 6 ) );
		QCOMPARE( colors.border, QColor( 7, 8, 9 ) );
		QCOMPARE( colors.accent, QColor( 10, 11, 12 ) );
		QCOMPARE( colors.selectedFill, QColor( 13, 14, 15 ) );
		QCOMPARE( colors.primaryText, QColor( 16, 17, 18 ) );
		QCOMPARE( colors.secondaryText, QColor( 19, 20, 21 ) );
		QCOMPARE( colors.selectedText, QColor( 22, 23, 24 ) );
	}

	void cardGeometryRemainsLogicalAtScale_data()
	{
		QTest::addColumn<qreal>("devicePixelRatio");
		QTest::newRow("100 percent") << 1.0;
		QTest::newRow("125 percent") << 1.25;
		QTest::newRow("150 percent") << 1.5;
		QTest::newRow("200 percent") << 2.0;
	}

	void cardGeometryRemainsLogicalAtScale()
	{
		QFETCH(qreal, devicePixelRatio);

		ComputerItemDelegate delegate(false);
		QStyleOptionViewItem option;
		option.decorationSize = QSize(320, 180);
		option.state = QStyle::State_Enabled | QStyle::State_Selected;

		QStandardItemModel model(1, 1);
		const auto index = model.index(0, 0);
		const auto thumbnailColor = QColor( 203, 17, 91 );
		QImage thumbnail(option.decorationSize, QImage::Format_ARGB32_Premultiplied);
		thumbnail.fill(thumbnailColor);
		const auto control = ComputerControlInterface::Pointer::create(
			Computer( {}, QStringLiteral("LAB-01") ) );
		model.setData(index, thumbnail, Qt::DecorationRole);
		model.setData(index, QVariant::fromValue(control), ComputerControlListModel::ControlInterfaceRole);

		const auto logicalSize = delegate.sizeHint(option, index);
		QCOMPARE(logicalSize.width(), 340);

		option.rect = QRect(QPoint{}, logicalSize);
		const auto indicator = delegate.selectionIndicatorRect(option);
		QCOMPARE(indicator.size(), QSize(16, 16));
		QCOMPARE(indicator.left(), 16);
		QVERIFY(option.rect.adjusted(4, 4, -4, -4).contains(indicator));

		QImage image(QSize(qCeil(logicalSize.width() * devicePixelRatio),
						   qCeil(logicalSize.height() * devicePixelRatio)), QImage::Format_ARGB32_Premultiplied);
		image.setDevicePixelRatio(devicePixelRatio);
		image.fill(Qt::transparent);
		QPainter painter(&image);
		delegate.paint(&painter, option, index);
		painter.end();

		const auto renderedLogicalSize = image.deviceIndependentSize();
		const auto roundingTolerance = 1.0 / devicePixelRatio;
		QVERIFY(qAbs(renderedLogicalSize.width() - logicalSize.width()) < roundingTolerance);
		QVERIFY(qAbs(renderedLogicalSize.height() - logicalSize.height()) < roundingTolerance);

		const auto previewCenter = delegate.previewRect(option).center();
		const auto physicalPreviewCenter = QPoint(qFloor(previewCenter.x() * devicePixelRatio),
											qFloor(previewCenter.y() * devicePixelRatio));
		QCOMPARE(image.pixelColor(physicalPreviewCenter), thumbnailColor);
	}

	void disconnectedAndAccessDeniedCopyRemainDistinct()
	{
		auto control = ComputerControlInterface::Pointer::create( Computer( {}, QStringLiteral("LAB-01") ) );
		QCOMPARE( ComputerItemDelegate::stateLabel( control ), QStringLiteral("Disconnected") );

		control->setAccessControlFailed( QStringLiteral("test denial") );
		QCOMPARE( ComputerItemDelegate::stateLabel( control ), QStringLiteral("Access denied") );
		QCOMPARE( ComputerItemDelegate::footerLabel( control ), QStringLiteral("Access denied") );
	}

	void userCopyPrefersFullNameThenDomainStrippedLogin()
	{
		auto control = ComputerControlInterface::Pointer::create( Computer( {}, QStringLiteral("LAB-01") ) );
		control->setUserInformation( QStringLiteral("CLASSROOM\\student"), QStringLiteral("Student Name") );
		QCOMPARE( ComputerItemDelegate::userLabel( control ), QStringLiteral("Student Name") );

		control->setUserInformation( QStringLiteral("CLASSROOM\\student"), {} );
		QCOMPARE( ComputerItemDelegate::userLabel( control ), QStringLiteral("student") );

		control->setUserInformation( {}, {} );
		QCOMPARE( ComputerItemDelegate::userLabel( control ), QStringLiteral("No user logged on") );
	}
};


int main(int argc, char** argv)
{
	VeyonCore::setupApplicationParameters();
	QApplication app(argc, argv);
	// Keep configuration reads and any upgrade writes isolated from an installed
	// Veyon/NextSpace configuration while exercising real production objects.
	QCoreApplication::setOrganizationName(QStringLiteral("NextSpace Tests"));
	QCoreApplication::setApplicationName(QStringLiteral("V3cc Teacher Console UI Convergence"));
	VeyonCore core(&app, VeyonCore::Component::Master, QStringLiteral("V3ccTest"));

	TeacherConsoleUiConvergenceTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "TeacherConsoleUiConvergenceTest.moc"
