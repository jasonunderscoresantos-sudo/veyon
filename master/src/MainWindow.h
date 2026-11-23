/*
 * MainWindow.h - main window of Veyon Master Application
 *
 * Copyright (c) 2004-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QMainWindow>

#include "ComputerControlInterface.h"

class QButtonGroup;
class QMenu;
class QToolButton;

class ToolButton;
class VeyonMaster;

namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow( VeyonMaster& masterCore, QWidget* parent = nullptr );
	~MainWindow() override;

	static bool initAuthentication();
	static bool initAccessControl();

	VeyonMaster& masterCore()
	{
		return m_master;
	}

	void reloadSubFeatures();

	ComputerControlInterfaceList selectedComputerControlInterfaces() const;

protected:
	void closeEvent( QCloseEvent* event ) override;
	bool eventFilter( QObject* object, QEvent* event ) override;
	void keyPressEvent( QKeyEvent *e ) override;


private Q_SLOTS:
	void showAboutDialog();
	void showOfflineHelp();

private:
	static int buttonId(const Feature& feature) // clazy:exclude=qt6-qhash-signature
	{
		return int(qHash(feature.uid()));
	}

	static constexpr const char* originalSizePropertyName()
	{
		return "originalSize";
	}

	static bool isMoreMenuFeature( const Feature& feature );
	QString toolBarLabel( const Feature& feature ) const;
	QString toolBarActiveLabel( const Feature& feature ) const;

	void addFeaturesToToolBar();
	void addSubFeaturesToToolButton( QToolButton* button, const Feature& parentFeature );
	void addSubFeatureActions( QMenu* menu, const Feature& parentFeature, const FeatureList& subFeatures,
							   QToolButton* modeButton );
	void reloadMoreMenu();

	void updateModeButtonGroup();

	void loadComputerPositions();
	void saveComputerPositions();

	Ui::MainWindow* ui;

	VeyonMaster& m_master;

	QButtonGroup* m_modeGroup;
	ToolButton* m_moreButton{nullptr};

} ;
