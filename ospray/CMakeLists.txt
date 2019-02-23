
OSPRAY_ADD_LIBRARY(ospray_module_wall SHARED
  OSPRayModuleWall.cpp
  )

TARGET_LINK_LIBRARIES(ospray_module_wall
  ospray_moduleWall_client
  ospray
  ospray_module_mpi
  )

#OSPRAY_CREATE_LIBRARY(ospray_module_displayWald
    #displayWald/displayWald.cpp
    #displayWald/displayWald.h
    
    #moduleInit.cpp
    
    #LINK
    #ospray
    #ospray_module_mpi
    #ospray_displayWald_client)


