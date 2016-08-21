#include <glbinding/Binding.h>
#include "mainwindow.h"
#include "renderwidget.h"
#include <QPainter>
#include <QPaintEngine>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVersionFunctions>
#include <QCoreApplication>

const int bubbleNum = 8;

RenderWidget::RenderWidget(QWidget* parent, Qt::WindowFlags f) :
   QOpenGLWidget(parent, f)
{
   setMinimumSize(300, 250);
}

RenderWidget::~RenderWidget()
{
   // And now release all OpenGL resources.
   makeCurrent();
   doneCurrent();
}

#include <common/log.h>

void RenderWidget::initializeGL()
{
   // auto funcs = reinterpret_cast<QOpenGLFunctions_4_5_Core *>(context()->versionFunctions(QOpenGLVersionProfile { ));
   mFunctions = new QOpenGLFunctions_4_5_Core {};
   mFunctions->initializeOpenGLFunctions();

   static auto vertexCode = R"(
      #version 420 core
      in vec2 fs_position;
      in vec2 fs_texCoord;
      out vec2 vs_texCoord;

      out gl_PerVertex {
         vec4 gl_Position;
      };

      void main()
      {
         vs_texCoord = fs_texCoord;
         gl_Position = vec4(fs_position, 0.0, 1.0);
      })";

   static auto pixelCode = R"(
      #version 420 core
      in vec2 vs_texCoord;
      out vec4 ps_color;
      uniform sampler2D sampler_0;

      void main()
      {
         ps_color = texture(sampler_0, vs_texCoord);
      })";

   static const GLfloat vertices[] = {
      -1.0f,  -1.0f,   0.0f, 1.0f,
      1.0f,  -1.0f,   1.0f, 1.0f,
      1.0f, 1.0f,   1.0f, 0.0f,

      1.0f, 1.0f,   1.0f, 0.0f,
      -1.0f, 1.0f,   0.0f, 0.0f,
      -1.0f,  -1.0f,   0.0f, 1.0f,
   };

   mVertexShader = new QOpenGLShader { QOpenGLShader::Vertex };
   mVertexShader->compileSourceCode(vertexCode);

   mFragmentShader = new QOpenGLShader { QOpenGLShader::Fragment };
   mFragmentShader->compileSourceCode(pixelCode);
   mFunctions->glBindFragDataLocation(mFragmentShader->shaderId(), 0, "ps_color");

   mShaderProgram = new QOpenGLShaderProgram { };
   mShaderProgram->addShader(mVertexShader);
   mShaderProgram->addShader(mFragmentShader);
   mShaderProgram->link();

   auto fs_position = mShaderProgram->attributeLocation("fs_position");
   auto fs_texCoord = mShaderProgram->attributeLocation("fs_texCoord");

   mVBO.create();
   mVBO.bind();
   mVBO.allocate(vertices, sizeof(vertices));
   mVBO.release();

   mVAO.create();
   mVBO.bind();
   mVAO.bind();
   mShaderProgram->setAttributeBuffer(fs_position, GL_FLOAT, 0, 2, 4 * sizeof(GLfloat));
   mShaderProgram->setAttributeBuffer(fs_texCoord, GL_FLOAT, 2, 2, 4 * sizeof(GLfloat));
   mShaderProgram->enableAttributeArray(fs_position);
   mShaderProgram->enableAttributeArray(fs_texCoord);
   mVAO.release();
   mVBO.release();

   mFunctions->glGenSamplers(1, &mSampler);
   mFunctions->glSamplerParameteri(mSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
   mFunctions->glSamplerParameteri(mSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
   mFunctions->glSamplerParameteri(mSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   mFunctions->glSamplerParameteri(mSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   glbinding::Binding::initialize();

   auto myContext = context();
   printf("test: %p", myContext);
}

void RenderWidget::paintGL()
{
   // Set up some needed GL state
   mFunctions->glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   mFunctions->glDisablei(GL_BLEND, 0);
   mFunctions->glDisable(GL_DEPTH_TEST);
   mFunctions->glDisable(GL_STENCIL_TEST);
   mFunctions->glDisable(GL_SCISSOR_TEST);
   mFunctions->glDisable(GL_CULL_FACE);

   mFunctions->glClearColor(0.6f, 0.2f, 0.2f, 1.0f);
   mFunctions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   mVAO.bind();
   mVBO.bind();
   mShaderProgram->bind();
   mFunctions->glBindSampler(0, mSampler);

   GLuint tvTexture = 0;
   GLuint drcTexture = 0;

   if (!mGraphicsDriver) {
      mGraphicsDriver = reinterpret_cast<decaf::OpenGLDriver *>(decaf::getGraphicsDriver());
   }

   if (mGraphicsDriver) {
      mGraphicsDriver->getSwapBuffers(&tvTexture, &drcTexture);

      if (tvTexture) {
         // Draw TV
         mFunctions->glBindTextureUnit(0, tvTexture);
         mFunctions->glViewportArrayv(0, 1, mTvViewport);
         mFunctions->glDrawArrays(GL_TRIANGLES, 0, 6);
      }

      if (drcTexture) {
         // Draw DRC
         mFunctions->glBindTextureUnit(0, drcTexture);
         mFunctions->glViewportArrayv(0, 1, mDrcViewport);
         mFunctions->glDrawArrays(GL_TRIANGLES, 0, 6);
      }
   }

   mShaderProgram->release();
   mVBO.release();
   mVAO.release();
   update();
}

void RenderWidget::resizeGL(int width, int height)
{
   // Recalculate TV / DRC positions
   updateViewports(width, height);
}

void RenderWidget::updateViewports(int windowWidth, int windowHeight)
{
   int TvWidth = 1280;
   int TvHeight = 720;

   int DrcWidth = 854;
   int DrcHeight = 480;

   int OuterBorder = 0;
   int ScreenSeparation = 5;

   int nativeHeight, nativeWidth;
   int tvLeft, tvBottom, tvTop, tvRight;
   int drcLeft, drcBottom, drcTop, drcRight;

   if (false /*config::display::layout == config::display::Toggle*/) {
      // For toggle mode only one screen is visible at a time, so we calculate the
      // screen position as if only the TV exists here
      nativeHeight = TvHeight;
      nativeWidth = TvWidth;
      DrcWidth = 0;
      DrcHeight = 0;
      ScreenSeparation = 0;
   } else {
      nativeHeight = DrcHeight + TvHeight + ScreenSeparation + 2 * OuterBorder;
      nativeWidth = std::max(DrcWidth, TvWidth) + 2 * OuterBorder;
   }

   if (windowWidth * nativeHeight >= windowHeight * nativeWidth) {
      // Align to height
      int drcBorder = (windowWidth * nativeHeight - windowHeight * DrcWidth + nativeHeight) / nativeHeight / 2;
      int tvBorder = /*config::display::stretch*/ false ? 0 : (windowWidth * nativeHeight - windowHeight * TvWidth + nativeHeight) / nativeHeight / 2;

      drcBottom = OuterBorder;
      drcTop = OuterBorder + (DrcHeight * windowHeight + nativeHeight / 2) / nativeHeight;
      drcLeft = drcBorder;
      drcRight = windowWidth - drcBorder;

      tvBottom = windowHeight - OuterBorder - (TvHeight * windowHeight + nativeHeight / 2) / nativeHeight;
      tvTop = windowHeight - OuterBorder;
      tvLeft = tvBorder;
      tvRight = windowWidth - tvBorder;
   } else {
      // Align to width
      int heightBorder = (windowHeight * nativeWidth - windowWidth * (DrcHeight + TvHeight + ScreenSeparation) + nativeWidth) / nativeWidth / 2;
      int drcBorder = (windowWidth - DrcWidth * windowWidth / nativeWidth + 1) / 2;
      int tvBorder = (windowWidth - TvWidth * windowWidth / nativeWidth + 1) / 2;

      drcBottom = heightBorder;
      drcTop = heightBorder + (DrcHeight * windowWidth + nativeWidth / 2) / nativeWidth;
      drcLeft = drcBorder;
      drcRight = windowWidth - drcBorder;

      tvTop = windowHeight - heightBorder;
      tvBottom = windowHeight - heightBorder - (TvHeight * windowWidth + nativeWidth / 2) / nativeWidth;
      tvLeft = tvBorder;
      tvRight = windowWidth - tvBorder;
   }

   if (false/*config::display::layout == config::display::Toggle*/) {
      // Copy TV size to DRC size
      drcLeft = tvLeft;
      drcRight = tvRight;
      drcTop = tvTop;
      drcBottom = tvBottom;
   }

   mTvViewport[0] = static_cast<float>(tvLeft);
   mTvViewport[1] = static_cast<float>(tvBottom);
   mTvViewport[2] = static_cast<float>(tvRight - tvLeft);
   mTvViewport[3] = static_cast<float>(tvTop - tvBottom);

   mDrcViewport[0] = static_cast<float>(drcLeft);
   mDrcViewport[1] = static_cast<float>(drcBottom);
   mDrcViewport[2] = static_cast<float>(drcRight - drcLeft);
   mDrcViewport[3] = static_cast<float>(drcTop - drcBottom);
}
