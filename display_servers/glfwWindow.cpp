/* 
   Copyright (c) 2016-2017 Ingo Wald

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include "glfwWindow.h"
#include <chrono>

namespace ospray {
  namespace dw {

    using std::endl;
    using std::cout;
    vec2f prev_mouse(-1);
    vec2f moveFrom(-1);
    vec2f moveTo(-1);
    float zoom = 0;
    int camera_changed = 0;

    Window::Window(const vec2i &size,
                         const vec2i &position,
                         const std::string &title,
                         bool doFullScreen, 
                         bool stereo,
                         int sock)
      : size(size),
        position(position),
        title(title),
        leftEye(NULL),
        rightEye(NULL),
        stereo(stereo),
        receivedFrameID(-1),
        displayedFrameID(-1),
        doFullScreen(doFullScreen),
        sock(sock)
    {
      create();
    }


    void Window::create()
    {
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
      
      if (doFullScreen) {
        auto *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        size = getScreenSize();
        glfwWindowHint(GLFW_AUTO_ICONIFY,false);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        
        this->handle = glfwCreateWindow(mode->width, mode->height,
                                  title.c_str(), monitor, nullptr);
      } else {
        glfwWindowHint(GLFW_DECORATED, 0);
        this->handle = glfwCreateWindow(size.x,size.y,title.c_str(),
                                  NULL,NULL);
        glfwSetWindowPos(this -> handle, position.x, position.y);

      }

      glfwMakeContextCurrent(this->handle);
      // glfwSetCursorPosCallback(this ->handle, cursorPosCallback);
    }

    void Window::setFrameBuffer(const uint32_t *leftEye, const uint32 *rightEye)
    {
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        this->leftEye = leftEye;
        this->rightEye = rightEye;
        receivedFrameID++;
        newFrameAvail.notify_one();
        // std::cout << "received frame id = " << receivedFrameID << std::endl;
      }
    }

    void Window::display() 
    {
      {
        std::unique_lock<std::mutex> lock(mutex);
        bool gotNewFrame 
          = newFrameAvail.wait_for(lock,std::chrono::milliseconds(100),
                                   [this](){return receivedFrameID > displayedFrameID; });
        // glfwShowWindow(window);

        if (!gotNewFrame)
          return;
        vec2i currentSize(0);
        glfwGetFramebufferSize(this->handle, &currentSize.x, &currentSize.y);

        glViewport(0, 0, currentSize.x, currentSize.y);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!leftEye) {
          /* invalid ... */
        } else {
          assert(rightEye == NULL);
          // no stereo
          // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          glDrawPixels(size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, leftEye);
        }
        glfwSwapBuffers(this->handle);
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        displayedFrameID++;
        newFrameDisplayed.notify_one();
      }
    }

    vec2i Window::getSize() const 
    { 
      return size; 
    }

    bool Window::doesStereo() const
    { 
      return stereo; 
    }
    
    void Window::run() 
    { 
      glfwSetCursorPosCallback(this ->handle, cursorPosCallback);
      while (!glfwWindowShouldClose(this->handle)) {
        glfwPollEvents();
        display();
        // send camera change status
        // std::cout << "camera changed " << camera_changed << std::endl;
        if(camera_changed == 1){
          // send 1
          int status = 1;
          send(sock, &status, 4, 0);
          // send camera moveFrom moveTo 
          send(sock, &moveFrom, sizeof(vec2f), 0);
          send(sock, &moveTo, sizeof(vec2f), 0);
          camera_changed = 0;
        }else if(camera_changed == 2){
          // send 2
          int status = 2;
          send(sock, &status, 4, 0);
          // send camera moveFrom moveTo 
          send(sock, &zoom, sizeof(float), 0);
          camera_changed = 0;
        }else if(camera_changed == 3){
          // send 3
          int status = 3;
          send(sock, &status, 4, 0);
          // send camera moveFrom moveTo 
          send(sock, &moveFrom, sizeof(vec2f), 0);
          send(sock, &moveTo, sizeof(vec2f), 0);
          camera_changed = 0;
        }else{
          // send 0
          int status = 0;
          int out = send(sock, &status, 4, 0);
          camera_changed = 0;
        }
      }
    }

    void cursorPosCallback(GLFWwindow *window, double x, double y){
      // WindowState *state = static_cast<WindowState*>(glfwGetWindowUserPointer(window));
      // WindowState *state = new WindowState();
        const vec2f mouse(x, y);
        if (prev_mouse != vec2f(-1)) {
          const bool leftDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
          const bool rightDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
          const bool middleDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
          const vec2f prev = prev_mouse;
          vec2i fbSize;
          glfwGetFramebufferSize(window, &fbSize.x, &fbSize.y);

          if (leftDown) {
            const vec2f mouseFrom(clamp(prev.x * 2.f / fbSize.x - 1.f,  -1.f, 1.f),
                                  clamp(1.f - 2.f * prev.y / fbSize.y, -1.f, 1.f));
            const vec2f mouseTo  (clamp(mouse.x * 2.f / fbSize.x - 1.f,  -1.f, 1.f),
                clamp(1.f - 2.f * mouse.y / fbSize.y, -1.f, 1.f));
            // state->camera.rotate(mouseFrom, mouseTo);
            camera_changed = 1;
            moveFrom = mouseFrom;
            moveTo = mouseTo;
            // std::cout << "camera rotation = " << mouseFrom << " " << mouseTo << std::endl;
          } else if (rightDown) {
            // state->camera.zoom(mouse.y - prev.y);
            camera_changed = 2;
            zoom = mouse.y - prev.y;
          } else if (middleDown) {
            const vec2f mouseFrom(clamp(prev.x * 2.f / fbSize.x - 1.f,  -1.f, 1.f),
                                  clamp(1.f - 2.f * prev.y / fbSize.y, -1.f, 1.f));
            const vec2f mouseTo   (clamp(mouse.x * 2.f / fbSize.x - 1.f,  -1.f, 1.f),
                clamp(1.f - 2.f * mouse.y / fbSize.y, -1.f, 1.f));
            const vec2f mouseDelta = mouseTo - mouseFrom;
            // state->camera.pan(mouseDelta);
            camera_changed = 3;
            moveFrom = mouseFrom;
            moveTo = mouseTo;
          }
        }
      prev_mouse = mouse;
    }

    
    
  } // ::ospray::dw
} // ::ospray
