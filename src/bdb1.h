#include <ruby.h>

#ifdef COMPAT185
#include <db_185.h>
#else
#include <db.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define db_recno_t int

#define DB_VERSION_MAJOR 1
#define DB_VERSION_MINOR -1
#define DB_RELEASE_PATCH -1

#define BDB1_MARSHAL 1
#define BDB1_TXN 2
#define BDB1_RE_SOURCE 4
#define BDB1_BT_COMPARE 8
#define BDB1_BT_PREFIX  16
#define BDB1_DUP_COMPARE 32
#define BDB1_H_HASH 64

#define DB_SET_RANGE    R_CURSOR
#define DB_FIRST        R_FIRST
#define DB_AFTER        R_IAFTER
#define DB_BEFORE       R_IBEFORE
#define DB_LAST         R_LAST
#define DB_NEXT         R_NEXT
#define DB_NOOVERWRITE  R_NOOVERWRITE
#define DB_PREV         R_PREV
#define DB_FIXEDLEN     R_FIXEDLEN
#define DB_DUP          R_DUP

#define DB_KEYEXIST 1
#define DB_NOTFOUND 1

#define DB_CREATE O_CREAT
#define DB_WRITE O_RDWR
#define DB_RDONLY O_RDONLY
#define DB_TRUNCATE O_TRUNC

#define BDB1_NEED_CURRENT (BDB1_BT_COMPARE | BDB1_BT_PREFIX | BDB1_DUP_COMPARE | BDB1_H_HASH)

typedef union {
	BTREEINFO bi;
	HASHINFO hi;
	RECNOINFO ri;
} DB_INFO;

typedef struct {
    int options, len, has_info;
    DBTYPE type;
    VALUE bt_compare, bt_prefix, h_hash;
    DB *dbp;
    u_int32_t flags;
    int array_base;
    VALUE marshal;
    DB_INFO info;
} bdb1_DB;

typedef struct {
    bdb1_DB *dbst;
    DBT *key;
    VALUE value;
} bdb1_VALUE;

struct deleg_class {
    int type;
    VALUE db;
    VALUE obj;
    VALUE key;
};

#define test_dump(dbst, key, a)					\
{								\
    int _bdb1_is_nil = 0;					\
    VALUE _bdb1_tmp_;						\
    if (dbst->marshal) {					\
        _bdb1_tmp_ = rb_funcall(dbst->marshal, id_dump, 1, a);	\
    }								\
    else {							\
        _bdb1_tmp_ = rb_obj_as_string(a);			\
        if (a == Qnil)						\
            _bdb1_is_nil = 1;					\
        else							\
            a = _bdb1_tmp_;					\
    }								\
    key.data = RSTRING(_bdb1_tmp_)->ptr;			\
    key.size = RSTRING(_bdb1_tmp_)->len + _bdb1_is_nil;		\
}

#define GetDB(obj, dbst)						\
{									\
    Data_Get_Struct(obj, bdb1_DB, dbst);				\
    if (dbst->dbp == 0) {						\
        rb_raise(bdb1_eFatal, "closed DB");				\
    }									\
    if (dbst->options & BDB1_NEED_CURRENT) {				\
    	rb_thread_local_aset(rb_thread_current(), id_current_db, obj);	\
    }									\
}

#define DATA_ZERO(key)	memset(&(key), 0, sizeof(key));

#define init_recno(dbst, key, recno)		\
{						\
    recno = 1;					\
    memset(&(key), 0, sizeof(key));		\
    if (dbst->type == DB_RECNO) {		\
        key.data = &recno;			\
        key.size = sizeof(db_recno_t);		\
    }						\
}

#define free_key(dbst, key)

#define test_recno(dbst, key, recno, a)		\
{						\
    if (dbst->type == DB_RECNO) {		\
        recno = NUM2INT(a) + dbst->array_base;	\
        key.data = &recno;			\
        key.size = sizeof(db_recno_t);		\
    }						\
    else {					\
        test_dump(dbst, key, a);		\
    }						\
}

extern void bdb1_deleg_mark _((struct deleg_class *));
extern void bdb1_deleg_free _((struct deleg_class *));
extern VALUE bdb1_s_new _((int, VALUE *, VALUE));
extern VALUE bdb1_put _((int, VALUE *, VALUE));
extern VALUE bdb1_get _((int, VALUE *, VALUE));
extern VALUE bdb1_del _((VALUE, VALUE));
extern VALUE bdb1_test_load _((bdb1_DB *, DBT));
extern int bdb1_test_error _((int));
extern VALUE bdb1_each_value _((VALUE));
extern VALUE bdb1_each_eulav _((VALUE));
extern VALUE bdb1_each_key _((VALUE));
extern VALUE bdb1_index _((VALUE, VALUE));
extern VALUE bdb1_has_value _((VALUE, VALUE));
extern VALUE bdb1_internal_value _((VALUE, VALUE, VALUE, int));
extern VALUE bdb1_to_type _((VALUE, VALUE, VALUE));
extern VALUE bdb1_clear _((VALUE));
extern VALUE bdb1_each_vc _((VALUE, int));
extern void bdb1_init_delegator _((void));
extern void bdb1_init_recnum _((void));

extern VALUE bdb1_mDb, bdb1_cCommon, bdb1_cDelegate, bdb1_cRecnum;
extern VALUE bdb1_mMarshal, bdb1_eFatal;

extern ID id_dump, id_load;
extern ID id_current_db;
