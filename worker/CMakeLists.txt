include(BuildVeyonApplication)

build_veyon_application(veyon-worker
	NAME Worker
	DESCRIPTION "NextSpace Student Agent"
	WINDOWS_ICON resources/branding/nextspace-student-agent.ico
	NEXTSPACE_ROLE
	SOURCES
	src/FeatureWorkerManagerConnection.cpp
	src/FeatureWorkerManagerConnection.h
	src/main.cpp
	src/VeyonWorker.cpp
	src/VeyonWorker.h
	resources/worker.qrc
	)
