include(BuildVeyonPlugin)

build_veyon_plugin(filetransfer
	NAME FileTransfer
	DESCRIPTION "NextSpace Classroom Management - File Transfer Plugin"
	SOURCES
	FileTransferPlugin.cpp
	FileTransferPlugin.h
	FileCollection.h
	FileCollectController.h
	FileCollectController.cpp
	FileCollectDialog.ui
	FileCollectDialog.h
	FileCollectDialog.cpp
	FileCollectTreeModel.h
	FileCollectTreeModel.cpp
	FileCollectWorker.h
	FileCollectWorker.cpp
	FileTransferConfiguration.h
	FileTransferConfigurationPage.cpp
	FileTransferConfigurationPage.h
	FileTransferConfigurationPage.ui
	FileTransferController.cpp
	FileTransferController.h
	FileTransferListModel.cpp
	FileTransferListModel.h
	FileTransferDialog.cpp
	FileTransferDialog.h
	FileTransferDialog.ui
	FileTransferUserConfiguration.h
	FileReadThread.cpp
	FileReadThread.h
	ProgressItemDelegate.cpp
	ProgressItemDelegate.h
	filetransfer.qrc
	)

# Teacher action icons for the Collect Files destination actions (V3AM-058)
target_link_libraries(filetransfer PRIVATE nextspace-action-icons)
