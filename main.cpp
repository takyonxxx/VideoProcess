#include "rtmp.h"

#include <QtWidgets>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Rtmp rtmp;
    rtmp.show();

    return app.exec();
};
