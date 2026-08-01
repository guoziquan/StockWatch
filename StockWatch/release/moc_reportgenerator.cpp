/****************************************************************************
** Meta object code from reading C++ file 'reportgenerator.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../reportgenerator.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'reportgenerator.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ReportGenerator_t {
    QByteArrayData data[16];
    char stringdata0[147];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ReportGenerator_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ReportGenerator_t qt_meta_stringdata_ReportGenerator = {
    {
QT_MOC_LITERAL(0, 0, 15), // "ReportGenerator"
QT_MOC_LITERAL(1, 16, 11), // "reportReady"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 8), // "filePath"
QT_MOC_LITERAL(4, 38, 7), // "allDone"
QT_MOC_LITERAL(5, 46, 5), // "count"
QT_MOC_LITERAL(6, 52, 5), // "error"
QT_MOC_LITERAL(7, 58, 3), // "msg"
QT_MOC_LITERAL(8, 62, 12), // "onKLineReply"
QT_MOC_LITERAL(9, 75, 14), // "QNetworkReply*"
QT_MOC_LITERAL(10, 90, 5), // "reply"
QT_MOC_LITERAL(11, 96, 16), // "onMoneyFlowReply"
QT_MOC_LITERAL(12, 113, 13), // "checkAndBuild"
QT_MOC_LITERAL(13, 127, 11), // "onBuildDone"
QT_MOC_LITERAL(14, 139, 4), // "path"
QT_MOC_LITERAL(15, 144, 2) // "ok"

    },
    "ReportGenerator\0reportReady\0\0filePath\0"
    "allDone\0count\0error\0msg\0onKLineReply\0"
    "QNetworkReply*\0reply\0onMoneyFlowReply\0"
    "checkAndBuild\0onBuildDone\0path\0ok"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ReportGenerator[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   49,    2, 0x06 /* Public */,
       4,    1,   52,    2, 0x06 /* Public */,
       6,    1,   55,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    1,   58,    2, 0x08 /* Private */,
      11,    1,   61,    2, 0x08 /* Private */,
      12,    0,   64,    2, 0x08 /* Private */,
      13,    2,   65,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::QString,    7,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,   14,   15,

       0        // eod
};

void ReportGenerator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        ReportGenerator *_t = static_cast<ReportGenerator *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->reportReady((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->allDone((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->onKLineReply((*reinterpret_cast< QNetworkReply*(*)>(_a[1]))); break;
        case 4: _t->onMoneyFlowReply((*reinterpret_cast< QNetworkReply*(*)>(_a[1]))); break;
        case 5: _t->checkAndBuild(); break;
        case 6: _t->onBuildDone((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QNetworkReply* >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QNetworkReply* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            typedef void (ReportGenerator::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ReportGenerator::reportReady)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (ReportGenerator::*_t)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ReportGenerator::allDone)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (ReportGenerator::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ReportGenerator::error)) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject ReportGenerator::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_ReportGenerator.data,
      qt_meta_data_ReportGenerator,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *ReportGenerator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ReportGenerator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ReportGenerator.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ReportGenerator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void ReportGenerator::reportReady(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ReportGenerator::allDone(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ReportGenerator::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
