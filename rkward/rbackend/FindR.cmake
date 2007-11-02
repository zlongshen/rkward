# find the R binary

MESSAGE(STATUS "Looking for R executable")
IF(R_EXECUTABLE)
	MESSAGE(STATUS "Specified by user")
ENDIF(R_EXECUTABLE)
FIND_PROGRAM(R_EXECUTABLE R)

IF(R_EXECUTABLE-NOTFOUND)
	MESSAGE(FATAL_ERROR "Could NOT find R (TODO: name option)")
ELSE(R_EXECUTABLE-NOTFOUND)
	MESSAGE(STATUS "Using R at ${R_EXECUTABLE}")
ENDIF(R_EXECUTABLE-NOTFOUND)

# find R_HOME

MESSAGE(STATUS "Looking for R_HOME")
IF(NOT R_HOME)
	EXEC_PROGRAM(${R_EXECUTABLE}
		ARGS CMD sh -c 'echo $R_HOME'
		OUTPUT_VARIABLE R_HOME)
ELSE(NOT R_HOME)
	MESSAGE(STATUS "Specified by user")
ENDIF(NOT R_HOME)
IF(NOT R_HOME)
	MESSAGE(FATAL_ERROR "Could NOT determine R_HOME (probably you misspecified the location of R)")
ELSE(NOT R_HOME)
	MESSAGE(STATUS "R_HOME is ${R_HOME}")
ENDIF(NOT R_HOME)

# find R include dir

MESSAGE(STATUS "Looking for R include files")
IF(NOT R_INCLUDEDIR)
	EXEC_PROGRAM(${R_EXECUTABLE}
		ARGS CMD sh -c 'echo $R_INCLUDE_DIR'
		OUTPUT_VARIABLE R_INCLUDEDIR)
ELSE(NOT R_INCLUDEDIR)
	MESSAGE(STATUS "Location specified by user")
ENDIF(NOT R_INCLUDEDIR)

IF(NOT R_INCLUDEDIR)
	SET(R_INCLUDEDIR ${R_HOME}/include)
	MESSAGE(STATUS "Not findable via R. Guessing")
ENDIF(NOT R_INCLUDEDIR)
MESSAGE(STATUS "Include files should be at ${R_INCLUDEDIR}. Checking for R.h")

FIND_FILE(R_H
	R.h
	PATHS ${R_INCLUDEDIR}
	NO_DEFAULT_PATH)
IF(NOT R_H)
	MESSAGE(FATAL_ERROR "Not found")
ELSE(NOT R_H)
	MESSAGE(STATUS "Found at ${R_H}")
	GET_FILENAME_COMPONENT(R_INCLUDEDIR ${R_H}
				PATH)
ENDIF(NOT R_H)

# check for existence of libR.so

MESSAGE(STATUS "Checking for existence of libR.so")
FIND_FILE(LIBR_SO
	libR.so
	PATHS ${R_HOME}/lib ${R_SHAREDLIBDIR}
	NO_DEFAULT_PATH)
IF(NOT LIBR_SO)
	MESSAGE(FATAL_ERROR "Not found. Make sure the location of R was detected correctly, above, and R was compiled with the --enable-shlib option")
ELSE(NOT LIBR_SO)
	MESSAGE(STATUS "Exists at ${LIBR_SO}")
	GET_FILENAME_COMPONENT(R_SHAREDLIBDIR ${LIBR_SO}
				PATH)
	SET(R_USED_LIBS R)
ENDIF(NOT LIBR_SO)

# for at least some versions of R, we seem to have to link against -lRlapack. Else loading some
# R packages will fail due to unresolved symbols. However, we can't do this unconditionally,
# as this is not available in some configurations of R

MESSAGE(STATUS "Checking whether we should link against libRlapack.so")
FIND_FILE(LIBR_LAPACK
	libRlapack.so
	PATHS ${R_SHAREDLIBDIR}
	NO_DEFAULT_PATH)
IF(NOT LIBR_LAPACK)
	MESSAGE(STATUS "No, ${R_SHAREDLIBDIR}/libRlapack.so does not exist")
ELSE(NOT LIBR_LAPACK)
	MESSAGE(STATUS "Yes, ${LIBR_LAPACK} exists")
	SET(R_USED_LIBS ${R_USED_LIBS} Rlapack gfortran)
ENDIF(NOT LIBR_LAPACK)

# for at least some versions of R, we seem to have to link against -lRblas. Else loading some
# R packages will fail due to unresolved symbols. However, we can't do this unconditionally,
# as this is not available in some configurations of R

MESSAGE(STATUS "Checking whether we should link against libRblas.so")
FIND_FILE(LIBR_BLAS
	libRblas.so
	PATHS ${R_SHAREDLIBDIR}
	NO_DEFAULT_PATH)
IF(NOT LIBR_BLAS)
	MESSAGE(STATUS "No, ${R_SHAREDLIBDIR}/libRblas.so does not exist")
ELSE(NOT LIBR_BLAS)
	MESSAGE(STATUS "Yes, ${LIBR_BLAS} exists")
	SET(R_USED_LIBS ${R_USED_LIBS} Rblas)
ENDIF(NOT LIBR_BLAS)



# find R package library location

MESSAGE(STATUS "Checking for R package library location to use")
IF(NOT R_LIBDIR)
	EXEC_PROGRAM(${R_EXECUTABLE}
		ARGS CMD sh -c 'echo $R_LIBS:$R_LIBS_SITE'
		OUTPUT_VARIABLE R_LIBDIR)
ELSE(NOT R_LIBDIR)
	MESSAGE(STATUS "Location specified by user")
ENDIF(NOT R_LIBDIR)

IF(NOT R_LIBDIR)
	MESSAGE(STATUS "Not reliably determined or specified. Guessing.")
	SET(R_LIBDIR ${R_HOME}/library)
ENDIF(NOT R_LIBDIR)

# strip whitespace
STRING(REGEX REPLACE "[ \n]+"
	"" R_LIBDIR
	"${R_LIBDIR}")

# strip leading colon(s)
STRING(REGEX REPLACE "^:+"
	"" R_LIBDIR
	"${R_LIBDIR}")

# find first path
STRING(REGEX REPLACE ":"
	" " R_LIBDIR
	"${R_LIBDIR}")

SET(R_LIBDIRS ${R_LIBDIR})
SEPARATE_ARGUMENTS(R_LIBDIRS)

SET(R_LIBDIR)
FOREACH(CURRENTDIR ${R_LIBDIRS})
	IF(NOT USE_R_LIBDIR)
		IF(EXISTS ${CURRENTDIR})
			SET(R_LIBDIR ${CURRENTDIR})
		ELSE(EXISTS ${CURRENTDIR})
			MESSAGE(STATUS "${CURRENTDIR} does not exist. Skipping")
		ENDIF(EXISTS ${CURRENTDIR})
	ENDIF(NOT USE_R_LIBDIR)
ENDFOREACH(CURRENTDIR ${R_LIBDIRS})

IF(NOT EXISTS ${R_LIBDIR})
	MESSAGE(FATAL_ERROR "No existing library location found")
ELSE(NOT EXISTS ${R_LIBDIR})
	MESSAGE(STATUS "Will use ${R_LIBDIR}")
ENDIF(NOT EXISTS ${R_LIBDIR})