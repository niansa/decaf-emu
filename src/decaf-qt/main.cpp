#include "mainwindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
   QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

   QSurfaceFormat fmt;
   fmt.setVersion(4, 5);
   fmt.setProfile(QSurfaceFormat::CoreProfile);
   fmt.setOption(QSurfaceFormat::DebugContext);
   QSurfaceFormat::setDefaultFormat(fmt);


   QApplication a { argc, argv };
   MainWindow w;
   w.show();
   return a.exec();
}
