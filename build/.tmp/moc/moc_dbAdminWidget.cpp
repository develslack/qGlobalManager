/****************************************************************************
** Meta object code from reading C++ file 'dbAdminWidget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/modules/control_panel/dbAdminWidget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dbAdminWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_DbAdminWidget_t {
    QByteArrayData data[7];
    char stringdata0[83];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DbAdminWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DbAdminWidget_t qt_meta_stringdata_DbAdminWidget = {
    {
QT_MOC_LITERAL(0, 0, 13), // "DbAdminWidget"
QT_MOC_LITERAL(1, 14, 15), // "onTableSelected"
QT_MOC_LITERAL(2, 30, 0), // ""
QT_MOC_LITERAL(3, 31, 9), // "tableName"
QT_MOC_LITERAL(4, 41, 17), // "onShowTableSchema"
QT_MOC_LITERAL(5, 59, 12), // "onExecuteSql"
QT_MOC_LITERAL(6, 72, 10) // "onClearSql"

    },
    "DbAdminWidget\0onTableSelected\0\0tableName\0"
    "onShowTableSchema\0onExecuteSql\0"
    "onClearSql"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DbAdminWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x08 /* Private */,
       4,    0,   37,    2, 0x08 /* Private */,
       5,    0,   38,    2, 0x08 /* Private */,
       6,    0,   39,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void DbAdminWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DbAdminWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onTableSelected((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->onShowTableSchema(); break;
        case 2: _t->onExecuteSql(); break;
        case 3: _t->onClearSql(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject DbAdminWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_DbAdminWidget.data,
    qt_meta_data_DbAdminWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *DbAdminWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DbAdminWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DbAdminWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int DbAdminWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
