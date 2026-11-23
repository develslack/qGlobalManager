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
    QByteArrayData data[20];
    char stringdata0[289];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DbAdminWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DbAdminWidget_t qt_meta_stringdata_DbAdminWidget = {
    {
QT_MOC_LITERAL(0, 0, 13), // "DbAdminWidget"
QT_MOC_LITERAL(1, 14, 21), // "onFilterTablesChanged"
QT_MOC_LITERAL(2, 36, 0), // ""
QT_MOC_LITERAL(3, 37, 4), // "text"
QT_MOC_LITERAL(4, 42, 14), // "onTableClicked"
QT_MOC_LITERAL(5, 57, 16), // "QListWidgetItem*"
QT_MOC_LITERAL(6, 74, 4), // "item"
QT_MOC_LITERAL(7, 79, 19), // "onTablesContextMenu"
QT_MOC_LITERAL(8, 99, 3), // "pos"
QT_MOC_LITERAL(9, 103, 14), // "onExecuteQuery"
QT_MOC_LITERAL(10, 118, 20), // "onViewTableStructure"
QT_MOC_LITERAL(11, 139, 14), // "onViewTableDdl"
QT_MOC_LITERAL(12, 154, 24), // "onViewIndexesAndTriggers"
QT_MOC_LITERAL(13, 179, 13), // "onClearEditor"
QT_MOC_LITERAL(14, 193, 12), // "onCreateDump"
QT_MOC_LITERAL(15, 206, 11), // "includeData"
QT_MOC_LITERAL(16, 218, 21), // "onExportSelectedTable"
QT_MOC_LITERAL(17, 240, 21), // "TableExporter::Format"
QT_MOC_LITERAL(18, 262, 6), // "format"
QT_MOC_LITERAL(19, 269, 19) // "onExportCurrentGrid"

    },
    "DbAdminWidget\0onFilterTablesChanged\0"
    "\0text\0onTableClicked\0QListWidgetItem*\0"
    "item\0onTablesContextMenu\0pos\0"
    "onExecuteQuery\0onViewTableStructure\0"
    "onViewTableDdl\0onViewIndexesAndTriggers\0"
    "onClearEditor\0onCreateDump\0includeData\0"
    "onExportSelectedTable\0TableExporter::Format\0"
    "format\0onExportCurrentGrid"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DbAdminWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   69,    2, 0x08 /* Private */,
       4,    1,   72,    2, 0x08 /* Private */,
       7,    1,   75,    2, 0x08 /* Private */,
       9,    0,   78,    2, 0x08 /* Private */,
      10,    0,   79,    2, 0x08 /* Private */,
      11,    0,   80,    2, 0x08 /* Private */,
      12,    0,   81,    2, 0x08 /* Private */,
      13,    0,   82,    2, 0x08 /* Private */,
      14,    1,   83,    2, 0x08 /* Private */,
      16,    1,   86,    2, 0x08 /* Private */,
      19,    1,   89,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::QPoint,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   15,
    QMetaType::Void, 0x80000000 | 17,   18,
    QMetaType::Void, 0x80000000 | 17,   18,

       0        // eod
};

void DbAdminWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DbAdminWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onFilterTablesChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->onTableClicked((*reinterpret_cast< QListWidgetItem*(*)>(_a[1]))); break;
        case 2: _t->onTablesContextMenu((*reinterpret_cast< const QPoint(*)>(_a[1]))); break;
        case 3: _t->onExecuteQuery(); break;
        case 4: _t->onViewTableStructure(); break;
        case 5: _t->onViewTableDdl(); break;
        case 6: _t->onViewIndexesAndTriggers(); break;
        case 7: _t->onClearEditor(); break;
        case 8: _t->onCreateDump((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 9: _t->onExportSelectedTable((*reinterpret_cast< TableExporter::Format(*)>(_a[1]))); break;
        case 10: _t->onExportCurrentGrid((*reinterpret_cast< TableExporter::Format(*)>(_a[1]))); break;
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
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
