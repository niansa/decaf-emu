#pragma once

#include <QOpenGLWidget>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions_4_5_Core>
#include <libdecaf/decaf.h>

class Bubble;
class MainWindow;

QT_FORWARD_DECLARE_CLASS(QOpenGLShader)
QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

class RenderWidget : public QOpenGLWidget
{
   Q_OBJECT
public:
   RenderWidget(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
   ~RenderWidget();

protected:
   void resizeGL(int w, int h) Q_DECL_OVERRIDE;
   void paintGL() Q_DECL_OVERRIDE;
   void initializeGL() Q_DECL_OVERRIDE;

   void updateViewports(int windowWidth, int windowHeight);

private:
   QOpenGLShaderProgram *mShaderProgram = nullptr;
   QOpenGLShader *mVertexShader = nullptr;
   QOpenGLShader *mFragmentShader = nullptr;
   QOpenGLVertexArrayObject mVAO;
   QOpenGLBuffer mVBO;
   QOpenGLFunctions_4_5_Core *mFunctions = nullptr;
   GLuint mSampler = 0;

   float mTvViewport[4];
   float mDrcViewport[4];

   decaf::OpenGLDriver *mGraphicsDriver = nullptr;
};
