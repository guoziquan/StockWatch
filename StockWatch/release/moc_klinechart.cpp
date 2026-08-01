/****************************************************************************
** Meta object code from reading C++ file 'klinechart.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.7)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../klinechart.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'klinechart.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.7. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_KLineInfoPanel_t {
    QByteArrayData data[1];
    char stringdata0[15];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_KLineInfoPanel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_KLineInfoPanel_t qt_meta_stringdata_KLineInfoPanel = {
    {
QT_MOC_LITERAL(0, 0, 14) // "KLineInfoPanel"

    },
    "KLineInfoPanel"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_KLineInfoPanel[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

void KLineInfoPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_UNUSED(_o);
    Q_UNUSED(_id);
    Q_UNUSED(_c);
    Q_UNUSED(_a);
}

const QMetaObject KLineInfoPanel::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_KLineInfoPanel.data,
      qt_meta_data_KLineInfoPanel,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *KLineInfoPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KLineInfoPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_KLineInfoPanel.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int KLineInfoPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    return _id;
}
struct qt_meta_stringdata_KLineChart_t {
    QByteArrayData data[7];
    char stringdata0[64];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_KLineChart_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_KLineChart_t qt_meta_stringdata_KLineChart = {
    {
QT_MOC_LITERAL(0, 0, 10), // "KLineChart"
QT_MOC_LITERAL(1, 11, 14), // "hoveredChanged"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 9), // "KLineData"
QT_MOC_LITERAL(4, 37, 2), // "kd"
QT_MOC_LITERAL(5, 40, 17), // "splitRatioChanged"
QT_MOC_LITERAL(6, 58, 5) // "ratio"

    },
    "KLineChart\0hoveredChanged\0\0KLineData\0"
    "kd\0splitRatioChanged\0ratio"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_KLineChart[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   24,    2, 0x06 /* Public */,
       5,    1,   27,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::Double,    6,

       0        // eod
};

void KLineChart::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        KLineChart *_t = static_cast<KLineChart *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->hoveredChanged((*reinterpret_cast< const KLineData(*)>(_a[1]))); break;
        case 1: _t->splitRatioChanged((*reinterpret_cast< double(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            typedef void (KLineChart::*_t)(const KLineData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KLineChart::hoveredChanged)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (KLineChart::*_t)(double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KLineChart::splitRatioChanged)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject KLineChart::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_KLineChart.data,
      qt_meta_data_KLineChart,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *KLineChart::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KLineChart::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_KLineChart.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int KLineChart::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void KLineChart::hoveredChanged(const KLineData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void KLineChart::splitRatioChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
