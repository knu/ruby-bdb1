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

#define BDB1_MARSHAL     (1<<0)
#define BDB1_TXN         (1<<1)
#define BDB1_RE_SOURCE   (1<<2)
#define BDB1_BT_COMPARE  (1<<3)
#define BDB1_BT_PREFIX   (1<<4)
#define BDB1_DUP_COMPARE (1<<5)
#define BDB1_H_HASH      (1<<6)
#define BDB1_NOT_OPEN    (1<<7)

#define BDB1_FUNCTION (BDB1_BT_COMPARE|BDB1_BT_PREFIX|BDB1_DUP_COMPARE|BDB1_H_HASH)

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

#define BDB1_NEED_CURRENT (BDB1_MARSHAL | BDB1_BT_COMPARE | BDB1_BT_PREFIX | BDB1_DUP_COMPARE | BDB1_H_HASH)

typedef union {
	BTREEINFO bi;
	HASHINFO hi;
	RECNOINFO ri;
} DB_INFO;

#define FILTER_KEY 0
#define FILTER_VALUE 1

typedef struct {
    int options, len, has_info;
    DBTYPE type;
    VALUE bt_compare, bt_prefix, h_hash;
    VALUE filter[4];
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

extern VALUE bdb1_deleg_to_orig _((VALUE));

#define GetDB(obj, dbst)						\
{									\
    Data_Get_Struct(obj, bdb1_DB, dbst);				\
    if (dbst->dbp == 0) {						\
        rb_raise(bdb1_eFatal, "closed DB");				\
    }									\
    if (dbst->options & BDB1_NEED_CURRENT) {				\
    	rb_thread_local_aset(rb_thread_current(), bdb1_id_current_db, obj);	\
    }									\
}

#define DATA_ZERO(key)	memset(&(key), 0, sizeof(key));

#define INIT_RECNO(dbst, key, recno)		\
{						\
    recno = 1;					\
    memset(&(key), 0, sizeof(key));		\
    if (dbst->type == DB_RECNO) {		\
        key.data = &recno;			\
        key.size = sizeof(db_recno_t);		\
    }						\
}

#define FREE_KEY(dbst, key)

extern void bdb1_deleg_mark _((struct deleg_class *));
extern void bdb1_deleg_free _((struct deleg_class *));
extern VALUE bdb1_init _((int, VALUE *, VALUE));
extern VALUE bdb1_put _((int, VALUE *, VALUE));
extern VALUE bdb1_get _((int, VALUE *, VALUE));
extern VALUE bdb1_del _((VALUE, VALUE));
extern VALUE bdb1_test_load _((VALUE, const DBT *, int));
extern int bdb1_test_error _((int));
extern VALUE bdb1_each_value _((VALUE));
extern VALUE bdb1_each_eulav _((VALUE));
extern VALUE bdb1_each_key _((VALUE));
extern VALUE bdb1_key _((VALUE, VALUE));
extern VALUE bdb1_has_value _((VALUE, VALUE));
extern VALUE bdb1_internal_value _((VALUE, VALUE, VALUE, int));
extern VALUE bdb1_to_type _((VALUE, VALUE, VALUE));
extern VALUE bdb1_clear _((VALUE));
extern VALUE bdb1_each_vc _((VALUE, int, int));
extern void bdb1_init_delegator _((void));
extern void bdb1_init_recnum _((void));

extern VALUE bdb1_mDb, bdb1_cCommon, bdb1_cDelegate, bdb1_cRecnum;
extern VALUE bdb1_mMarshal, bdb1_eFatal;

extern ID bdb1_id_current_db;
