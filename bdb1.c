#include <ruby.h>
#include <db.h>
#include <errno.h>

#include "trans.h"
#define db_recno_t int

#define DB_VERSION_MAJOR 1
#define DB_VERSION_MINOR -1
#define DB_RELEASE_PATCH -1

#define BDB_MARSHAL 1
#define BDB_TXN 2
#define BDB_RE_SOURCE 4
#define BDB_BT_COMPARE 8
#define BDB_BT_PREFIX  16
#define BDB_DUP_COMPARE 32
#define BDB_H_HASH 64

#define BDB_NEED_CURRENT (BDB_BT_COMPARE | BDB_BT_PREFIX | BDB_DUP_COMPARE | BDB_H_HASH)


static VALUE bdb_eFatal;
static VALUE bdb_mDb;
static VALUE bdb_cCommon, bdb_cBtree, bdb_cHash, bdb_cRecno, bdb_cDelegate;

static VALUE bdb_cUnknown, bdb_mMarshal;
static ID id_dump, id_load, id_current_db;
static ID id_bt_compare, id_bt_prefix, id_dup_compare, id_h_hash, id_proc_call;

static VALUE bdb_errstr;
static int bdb_errcall = 0;

typedef union {
	BTREEINFO bi;
	HASHINFO hi;
	RECNOINFO ri;
} DB_INFO;

typedef struct {
    int options, no_thread, has_info;
    DBTYPE type;
    VALUE bt_compare, bt_prefix, dup_compare, h_hash;
    DB *dbp;
    u_int32_t flags;
    int array_base;
    VALUE marshal;
    DB_INFO info;
} bdb_DB;

typedef struct {
    bdb_DB *dbst;
    DBT *key;
    VALUE value;
} bdb_VALUE;

static char *
db_strerror(int err)
{
    if (err == 0)
        return "" ;

    if (err > 0)
        return strerror(err) ;

    return "Unknown Error" ;
}

static int
test_error(comm)
    int comm;
{
    VALUE error;

    switch (comm) {
    case 0:
    case 1:
        break;
    default:
        error = bdb_eFatal;
        if (bdb_errcall) {
            bdb_errcall = 0;
            rb_raise(error, "%s -- %s", RSTRING(bdb_errstr)->ptr, db_strerror(comm));
        }
        else
            rb_raise(error, "%s", db_strerror(errno));
    }
    return comm;
}

static VALUE
bdb_obj_init(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    return Qtrue;
}

/* DATABASE */
#define test_dump(dbst, key, a)					\
{								\
    int _bdb_is_nil = 0;					\
    VALUE _bdb_tmp_;						\
    if (dbst->marshal) {					\
        _bdb_tmp_ = rb_funcall(dbst->marshal, id_dump, 1, a);	\
    }								\
    else {							\
        _bdb_tmp_ = rb_obj_as_string(a);			\
        if (a == Qnil)						\
            _bdb_is_nil = 1;					\
        else							\
            a = _bdb_tmp_;					\
    }								\
    key.data = RSTRING(_bdb_tmp_)->ptr;				\
    key.size = RSTRING(_bdb_tmp_)->len + _bdb_is_nil;		\
}


static VALUE
test_load(dbst, a)
    bdb_DB *dbst;
    DBT a;
{
    VALUE res;
    int i;

    if (dbst->marshal) {
        res = rb_funcall(dbst->marshal, id_load, 1, rb_str_new(a.data, a.size));
    }
    else {
	if (a.size == 1 && ((char *)a.data)[0] == '\000') {
	    res = Qnil;
	}
	else {
	    res = rb_tainted_str_new(a.data, a.size);
	}
    }
    return res;
}

static ID id_send;

struct deleg_class {
    int type;
    VALUE db;
    VALUE obj;
    VALUE key;
};

void
bdb_deleg_free(delegst)
    struct deleg_class *delegst;
{
    free(delegst);
}

void
bdb_deleg_mark(delegst)
    struct deleg_class *delegst;
{
    bdb_DB *dbst;

    if (delegst->db) {
	Data_Get_Struct(delegst->db, bdb_DB, dbst);
	if (dbst->dbp) {
	    rb_gc_mark(delegst->db);
	    if (delegst->key) rb_gc_mark(delegst->key);
	}
    }
    if (delegst->obj) rb_gc_mark(delegst->obj);
}

static VALUE bdb_put _((int, VALUE *, VALUE));

static VALUE
bdb_deleg_each(tmp)
    VALUE *tmp;
{
    return rb_funcall2(tmp[0], id_send, (int)tmp[1], (VALUE *)tmp[2]);
}

static VALUE
bdb_deleg_yield(i, res)
    VALUE i, res;
{
    return rb_ary_push(res, rb_yield(i));
}

static VALUE
bdb_deleg_missing(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    struct deleg_class *delegst, *newst;
    bdb_DB *dbst;
    VALUE res, new;

    Data_Get_Struct(obj, struct deleg_class, delegst);
    if (rb_block_given_p()) {
	VALUE tmp[3];

	tmp[0] = delegst->obj;
	tmp[1] = (VALUE)argc;
	tmp[2] = (VALUE)argv;
	res = rb_ary_new();
	rb_iterate(bdb_deleg_each, (VALUE)tmp, bdb_deleg_yield, res);
    }
    else {
	res = rb_funcall2(delegst->obj, id_send, argc, argv);
    }
    Data_Get_Struct(delegst->db, bdb_DB, dbst);
    if (dbst->dbp) {
	VALUE nargv[2];

	if (!SPECIAL_CONST_P(res) &&
	    (TYPE(res) != T_DATA || 
	     RDATA(res)->dmark != (RUBY_DATA_FUNC)bdb_deleg_mark)) {
	    new = Data_Make_Struct(bdb_cDelegate, struct deleg_class, 
				   bdb_deleg_mark, bdb_deleg_free, newst);
	    newst->db = delegst->db;
	    newst->obj = res;
	    newst->key = (!delegst->type)?obj:delegst->key;
	    newst->type = 1;
	    res = new;
	}
	if (!delegst->type) {
	    nargv[0] = delegst->key;
	    nargv[1] = delegst->obj;
	}
	else {
	    Data_Get_Struct(delegst->key, struct deleg_class, newst);
	    nargv[0] = newst->key;
	    nargv[1] = newst->obj;
	}
	bdb_put(2, nargv, delegst->db);
    }
    return res;
}

#define DELEG_0(id)						\
   VALUE obj;							\
   {								\
       struct deleg_class *delegst;				\
       Data_Get_Struct(obj, struct deleg_class, delegst);	\
       return rb_funcall2(delegst->obj, id, 0, 0);		\
   }

static VALUE bdb_deleg_inspect(obj) DELEG_0(rb_intern("inspect"))
static VALUE bdb_deleg_to_s(obj)    DELEG_0(rb_intern("to_s"))
static VALUE bdb_deleg_to_str(obj)  DELEG_0(rb_intern("to_str"))
static VALUE bdb_deleg_to_a(obj)    DELEG_0(rb_intern("to_a"))
static VALUE bdb_deleg_to_ary(obj)  DELEG_0(rb_intern("to_ary"))
static VALUE bdb_deleg_to_i(obj)    DELEG_0(rb_intern("to_i"))
static VALUE bdb_deleg_to_int(obj)  DELEG_0(rb_intern("to_int"))
static VALUE bdb_deleg_to_f(obj)    DELEG_0(rb_intern("to_f"))
static VALUE bdb_deleg_to_hash(obj) DELEG_0(rb_intern("to_hash"))
static VALUE bdb_deleg_to_io(obj)   DELEG_0(rb_intern("to_io"))
static VALUE bdb_deleg_to_proc(obj) DELEG_0(rb_intern("to_proc"))

static VALUE
bdb_deleg_to_orig(obj)
    VALUE obj;
{
    struct deleg_class *delegst;
    Data_Get_Struct(obj, struct deleg_class, delegst);
    return delegst->obj;
}

static VALUE
bdb_deleg_orig(obj)
    VALUE obj;
{
    return obj;
}

void bdb_init_delegator()
{
    id_send = rb_intern("send");
    bdb_cDelegate = rb_define_class_under(bdb_mDb, "Delegate", rb_cObject);
    {
	VALUE ary;
	char *method;
	int i;

	ary = rb_class_instance_methods(0, 0, rb_mKernel);
	for (i = 0; i < RARRAY(ary)->len; i++) {
	    method = RSTRING(RARRAY(ary)->ptr[i])->ptr;
	    if (!strcmp(method, "==") ||
		!strcmp(method, "===") || !strcmp(method, "=~")) continue;
	    rb_undef_method(bdb_cDelegate, method);
	}
    }
    rb_define_method(bdb_cDelegate, "method_missing", bdb_deleg_missing, -1);
    rb_define_method(bdb_cDelegate, "inspect", bdb_deleg_inspect, 0);
    rb_define_method(bdb_cDelegate, "to_s", bdb_deleg_to_s, 0);
    rb_define_method(bdb_cDelegate, "to_str", bdb_deleg_to_str, 0);
    rb_define_method(bdb_cDelegate, "to_a", bdb_deleg_to_a, 0);
    rb_define_method(bdb_cDelegate, "to_ary", bdb_deleg_to_ary, 0);
    rb_define_method(bdb_cDelegate, "to_i", bdb_deleg_to_i, 0);
    rb_define_method(bdb_cDelegate, "to_int", bdb_deleg_to_int, 0);
    rb_define_method(bdb_cDelegate, "to_f", bdb_deleg_to_f, 0);
    rb_define_method(bdb_cDelegate, "to_hash", bdb_deleg_to_hash, 0);
    rb_define_method(bdb_cDelegate, "to_io", bdb_deleg_to_io, 0);
    rb_define_method(bdb_cDelegate, "to_proc", bdb_deleg_to_proc, 0);
    /* don't use please */
    rb_define_method(bdb_cDelegate, "to_orig", bdb_deleg_to_orig, 0);
    rb_define_method(rb_mKernel, "to_orig", bdb_deleg_orig, 0);
}

static VALUE 
test_load_dyna(obj, key, val)
    VALUE obj;
    DBT key, val;
{
    bdb_DB *dbst;
    VALUE del, res, tmp;
    struct deleg_class *delegst;
    
    Data_Get_Struct(obj, bdb_DB, dbst);
    res = test_load(dbst, val);
    if (dbst->marshal && !SPECIAL_CONST_P(res)) {
	del = Data_Make_Struct(bdb_cDelegate, struct deleg_class, 
			       bdb_deleg_mark, bdb_deleg_free, delegst);
	delegst->db = obj;
	if (dbst->type == DB_RECNO) {
	    tmp = INT2NUM((*(db_recno_t *)key.data) - dbst->array_base);
	}
	else {
	    tmp = rb_str_new(key.data, key.size);
	}
	delegst->key = rb_funcall(dbst->marshal, id_load, 1, tmp);
	delegst->obj = res;
	res = del;
    }
    return res;
}

static int
bdb_bt_compare(a, b)
    DBT *a, *b;
{
    VALUE obj, av, bv, res;
    bdb_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), id_current_db)) == Qnil) {
	rb_raise(bdb_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    av = test_load(dbst, *a);
    bv = test_load(dbst, *b);
    if (dbst->bt_compare == 0)
	res = rb_funcall(obj, id_bt_compare, 2, av, bv);
    else
	res = rb_funcall(dbst->bt_compare, id_proc_call, 2, av, bv);
    return NUM2INT(res);
} 

static size_t
bdb_bt_prefix(a, b)
    DBT *a, *b;
{
    VALUE obj, av, bv, res;
    bdb_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), id_current_db)) == Qnil) {
	rb_raise(bdb_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    av = test_load(dbst, *a);
    bv = test_load(dbst, *b);
    if (dbst->bt_prefix == 0)
	res = rb_funcall(obj, id_bt_prefix, 2, av, bv);
    else
	res = rb_funcall(dbst->bt_prefix, id_proc_call, 2, av, bv);
    return NUM2INT(res);
} 

static int
bdb_dup_compare(a, b)
    DBT *a, *b;
{
    VALUE obj, av, bv, res;
    bdb_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), id_current_db)) == Qnil) {
	rb_raise(bdb_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    av = test_load(dbst, *a);
    bv = test_load(dbst, *b);
    if (dbst->dup_compare == 0)
	res = rb_funcall(obj, id_dup_compare, 2, av, bv);
    else
	res = rb_funcall(dbst->dup_compare, id_proc_call, 2, av, bv);
    return NUM2INT(res);
}

static u_int32_t
bdb_h_hash(bytes, length)
    void *bytes;
    u_int32_t length;
{
    VALUE obj, st, res;
    bdb_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), id_current_db)) == Qnil) {
	rb_raise(bdb_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb_DB, dbst);
    st = rb_tainted_str_new((char *)bytes, length);
    if (dbst->h_hash == 0)
	res = rb_funcall(obj, id_h_hash, 1, st);
    else
	res = rb_funcall(dbst->h_hash, id_proc_call, 1, st);
    return NUM2UINT(res);
}

static void
bdb_free(dbst)
    bdb_DB *dbst;
{
    if (dbst->dbp) {
        test_error(dbst->dbp->close(dbst->dbp));
        dbst->dbp = NULL;
    }
    free(dbst);
}

static void
bdb_mark(dbst)
    bdb_DB *dbst;
{
    if (dbst->marshal) rb_gc_mark(dbst->marshal);
    if (dbst->bt_compare) rb_gc_mark(dbst->bt_compare);
    if (dbst->bt_prefix) rb_gc_mark(dbst->bt_prefix);
    if (dbst->dup_compare) rb_gc_mark(dbst->dup_compare);
    if (dbst->h_hash) rb_gc_mark(dbst->h_hash);
}

static VALUE
bdb_i185_btree(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = RSTRING(key)->ptr;
    if (strcmp(options, "set_flags") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.flags = NUM2INT(value);
    }
    else if (strcmp(options, "set_cachesize") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.cachesize = NUM2INT(value);
    }
    else if (strcmp(options, "set_bt_minkey") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.minkeypage = NUM2INT(value);
    }
    else if (strcmp(options, "set_pagesize") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.psize = NUM2INT(value);
    }
    else if (strcmp(options, "set_bt_compare") == 0) {
	if (!rb_obj_is_kind_of(value, rb_cProc)) 
	    rb_raise(bdb_eFatal, "expected a Proc object");
	dbst->has_info = Qtrue;
	dbst->options |= BDB_BT_COMPARE;
	dbst->bt_compare = value;
	dbst->info.bi.compare = bdb_bt_compare;
    }
    else if (strcmp(options, "set_bt_prefix") == 0) {
	if (!rb_obj_is_kind_of(value, rb_cProc)) 
	    rb_raise(bdb_eFatal, "expected a Proc object");
	dbst->has_info = Qtrue;
	dbst->options |= BDB_BT_PREFIX;
	dbst->bt_prefix = value;
	dbst->info.bi.prefix = bdb_bt_prefix;
    }
    else if (strcmp(options, "set_lorder") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.lorder = NUM2INT(value);
    }
    else if (strcmp(options, "marshal") == 0) {
        switch (value) {
        case Qtrue: dbst->marshal = bdb_mMarshal; break;
        case Qfalse: dbst->marshal = Qfalse; break;
        default: 
	    if (!rb_respond_to(value, id_load) ||
		!rb_respond_to(value, id_dump)) {
		rb_raise(bdb_eFatal, "marshal value must be true or false");
	    }
	    dbst->marshal = value;
	    break;
        }
    }
    return Qnil;
}

static VALUE
bdb_i185_hash(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = RSTRING(key)->ptr;
    if (strcmp(options, "set_h_ffactor") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.hi.ffactor = NUM2INT(value);
    }
    else if (strcmp(options, "set_h_nelem") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.hi.nelem = NUM2INT(value);
    }
    else if (strcmp(options, "set_cachesize") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.hi.cachesize = NUM2INT(value);
    }
    else if (strcmp(options, "set_h_hash") == 0) {
	if (!rb_obj_is_kind_of(value, rb_cProc)) 
	    rb_raise(bdb_eFatal, "expected a Proc object");
	dbst->has_info = Qtrue;
	dbst->options |= BDB_H_HASH;
	dbst->h_hash = value;
	dbst->info.hi.hash = bdb_h_hash;
    }
    else if (strcmp(options, "set_lorder") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.hi.lorder = NUM2INT(value);
    }
    else if (strcmp(options, "marshal") == 0) {
        switch (value) {
        case Qtrue: dbst->marshal = bdb_mMarshal; break;
        case Qfalse: dbst->marshal = Qfalse; break;
        default: 
	    if (!rb_respond_to(value, id_load) ||
		!rb_respond_to(value, id_dump)) {
		rb_raise(bdb_eFatal, "marshal value must be true or false");
	    }
	    dbst->marshal = value;
	    break;
        }
    }
    return Qnil;
}

static VALUE
bdb_i185_recno(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = RSTRING(key)->ptr;
    if (strcmp(options, "set_flags") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.flags = NUM2INT(value);
    }
    else if (strcmp(options, "set_re_delim") == 0) {
	int ch;
	if (TYPE(value) == T_STRING) {
	    dbst->info.ri.bval = RSTRING(value)->ptr[0];
	}
	else {
	    dbst->info.ri.bval = NUM2INT(value);
	}
	dbst->has_info = Qtrue;
	dbst->info.ri.flags |= R_FIXEDLEN;
    }
    else if (strcmp(options, "set_re_len") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.reclen = NUM2INT(value);
	dbst->info.ri.flags |= R_FIXEDLEN;
    }
    else if (strcmp(options, "set_re_pad") == 0) {
	int ch;
	if (TYPE(value) == T_STRING) {
	    dbst->info.ri.bval = RSTRING(value)->ptr[0];
	}
	else {
	    dbst->info.ri.bval = NUM2INT(value);
	}
	dbst->has_info = Qtrue;
	dbst->info.ri.flags |= R_FIXEDLEN;
    }
    else if (strcmp(options, "set_cachesize") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.cachesize = NUM2INT(value);
    }
    else if (strcmp(options, "set_pagesize") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.psize = NUM2INT(value);
    }
    else if (strcmp(options, "set_lorder") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.lorder = NUM2INT(value);
    }
    else if (strcmp(options, "set_array_base") == 0 ||
	     strcmp(options, "array_base") == 0) {
	int ary_base = NUM2INT(value);
	switch (ary_base) {
	case 0: dbst->array_base = 1; break;
	case 1: dbst->array_base = 0; break;
	default: rb_raise(bdb_eFatal, "array base must be 0 or 1");
	}
    }
    else if (strcmp(options, "marshal") == 0) {
        switch (value) {
        case Qtrue: dbst->marshal = bdb_mMarshal; break;
        case Qfalse: dbst->marshal = Qfalse; break;
        default: 
	    if (!rb_respond_to(value, id_load) ||
		!rb_respond_to(value, id_dump)) {
		rb_raise(bdb_eFatal, "marshal value must be true or false");
	    }
	    dbst->marshal = value;
	    break;
        }
    }
    return Qnil;
}

static VALUE
bdb_open_common(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE a, b, c, d, f, res;
    int nb, type, mode, oflags;
    char *name;
    bdb_DB *dbst;

    f = Qnil; name = NULL;
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    oflags = DB_RDONLY;
    if (argc && TYPE(argv[argc - 1]) == T_HASH) {
	f = argv[argc - 1];
	argc--;
    }
    switch(nb = rb_scan_args(argc, argv, "13", &a, &b, &c, &d)) {
    case 4:
	mode = NUM2INT(d);
    case 3:
	if (TYPE(c) == T_STRING) {
	    if (strcmp(RSTRING(c)->ptr, "r") == 0)
		oflags = DB_RDONLY;
	    else if (strcmp(RSTRING(c)->ptr, "r+") == 0)
		oflags = 0;
	    else if (strcmp(RSTRING(c)->ptr, "w") == 0 ||
		     strcmp(RSTRING(c)->ptr, "w+") == 0)
		oflags = DB_CREATE | DB_TRUNCATE | DB_WRITE;
	    else if (strcmp(RSTRING(c)->ptr, "a") == 0 ||
		     strcmp(RSTRING(c)->ptr, "a+") == 0)
		oflags = DB_CREATE | DB_WRITE;
	    else {
		rb_raise(bdb_eFatal, "flags must be r, r+, w, w+ a or a+");
	    }
	}
	else if (c == Qnil)
	    oflags = 0;
	else
	    oflags = NUM2INT(c);
    case 2:
	if (!NIL_P(b)) {
	    Check_SafeStr(b);
	    name = RSTRING(b)->ptr;
	}
	else {
	    name = NULL;
	}
    case 1:
	switch (NUM2INT(a)) {
	case DB_BTREE:
	    type = 0;
	    break;
	case DB_HASH:
	    type = 1;
	    break;
	case DB_RECNO:
	    type = 2;
	    break;
	default:
	    rb_raise(bdb_eFatal, "Unknown db185 type %d", NUM2INT(b));
	}
    }
    res = Data_Make_Struct(obj, bdb_DB, bdb_mark, bdb_free, dbst);
    dbst->type = NUM2INT(a);
    if (!NIL_P(f)) {
	if (TYPE(f) != T_HASH) {
            rb_raise(bdb_eFatal, "options must be an hash");
	}
	switch(type) {
	case 0:
	    rb_iterate(rb_each, f, bdb_i185_btree, res);
	    break;
	case 1:
	    rb_iterate(rb_each, f, bdb_i185_hash, res);
	    break;
	case 2:
	    rb_iterate(rb_each, f, bdb_i185_recno, res);
	    break;
	}
    }
    if (name == NULL) oflags = O_CREAT | O_RDWR;
    if ((dbst->dbp = dbopen(name, oflags, mode, type, 
			     (void *)dbst->has_info?&dbst->info:NULL)) == NULL) {
	rb_raise(bdb_eFatal, "Failed `%s'", db_strerror(errno));
    }
    return res;
}

#define GetDB(obj, dbst)						\
{									\
    Data_Get_Struct(obj, bdb_DB, dbst);					\
    if (dbst->dbp == 0) {						\
        rb_raise(bdb_eFatal, "closed DB");				\
    }									\
    if (dbst->options & BDB_NEED_CURRENT) {				\
    	rb_thread_local_aset(rb_thread_current(), id_current_db, obj);	\
    }									\
}

static VALUE
bdb_close(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE opt;
    bdb_DB *dbst;
    if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4)
	rb_raise(rb_eSecurityError, "Insecure: can't close the database");
    Data_Get_Struct(obj, bdb_DB, dbst);
    if (dbst->dbp != NULL) {
	test_error(dbst->dbp->close(dbst->dbp));
	dbst->dbp = NULL;
    }
    return Qnil;
}

static VALUE bdb_recno_length(VALUE);

static VALUE
bdb_s_new(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE *nargv;
    VALUE res, cl, bb;
    bdb_DB *ret;

    cl = obj;
    while (cl) {
	if (cl == bdb_cBtree || RCLASS(cl)->m_tbl == RCLASS(bdb_cBtree)->m_tbl) {
	    bb = INT2NUM(DB_BTREE); 
	    break;
	}
	else if (cl == bdb_cHash || RCLASS(cl)->m_tbl == RCLASS(bdb_cHash)->m_tbl) {
	    bb = INT2NUM(DB_HASH); 
	    break;
	}
	else if (cl == bdb_cRecno || RCLASS(cl)->m_tbl == RCLASS(bdb_cRecno)->m_tbl) {
	    bb = INT2NUM(DB_RECNO);
	    break;
	}
	cl = RCLASS(cl)->super;
    }
    if (!cl) {
	rb_raise(bdb_eFatal, "unknown database type");
    }
    nargv = ALLOCA_N(VALUE, argc + 1);
    nargv[0] = bb;
    MEMCPY(nargv + 1, argv, VALUE, argc);
    res = bdb_open_common(argc + 1, nargv, obj);
    Data_Get_Struct(res, bdb_DB, ret);
    rb_obj_call_init(res, argc, argv);
    return res;
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

static VALUE
bdb_append(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    bdb_DB *dbst;
    DBT key, data;
    db_recno_t recno;
    int i;
    VALUE *a;

    rb_secure(4);
    if (argc < 1)
	return obj;
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    test_error(dbst->dbp->seq(dbst->dbp, &key, &data, R_LAST));
    for (i = 0, a = argv; i < argc; i++, a++) {
	memset(&data, 0, sizeof(data));
	test_dump(dbst, data, *a);
	test_error(dbst->dbp->put(dbst->dbp, &key, &data, R_IAFTER));
    }
    return obj;
}

static VALUE bdb_get();

static VALUE
bdb_put(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE a, b, c;
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    flags = 0;
    a = b = c = Qnil;
    if (rb_scan_args(argc, argv, "21", &a, &b, &c) == 3) {
        flags = NUM2INT(c);
    }
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    test_recno(dbst, key, recno, a);
    test_dump(dbst, data, b);
    ret = test_error(dbst->dbp->put(dbst->dbp, &key, &data, flags));
    if (ret == DB_KEYEXIST)
	return Qfalse;
    else {
	return b;
    }
}

static VALUE
test_load_key(dbst, key)
    bdb_DB *dbst;
    DBT key;
{
    if (dbst->type == DB_RECNO)
        return INT2NUM((*(db_recno_t *)key.data) - dbst->array_base);
    return test_load(dbst, key);
}

static VALUE
bdb_assoc(dbst, recno, key, data)
    bdb_DB *dbst;
    int recno;
    DBT key, data;
{
    return rb_assoc_new(test_load_key(dbst, key), test_load(dbst, data));
}

static VALUE
bdb_assoc_dyna(obj, key, data)
    VALUE obj;
    DBT key, data;
{
    bdb_DB *dbst;
    GetDB(obj, dbst);
    return rb_assoc_new(test_load_key(dbst, key), test_load_dyna(obj, key, data));
}

static VALUE
bdb_get_internal(argc, argv, obj, notfound, dyna)
    int argc;
    VALUE *argv;
    VALUE obj;
    VALUE notfound;
    int dyna;
{
    VALUE a, b, c;
    bdb_DB *dbst;
    DBT key, data;
    DBT datas;
    int flagss;
    int ret, flags;
    db_recno_t recno;

    flagss = flags = 0;
    GetDB(obj, dbst);
    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));
    DATA_ZERO(data);
    switch(rb_scan_args(argc, argv, "12", &a, &b, &c)) {
    case 3:
        flags = NUM2INT(c);
	break;
    case 2:
	flagss = flags = NUM2INT(b);
	break;
    }
    test_recno(dbst, key, recno, a);
    ret = test_error(dbst->dbp->get(dbst->dbp, &key, &data, flags));
    if (ret == DB_NOTFOUND)
        return notfound;
    if (dyna) {
	return test_load_dyna(obj, key, data);
    }
    else {
	return test_load(dbst, data);
    }
}

static VALUE
bdb_get(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    return bdb_get_internal(argc, argv, obj, Qnil, 0);
}

static VALUE
bdb_get_dyna(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    return bdb_get_internal(argc, argv, obj, Qnil, 1);
}

static VALUE
bdb_has_key(obj, key)
    VALUE obj, key;
{
    return bdb_get_internal(1, &key, obj, Qfalse);
}

static VALUE
bdb_del(obj, a)
    VALUE a, obj;
{
    bdb_DB *dbst;
    DBT key;
    int ret;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    if (dbst->type == DB_HASH) {
	rb_warning("delete can give strange result with DB_HASH");
    }
    memset(&key, 0, sizeof(DBT));
    test_recno(dbst, key, recno, a);
    ret = test_error(dbst->dbp->del(dbst->dbp, &key, 0));
    if (ret == DB_NOTFOUND)
        return Qnil;
    else
        return obj;
}

static VALUE
bdb_intern_shift_pop(obj, depart)
    VALUE obj;
    int depart;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, depart));
    if (ret == DB_NOTFOUND) {
        return Qnil;
    }
    test_error(dbst->dbp->del(dbst->dbp, &key, R_CURSOR));
    return bdb_assoc(dbst, recno, key, data);
}

static VALUE
bdb_shift(obj)
    VALUE obj;
{
    return bdb_intern_shift_pop(obj, DB_FIRST);
}

static VALUE
bdb_pop(obj)
    VALUE obj;
{
    return bdb_intern_shift_pop(obj, DB_LAST);
}

static VALUE
bdb_empty(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, DB_FIRST | flags));
    if (ret == DB_NOTFOUND) {
        return Qtrue;
    }
    free_key(dbst, key);
    return Qfalse;
}

static VALUE
bdb_delete_if(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data, save;
    int ret, ret1, flags;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
	flags = DB_NEXT;
        if (RTEST(rb_yield(bdb_assoc(dbst, recno, key, data)))) {
            test_error(dbst->dbp->del(dbst->dbp, &key, 0));
        }
    } while (1);
    return obj;
}

static VALUE
bdb_length(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, value, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    value = 0;
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return INT2NUM(value);
        }
	flags = DB_NEXT;
	free_key(dbst, key);
        value++;
    } while (1);
    return INT2NUM(value);
}

static VALUE
bdb_each_valuec(obj, sens)
    VALUE obj;
    int sens;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
	flags = sens;
	free_key(dbst, key);
        rb_yield(test_load(dbst, data));
    } while (1);
}

static VALUE bdb_each_value(obj) VALUE obj;{ return bdb_each_valuec(obj, DB_NEXT); }
static VALUE bdb_each_eulav(obj) VALUE obj;{ return bdb_each_valuec(obj, DB_PREV); }

static VALUE
bdb_each_keyc(obj, sens)
    VALUE obj;
    int sens;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
	rb_yield(test_load_key(dbst, key));
	flags = sens;
    } while (1);
    return obj;
}

static VALUE bdb_each_key(obj) VALUE obj;{ return bdb_each_keyc(obj, DB_NEXT); }
static VALUE bdb_each_yek(obj) VALUE obj;{ return bdb_each_keyc(obj, DB_PREV); }

static VALUE
bdb_each_common(obj, sens)
    VALUE obj;
    int sens;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
        rb_yield(bdb_assoc(dbst, recno, key, data));
	flags = sens;
    } while (1);
    return obj;
}

static VALUE bdb_each_pair(obj) VALUE obj;{ return bdb_each_common(obj, DB_NEXT); }
static VALUE bdb_each_riap(obj) VALUE obj;{ return bdb_each_common(obj, DB_PREV); }

static VALUE
bdb_to_a(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE ary;

    GetDB(obj, dbst);
    ary = rb_ary_new();
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return ary;
        }
        rb_ary_push(ary, bdb_assoc(dbst, recno, key, data));
	flags = DB_NEXT;
    } while (1);
    return ary;
}

static VALUE
bdb_to_hash(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE hash;

    GetDB(obj, dbst);
    hash = rb_hash_new();
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return hash;
        }
        rb_hash_aset(hash, test_load_key(dbst, key), test_load(dbst, data));
	flags = DB_NEXT;
    } while (1);
    return hash;
}

static VALUE
bdb_reject(obj)
    VALUE obj;
{
    return rb_hash_delete_if(bdb_to_hash(obj));
}
 
static VALUE
bdb_values(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE ary;

    GetDB(obj, dbst);
    ary = rb_ary_new();
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return ary;
        }
	flags = DB_NEXT;
	free_key(dbst, key);
        rb_ary_push(ary, test_load(dbst, data));
    } while (1);
    return ary;
}

static VALUE
bdb_internal_value(obj, a, b)
    VALUE obj, a, b;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
	if (ret == DB_NOTFOUND) {
	    return Qfalse;
	}
	flags = DB_NEXT;
	if (rb_equal(a, test_load(dbst, data)) == Qtrue) {
	    VALUE d;
	    
	    d = (b == Qfalse)?Qtrue:test_load_key(dbst, key);
	    free_key(dbst, key);
	    return  d;
	}
	free_key(dbst, key);
    } while (1);
    return Qfalse;
}

static VALUE
bdb_index(obj, a)
    VALUE obj, a;
{
    return bdb_internal_value(obj, a, Qtrue);
}

static VALUE
bdb_indexes(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    VALUE indexes;
    int i;

    indexes = rb_ary_new2(argc);
    for (i = 0; i < argc; i++) {
	RARRAY(indexes)->ptr[i] = bdb_get(1, &argv[i], obj);
    }
    RARRAY(indexes)->len = i;
    return indexes;
}

static VALUE
bdb_has_value(obj, a)
    VALUE obj, a;
{
    return bdb_internal_value(obj, a, Qfalse);
}

static VALUE
bdb_keys(obj)
    VALUE obj;
{
    bdb_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE ary;

    GetDB(obj, dbst);
    ary = rb_ary_new();
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return ary;
        }
	rb_ary_push(ary, test_load_key(dbst, key));
	free_key(dbst, key);
	flags = DB_NEXT;
    } while (1);
    return ary;
}

static VALUE
bdb_sync(obj)
    VALUE obj;
{
    bdb_DB *dbst;

    if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4)
	rb_raise(rb_eSecurityError, "Insecure: can't sync the database");
    GetDB(obj, dbst);
    test_error(dbst->dbp->sync(dbst->dbp, 0));
    return Qtrue;
}

void
Init_bdb1()
{
    bdb_mMarshal = rb_const_get(rb_cObject, rb_intern("Marshal"));
    id_dump = rb_intern("dump");
    id_load = rb_intern("load");
    id_current_db = rb_intern("bdb_current_db");
    id_bt_compare = rb_intern("bdb_bt_compare");
    id_bt_prefix = rb_intern("bdb_bt_prefix");
    id_dup_compare = rb_intern("bdb_dup_compare");
    id_h_hash = rb_intern("bdb_h_hash");
    id_proc_call = rb_intern("call");
    bdb_mDb = rb_define_module("BDB1");
    bdb_eFatal = rb_define_class_under(bdb_mDb, "Fatal", rb_eStandardError);
/* CONSTANT */
    rb_define_const(bdb_mDb, "VERSION_MAJOR", INT2FIX(DB_VERSION_MAJOR));
    rb_define_const(bdb_mDb, "VERSION_MINOR", INT2FIX(DB_VERSION_MINOR));
    rb_define_const(bdb_mDb, "RELEASE_PATCH", INT2FIX(DB_RELEASE_PATCH));
    rb_define_const(bdb_mDb, "VERSION", rb_str_new2("1.x.x"));
    rb_define_const(bdb_mDb, "BTREE", INT2FIX(DB_BTREE));
    rb_define_const(bdb_mDb, "HASH", INT2FIX(DB_HASH));
    rb_define_const(bdb_mDb, "RECNO", INT2FIX(DB_RECNO));
    rb_define_const(bdb_mDb, "AFTER", INT2FIX(DB_AFTER));
    rb_define_const(bdb_mDb, "BEFORE", INT2FIX(DB_BEFORE));
    rb_define_const(bdb_mDb, "CREATE", INT2FIX(DB_CREATE));
    rb_define_const(bdb_mDb, "DUP", INT2FIX(DB_DUP));
    rb_define_const(bdb_mDb, "FIRST", INT2FIX(DB_FIRST));
    rb_define_const(bdb_mDb, "LAST", INT2FIX(DB_LAST));
    rb_define_const(bdb_mDb, "NEXT", INT2FIX(DB_NEXT));
    rb_define_const(bdb_mDb, "PREV", INT2FIX(DB_PREV));
    rb_define_const(bdb_mDb, "RDONLY", INT2FIX(DB_RDONLY));
    rb_define_const(bdb_mDb, "SET_RANGE", INT2FIX(DB_SET_RANGE));
    rb_define_const(bdb_mDb, "TRUNCATE", INT2FIX(DB_TRUNCATE));
    rb_define_const(bdb_mDb, "NOOVERWRITE", INT2FIX(DB_NOOVERWRITE));
/* DATABASE */
    bdb_cCommon = rb_define_class_under(bdb_mDb, "Common", rb_cObject);
    rb_define_private_method(bdb_cCommon, "initialize", bdb_obj_init, -1);
    rb_include_module(bdb_cCommon, rb_mEnumerable);
    rb_define_singleton_method(bdb_cCommon, "new", bdb_s_new, -1);
    rb_define_singleton_method(bdb_cCommon, "create", bdb_s_new, -1);
    rb_define_singleton_method(bdb_cCommon, "open", bdb_s_new, -1);
    rb_define_method(bdb_cCommon, "close", bdb_close, -1);
    rb_define_method(bdb_cCommon, "db_close", bdb_close, -1);
    rb_define_method(bdb_cCommon, "put", bdb_put, -1);
    rb_define_method(bdb_cCommon, "db_put", bdb_put, -1);
    rb_define_method(bdb_cCommon, "[]=", bdb_put, -1);
    rb_define_method(bdb_cCommon, "store", bdb_put, -1);
    rb_define_method(bdb_cCommon, "get", bdb_get_dyna, -1);
    rb_define_method(bdb_cCommon, "db_get", bdb_get_dyna, -1);
    rb_define_method(bdb_cCommon, "[]", bdb_get_dyna, -1);
    rb_define_method(bdb_cCommon, "fetch", bdb_get_dyna, -1);
    rb_define_method(bdb_cCommon, "delete", bdb_del, 1);
    rb_define_method(bdb_cCommon, "del", bdb_del, 1);
    rb_define_method(bdb_cCommon, "db_del", bdb_del, 1);
    rb_define_method(bdb_cCommon, "sync", bdb_sync, 0);
    rb_define_method(bdb_cCommon, "db_sync", bdb_sync, 0);
    rb_define_method(bdb_cCommon, "flush", bdb_sync, 0);
    rb_define_method(bdb_cCommon, "each", bdb_each_pair, 0);
    rb_define_method(bdb_cCommon, "each_value", bdb_each_value, 0);
    rb_define_method(bdb_cCommon, "reverse_each_value", bdb_each_eulav, 0);
    rb_define_method(bdb_cCommon, "each_key", bdb_each_key, 0);
    rb_define_method(bdb_cCommon, "reverse_each_key", bdb_each_yek, 0);
    rb_define_method(bdb_cCommon, "each_pair", bdb_each_pair, 0);
    rb_define_method(bdb_cCommon, "reverse_each", bdb_each_riap, 0);
    rb_define_method(bdb_cCommon, "reverse_each_pair", bdb_each_riap, 0);
    rb_define_method(bdb_cCommon, "keys", bdb_keys, 0);
    rb_define_method(bdb_cCommon, "values", bdb_values, 0);
    rb_define_method(bdb_cCommon, "delete_if", bdb_delete_if, 0);
    rb_define_method(bdb_cCommon, "reject!", bdb_delete_if, 0);
    rb_define_method(bdb_cCommon, "reject", bdb_reject, 0);
    rb_define_method(bdb_cCommon, "include?", bdb_has_key, 1);
    rb_define_method(bdb_cCommon, "has_key?", bdb_has_key, 1);
    rb_define_method(bdb_cCommon, "key?", bdb_has_key, 1);
    rb_define_method(bdb_cCommon, "member?", bdb_has_key, 1);
    rb_define_method(bdb_cCommon, "has_value?", bdb_has_value, 1);
    rb_define_method(bdb_cCommon, "value?", bdb_has_value, 1);
    rb_define_method(bdb_cCommon, "to_a", bdb_to_a, 0);
    rb_define_method(bdb_cCommon, "to_hash", bdb_to_hash, 0);
    rb_define_method(bdb_cCommon, "empty?", bdb_empty, 0);
    rb_define_method(bdb_cCommon, "length", bdb_length, 0);
    rb_define_alias(bdb_cCommon,  "size", "length");
    rb_define_method(bdb_cCommon, "index", bdb_index, 1);
    rb_define_method(bdb_cCommon, "indexes", bdb_indexes, -1);
    rb_define_method(bdb_cCommon, "indices", bdb_indexes, -1);
    bdb_cBtree = rb_define_class_under(bdb_mDb, "Btree", bdb_cCommon);
    bdb_cHash = rb_define_class_under(bdb_mDb, "Hash", bdb_cCommon);
    rb_undef_method(bdb_cHash, "delete_if");
    rb_undef_method(bdb_cHash, "reverse_each_value");
    rb_undef_method(bdb_cHash, "reverse_each_key");
    rb_undef_method(bdb_cHash, "reverse_each_pair");
    rb_undef_method(bdb_cHash, "reverse_each");
    bdb_cRecno = rb_define_class_under(bdb_mDb, "Recno", bdb_cCommon);
    rb_define_method(bdb_cRecno, "shift", bdb_shift, 0);
    rb_define_method(bdb_cRecno, "push", bdb_append, -1);
    rb_define_method(bdb_cRecno, "pop", bdb_pop, 0);
    bdb_cUnknown = rb_define_class_under(bdb_mDb, "Unknown", bdb_cCommon);
    bdb_errstr = rb_tainted_str_new(0, 0);
    rb_global_variable(&bdb_errstr);
    bdb_init_delegator();
}
