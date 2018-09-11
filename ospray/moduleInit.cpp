#include <iostream>


namespace ospray{
    namespace dw{
        extern "C" void ospray_init_module_wall(){
            std::cout << "#osp: initializing wall module" << std::endl;
        }
    } //ospray::dw 
} // ospray

