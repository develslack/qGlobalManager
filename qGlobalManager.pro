# ==============================================================================
# Global Manager (qGlobalManager) - Archivo de proyecto QMake
# Compatible con Qt 5 (Slackware 15) y Qt 6 | C++17+ | Consola y Qt Creator
# ==============================================================================

TEMPLATE = app
TARGET   = qGlobalManager
VERSION  = 0.1.0

# Módulos base de Qt (interfaz gráfica y persistencia SQLite)
QT += core gui sql svg network printsupport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Estándar C++ moderno compatible con Qt 5.15 y obligatorio en Qt 6
CONFIG += c++17 utf8_source

# Advertencias útiles y preparación para migración limpia entre Qt 5 y Qt 6
DEFINES += \
    APP_EXEC_NAME=\\\"$$TARGET\\\" \
    APP_DISPLAY_NAME=\\\"Global Manager\\\" \
    APP_VERSION=\\\"$$VERSION\\\" \
    QT_DEPRECATED_WARNINGS

# En Qt 5, deshabilitar APIs obsoletas anteriores a 5.15 para facilitar el salto a Qt 6
equals(QT_MAJOR_VERSION, 5) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00
}
equals(QT_MAJOR_VERSION, 6) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
}

# ------------------------------------------------------------------------------
# Configuración por modo de compilación (Debug / Release)
# ------------------------------------------------------------------------------
CONFIG(debug, debug|release) {
    DEFINES += APP_DEBUG_BUILD
    !msvc: QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
} else {
    DEFINES += QT_NO_DEBUG_OUTPUT
    !msvc: QMAKE_CXXFLAGS += -O2 -Wall -Wextra
}

# ------------------------------------------------------------------------------
# Directorios de salida (desarrollo en build/ y temporales aislados)
# ------------------------------------------------------------------------------
# Si build.sh pasa DESTDIR=. desde build/, se respeta; en qmake directo/Qt Creator usa build/
isEmpty(DESTDIR) {
    DESTDIR = $$PWD/build
}

# Aislar archivos intermedios para no ensuciar el árbol de código fuente
BUILD_TMP_DIR = $$OUT_PWD/.tmp
OBJECTS_DIR   = $$BUILD_TMP_DIR/obj
MOC_DIR       = $$BUILD_TMP_DIR/moc
RCC_DIR       = $$BUILD_TMP_DIR/rcc
UI_DIR        = $$BUILD_TMP_DIR/ui

# ------------------------------------------------------------------------------
# Rutas de cabeceras (permite #include limpio entre core, ui y modules)
# ------------------------------------------------------------------------------
INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/core \
    $$PWD/src/ui \
    $$PWD/src/modules

DEPENDPATH += $$INCLUDEPATH

# ------------------------------------------------------------------------------
# Archivos fuente y cabeceras del proyecto
# ------------------------------------------------------------------------------

# 1. Punto de entrada
SOURCES += \
    src/main.cpp

# 2. Núcleo transversal (persistencia SQLite, sesión, permisos, timer de inactividad)
HEADERS += \
    src/core/db.h \
    src/core/icons.h \
    src/core/licenseToken.h \
    src/core/mailer.h \
    src/core/moduleBase.h \
    src/core/session.h \
    src/core/theme.h \
    src/core/userCredentials.h \
    src/core/bootstrapConfig.h \
    src/core/tableExporter.h \
    src/core/elevationManager.h \
    src/core/externalSourceManager.h \
    src/core/oauth2Google.h

SOURCES += \
    src/core/db.cpp \
    src/core/icons.cpp \
    src/core/licenseToken.cpp \
    src/core/mailer.cpp \
    src/core/moduleBase.cpp \
    src/core/session.cpp \
    src/core/theme.cpp \
    src/core/userCredentials.cpp \
    src/core/bootstrapConfig.cpp \
    src/core/tableExporter.cpp \
    src/core/elevationManager.cpp \
    src/core/externalSourceManager.cpp \
    src/core/oauth2Google.cpp

HEADERS += \
    src/ui/changePasswordDialog.h \
    src/ui/loginDialog.h \
    src/ui/setupWizard.h \
    src/ui/elevationDialog.h \
    src/ui/mainWindow.h

SOURCES += \
    src/ui/changePasswordDialog.cpp \
    src/ui/loginDialog.cpp \
    src/ui/setupWizard.cpp \
    src/ui/elevationDialog.cpp \
    src/ui/mainWindow.cpp

# 4. Módulos funcionales (src/modules/<nombre_modulo>/<nombre_modulo>.h/.cpp)
# Al agregar un módulo nuevo, suma sus archivos aquí o crea un <nombre_modulo>.pri dentro de su carpeta:
# HEADERS += src/modules/ejemplo/ejemplo.h
# SOURCES += src/modules/ejemplo/ejemplo.cpp

MODULE_PRIS = $$files($$PWD/src/modules/*.pri, true)
for(mod_pri, MODULE_PRIS) {
    include($$mod_pri)
}

# Recursos Qt (se incluirán automáticamente si creas resources/resources.qrc)
exists($$PWD/resources/resources.qrc) {
    RESOURCES += resources/resources.qrc
}

# ------------------------------------------------------------------------------
# Configuración específica de plataforma y reglas de instalación
# ------------------------------------------------------------------------------
unix:!macx {
    isEmpty(PREFIX) {
        PREFIX = /opt/$$TARGET
    }

    # Permitir que bin/qGlobalManager resuelva librerías empaquetadas en ../libs/
    QMAKE_LFLAGS += -Wl,-rpath,\'\$\$ORIGIN/../libs\'

    target.path = $$PREFIX/bin
    INSTALLS += target

    exists($$PWD/resources/icons) {
        icons.files = $$PWD/resources/icons/*
        icons.path  = $$PREFIX/icons
        INSTALLS += icons
    }

    exists($$PWD/documentation) {
        docs.files = $$PWD/documentation/*
        docs.path  = $$PREFIX/documentation
        INSTALLS += docs
    }
}

win32 {
    isEmpty(PREFIX) {
        PREFIX = C:/Programs_Files/$$TARGET
    }

    # Mantener la aplicación 100% gráfica (sin ventana de consola) en Release
    CONFIG(release, debug|release) {
        CONFIG -= console
        CONFIG += windows
    }

    exists($$PWD/resources/icons/$${TARGET}.ico) {
        RC_ICONS = resources/icons/$${TARGET}.ico
    }

    target.path = $$PREFIX/bin
    INSTALLS += target
}

# Archivos auxiliares visibles dentro de Qt Creator
DISTFILES += \
    build.sh
