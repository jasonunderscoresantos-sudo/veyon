/*
 * NextSpaceHelp.cpp - NextSpace offline help launcher
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

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include "NextSpaceHelp.h"


namespace
{

// the only two help files; both are fixed names in the help folder next to the running executable
QString installedHelpFile( const QString& fileName )
{
	return QDir( QCoreApplication::applicationDirPath() ).filePath( QStringLiteral("help/") + fileName );
}



QString translate( const char* text )
{
	return QCoreApplication::translate( "NextSpaceHelp", text );
}



bool showPlainText( QWidget* parent, const QString& textFile )
{
	if( QFileInfo( textFile ).isFile() == false )
	{
		return false;
	}

	QFile file( textFile );
	if( file.open( QFile::ReadOnly | QFile::Text ) == false )
	{
		return false;
	}
	const auto text = QString::fromUtf8( file.readAll() );
	file.close();

	QDialog dialog( parent );
	dialog.setWindowTitle( translate( "NextSpace Offline Help" ) );
	dialog.resize( 760, 600 );

	auto textView = new QPlainTextEdit( &dialog );
	textView->setReadOnly( true );
	textView->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
	textView->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
	textView->setAccessibleName( translate( "Offline help text" ) );
	textView->setPlainText( text );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close, &dialog );
	QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	auto layout = new QVBoxLayout( &dialog );
	layout->addWidget( textView );
	layout->addWidget( buttons );

	textView->setFocus();
	dialog.exec();

	return true;
}

}



void NextSpaceHelp::open( QWidget* parent )
{
	const auto htmlFile = installedHelpFile( QStringLiteral("index.html") );
	const auto textFile = installedHelpFile( QStringLiteral("quick-reference.txt") );

	if( QFileInfo( htmlFile ).isFile() &&
		QDesktopServices::openUrl( QUrl::fromLocalFile( htmlFile ) ) )
	{
		return;
	}

	if( showPlainText( parent, textFile ) )
	{
		return;
	}

	QMessageBox::warning( parent, translate( "NextSpace Offline Help" ),
						  translate( "The offline help could not be opened because its files are missing from the "
									 "help folder of the NextSpace Classroom Management installation.\n\n"
									 "Repair or reinstall NextSpace Classroom Management, or contact "
									 "support@edulearntechnologies.com for help." ) );
}
