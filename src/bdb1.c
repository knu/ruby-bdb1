#include "bdb1.h"
#include "version.h"

VALUE bdb1_eFatal;
VALUE bdb1_mDb, bdb1_mMarshal, bdb1_cCommon, bdb1_cRecnum;
static ID id_dump, id_load;

ID bdb1_id_current_db;

static VALUE bdb1_cBtree, bdb1_cHash, bdb1_cUnknown;

static ID id_bt_compare, id_bt_prefix, id_h_hash, bdb1_id_call;

static VALUE bdb1_errstr;
static int bdb1_errcall = 0;

static char *
db_strerror(int err)
{
    if (err == 0)
        return "" ;

    if (err > 0)
        return strerror(err) ;

    return "Unknown Error" ;
}

int
bdb1_test_error(comm)
    int comm;
{
    VALUE error;

    switch (comm) {
    case 0:
    case 1:
        break;
    default:
        error = bdb1_eFatal;
        if (bdb1_errcall) {
            bdb1_errcall = 0;
            rb_raise(error, "%s -- %s", StringValuePtr(bdb1_errstr), db_strerror(comm));
        }
        else
            rb_raise(error, "%s", db_strerror(errno));
    }
    return comm;
}

static VALUE
test_dump(obj, key, a, type_kv)
    VALUE obj;
    DBT *key;
    VALUE a;
    int type_kv;
{
    bdb1_DB *dbst;
    int is_nil = 0;
    VALUE tmp = a;

    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->filter[type_kv]) {
	if (FIXNUM_P(dbst->filter[type_kv])) {
	    tmp = rb_funcall(obj, NUM2INT(dbst->filter[type_kv]), 1, a);
	}
	else {
	    tmp = rb_funcall(dbst->filter[type_kv], bdb1_id_call, 1, a);
 	}
    }
    if (dbst->marshal) {
        if (rb_obj_is_kind_of(a, bdb1_cDelegate)) {
	    tmp = bdb1_deleg_to_orig(tmp);
	}
	tmp = rb_funcall(dbst->marshal, id_dump, 1, tmp);
	if (TYPE(tmp) != T_STRING) {
 	    rb_raise(rb_eTypeError, "dump() must return String");
 	}
    }
    else {
        tmp = rb_obj_as_string(tmp);
        if (a == Qnil)
            is_nil = 1;
    }
    key->data = StringValuePtr(tmp);
    key->size = RSTRING(tmp)->len + is_nil;
    return tmp;
}

static VALUE
test_ret(obj, tmp, a, type_kv)
    VALUE obj, tmp, a;
    int type_kv;
{
    bdb1_DB *dbst;
    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->marshal || a == Qnil) {
	return a;
    }
    if (dbst->filter[type_kv]) {
	return rb_obj_as_string(a);
    }
    return tmp;
}

static VALUE
test_recno(obj, key, recno, a)
    VALUE obj;
    DBT *key;
    db_recno_t *recno;
    VALUE a;
{
    bdb1_DB *dbst;
    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->type == DB_RECNO) {
        *recno = NUM2INT(a) + dbst->array_base;
        key->data = recno;
        key->size = sizeof(db_recno_t);
	return a;
    }
    return test_dump(obj, key, a, FILTER_KEY);
}

VALUE
bdb1_test_load(obj, a, type_kv)
    VALUE obj;
    DBT *a;
    int type_kv;
{
    VALUE res;
    int i;
    bdb1_DB *dbst;

    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->marshal) {
	res = rb_str_new(a->data, a->size);
	if (dbst->filter[2 + type_kv]) {
	    if (FIXNUM_P(dbst->filter[2 + type_kv])) {
		res = rb_funcall(obj, 
				 NUM2INT(dbst->filter[2 + type_kv]), 1, res);
	    }
	    else {
		res = rb_funcall(dbst->filter[2 + type_kv],
				 bdb1_id_call, 1, res);
	    }
	}
	res = rb_funcall(dbst->marshal, id_load, 1, res);
    }
    else {
	if (a->size == 1 && ((char *)a->data)[0] == '\000') {
	    res = Qnil;
	}
	else {
	    res = rb_tainted_str_new(a->data, a->size);
	    if (dbst->filter[2 + type_kv]) {
		if (FIXNUM_P(dbst->filter[2 + type_kv])) {
		    res = rb_funcall(obj, 
				     NUM2INT(dbst->filter[2 + type_kv]),
				     1, res);
		}
		else {
		    res = rb_funcall(dbst->filter[2 + type_kv],
				     bdb1_id_call, 1, res);
		}
	    }
	}
    }
    return res;
}

static VALUE
test_load_dyna(obj, key, val)
    VALUE obj;
    DBT *key, *val;
{
    bdb1_DB *dbst;
    VALUE del, res, tmp;
    struct deleg_class *delegst;
    
    Data_Get_Struct(obj, bdb1_DB, dbst);
    res = bdb1_test_load(obj, val, FILTER_VALUE);
    if (dbst->marshal && !SPECIAL_CONST_P(res)) {
	del = Data_Make_Struct(bdb1_cDelegate, struct deleg_class, 
			       bdb1_deleg_mark, bdb1_deleg_free, delegst);
	delegst->db = obj;
	if (dbst->type == DB_RECNO) {
	    tmp = INT2NUM((*(db_recno_t *)key->data) - dbst->array_base);
	}
	else {
	    tmp = rb_str_new(key->data, key->size);
	}
	delegst->key = rb_funcall(dbst->marshal, id_load, 1, tmp);
	delegst->obj = res;
	res = del;
    }
    return res;
}


static int
bdb1_bt_compare(a, b)
    DBT *a, *b;
{
    VALUE obj, av, bv, res;
    bdb1_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), bdb1_id_current_db)) == Qnil) {
	rb_raise(bdb1_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    av = bdb1_test_load(obj, a, FILTER_VALUE);
    bv = bdb1_test_load(obj, b, FILTER_VALUE);
    if (dbst->bt_compare == 0)
	res = rb_funcall(obj, id_bt_compare, 2, av, bv);
    else
	res = rb_funcall(dbst->bt_compare, bdb1_id_call, 2, av, bv);
    return NUM2INT(res);
} 

static size_t
bdb1_bt_prefix(a, b)
    DBT *a, *b;
{
    VALUE obj, av, bv, res;
    bdb1_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), bdb1_id_current_db)) == Qnil) {
	rb_raise(bdb1_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    av = bdb1_test_load(obj, a, FILTER_VALUE);
    bv = bdb1_test_load(obj, b, FILTER_VALUE);
    if (dbst->bt_prefix == 0)
	res = rb_funcall(obj, id_bt_prefix, 2, av, bv);
    else
	res = rb_funcall(dbst->bt_prefix, bdb1_id_call, 2, av, bv);
    return NUM2INT(res);
} 

static u_int32_t
bdb1_h_hash(bytes, length)
    void *bytes;
    u_int32_t length;
{
    VALUE obj, st, res;
    bdb1_DB *dbst;

    if ((obj = rb_thread_local_aref(rb_thread_current(), bdb1_id_current_db)) == Qnil) {
	rb_raise(bdb1_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    st = rb_tainted_str_new((char *)bytes, length);
    if (dbst->h_hash == 0)
	res = rb_funcall(obj, id_h_hash, 1, st);
    else
	res = rb_funcall(dbst->h_hash, bdb1_id_call, 1, st);
    return NUM2UINT(res);
}

static void
bdb1_i_close(dbst)
    bdb1_DB *dbst;
{
    if (dbst->dbp != NULL && !(dbst->options & BDB1_NOT_OPEN)) {
	dbst->options |= BDB1_NOT_OPEN;
	bdb1_test_error(dbst->dbp->close(dbst->dbp));
    }
    dbst->dbp = NULL;
}

static void
bdb1_free(dbst)
    bdb1_DB *dbst;
{
    bdb1_i_close(dbst);
    free(dbst);
}

static void
bdb1_mark(dbst)
    bdb1_DB *dbst;
{
    int i;

    rb_gc_mark(dbst->marshal);
    rb_gc_mark(dbst->bt_compare);
    rb_gc_mark(dbst->bt_prefix);
    rb_gc_mark(dbst->h_hash);
    for (i = 0; i < 4; i++) {
	rb_gc_mark(dbst->filter[i]);
    }
}

static VALUE
bdb1_i185_btree(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb1_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb1_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = StringValuePtr(key);
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
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->has_info = Qtrue;
	dbst->options |= BDB1_BT_COMPARE;
	dbst->bt_compare = value;
	dbst->info.bi.compare = bdb1_bt_compare;
    }
    else if (strcmp(options, "set_bt_prefix") == 0) {
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->has_info = Qtrue;
	dbst->options |= BDB1_BT_PREFIX;
	dbst->bt_prefix = value;
	dbst->info.bi.prefix = bdb1_bt_prefix;
    }
    else if (strcmp(options, "set_lorder") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.bi.lorder = NUM2INT(value);
    }
    return Qnil;
}

static VALUE
bdb1_i185_hash(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb1_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb1_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = StringValuePtr(key);
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
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->has_info = Qtrue;
	dbst->options |= BDB1_H_HASH;
	dbst->h_hash = value;
	dbst->info.hi.hash = bdb1_h_hash;
    }
    else if (strcmp(options, "set_lorder") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.hi.lorder = NUM2INT(value);
    }
    return Qnil;
}

static VALUE
bdb1_i185_recno(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb1_DB *dbst;
    char *options, *str;

    Data_Get_Struct(dbstobj, bdb1_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = StringValuePtr(key);
    if (strcmp(options, "set_flags") == 0) {
	dbst->has_info = Qtrue;
	dbst->info.ri.flags = NUM2INT(value);
    }
    else if (strcmp(options, "set_re_delim") == 0) {
	int ch;
	if (TYPE(value) == T_STRING) {
	    str = StringValuePtr(value);
	    dbst->info.ri.bval = str[0];
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
	    str = StringValuePtr(value);
	    dbst->info.ri.bval = str[0];
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
	default: rb_raise(bdb1_eFatal, "array base must be 0 or 1");
	}
    }
    return Qnil;
}

static VALUE
bdb1_i185_common(obj, dbstobj)
    VALUE obj, dbstobj;
{
    VALUE key, value;
    bdb1_DB *dbst;
    char *options;

    Data_Get_Struct(dbstobj, bdb1_DB, dbst);
    key = rb_ary_entry(obj, 0);
    value = rb_ary_entry(obj, 1);
    key = rb_obj_as_string(key);
    options = StringValuePtr(key);
    if (strcmp(options, "marshal") == 0) {
        switch (value) {
        case Qtrue: 
	    dbst->marshal = bdb1_mMarshal;
	    dbst->options |= BDB1_MARSHAL;
	    break;
        case Qfalse: 
	    dbst->marshal = Qfalse;
	    dbst->options &= ~BDB1_MARSHAL;
	    break;
        default: 
	    if (!rb_respond_to(value, id_load) ||
		!rb_respond_to(value, id_dump)) {
		rb_raise(bdb1_eFatal, "marshal value must be true or false");
	    }
	    dbst->marshal = value;
	    dbst->options |= BDB1_MARSHAL;
	    break;
        }
    }
    else if (strcmp(options, "set_store_key") == 0) {
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->filter[FILTER_KEY] = value;
    }
    else if (strcmp(options, "set_fetch_key") == 0) {
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->filter[2 + FILTER_KEY] = value;
    }
    else if (strcmp(options, "set_store_value") == 0) {
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->filter[FILTER_VALUE] = value;
    }
    else if (strcmp(options, "set_fetch_value") == 0) {
        if (!rb_respond_to(value, bdb1_id_call)) {
            rb_raise(bdb1_eFatal, "arg must respond to #call");
        }
	dbst->filter[2 + FILTER_VALUE] = value;
    }
    return Qnil;
}

static int
bdb1_hard_count(dbp)
    DB *dbp;
{
    DBT key, data;
    db_recno_t recno;
    long count = 0;

    DATA_ZERO(key);
    key.data = &recno;
    key.size = sizeof(db_recno_t);
    DATA_ZERO(data);
    if (bdb1_test_error(dbp->seq(dbp, &key, &data, DB_LAST)) == 0) {
	count = *(db_recno_t *)key.data;
    }
    return count;
}

VALUE
bdb1_init(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE b, c, d, f;
    int mode, oflags;
    char *name;
    bdb1_DB *dbst;

    f = Qnil; name = NULL;
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    oflags = DB_RDONLY;
    if (argc && TYPE(argv[argc - 1]) == T_HASH) {
	f = argv[argc - 1];
	argc--;
    }
    switch(rb_scan_args(argc, argv, "03", &b, &c, &d)) {
    case 3:
	mode = NUM2INT(d);
	/* ... */
    case 2:
	if (TYPE(c) == T_STRING) {
	    char *m = StringValuePtr(c);
	    if (strcmp(m, "r") == 0) {
		oflags = DB_RDONLY;
	    }
	    else if (strcmp(m, "r+") == 0) {
		oflags = DB_WRITE;
	    }
	    else if (strcmp(m, "w") == 0 || strcmp(m, "w+") == 0) {
		oflags = DB_CREATE | DB_TRUNCATE | DB_WRITE;
	    }
	    else if (strcmp(m, "a") == 0 || strcmp(m, "a+") == 0) {
		oflags = DB_CREATE | DB_WRITE;
	    }
	    else {
		rb_raise(bdb1_eFatal, "flags must be r, r+, w, w+, a or a+");
	    }
	}
	else if (c == Qnil) {
	    oflags = DB_RDONLY;
	}
	else {
	    oflags = NUM2INT(c);
	}
	/* ... */
    case 1:
	if (!NIL_P(b)) {
	    SafeStringValue(b);
	    name = StringValuePtr(b);
	}
	else {
	    name = NULL;
	}
	/* ... */
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->type < DB_BTREE || dbst->type > DB_RECNO) {
	rb_raise(bdb1_eFatal, "Unknown db185 type %d", dbst->type);
    }
    if (!NIL_P(f)) {
	if (TYPE(f) != T_HASH) {
            rb_raise(bdb1_eFatal, "options must be an hash");
	}
	switch(dbst->type) {
	case 0:
	    rb_iterate(rb_each, f, bdb1_i185_btree, obj);
	    if (dbst->bt_compare == 0 && rb_respond_to(obj, id_bt_compare)) {
		dbst->has_info = Qtrue;
		dbst->options |= BDB1_BT_COMPARE;
		dbst->info.bi.compare = bdb1_bt_compare;
	    }
	    if (dbst->bt_prefix == 0 && rb_respond_to(obj, id_bt_prefix)) {
		dbst->has_info = Qtrue;
		dbst->options |= BDB1_BT_PREFIX;
		dbst->info.bi.prefix = bdb1_bt_prefix;
	    }
	    break;
	case 1:
	    rb_iterate(rb_each, f, bdb1_i185_hash, obj);
	    if (dbst->h_hash == 0 && rb_respond_to(obj, id_h_hash)) {
		dbst->has_info = Qtrue;
		dbst->options |= BDB1_H_HASH;
		dbst->info.hi.hash = bdb1_h_hash;
	    }
	    break;
	case 2:
	    rb_iterate(rb_each, f, bdb1_i185_recno, obj);
	    break;
	}
	rb_iterate(rb_each, f, bdb1_i185_common, obj);
    }
    if (name == NULL) oflags = O_CREAT | O_RDWR;
    if ((dbst->dbp = dbopen(name, oflags, mode, dbst->type, 
			     (void *)dbst->has_info?&dbst->info:NULL)) == NULL) {
	rb_raise(bdb1_eFatal, "Failed `%s'", db_strerror(errno));
    }
    dbst->options &= ~BDB1_NOT_OPEN;
    if (dbst->type == 2) {
	dbst->len = bdb1_hard_count(dbst->dbp);
    }
    return obj;
}

static VALUE
bdb1_close(obj)
    VALUE obj;
{
    VALUE opt;
    bdb1_DB *dbst;

    if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4) {
	rb_raise(rb_eSecurityError, "Insecure: can't close the database");
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    bdb1_i_close(dbst);
    return Qnil;
}

VALUE
bdb1_s_alloc(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    VALUE res, cl;

    res = Data_Make_Struct(obj, bdb1_DB, bdb1_mark, bdb1_free, dbst);
    dbst->options |= BDB1_NOT_OPEN;
    cl = obj;
    while (cl) {
	if (cl == bdb1_cBtree || RCLASS(cl)->m_tbl == RCLASS(bdb1_cBtree)->m_tbl) {
	    dbst->type = DB_BTREE; 
	    break;
	}
	else if (cl == bdb1_cHash || RCLASS(cl)->m_tbl == RCLASS(bdb1_cHash)->m_tbl) {
	    dbst->type = DB_HASH; 
	    break;
	}
	else if (cl == bdb1_cRecnum || RCLASS(cl)->m_tbl == RCLASS(bdb1_cRecnum)->m_tbl) {
	    dbst->type = DB_RECNO;
	    break;
	}
	cl = RCLASS(cl)->super;
    }
    if (!cl) {
	rb_raise(bdb1_eFatal, "unknown database type");
    }
    if (rb_respond_to(obj, id_load) && rb_respond_to(obj, id_dump)) {
	dbst->marshal = obj;
	dbst->options |= BDB1_MARSHAL;
    }
    if (rb_method_boundp(obj, rb_intern("bdb1_store_key"), 0) == Qtrue) {
	dbst->filter[FILTER_KEY] = INT2FIX(rb_intern("bdb1_store_key"));
    }
    if (rb_method_boundp(obj, rb_intern("bdb1_fetch_key"), 0) == Qtrue) {
	dbst->filter[2 + FILTER_KEY] = INT2FIX(rb_intern("bdb1_fetch_key"));
    }
    if (rb_method_boundp(obj, rb_intern("bdb1_store_value"), 0) == Qtrue) {
	dbst->filter[FILTER_VALUE] = INT2FIX(rb_intern("bdb1_store_value"));
    }
    if (rb_method_boundp(obj, rb_intern("bdb1_fetch_value"), 0) == Qtrue) {
	dbst->filter[2 + FILTER_VALUE] = INT2FIX(rb_intern("bdb1_fetch_value"));
    }
    return res;
}

static VALUE
bdb1_s_new(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE st, res;

    res = rb_funcall2(obj, rb_intern("allocate"), 0, 0);
    rb_obj_call_init(res, argc, argv);
    return res;
}

static VALUE
bdb1_i_create(obj, db)
    VALUE obj, db;
{
    VALUE tmp[2];
    tmp[0] = rb_ary_entry(obj, 0);
    tmp[1] = rb_ary_entry(obj, 1);
    bdb1_put(2, tmp, db);
    return Qnil;
} 

static VALUE
bdb1_s_create(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE res;
    int i;

    res = rb_funcall2(obj, rb_intern("new"), 0, 0);
    if (argc == 1 && TYPE(argv[0]) == T_HASH) {
	rb_iterate(rb_each, argv[0], bdb1_i_create, res);
	return res;
    }
    if (argc % 2 != 0) {
        rb_raise(rb_eArgError, "odd number args for %s", rb_class2name(obj));
    }
    for (i = 0; i < argc; i += 2) {
        bdb1_put(2, argv + i, res);
    }
    return res;
}

static VALUE
bdb1_s_open(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE res = rb_funcall2(obj, rb_intern("new"), argc, argv);
    if (rb_block_given_p()) {
	return rb_ensure(rb_yield, res, bdb1_close, res);
    }
    return res;
}

static VALUE
bdb1_append(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    bdb1_DB *dbst;
    DBT key, data;
    db_recno_t recno;
    int i;
    VALUE *a;
    volatile VALUE b = Qnil;

    rb_secure(4);
    if (argc < 1)
	return obj;
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, R_LAST));
    for (i = 0, a = argv; i < argc; i++, a++) {
	DATA_ZERO(data);
	b = test_dump(obj, &data, *a, FILTER_VALUE);
	bdb1_test_error(dbst->dbp->put(dbst->dbp, &key, &data, R_IAFTER));
    }
    return obj;
}

VALUE
bdb1_put(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    volatile VALUE a0 = Qnil;
    volatile VALUE b0 = Qnil;
    VALUE a, b, c;
    bdb1_DB *dbst;
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
    DATA_ZERO(key);
    DATA_ZERO(data);
    a0 = test_recno(obj, &key, &recno, a);
    b0 = test_dump(obj, &data, b, FILTER_VALUE);
    ret = bdb1_test_error(dbst->dbp->put(dbst->dbp, &key, &data, flags));
    if (ret == DB_KEYEXIST)
	return Qfalse;
    else {
	return test_ret(obj, b0, b, FILTER_VALUE);
    }
}

static VALUE
bdb1_assign(obj, a, b)
    VALUE obj, a, b;
{
    VALUE tmp[2];
    tmp[0] = a;
    tmp[1] = b;
    bdb1_put(2, tmp, obj);
    return b;
}

static VALUE
test_load_key(obj, key)
    VALUE obj;
    DBT *key;
{
    bdb1_DB *dbst;
    Data_Get_Struct(obj, bdb1_DB, dbst);
    if (dbst->type == DB_RECNO)
        return INT2NUM((*(db_recno_t *)key->data) - dbst->array_base);
    return bdb1_test_load(obj, key, FILTER_KEY);
}

static VALUE
bdb1_assoc(obj, key, data)
    VALUE obj;
    DBT *key, *data;
{
    return rb_assoc_new(test_load_key(obj, key), 
			bdb1_test_load(obj, data, FILTER_VALUE));
}

static VALUE
bdb1_assoc_dyna(obj, key, data)
    VALUE obj;
    DBT *key, *data;
{
    return rb_assoc_new(test_load_key(obj, key), 
			test_load_dyna(obj, key, data));
}

static VALUE
bdb1_get_internal(argc, argv, obj, notfound, dyna)
    int argc;
    VALUE *argv;
    VALUE obj;
    VALUE notfound;
    int dyna;
{
    volatile VALUE a = Qnil;
    VALUE b, c;
    bdb1_DB *dbst;
    DBT key, data;
    DBT datas;
    int flagss;
    int ret, flags;
    db_recno_t recno;

    flagss = flags = 0;
    GetDB(obj, dbst);
    DATA_ZERO(key);
    DATA_ZERO(data);
    switch(rb_scan_args(argc, argv, "12", &a, &b, &c)) {
    case 3:
        flags = NUM2INT(c);
	break;
    case 2:
	flagss = flags = NUM2INT(b);
	break;
    }
    a = test_recno(obj, &key, &recno, a);
    ret = bdb1_test_error(dbst->dbp->get(dbst->dbp, &key, &data, flags));
    if (ret == DB_NOTFOUND)
        return notfound;
    if (dyna) {
	return test_load_dyna(obj, &key, &data);
    }
    else {
	return bdb1_test_load(obj, &data, FILTER_VALUE);
    }
}

VALUE
bdb1_get(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    return bdb1_get_internal(argc, argv, obj, Qnil, 0);
}

static VALUE
bdb1_get_dyna(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    return bdb1_get_internal(argc, argv, obj, Qnil, 1);
}

static VALUE
bdb1_fetch(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE key, if_none;
    VALUE val;

    rb_scan_args(argc, argv, "11", &key, &if_none);
    val = bdb1_get_internal(1, argv, obj, Qundef, 1);
    if (val == Qundef) {
	if (rb_block_given_p()) {
	    if (argc > 1) {
		rb_raise(rb_eArgError, "wrong # of arguments", argc);
	    }
	    return rb_yield(key);
	}
	if (argc == 1) {
	    rb_raise(rb_eIndexError, "key not found");
	}
	return if_none;
    }
    return val;
}

static VALUE
bdb1_has_key(obj, key)
    VALUE obj, key;
{
    return bdb1_get_internal(1, &key, obj, Qfalse);
}

static VALUE
bdb1_has_both(obj, a, b)
    VALUE obj, a, b;
{
    bdb1_DB *dbst;
    DBT key, data;
    DBT keys, datas;
    int ret, flags;
    db_recno_t recno;
    volatile VALUE c = Qnil;
    volatile VALUE d = Qnil;

    GetDB(obj, dbst);
    DATA_ZERO(key);
    DATA_ZERO(data);
    c = test_recno(obj, &key, &recno, a);
    d = test_dump(obj, &data, b, FILTER_VALUE);
    MEMCPY(&keys, &key, DBT, 1);
    MEMCPY(&datas, &data, DBT, 1);
    flags = (dbst->type == DB_HASH)?DB_FIRST:R_CURSOR;
    while (1) {
	ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
	if (ret == DB_NOTFOUND) {
	    return Qfalse;
	}
	if (key.size == keys.size &&
	    memcmp(keys.data, key.data, key.size) == 0 &&
	    data.size == datas.size &&
	    memcmp(datas.data, data.data, data.size) == 0) {
	    return Qtrue;
	}
	flags = DB_NEXT;
    }
    return Qnil;
}

VALUE
bdb1_del(obj, a)
    VALUE a, obj;
{
    bdb1_DB *dbst;
    DBT key;
    int ret;
    db_recno_t recno;
    volatile VALUE c = Qnil;

    rb_secure(4);
    GetDB(obj, dbst);
    if (dbst->type == DB_HASH) {
	rb_warning("delete can give strange result with DB_HASH");
    }
    DATA_ZERO(key);
    c = test_recno(obj, &key, &recno, a);
    ret = bdb1_test_error(dbst->dbp->del(dbst->dbp, &key, 0));
    if (ret == DB_NOTFOUND)
        return Qnil;
    else
        return obj;
}

static VALUE
bdb1_empty(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, DB_FIRST));
    if (ret == DB_NOTFOUND) {
        return Qtrue;
    }
    FREE_KEY(dbst, key);
    return Qfalse;
}

static VALUE
bdb1_delete_if(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data, save;
    int ret, ret1, flags;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
	flags = DB_NEXT;
        if (RTEST(rb_yield(bdb1_assoc(obj, &key, &data)))) {
	    bdb1_test_error(dbst->dbp->del(dbst->dbp, 0, R_CURSOR));
        }
    } while (1);
    return obj;
}

VALUE
bdb1_clear(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data, save;
    int ret, value, flags;
    db_recno_t recno;

    rb_secure(4);
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    value = 0;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return INT2NUM(value);
        }
	value++;
	bdb1_test_error(dbst->dbp->del(dbst->dbp, 0, R_CURSOR));
    } while (1);
    return INT2NUM(-1);
}

static VALUE
bdb1_length(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, value, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    if (dbst->type == DB_RECNO) {
	return INT2NUM(bdb1_hard_count(dbst->dbp));
    }
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    value = 0;
    flags = DB_FIRST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return INT2NUM(value);
        }
	flags = DB_NEXT;
	FREE_KEY(dbst, key);
        value++;
    } while (1);
    return INT2NUM(value);
}

static VALUE
bdb1_each_valuec(obj, sens, result)
    VALUE obj, result;
    int sens;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE interm, rest;
    
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return result;
        }
	flags = sens;
	FREE_KEY(dbst, key);
	interm = bdb1_test_load(obj, &data, FILTER_VALUE);
        rest = rb_yield(interm);
	if (result != Qnil && RTEST(rest)) {
	    rb_ary_push(result, interm);
	}
    } while (1);
}

VALUE bdb1_each_value(obj) VALUE obj;{ return bdb1_each_valuec(obj, DB_NEXT, Qnil); }
VALUE bdb1_each_eulav(obj) VALUE obj;{ return bdb1_each_valuec(obj, DB_PREV, Qnil); }

static VALUE
bdb1_each_keyc(obj, sens)
    VALUE obj;
    int sens;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
	rb_yield(test_load_key(obj, &key));
	flags = sens;
    } while (1);
    return obj;
}

VALUE bdb1_each_key(obj) VALUE obj;{ return bdb1_each_keyc(obj, DB_NEXT); }
static VALUE bdb1_each_yek(obj) VALUE obj;{ return bdb1_each_keyc(obj, DB_PREV); }

static VALUE
bdb1_each_common(obj, sens)
    VALUE obj;
    int sens;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return Qnil;
        }
        rb_yield(bdb1_assoc(obj, &key, &data));
	flags = sens;
    } while (1);
    return obj;
}

static VALUE bdb1_each_pair(obj) VALUE obj;{ return bdb1_each_common(obj, DB_NEXT); }
static VALUE bdb1_each_riap(obj) VALUE obj;{ return bdb1_each_common(obj, DB_PREV); }

VALUE
bdb1_to_type(obj, result, flag)
    VALUE obj, result, flag;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = (flag == Qnil)?DB_LAST:DB_FIRST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return result;
        }
	switch (TYPE(result)) {
	case T_ARRAY:
	    if (flag == Qtrue) {
		rb_ary_push(result, bdb1_assoc(obj, &key, &data));
	    }
	    else {
		rb_ary_push(result, bdb1_test_load(obj, &data, FILTER_VALUE));
	    }
	    break;
	case T_HASH:
	    if (flag == Qtrue) {
		rb_hash_aset(result, test_load_key(obj, &key), 
			     bdb1_test_load(obj, &data, FILTER_VALUE));
	    }
	    else {
		rb_hash_aset(result, bdb1_test_load(obj, &data, FILTER_VALUE),
			     test_load_key(obj, &key));
	    }
	}
	flags = (flag == Qnil)?DB_PREV:DB_NEXT;
    } while (1);
    return result;
}

static VALUE
bdb1_to_a(obj)
    VALUE obj;
{
    return bdb1_to_type(obj, rb_ary_new(), Qtrue);
}

static VALUE
bdb1_invert(obj)
    VALUE obj;
{
    return bdb1_to_type(obj, rb_hash_new(), Qfalse);
}

static VALUE
bdb1_to_hash(obj)
    VALUE obj;
{
    return bdb1_to_type(obj, rb_hash_new(), Qtrue);
}

static VALUE
bdb1_each_kv(obj, a, result, flag)
    VALUE obj, a, result, flag;
{
    bdb1_DB *dbst;
    DBT keys, key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE k;
    volatile VALUE b = Qnil;

    GetDB(obj, dbst);
    b = test_recno(obj, &key, &recno, a);
    MEMCPY(&keys, &key, DBT, 1);
    DATA_ZERO(data);
    flags = R_CURSOR;
    while (1) {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
	if (ret == DB_NOTFOUND || keys.size != key.size ||
	    memcmp(keys.data, key.data, key.size) != 0) {
	    return (result == Qnil)?obj:result;
	}
	k =  bdb1_test_load(obj, &data, FILTER_VALUE); 
	if (RTEST(flag)) {
	    k = rb_assoc_new(test_load_key(obj, &key), k);
	}
	if (NIL_P(result)) {
	    rb_yield(k);
	}
	else {
	    rb_ary_push(result, k);
	}
	flags = DB_NEXT;
    }
    return Qnil;
}

static VALUE
bdb1_bt_duplicates(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE a, b;

    if (rb_scan_args(argc, argv, "11", &a, &b) == 1) {
	b = Qtrue;
    }
    return bdb1_each_kv(obj, a, rb_ary_new(), b);
}

static VALUE
bdb1_bt_dup(obj, a)
    VALUE a, obj;
{
    return bdb1_each_kv(obj, a, Qnil, Qtrue);
}

static VALUE
bdb1_bt_dupval(obj, a)
    VALUE a, obj;
{
    return bdb1_each_kv(obj, a, Qnil, Qfalse);
}

static VALUE
bdb1_reject(obj)
    VALUE obj;
{
    return rb_hash_delete_if(bdb1_to_hash(obj));
}
 
static VALUE
bdb1_values(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE ary;

    GetDB(obj, dbst);
    ary = rb_ary_new();
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return ary;
        }
	flags = DB_NEXT;
	FREE_KEY(dbst, key);
        rb_ary_push(ary, bdb1_test_load(obj, &data, FILTER_VALUE));
    } while (1);
    return ary;
}

VALUE
bdb1_internal_value(obj, a, b, sens)
    VALUE obj, a, b;
    int sens;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;

    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = (sens == DB_NEXT)?DB_FIRST:DB_LAST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
	if (ret == DB_NOTFOUND) {
	    return (b == Qfalse)?Qfalse:Qnil;
	}
	flags = sens;
	if (rb_equal(a, bdb1_test_load(obj, &data, FILTER_VALUE)) == Qtrue) {
	    VALUE d;
	    
	    d = (b == Qfalse)?Qtrue:test_load_key(obj, &key);
	    FREE_KEY(dbst, key);
	    return  d;
	}
	FREE_KEY(dbst, key);
    } while (1);
    return (b == Qfalse)?Qfalse:Qnil;
}

VALUE
bdb1_index(obj, a)
    VALUE obj, a;
{
    return bdb1_internal_value(obj, a, Qtrue, DB_NEXT);
}

static VALUE
bdb1_indexes(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    VALUE indexes;
    int i;

#if RUBY_VERSION_CODE >= 172
    rb_warn("BDB1#%s is deprecated; use BDB1#values_at",
	    rb_id2name(rb_frame_last_func()));
#endif
    indexes = rb_ary_new2(argc);
    for (i = 0; i < argc; i++) {
	RARRAY(indexes)->ptr[i] = bdb1_get(1, argv + i, obj);
    }
    RARRAY(indexes)->len = i;
    return indexes;
}

VALUE
bdb1_has_value(obj, a)
    VALUE obj, a;
{
    return bdb1_internal_value(obj, a, Qfalse, DB_NEXT);
}

static VALUE
bdb1_keys(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE ary;

    GetDB(obj, dbst);
    ary = rb_ary_new();
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
            return ary;
        }
	rb_ary_push(ary, test_load_key(obj, &key));
	FREE_KEY(dbst, key);
	flags = DB_NEXT;
    } while (1);
    return ary;
}

static VALUE
bdb1_sync(obj)
    VALUE obj;
{
    bdb1_DB *dbst;

    if (!OBJ_TAINTED(obj) && rb_safe_level() >= 4)
	rb_raise(rb_eSecurityError, "Insecure: can't sync the database");
    GetDB(obj, dbst);
    bdb1_test_error(dbst->dbp->sync(dbst->dbp, 0));
    return Qtrue;
}

#if RUBY_VERSION_CODE >= 172

static VALUE
bdb1_values_at(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE result = rb_ary_new2(argc);
    long i;

    for (i = 0; i < argc; i++) {
	rb_ary_push(result, bdb1_get(1, argv + i, obj));
    }
    return result;
}

static VALUE
bdb1_select(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE result = rb_ary_new();
    long i;

    if (rb_block_given_p()) {
	if (argc > 0) {
	    rb_raise(rb_eArgError, "wrong number arguments(%d for 0)", argc);
	}
	return bdb1_each_valuec(obj, DB_NEXT, result);
    }
    rb_warn("Common#select(index..) is deprecated; use Common#values_at");
    return bdb1_values_at(argc, argv, obj);
}

#endif

VALUE
bdb1_each_vc(obj, replace, rtest)
    VALUE obj;
    int replace, rtest;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, flags;
    db_recno_t recno;
    VALUE res, result, val;
    
    GetDB(obj, dbst);
    INIT_RECNO(dbst, key, recno);
    DATA_ZERO(data);
    flags = DB_FIRST;
    result = rb_ary_new();
    do {
        ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, flags));
        if (ret == DB_NOTFOUND) {
	    return result;
        }
	flags = DB_NEXT;
	FREE_KEY(dbst, key);
        val = bdb1_test_load(obj, &data, FILTER_VALUE);
        res = rb_yield(val);
	if (rtest) {
	    if (RTEST(res)) {
		rb_ary_push(result, val);
	    }
	}
	else {
	    rb_ary_push(result, res);
	}
	if (replace == Qtrue) {
	    DATA_ZERO(data);
	    res = test_dump(obj, &data, res, FILTER_VALUE);
	    bdb1_test_error(dbst->dbp->put(dbst->dbp, &key, &data, 0));
	}
    } while (1);
    return Qnil;
}

void
Init_bdb1()
{
    bdb1_mMarshal = rb_const_get(rb_cObject, rb_intern("Marshal"));
    id_dump = rb_intern("dump");
    id_load = rb_intern("load");
    bdb1_id_current_db = rb_intern("bdb1_current_db");
    id_bt_compare = rb_intern("bdb1_bt_compare");
    id_bt_prefix = rb_intern("bdb1_bt_prefix");
    id_h_hash = rb_intern("bdb1_h_hash");
    bdb1_id_call = rb_intern("call");
    if (rb_const_defined_at(rb_cObject, rb_intern("BDB1"))) {
	rb_raise(rb_eNameError, "class already defined");
    }
    bdb1_mDb = rb_define_module("BDB1");
    bdb1_eFatal = rb_define_class_under(bdb1_mDb, "Fatal", rb_eStandardError);
/* CONSTANT */
    rb_define_const(bdb1_mDb, "VERSION_MAJOR", INT2FIX(DB_VERSION_MAJOR));
    rb_define_const(bdb1_mDb, "VERSION_MINOR", INT2FIX(DB_VERSION_MINOR));
    rb_define_const(bdb1_mDb, "RELEASE_PATCH", INT2FIX(DB_RELEASE_PATCH));
    rb_define_const(bdb1_mDb, "VERSION", rb_str_new2("1.x.x"));
    rb_define_const(bdb1_mDb, "BTREE", INT2FIX(DB_BTREE));
    rb_define_const(bdb1_mDb, "HASH", INT2FIX(DB_HASH));
    rb_define_const(bdb1_mDb, "RECNO", INT2FIX(DB_RECNO));
    rb_define_const(bdb1_mDb, "AFTER", INT2FIX(DB_AFTER));
    rb_define_const(bdb1_mDb, "BEFORE", INT2FIX(DB_BEFORE));
    rb_define_const(bdb1_mDb, "CREATE", INT2FIX(DB_CREATE));
    rb_define_const(bdb1_mDb, "DUP", INT2FIX(DB_DUP));
    rb_define_const(bdb1_mDb, "FIRST", INT2FIX(DB_FIRST));
    rb_define_const(bdb1_mDb, "LAST", INT2FIX(DB_LAST));
    rb_define_const(bdb1_mDb, "NEXT", INT2FIX(DB_NEXT));
    rb_define_const(bdb1_mDb, "PREV", INT2FIX(DB_PREV));
    rb_define_const(bdb1_mDb, "RDONLY", INT2FIX(DB_RDONLY));
    rb_define_const(bdb1_mDb, "SET_RANGE", INT2FIX(DB_SET_RANGE));
    rb_define_const(bdb1_mDb, "TRUNCATE", INT2FIX(DB_TRUNCATE));
    rb_define_const(bdb1_mDb, "WRITE", INT2FIX(DB_WRITE));
    rb_define_const(bdb1_mDb, "NOOVERWRITE", INT2FIX(DB_NOOVERWRITE));
/* DATABASE */
    bdb1_cCommon = rb_define_class_under(bdb1_mDb, "Common", rb_cObject);
    rb_define_private_method(bdb1_cCommon, "initialize", bdb1_init, -1);
    rb_include_module(bdb1_cCommon, rb_mEnumerable);
#if RUBY_VERSION_CODE >= 180
    rb_define_alloc_func(bdb1_cCommon, bdb1_s_alloc);
#else
    rb_define_singleton_method(bdb1_cCommon, "allocate", bdb1_s_alloc, 0);
#endif
    rb_define_singleton_method(bdb1_cCommon, "new", bdb1_s_new, -1);
    rb_define_singleton_method(bdb1_cCommon, "create", bdb1_s_new, -1);
    rb_define_singleton_method(bdb1_cCommon, "open", bdb1_s_open, -1);
    rb_define_singleton_method(bdb1_cCommon, "[]", bdb1_s_create, -1);
    rb_define_method(bdb1_cCommon, "close", bdb1_close, 0);
    rb_define_method(bdb1_cCommon, "db_close", bdb1_close, 0);
    rb_define_method(bdb1_cCommon, "put", bdb1_put, -1);
    rb_define_method(bdb1_cCommon, "db_put", bdb1_put, -1);
    rb_define_method(bdb1_cCommon, "[]=", bdb1_assign, 2);
    rb_define_method(bdb1_cCommon, "store", bdb1_put, -1);
    rb_define_method(bdb1_cCommon, "get", bdb1_get_dyna, -1);
    rb_define_method(bdb1_cCommon, "db_get", bdb1_get_dyna, -1);
    rb_define_method(bdb1_cCommon, "[]", bdb1_get_dyna, -1);
    rb_define_method(bdb1_cCommon, "fetch", bdb1_fetch, -1);
    rb_define_method(bdb1_cCommon, "delete", bdb1_del, 1);
    rb_define_method(bdb1_cCommon, "del", bdb1_del, 1);
    rb_define_method(bdb1_cCommon, "db_del", bdb1_del, 1);
    rb_define_method(bdb1_cCommon, "sync", bdb1_sync, 0);
    rb_define_method(bdb1_cCommon, "db_sync", bdb1_sync, 0);
    rb_define_method(bdb1_cCommon, "flush", bdb1_sync, 0);
    rb_define_method(bdb1_cCommon, "each", bdb1_each_pair, 0);
    rb_define_method(bdb1_cCommon, "each_value", bdb1_each_value, 0);
    rb_define_method(bdb1_cCommon, "reverse_each_value", bdb1_each_eulav, 0);
    rb_define_method(bdb1_cCommon, "each_key", bdb1_each_key, 0);
    rb_define_method(bdb1_cCommon, "reverse_each_key", bdb1_each_yek, 0);
    rb_define_method(bdb1_cCommon, "each_pair", bdb1_each_pair, 0);
    rb_define_method(bdb1_cCommon, "reverse_each", bdb1_each_riap, 0);
    rb_define_method(bdb1_cCommon, "reverse_each_pair", bdb1_each_riap, 0);
    rb_define_method(bdb1_cCommon, "keys", bdb1_keys, 0);
    rb_define_method(bdb1_cCommon, "values", bdb1_values, 0);
    rb_define_method(bdb1_cCommon, "delete_if", bdb1_delete_if, 0);
    rb_define_method(bdb1_cCommon, "reject!", bdb1_delete_if, 0);
    rb_define_method(bdb1_cCommon, "reject", bdb1_reject, 0);
    rb_define_method(bdb1_cCommon, "clear", bdb1_clear, 0);
    rb_define_method(bdb1_cCommon, "include?", bdb1_has_key, 1);
    rb_define_method(bdb1_cCommon, "has_key?", bdb1_has_key, 1);
    rb_define_method(bdb1_cCommon, "key?", bdb1_has_key, 1);
    rb_define_method(bdb1_cCommon, "member?", bdb1_has_key, 1);
    rb_define_method(bdb1_cCommon, "has_value?", bdb1_has_value, 1);
    rb_define_method(bdb1_cCommon, "value?", bdb1_has_value, 1);
    rb_define_method(bdb1_cCommon, "has_both?", bdb1_has_both, 2);
    rb_define_method(bdb1_cCommon, "both?", bdb1_has_both, 2);
    rb_define_method(bdb1_cCommon, "to_a", bdb1_to_a, 0);
    rb_define_method(bdb1_cCommon, "to_hash", bdb1_to_hash, 0);
    rb_define_method(bdb1_cCommon, "invert", bdb1_invert, 0);
    rb_define_method(bdb1_cCommon, "empty?", bdb1_empty, 0);
    rb_define_method(bdb1_cCommon, "length", bdb1_length, 0);
    rb_define_alias(bdb1_cCommon,  "size", "length");
    rb_define_method(bdb1_cCommon, "index", bdb1_index, 1);
    rb_define_method(bdb1_cCommon, "indexes", bdb1_indexes, -1);
    rb_define_method(bdb1_cCommon, "indices", bdb1_indexes, -1);
#if RUBY_VERSION_CODE >= 172
    rb_define_method(bdb1_cCommon, "select", bdb1_select, -1);
    rb_define_method(bdb1_cCommon, "values_at", bdb1_values_at, -1);
#endif
    bdb1_cBtree = rb_define_class_under(bdb1_mDb, "Btree", bdb1_cCommon);
    rb_define_method(bdb1_cBtree, "duplicates", bdb1_bt_duplicates, -1);
    rb_define_method(bdb1_cBtree, "each_dup", bdb1_bt_dup, 1);
    rb_define_method(bdb1_cBtree, "each_dup_value", bdb1_bt_dupval, 1);
    bdb1_cHash = rb_define_class_under(bdb1_mDb, "Hash", bdb1_cCommon);
    rb_undef_method(bdb1_cHash, "delete_if");
    rb_undef_method(bdb1_cHash, "reverse_each_value");
    rb_undef_method(bdb1_cHash, "reverse_each_key");
    rb_undef_method(bdb1_cHash, "reverse_each_pair");
    rb_undef_method(bdb1_cHash, "reverse_each");
    bdb1_cUnknown = rb_define_class_under(bdb1_mDb, "Unknown", bdb1_cCommon);
    bdb1_errstr = rb_tainted_str_new(0, 0);
    rb_global_variable(&bdb1_errstr);
    bdb1_init_delegator();
    bdb1_init_recnum();
}
