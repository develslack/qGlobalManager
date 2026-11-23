/****************************************************************************
** Meta object code from reading C++ file 'mainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/ui/mainWindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[20];
    char stringdata0[336];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 16), // "onModuleSelected"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 3), // "row"
QT_MOC_LITERAL(4, 33, 22), // "onToggleSidebarClicked"
QT_MOC_LITERAL(5, 56, 21), // "onControlPanelClicked"
QT_MOC_LITERAL(6, 78, 16), // "onSessionStarted"
QT_MOC_LITERAL(7, 95, 14), // "onSessionEnded"
QT_MOC_LITERAL(8, 110, 19), // "expiredByInactivity"
QT_MOC_LITERAL(9, 130, 24), // "onPasswordPolicyResolved"
QT_MOC_LITERAL(10, 155, 26), // "onInactivityWarningStarted"
QT_MOC_LITERAL(11, 182, 16), // "secondsRemaining"
QT_MOC_LITERAL(12, 199, 25), // "onInactivityCountdownTick"
QT_MOC_LITERAL(13, 225, 15), // "onLogoutClicked"
QT_MOC_LITERAL(14, 241, 18), // "onElevationChanged"
QT_MOC_LITERAL(15, 260, 6), // "active"
QT_MOC_LITERAL(16, 267, 15), // "onElevationTick"
QT_MOC_LITERAL(17, 283, 16), // "remainingSeconds"
QT_MOC_LITERAL(18, 300, 18), // "onElevationExpired"
QT_MOC_LITERAL(19, 319, 16) // "scopeDisplayName"

    },
    "MainWindow\0onModuleSelected\0\0row\0"
    "onToggleSidebarClicked\0onControlPanelClicked\0"
    "onSessionStarted\0onSessionEnded\0"
    "expiredByInactivity\0onPasswordPolicyResolved\0"
    "onInactivityWarningStarted\0secondsRemaining\0"
    "onInactivityCountdownTick\0onLogoutClicked\0"
    "onElevationChanged\0active\0onElevationTick\0"
    "remainingSeconds\0onElevationExpired\0"
    "scopeDisplayName"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   74,    2, 0x08 /* Private */,
       4,    0,   77,    2, 0x08 /* Private */,
       5,    0,   78,    2, 0x08 /* Private */,
       6,    0,   79,    2, 0x08 /* Private */,
       7,    1,   80,    2, 0x08 /* Private */,
       9,    0,   83,    2, 0x08 /* Private */,
      10,    1,   84,    2, 0x08 /* Private */,
      12,    1,   87,    2, 0x08 /* Private */,
      13,    0,   90,    2, 0x08 /* Private */,
      14,    1,   91,    2, 0x08 /* Private */,
      16,    1,   94,    2, 0x08 /* Private */,
      18,    1,   97,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    8,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   11,
    QMetaType::Void, QMetaType::Int,   11,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   15,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void, QMetaType::QString,   19,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onModuleSelected((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->onToggleSidebarClicked(); break;
        case 2: _t->onControlPanelClicked(); break;
        case 3: _t->onSessionStarted(); break;
        case 4: _t->onSessionEnded((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->onPasswordPolicyResolved(); break;
        case 6: _t->onInactivityWarningStarted((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 7: _t->onInactivityCountdownTick((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: _t->onLogoutClicked(); break;
        case 9: _t->onElevationChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 10: _t->onElevationTick((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 11: _t->onElevationExpired((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
