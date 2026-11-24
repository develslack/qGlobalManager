/****************************************************************************
** Meta object code from reading C++ file 'supportTablesWidget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/modules/control_panel/supportTablesWidget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'supportTablesWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SupportTablesWidget_t {
    QByteArrayData data[25];
    char stringdata0[374];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SupportTablesWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SupportTablesWidget_t qt_meta_stringdata_SupportTablesWidget = {
    {
QT_MOC_LITERAL(0, 0, 19), // "SupportTablesWidget"
QT_MOC_LITERAL(1, 20, 21), // "onFilterTablesChanged"
QT_MOC_LITERAL(2, 42, 0), // ""
QT_MOC_LITERAL(3, 43, 4), // "text"
QT_MOC_LITERAL(4, 48, 18), // "onListItemSelected"
QT_MOC_LITERAL(5, 67, 3), // "row"
QT_MOC_LITERAL(6, 71, 20), // "onShowTableInfoModal"
QT_MOC_LITERAL(7, 92, 12), // "onSaveRecord"
QT_MOC_LITERAL(8, 105, 11), // "onResetForm"
QT_MOC_LITERAL(9, 117, 12), // "onEditRecord"
QT_MOC_LITERAL(10, 130, 8), // "recordId"
QT_MOC_LITERAL(11, 139, 4), // "code"
QT_MOC_LITERAL(12, 144, 4), // "name"
QT_MOC_LITERAL(13, 149, 11), // "description"
QT_MOC_LITERAL(14, 161, 17), // "onDeprecateRecord"
QT_MOC_LITERAL(15, 179, 11), // "displayName"
QT_MOC_LITERAL(16, 191, 8), // "isSystem"
QT_MOC_LITERAL(17, 200, 18), // "onReactivateRecord"
QT_MOC_LITERAL(18, 219, 26), // "onConfigureRolePermissions"
QT_MOC_LITERAL(19, 246, 6), // "roleId"
QT_MOC_LITERAL(20, 253, 8), // "roleName"
QT_MOC_LITERAL(21, 262, 19), // "onAddExternalSource"
QT_MOC_LITERAL(22, 282, 27), // "onEditCurrentExternalSource"
QT_MOC_LITERAL(23, 310, 27), // "onSyncCurrentExternalSource"
QT_MOC_LITERAL(24, 338, 35) // "onToggleCurrentExternalSource..."

    },
    "SupportTablesWidget\0onFilterTablesChanged\0"
    "\0text\0onListItemSelected\0row\0"
    "onShowTableInfoModal\0onSaveRecord\0"
    "onResetForm\0onEditRecord\0recordId\0"
    "code\0name\0description\0onDeprecateRecord\0"
    "displayName\0isSystem\0onReactivateRecord\0"
    "onConfigureRolePermissions\0roleId\0"
    "roleName\0onAddExternalSource\0"
    "onEditCurrentExternalSource\0"
    "onSyncCurrentExternalSource\0"
    "onToggleCurrentExternalSourceActive"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SupportTablesWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   79,    2, 0x08 /* Private */,
       4,    1,   82,    2, 0x08 /* Private */,
       6,    0,   85,    2, 0x08 /* Private */,
       7,    0,   86,    2, 0x08 /* Private */,
       8,    0,   87,    2, 0x08 /* Private */,
       9,    4,   88,    2, 0x08 /* Private */,
      14,    3,   97,    2, 0x08 /* Private */,
      17,    2,  104,    2, 0x08 /* Private */,
      18,    3,  109,    2, 0x08 /* Private */,
      21,    0,  116,    2, 0x08 /* Private */,
      22,    0,  117,    2, 0x08 /* Private */,
      23,    0,  118,    2, 0x08 /* Private */,
      24,    0,  119,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString,   10,   11,   12,   13,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Bool,   10,   15,   16,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   10,   15,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::Bool,   19,   20,   16,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void SupportTablesWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SupportTablesWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onFilterTablesChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->onListItemSelected((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->onShowTableInfoModal(); break;
        case 3: _t->onSaveRecord(); break;
        case 4: _t->onResetForm(); break;
        case 5: _t->onEditRecord((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4]))); break;
        case 6: _t->onDeprecateRecord((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 7: _t->onReactivateRecord((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 8: _t->onConfigureRolePermissions((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< bool(*)>(_a[3]))); break;
        case 9: _t->onAddExternalSource(); break;
        case 10: _t->onEditCurrentExternalSource(); break;
        case 11: _t->onSyncCurrentExternalSource(); break;
        case 12: _t->onToggleCurrentExternalSourceActive(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SupportTablesWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_SupportTablesWidget.data,
    qt_meta_data_SupportTablesWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SupportTablesWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SupportTablesWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SupportTablesWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SupportTablesWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
