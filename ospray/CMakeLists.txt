
OSPRAY_ADD_LIBRARY(ospray_module_wall SHARED
  OSPRayModuleWall.cpp
  )
find_package(dwClient REQUIRED)

TARGET_LINK_LIBRARIES(ospray_module_wall
  ospray
  ospray_module_mpi
  dwClient::dwClient
  )


