OSPRAY_CONFIGURE_MPI()

# simple display-wall driver
OPTION(OSPRAY_MODULE_WALL "Wall" ON)

IF (NOT OSPRAY_MODULE_MPI)
  MESSAGE(WARNING "Need OPRAY_MODULE_MPI turned on to build OSPRAY_MODULE_WALL")
  SET(OSPRAY_MODULE_WALL OFF)
ENDIF()

IF (OSPRAY_MODULE_WALL)

  #SET(OSPRAY_DISPLAY_WALD_VERSION_MAJOR 0)
  #SET(OSPRAY_DISPLAY_WALD_VERSION_MINOR 1)
  #SET(OSPRAY_DISPLAY_WALD_VERSION_PATCH 0)

  #SET(OSPRAY_DISPLAY_WALD_VERSION
    #${OSPRAY_DISPLAY_WALD_VERSION_MAJOR}.${OSPRAY_DISPLAY_WALD_VERSION_MINOR}.${OSPRAY_DISPLAY_WALD_VERSION_PATCH})


  SET(OSPRAY_DEFAULT_COMPONENT wall)
  OSPRAY_BUILD_COMPONENT(mpiCommon)

  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/ospray/include)
  INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
  
  ## cppzmq
  #FIND_PACKAGE(cppzmq)
  #IF(cppzmq_FOUND)
    #INCLUDE_DIRECTORIES(${ZeroMQ_INCLUDE_DIR} ${cppzmq_INCLUDE_DIR})
  #ENDIF()


  #LIST(APPEND LIBS ${cppzmq_LIBRARY} ${ZeroMQ_LIBRARY})
  LIST(APPEND LIBS)
  MESSAGE(STATUS "LIBS ${LIBS}")

    # the ospray pixelop one can use to get a ospray frame buffer to the
  # display wall
  ADD_SUBDIRECTORY(ospray)

  #SET(DISPLAY_WALD_DIR  "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "" FORCE)

ENDIF()
