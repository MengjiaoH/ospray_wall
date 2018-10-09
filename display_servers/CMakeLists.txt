find_package(OpenGL REQUIRED)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/apps/exampleViewer/common/glfw/include)

ADD_EXECUTABLE(ospWall
  main.cpp
  glfwWindow.cpp
  #Dispatcher.cpp
  processIncomingTiles.cpp
  Server.cpp
  )

TARGET_LINK_LIBRARIES(ospWall
  ospray_wall_common
  glfw
  ${OPENGL_LIBRARIES}
  ${LIBS}
  )

#ADD_EXECUTABLE(testZMQ_REP
#    testZMQ_REP.cpp
#  )

#TARGET_LINK_LIBRARIES(testZMQ_REP
#    ${LIBS}
#   ) 
