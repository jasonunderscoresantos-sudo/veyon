/*
 * FileCollectDialog.cpp - implementation of FileCollectDialog
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

#include <QDesktopServices>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>

#include "FileCollectController.h"
#include "FileCollectDialog.h"
#include "FileCollectTreeModel.h"
#include "FileSystemBrowser.h"
#include "Filesystem.h"
#include "ProgressItemDelegate.h"
#include "TeacherActionIcons.h"

#include "ui_FileCollectDialog.h"

FileCollectDialog::FileCollectDialog(FileCollectController* controller, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::FileCollectDialog),
	m_controller(controller),
	m_model(nullptr)
{
	ui->setupUi(this);
	// V3ax: the model draws its transfer-state icons from the tree view's palette at 20 logical px, so it is
	// created once setupUi() has created the view
	m_model = new FileCollectTreeModel(controller, ui->collectionsTreeView->palette(), this);
	ui->collectionsTreeView->setIconSize(QSize(20, 20));
	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Start"));
	ui->browseDestinationDirectoryButton->setIcon(TeacherActionIcons::forSecondaryControl(TeacherActionIcons::SecondaryControl::CollectFilesOpenDestination, ui->browseDestinationDirectoryButton->palette()));
	ui->browseDestinationDirectoryButton->setIconSize(QSize(20, 20));

	ui->collectionsTreeView->setItemDelegateForColumn(int(FileCollectTreeModel::Column::Progress),
													  new ProgressItemDelegate(ui->collectionsTreeView));
	ui->collectionsTreeView->setModel(m_model);

	// Initialize source directory from controller / default config
	ui->sourceDirectoryEdit->setText(m_controller->collectSourceDirectory());

	// Initialize file pattern
	ui->filePatternEdit->setText(m_controller->filePattern());

	// Initialize destination directory
	ui->destinationDirectoryEdit->setText(m_controller->destinationDirectory());

	connect (ui->browseDestinationDirectoryButton, &QAbstractButton::clicked, this, [this]() {
		const auto dir = QFileDialog::getExistingDirectory(this, tr("Select destination directory"),
														   ui->destinationDirectoryEdit->text());
		if (dir.isEmpty() == false)
		{
			ui->destinationDirectoryEdit->setText(dir);
			m_controller->setDestinationDirectory(dir);
		}
	});

	connect (m_controller, &FileCollectController::overallProgressChanged, this, [this]() {
		ui->progressBar->setValue(m_controller->overallProgress());
	});

	connect (m_controller, &FileCollectController::started, this, [this]() {
		// disable settings during transfer
		ui->settingsGroupBox->setEnabled(false);

		ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
	});

	connect (m_controller, &FileCollectController::finished, this, [this]() {
		ui->buttonBox->setStandardButtons(QDialogButtonBox::Open | QDialogButtonBox::Close);

		ui->buttonBox->button(QDialogButtonBox::Open)->setText(tr("Open output directory"));
		ui->buttonBox->button(QDialogButtonBox::Open)->setIcon(TeacherActionIcons::forSecondaryControl(TeacherActionIcons::SecondaryControl::CollectFilesOpenDestination, ui->buttonBox->button(QDialogButtonBox::Open)->palette()));
		ui->buttonBox->button(QDialogButtonBox::Open)->setIconSize(QSize(20, 20));
	});

	const auto availableSize = screen()->availableSize();
	move(availableSize.width() / 8, availableSize.height() / 8);
	resize(availableSize * 3 / 4);
}



FileCollectDialog::~FileCollectDialog()
{
	delete ui;
}



void FileCollectDialog::start()
{
	switch (m_controller->collectionDirectory())
	{
	case FileCollectController::CollectionDirectory::PromptUserForName:
	{
		const auto collectionName = QInputDialog::getText(this, tr("Enter collection name" ),
														  tr("Please enter a name for this file collection:"));
		if (collectionName.isEmpty())
		{
			return;
		}

		m_controller->setCollectionName(collectionName);
		break;
	}
	case FileCollectController::CollectionDirectory::DateTimeSubdirectory:
		m_controller->setCollectionName(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
		break;
	default:
		break;
	}

	// Pass source directory and file pattern to controller
	const auto sourceDir = ui->sourceDirectoryEdit->text().trimmed();
	if (sourceDir.isEmpty() == false)
	{
		m_controller->setCollectSourceDirectory(sourceDir);
	}

	const auto filePattern = ui->filePatternEdit->text().trimmed();
	if (filePattern.isEmpty() == false)
	{
		m_controller->setFilePattern(filePattern);
	}

	if (VeyonCore::filesystem().ensurePathExists(m_controller->outputDirectory()) == false)
	{
		QMessageBox::critical(this, tr("Output directory creation failed"),
							  tr("The output directory \"%1\" does not exist and could not be created. "
								 "Please check the configuration and the file permissions for the configured destination directory.")
							  .arg(m_controller->outputDirectory()));
		return;
	}

	m_controller->start();
}



void FileCollectDialog::openOutputDirectory()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(m_controller->outputDirectory() + QDir::separator()));

}



void FileCollectDialog::accept()
{
	if (ui->buttonBox->standardButtons().testFlag(QDialogButtonBox::Open))
	{
		openOutputDirectory();
	}
	else
	{
		start();
	}
}



void FileCollectDialog::reject()
{
	if (m_controller->isRunning())
	{
		m_controller->stop();
		ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
	}
	else
	{
		QDialog::reject();
	}
}
