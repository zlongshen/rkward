INCLUDE(FindR.cmake)
IF(NOT WIN32)
	# for pthread_at_fork
	FIND_PACKAGE(Threads)
ENDIF(NOT WIN32)

ADD_SUBDIRECTORY( rpackages )
ADD_SUBDIRECTORY( rkwarddevice )

INCLUDE_DIRECTORIES( ${R_INCLUDEDIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )

SET (
	rbackend_shared_SRCS
	rkrbackendprotocol_shared.cpp
	rdata.cpp
)

SET (
	rbackend_backend_SRCS
	rkrbackend.cpp
	rksignalsupport.cpp
	rklocalesupport.cpp
	rkrsupport.cpp
	rkstructuregetter.cpp
	rkrbackendprotocol_backend.cpp
	rkreventloop.cpp
)

SET (
	rbackend_frontend_SRCS
	rinterface.cpp
	rcommand.cpp
	rcommandreceiver.cpp
	rcommandstack.cpp
	rkrbackendprotocol_frontend.cpp
	rksessionvars.cpp
)

SET (
	rbackend_FRONTEND_SRCS
	${rbackend_frontend_SRCS}
	${rbackend_shared_SRCS}
	rkfrontendtransmitter.cpp
	rktransmitter.cpp
)
QT4_AUTOMOC(${rbackend_FRONTEND_SRCS})
ADD_LIBRARY(rbackend STATIC ${rbackend_FRONTEND_SRCS})
TARGET_LINK_LIBRARIES(rbackend rkgraphicsdevice.frontend)

SET (
	rbackend_BACKEND_SRCS
	${rbackend_backend_SRCS}
	${rbackend_shared_SRCS}
	rkbackendtransmitter.cpp
	rktransmitter.cpp
)
QT4_AUTOMOC(${rbackend_BACKEND_SRCS})
ADD_DEFINITIONS (-DRKWARD_SPLIT_PROCESS)
LINK_DIRECTORIES(${R_SHAREDLIBDIR})
ADD_EXECUTABLE(rkward.rbackend ${rbackend_BACKEND_SRCS})
TARGET_LINK_LIBRARIES(rkward.rbackend rkgraphicsdevice.backend ${R_USED_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY} ${GETTEXT_LIBRARIES} )

IF(WIN32)
	# on Widows, we install to the rbackend subdirectory, because 1) LIBEXEC_INSTALL_DIR == BIN_INSTALL_DIR and 2) we don't want the backend to pick up
	# all the KDE library versions, first, when loading DLLs
	INSTALL(TARGETS rkward.rbackend DESTINATION ${BIN_INSTALL_DIR}/rbackend)
ELSE(WIN32)
		INSTALL(TARGETS rkward.rbackend DESTINATION ${RKWARD_FRONTEND_LOCATION})
ENDIF(WIN32)
