#pragma once

#include <QApplication>

inline QApplication& annotationTestApplication() {
    static int argc{1};
    static char applicationName[] = "annotation-test";
    static char* argv[]{applicationName, nullptr};
    static QApplication application(argc, argv);
    return application;
}
