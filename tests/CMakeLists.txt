# ------------------------------------------------------------------
# simple test tool that connects to a display all and renders a test
# frame
# ------------------------------------------------------------------

add_subdirectory(shared_libs)

ADD_EXECUTABLE(ospDwTestMain
  testMain.cpp
  )
TARGET_LINK_LIBRARIES(ospDwTestMain
  ospray_moduleWall_client
  ospray_wall_common
  )

ADD_EXECUTABLE(ospDwTestOSPMain 
    testOSPRay.cpp)
TARGET_LINK_LIBRARIES(ospDwTestOSPMain
    ospray_moduleWall_client
    ospray_wall_common
    shared_libs)

ADD_EXECUTABLE(testSender
    testSender.cpp
    )

TARGET_LINK_LIBRARIES(testSender
    ${LIBS}
    ospray_wall_common
    )

ADD_EXECUTABLE(testReceiver
    testReceiver.cpp
    )

TARGET_LINK_LIBRARIES(testReceiver
    ${LIBS}
    ospray_wall_common
    )
