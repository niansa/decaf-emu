#include <glbinding/Binding.h>
#include <glbinding/Meta.h>
#include "mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
   : QMainWindow(parent)
{
   ui.setupUi(this);
}


static std::string
getPathBasename(const std::string &path)
{
   auto pos = path.find_last_of("/\\");

   if (!pos) {
      return path;
   } else {
      return path.substr(pos + 1);
   }
}

void MainWindow::actOpen()
{
   auto gamePath = QFileDialog::getOpenFileName(this, tr("Open File"), QString(), tr("Wii U Executable (*.rpx)"));

   if (gamePath.isEmpty()) {
      return;
   }

   auto logFile = getPathBasename(gamePath.toStdString());
   auto logLevel = spdlog::level::info;
   std::vector<spdlog::sink_ptr> sinks;
   sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_st>(logFile, "txt", 23, 59, true));

   // Initialise libdecaf logger
   decaf::initialiseLogging(sinks, logLevel);

   // Create OpenGL graphics driver
   mGraphicsDriver = decaf::createGLDriver();
   decaf::setGraphicsDriver(mGraphicsDriver);

   // Set input provider
   decaf::setInputDriver(this);
   decaf::addEventListener(this);

   decaf::setClipboardTextCallbacks(
      []() -> const char * {
         return "";
      },
      [](const char *text) {
      });

   // Initialise emulator
   if (!decaf::initialise(gamePath.toStdString())) {
      return;
   }

   decaf::debugger::initialise();

   mGraphicsThread = new GraphicsThread(this, mGraphicsDriver, ui.centralWidget->context());
   mGraphicsThread->start();

   // Start emulator
   decaf::start();
}

void MainWindow::actExit()
{
}

#include <QOpenGLContext>
#include <common/log.h>
static std::string
getGlDebugSource(gl::GLenum source)
{
   switch (source) {
   default:
      return glbinding::Meta::getString(source);
   }
}

static std::string
getGlDebugType(gl::GLenum severity)
{
   switch (severity) {
   default:
      return glbinding::Meta::getString(severity);
   }
}

static std::string
getGlDebugSeverity(gl::GLenum severity)
{
   switch (severity) {
   default:
      return glbinding::Meta::getString(severity);
   }
}

static void GL_APIENTRY
debugMessageCallback(gl::GLenum source, gl::GLenum type, gl::GLuint id, gl::GLenum severity,
                     gl::GLsizei length, const gl::GLchar* message, const void *userParam)
{
   for (auto filterID : decaf::config::gpu::debug_filters) {
      if (filterID == id) {
         return;
      }
   }

   auto outputStr = fmt::format("GL Message ({}, {}, {}, {}) {}", id,
                                getGlDebugSource(source),
                                getGlDebugType(type),
                                getGlDebugSeverity(severity),
                                message);

   if (severity == GL_DEBUG_SEVERITY_HIGH) {
      gLog->warn(outputStr);
   } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
      gLog->debug(outputStr);
   } else if (severity == GL_DEBUG_SEVERITY_LOW) {
      gLog->trace(outputStr);
   } else if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
      gLog->info(outputStr);
   } else {
      gLog->info(outputStr);
   }
}

void
GraphicsThread::run()
{
   // Create GL context
   QOpenGLContext context;
   //context.setShareContext(QOpenGLContext::globalShareContext());
   context.create();

   auto group = context.shareGroup();
   auto list = group->shares();

   for (auto item : list) {
      gLog->debug("GraphicsThread Shares: {}", reinterpret_cast<std::uintptr_t>(item));
   }

   // let's go!
   context.makeCurrent(mWindowContext->surface());
   glbinding::Binding::initialize();

   glbinding::setCallbackMaskExcept(glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue, { "glGetError" });
   glbinding::setAfterCallback([](const glbinding::FunctionCall &call) {
      auto error = glbinding::Binding::GetError.directCall();

      if (error != GL_NO_ERROR) {
         fmt::MemoryWriter writer;
         writer << call.function->name() << "(";

         for (unsigned i = 0; i < call.parameters.size(); ++i) {
            writer << call.parameters[i]->asString();
            if (i < call.parameters.size() - 1)
               writer << ", ";
         }

         writer << ")";

         if (call.returnValue) {
            writer << " -> " << call.returnValue->asString();
         }

         gLog->error("OpenGL error: {} with {}", glbinding::Meta::getString(error), writer.str());
      }
   });
   mGraphicsDriver->run();
}

// VPAD
vpad::Type
MainWindow::getControllerType(vpad::Channel channel)
{
   return vpad::Type::DRC;
}

ButtonStatus
MainWindow::getButtonStatus(vpad::Channel channel, vpad::Core button)
{
   return ButtonStatus::ButtonReleased;
}

float
MainWindow::getAxisValue(vpad::Channel channel, vpad::CoreAxis axis)
{
   return 0.0f;
}

bool
MainWindow::getTouchPosition(vpad::Channel channel, vpad::TouchPosition &position)
{
   return false;
}

// WPAD
wpad::Type
MainWindow::getControllerType(wpad::Channel channel)
{
   return wpad::Type::Disconnected;
}

ButtonStatus
MainWindow::getButtonStatus(wpad::Channel channel, wpad::Core button)
{
   return ButtonStatus::ButtonReleased;
}

ButtonStatus
MainWindow::getButtonStatus(wpad::Channel channel, wpad::Classic button)
{
   return ButtonStatus::ButtonReleased;
}

ButtonStatus
MainWindow::getButtonStatus(wpad::Channel channel, wpad::Nunchuck button)
{
   return ButtonStatus::ButtonReleased;
}

ButtonStatus
MainWindow::getButtonStatus(wpad::Channel channel, wpad::Pro button)
{
   return ButtonStatus::ButtonReleased;
}

float
MainWindow::getAxisValue(wpad::Channel channel, wpad::NunchuckAxis axis)
{
   return 0.0f;
}

float
MainWindow::getAxisValue(wpad::Channel channel, wpad::ProAxis axis)
{
   return 0.0f;
}

// Events
void
MainWindow::onGameLoaded(const decaf::GameInfo &info)
{
}
