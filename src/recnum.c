#include "bdb1.h"

static ID id_cmp;

static VALUE
bdb1_recnum_s_new(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE *nargv;
    VALUE array = rb_str_new2("array_base");
    VALUE sarray = rb_str_new2("set_array_base");

    if (!argc || TYPE(argv[argc - 1]) != T_HASH) {
	nargv = ALLOCA_N(VALUE, argc + 1);
	MEMCPY(nargv, argv, VALUE, argc);
	nargv[argc] = rb_hash_new();
	argv = nargv;
	argc++;
    }
    rb_hash_aset(argv[argc - 1], array, INT2FIX(0));
    if (rb_hash_aref(argv[argc - 1], sarray) != RHASH(argv[argc - 1])->ifnone) {
	rb_hash_aset(argv[argc - 1], sarray, INT2FIX(0));
    }
    return bdb1_s_new(argc, argv, obj);
} 

static VALUE
bdb1_sary_subseq(obj, beg, len)
    VALUE obj;
    long beg, len;
{
    VALUE ary2, a;
    bdb1_DB *dbst;
    long i;

    GetDB(obj, dbst);
    if (beg > dbst->len) return Qnil;
    if (beg < 0 || len < 0) return Qnil;

    if (beg + len > dbst->len) {
	len = dbst->len - beg;
    }
    if (len <= 0) return rb_ary_new2(0);

    ary2 = rb_ary_new2(len);
    for (i = 0; i < len; i++) {
	a = INT2NUM(i + beg);
	rb_ary_push(ary2, bdb1_get(1, &a, obj));
    }
    return ary2;
}

static VALUE
bdb1_sary_entry(obj, position)
    VALUE obj, position;
{
    bdb1_DB *dbst;
    long offset;

    GetDB(obj, dbst);
    if (dbst->len == 0) return Qnil;
    offset = NUM2LONG(position);
    if (offset < 0) {
	offset += dbst->len;
    }
    if (offset < 0 || dbst->len <= offset) return Qnil;
    position = INT2NUM(offset);
    return bdb1_get(1, &position, obj);
}

static VALUE
bdb1_sary_aref(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE arg1, arg2;
    long beg, len;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (rb_scan_args(argc, argv, "11", &arg1, &arg2) == 2) {
	beg = NUM2LONG(arg1);
	len = NUM2LONG(arg2);
	if (beg < 0) {
	    beg = dbst->len + beg;
	}
	return bdb1_sary_subseq(obj, beg, len);
    }

    if (FIXNUM_P(arg1)) {
	return bdb1_sary_entry(obj, arg1);
    }
    else if (TYPE(arg1) == T_BIGNUM) {
	rb_raise(rb_eIndexError, "index too big");
    }
    else {
	switch (rb_range_beg_len(arg1, &beg, &len, dbst->len, 0)) {
	  case Qfalse:
	    break;
	  case Qnil:
	    return Qnil;
	  default:
	    return bdb1_sary_subseq(obj, beg, len);
	}
    }
    return bdb1_sary_entry(obj, arg1);
}

static VALUE
bdb1_intern_shift_pop(obj, depart, len)
    VALUE obj;
    int depart, len;
{
    bdb1_DB *dbst;
    DBT key, data;
    int ret, i;
    db_recno_t recno;
    VALUE res;

    rb_secure(4);
    GetDB(obj, dbst);
    init_recno(dbst, key, recno);
    DATA_ZERO(data);
    res = rb_ary_new2(len);
    for (i = 0; i < len; i++) {
	ret = bdb1_test_error(dbst->dbp->seq(dbst->dbp, &key, &data, depart));
	if (ret == DB_NOTFOUND) break;
	rb_ary_push(res, bdb1_test_load(dbst, data));
	bdb1_test_error(dbst->dbp->del(dbst->dbp, 0, R_CURSOR));
	if (dbst->len > 0) dbst->len--;
    }
    if (RARRAY(res)->len == 0) return Qnil;
    else if (RARRAY(res)->len == 1) return RARRAY(res)->ptr[0];
    else return res;
}

static void
bdb1_sary_replace(obj, beg, len, rpl)
    VALUE obj, rpl;
    long beg, len;
{
    long i, j, rlen;
    VALUE tmp[2];
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (len < 0) rb_raise(rb_eIndexError, "negative length %d", len);
    if (beg < 0) {
	beg += dbst->len;
    }
    if (beg < 0) {
	beg -= dbst->len;
	rb_raise(rb_eIndexError, "index %d out of array", beg);
    }
    if (beg + len > dbst->len) {
	len = dbst->len - beg;
    }

    if (NIL_P(rpl)) {
	rpl = rb_ary_new2(0);
    }
    else if (TYPE(rpl) != T_ARRAY) {
	rpl = rb_ary_new3(1, rpl);
    }
    rlen = RARRAY(rpl)->len;

    tmp[1] = Qnil;
    if (beg >= dbst->len) {
	for (i = dbst->len; i < beg; i++) {
	    tmp[0] = INT2NUM(i);
	    bdb1_put(2, tmp, obj);
	    dbst->len++;
	}
	for (i = beg, j = 0; j < RARRAY(rpl)->len; i++, j++) {
	    tmp[0] = INT2NUM(i);
	    tmp[1] = RARRAY(rpl)->ptr[j];
	    bdb1_put(2, tmp, obj);
	    dbst->len++;
	}
    }
    else {
	if (len < rlen) {
	    tmp[1] = Qnil;
	    for (i = dbst->len - 1; i >= (beg + len); i--) {
		tmp[0] = INT2NUM(i);
		tmp[1] = bdb1_get(1, tmp, obj);
		tmp[0] = INT2NUM(i + rlen - len);
		bdb1_put(2, tmp, obj);
	    }
	    dbst->len += rlen - len;
	}
	for (i = beg, j = 0; j < rlen; i++, j++) {
	    tmp[0] = INT2NUM(i);
	    tmp[1] = RARRAY(rpl)->ptr[j];
	    bdb1_put(2, tmp, obj);
	}
	if (len > rlen) {
	    for (i = beg + len; i < dbst->len; i++) {
		tmp[0] = INT2NUM(i);
		tmp[1] = bdb1_get(1, tmp, obj);
		tmp[0] = INT2NUM(i + rlen - len);
		bdb1_put(2, tmp, obj);
	    }
	    bdb1_intern_shift_pop(obj, DB_LAST, len - rlen);
	}
    }
}

static VALUE
bdb1_sary_aset(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    long  beg, len;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (argc == 3) {
	bdb1_sary_replace(obj, NUM2LONG(argv[0]), NUM2LONG(argv[1]), argv[2]);
	return argv[2];
    }
    if (argc != 2) {
	rb_raise(rb_eArgError, "wrong # of arguments(%d for 2)", argc);
    }
    if (FIXNUM_P(argv[0])) {
	beg = FIX2LONG(argv[0]);
	goto fixnum;
    }
    else if (rb_range_beg_len(argv[0], &beg, &len, dbst->len, 1)) {
	bdb1_sary_replace(obj, beg, len, argv[1]);
	return argv[1];
    }
    if (TYPE(argv[0]) == T_BIGNUM) {
	rb_raise(rb_eIndexError, "index too big");
    }

    beg = NUM2LONG(argv[0]);
  fixnum:
    if (beg < 0) {
	beg += dbst->len;
	if (beg < 0) {
	    rb_raise(rb_eIndexError, "index %d out of array",
		     beg - dbst->len);
	}
    }
    if (beg > dbst->len) {
	VALUE nargv[2];
	int i;

	nargv[1] = Qnil;
	for (i = dbst->len; i < beg; i++) {
	    nargv[0] = INT2NUM(i);
	    bdb1_put(2, nargv, obj);
	    dbst->len++;
	}
    }
    argv[0] = INT2NUM(beg);
    bdb1_put(2, argv, obj);
    dbst->len++;
    return argv[1];
}

static VALUE
bdb1_sary_at(obj, pos)
    VALUE obj, pos;
{
    return bdb1_sary_entry(obj, pos);
}

static VALUE
bdb1_sary_first(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    VALUE tmp;

    GetDB(obj, dbst);
    tmp = INT2NUM(0);
    return bdb1_get(1, &tmp, obj);
}

static VALUE
bdb1_sary_last(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    VALUE tmp;

    GetDB(obj, dbst);
    if (!dbst->len) return Qnil;
    tmp = INT2NUM(dbst->len);
    return bdb1_get(1, &tmp, obj);
}

static VALUE
bdb1_sary_fetch(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    VALUE pos, ifnone;
    bdb1_DB *dbst;
    long idx;

    GetDB(obj, dbst);
    rb_scan_args(argc, argv, "11", &pos, &ifnone);
    idx = NUM2LONG(pos);

    if (idx < 0) {
	idx +=  dbst->len;
    }
    if (idx < 0 || dbst->len <= idx) {
	return ifnone;
    }
    pos = INT2NUM(idx);
    return bdb1_get(1, &pos, obj);
}
    

static VALUE
bdb1_sary_concat(obj, y)
    VALUE obj, y;
{
    bdb1_DB *dbst;
    long i;
    VALUE tmp[2];

    y = rb_convert_type(y, T_ARRAY, "Array", "to_ary");
    GetDB(obj, dbst);
    for (i = 0; i < RARRAY(y)->len; i++) {
	tmp[0] = INT2NUM(dbst->len);
	tmp[1] = RARRAY(y)->ptr[i];
	bdb1_put(2, tmp, obj);
	dbst->len++;
    }
    return obj;
}
    
static VALUE
bdb1_sary_push(obj, y)
    VALUE obj, y;
{
    bdb1_DB *dbst;
    VALUE tmp[2];

    GetDB(obj, dbst);
    tmp[0] = INT2NUM(dbst->len);
    tmp[1] = y;
    bdb1_put(2, tmp, obj);
    dbst->len++;
    return obj;
}

static VALUE
bdb1_sary_push_m(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    bdb1_DB *dbst;
    long i;
    VALUE tmp[2];

    if (argc == 0) {
	rb_raise(rb_eArgError, "wrong # of arguments(at least 1)");
    }
    if (argc > 0) {
	GetDB(obj, dbst);
	for (i = 0; i < argc; i++) {
	    tmp[0] = INT2NUM(dbst->len);
	    tmp[1] = argv[i];
	    bdb1_put(2, tmp, obj);
	    dbst->len++;
	}
    }
    return obj;
}
    
static VALUE
bdb1_sary_shift(obj)
    VALUE obj;
{
    VALUE res;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (dbst->len == 0) return Qnil;
    res = bdb1_intern_shift_pop(obj, DB_FIRST, 1);
    return res;
}

static VALUE
bdb1_sary_pop(obj)
    VALUE obj;
{
    VALUE res;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (dbst->len == 0) return Qnil;
    res = bdb1_intern_shift_pop(obj, DB_LAST, 1);
    return res;
}

static VALUE
bdb1_sary_unshift_m(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    bdb1_DB *dbst;
    VALUE tmp[3];
    long i;

    if (argc == 0) {
	rb_raise(rb_eArgError, "wrong # of arguments(at least 1)");
    }
    if (argc > 0) {
	GetDB(obj, dbst);
	tmp[0] = INT2NUM(0);
	tmp[2] = INT2NUM(R_IBEFORE);
	for (i = argc - 1; i >= 0; i--) {
	    tmp[1] = argv[i];
	    bdb1_put(3, tmp, obj);
	    dbst->len++;
	}
    }
    return obj;
}

static VALUE
bdb1_sary_length(obj)
    VALUE obj;
{
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (dbst->len < 0) rb_raise(bdb1_eFatal, "Invalid BDB::Recnum");
    return INT2NUM(dbst->len);
}

static VALUE
bdb1_sary_empty_p(obj)
    VALUE obj;
{
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (dbst->len < 0) rb_raise(bdb1_eFatal, "Invalid BDB::Recnum");
    return (dbst->len)?Qfalse:Qtrue;
}

static VALUE
bdb1_sary_rindex(obj, a)
    VALUE obj, a;
{
    return bdb1_internal_value(obj, a, Qtrue, DB_PREV);
}

static VALUE
bdb1_sary_indexes(argc, argv, obj)
    int argc;
    VALUE obj, *argv;
{
    VALUE indexes;
    int i;
    indexes = rb_ary_new2(argc);
    for (i = 0; i < argc; i++) {
	rb_ary_push(indexes, bdb1_sary_entry(obj, argv[i]));
    }
    return indexes;
}

static VALUE
bdb1_sary_to_a(obj)
    VALUE obj;
{
    return bdb1_to_type(obj, rb_ary_new(), Qfalse);
}

static VALUE
bdb1_sary_reverse_m(obj)
    VALUE obj;
{
    return bdb1_to_type(obj, rb_ary_new(), Qnil);
}

static VALUE
bdb1_sary_reverse_bang(obj)
    VALUE obj;
{
    long i, j;
    bdb1_DB *dbst;
    VALUE tmp[2], interm;

    GetDB(obj, dbst);
    if (dbst->len <= 1) return obj;
    i = 0;
    j = dbst->len - 1;
    while (i < j) {
	tmp[0] = INT2NUM(i);
	interm = bdb1_get(1, tmp, obj);
	tmp[0] = INT2NUM(j);
	tmp[1] = bdb1_get(1, tmp, obj);
	tmp[0] = INT2NUM(i);
	bdb1_put(2, tmp, obj);
	tmp[0] = INT2NUM(j);
	tmp[1] = interm;
	bdb1_put(2, tmp, obj);
	i++; j--;
    }
    return obj;
}

static VALUE
bdb1_sary_collect_bang(obj)
    VALUE obj;
{
    return bdb1_each_vc(obj, Qtrue);
}

static VALUE
bdb1_sary_collect(obj)
    VALUE obj;
{
    if (!rb_block_given_p()) {
	return bdb1_sary_to_a(obj);
    }
    return bdb1_each_vc(obj, Qfalse);
}

static VALUE
bdb1_sary_filter(obj)
    VALUE obj;
{
    rb_warn("BDB1::Recnum#filter is deprecated; use BDB1::Recnum#collect!");
    return bdb1_sary_collect_bang(obj);
}

static VALUE
bdb1_sary_delete(obj, item)
    VALUE obj, item;
{
    bdb1_DB *dbst;
    long i1, i2;
    VALUE tmp, a;

    GetDB(obj, dbst);
    i2 = dbst->len;
    for (i1 = 0; i1 < dbst->len;) {
	tmp = INT2NUM(i1);
	a = bdb1_get(1, &tmp, obj);
	if (rb_equal(a, item)) {
	    bdb1_del(obj, INT2NUM(i1));
	    dbst->len--;
	}
	else {
	    i1++;
	}
    }
    if (dbst->len == i2) {
	if (rb_block_given_p()) {
	    return rb_yield(item);
	}
	return Qnil;
    }
    return item;
}

static VALUE
bdb1_sary_delete_at_m(obj, a)
    VALUE obj, a;
{
    bdb1_DB *dbst;
    long pos;
    VALUE tmp;
    VALUE del = Qnil;

    GetDB(obj, dbst);
    pos = NUM2INT(a);
    if (pos >= dbst->len) return Qnil;
    if (pos < 0) pos += dbst->len;
    if (pos < 0) return Qnil;

    tmp = INT2NUM(pos);
    del = bdb1_get(1, &tmp, obj);
    bdb1_del(obj, tmp);
    dbst->len--;
    return del;
}

static VALUE
bdb1_sary_reject_bang(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    long i1, i2;
    VALUE tmp, a;

    GetDB(obj, dbst);
    i2 = dbst->len;
    for (i1 = 0; i1 < dbst->len;) {
	tmp = INT2NUM(i1);
	a = bdb1_get(1, &tmp, obj);
	if (!RTEST(rb_yield(a))) {
	    i1++;
	    continue;
	}
	bdb1_del(obj, tmp);
	dbst->len--;
    }
    if (dbst->len == i2) return Qnil;
    return obj;
}

static VALUE
bdb1_sary_delete_if(obj)
    VALUE obj;
{
    bdb1_sary_reject_bang(obj);
    return obj;
}

static VALUE
bdb1_sary_replace_m(obj, obj2)
    VALUE obj, obj2;
{
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    obj2 = rb_convert_type(obj2, T_ARRAY, "Array", "to_ary");
    bdb1_sary_replace(obj, 0, dbst->len, obj2);
    return obj;
}

static VALUE
bdb1_sary_clear(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    bdb1_clear(obj);
    GetDB(obj, dbst);
    dbst->len = 0;
    return obj;
}

static VALUE
bdb1_sary_fill(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE item, arg1, arg2, tmp[2];
    long beg, len, i;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    rb_scan_args(argc, argv, "12", &item, &arg1, &arg2);
    switch (argc) {
      case 1:
	  len = dbst->len;
	  beg = 0;
	  break;
      case 2:
	if (rb_range_beg_len(arg1, &beg, &len, dbst->len, 1)) {
	    break;
	}
	/* fall through */
      case 3:
	beg = NIL_P(arg1)?0:NUM2LONG(arg1);
	if (beg < 0) {
	    beg += dbst->len;
	    if (beg < 0) beg = 0;
	}
	len = NIL_P(arg2)?dbst->len - beg:NUM2LONG(arg2);
	break;
    }
    tmp[1] = item;
    for (i = 0; i < len; i++) {
	tmp[0] = INT2NUM(i + beg);
	bdb1_put(2, tmp, obj);
	if ((i + beg) >= dbst->len) dbst->len++;
    }
    return obj;
}

static VALUE
bdb1_sary_cmp(obj, obj2)
    VALUE obj, obj2;
{
    bdb1_DB *dbst, *dbst2;
    VALUE a, a2, tmp, ary;
    long i, len;

    if (obj == obj2) return INT2FIX(0);
    GetDB(obj, dbst);
    len = dbst->len;
    if (!rb_obj_is_kind_of(obj2, bdb1_cRecnum)) {
	obj2 = rb_convert_type(obj2, T_ARRAY, "Array", "to_ary");
	if (len > RARRAY(obj2)->len) {
	    len = RARRAY(obj2)->len;
	}
	ary = Qtrue;
    }
    else {
	GetDB(obj2, dbst2);
	if (len > dbst2->len) {
	    len = dbst2->len;
	}
	ary = Qfalse;
    }
    for (i = 0; i < len; i++) {
	tmp = INT2NUM(i);
	a = bdb1_get(1, &tmp, obj);
	if (ary) {
	    a2 = RARRAY(obj2)->ptr[i];
	}
	else {
	    a2 = bdb1_get(1, &tmp, obj2);
	}
	tmp = rb_funcall(a, id_cmp, 1, a2);
	if (tmp != INT2FIX(0)) {
	    return tmp;
	}
    }
    len = dbst->len - ary?RARRAY(obj2)->len:dbst2->len;
    if (len == 0) return INT2FIX(0);
    if (len > 0) return INT2FIX(1);
    return INT2FIX(-1);
}

static VALUE
bdb1_sary_slice_bang(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE arg1, arg2;
    long pos, len;
    bdb1_DB *dbst;

    GetDB(obj, dbst);
    if (rb_scan_args(argc, argv, "11", &arg1, &arg2) == 2) {
	pos = NUM2LONG(arg1);
	len = NUM2LONG(arg2);
      delete_pos_len:
	if (pos < 0) {
	    pos = dbst->len + pos;
	}
	arg2 = bdb1_sary_subseq(obj, pos, len);
	bdb1_sary_replace(obj, pos, len, Qnil);
	return arg2;
    }

    if (!FIXNUM_P(arg1) && rb_range_beg_len(arg1, &pos, &len, dbst->len, 1)) {
	goto delete_pos_len;
    }

    pos = NUM2LONG(arg1);
    if (pos >= dbst->len) return Qnil;
    if (pos < 0) pos += dbst->len;
    if (pos < 0) return Qnil;

    arg1 = INT2NUM(pos);
    arg2 = bdb1_sary_at(obj, arg1);
    if (bdb1_del(obj, arg1) != Qnil) dbst->len--;
    return arg2;
}

static VALUE
bdb1_sary_plus(obj, y)
    VALUE obj, y;
{
    return rb_ary_plus(bdb1_sary_to_a(obj), y);
}

static VALUE
bdb1_sary_times(obj, y)
    VALUE obj, y;
{
    return rb_funcall(bdb1_sary_to_a(obj), rb_intern("*"), 1, y);
}

static VALUE
bdb1_sary_diff(obj, y)
    VALUE obj, y;
{
    return rb_funcall(bdb1_sary_to_a(obj), rb_intern("-"), 1, y);
}

static VALUE
bdb1_sary_and(obj, y)
    VALUE obj, y;
{
    return rb_funcall(bdb1_sary_to_a(obj), rb_intern("&"), 1, y);
}

static VALUE
bdb1_sary_or(obj, y)
    VALUE obj, y;
{
    return rb_funcall(bdb1_sary_to_a(obj), rb_intern("|"), 1, y);
}

static VALUE
bdb1_sary_compact(obj)
    VALUE obj;
{
    return rb_funcall(bdb1_sary_to_a(obj), rb_intern("compact!"), 0, 0);
}

static VALUE
bdb1_sary_compact_bang(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    long i, j;
    VALUE tmp;

    GetDB(obj, dbst);
    j = dbst->len;
    for (i = 0; i < dbst->len; ) {
	tmp = INT2NUM(i);
	tmp = bdb1_get(1, &tmp, obj);
	if (NIL_P(tmp)) {
	    bdb1_del(obj, INT2NUM(i));
	    dbst->len--;
	}
	else {
	    i++;
	}
    }
    if (dbst->len == j) return Qnil;
    return obj;
}

static VALUE
bdb1_sary_nitems(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    long i, j;
    VALUE tmp;

    GetDB(obj, dbst);
    j = 0;
    for (i = 0; i < dbst->len; i++) {
	tmp = INT2NUM(i);
	tmp = bdb1_get(1, &tmp, obj);
	if (!NIL_P(tmp)) j++;
    }
    return INT2NUM(j);
}

static VALUE
bdb1_sary_each_index(obj)
    VALUE obj;
{
    bdb1_DB *dbst;
    long i;

    GetDB(obj, dbst);
    for (i = 0; i< dbst->len; i++) {
	rb_yield(INT2NUM(i));
    }
    return obj;
}

void bdb1_init_recnum()
{
    id_cmp = rb_intern("<=>");
    bdb1_cRecnum = rb_define_class_under(bdb1_mDb, "Recnum", bdb1_cCommon);
    rb_const_set(bdb1_mDb, rb_intern("Recno"), bdb1_cRecnum);
    rb_define_singleton_method(bdb1_cRecnum, "new", bdb1_recnum_s_new, -1);
    rb_define_singleton_method(bdb1_cRecnum, "create", bdb1_recnum_s_new, -1);
    rb_define_singleton_method(bdb1_cRecnum, "open", bdb1_recnum_s_new, -1);
    rb_define_method(bdb1_cRecnum, "[]", bdb1_sary_aref, -1);
    rb_define_method(bdb1_cRecnum, "[]=", bdb1_sary_aset, -1);
    rb_define_method(bdb1_cRecnum, "at", bdb1_sary_at, 1);
    rb_define_method(bdb1_cRecnum, "fetch", bdb1_sary_fetch, -1);
    rb_define_method(bdb1_cRecnum, "first", bdb1_sary_first, 0);
    rb_define_method(bdb1_cRecnum, "last", bdb1_sary_last, 0);
    rb_define_method(bdb1_cRecnum, "concat", bdb1_sary_concat, 1);
    rb_define_method(bdb1_cRecnum, "<<", bdb1_sary_push, 1);
    rb_define_method(bdb1_cRecnum, "push", bdb1_sary_push_m, -1);
    rb_define_method(bdb1_cRecnum, "pop", bdb1_sary_pop, 0);
    rb_define_method(bdb1_cRecnum, "shift", bdb1_sary_shift, 0);
    rb_define_method(bdb1_cRecnum, "unshift", bdb1_sary_unshift_m, -1);
    rb_define_method(bdb1_cRecnum, "each", bdb1_each_value, 0);
    rb_define_method(bdb1_cRecnum, "each_index", bdb1_sary_each_index, 0);
    rb_define_method(bdb1_cRecnum, "reverse_each", bdb1_each_eulav, 0);
    rb_define_method(bdb1_cRecnum, "length", bdb1_sary_length, 0);
    rb_define_alias(bdb1_cRecnum,  "size", "length");
    rb_define_method(bdb1_cRecnum, "empty?", bdb1_sary_empty_p, 0);
    rb_define_method(bdb1_cRecnum, "index", bdb1_index, 1);
    rb_define_method(bdb1_cRecnum, "rindex", bdb1_sary_rindex, 1);
    rb_define_method(bdb1_cRecnum, "indexes", bdb1_sary_indexes, -1);
    rb_define_method(bdb1_cRecnum, "indices", bdb1_sary_indexes, -1);
    rb_define_method(bdb1_cRecnum, "reverse", bdb1_sary_reverse_m, 0);
    rb_define_method(bdb1_cRecnum, "reverse!", bdb1_sary_reverse_bang, 0);
    rb_define_method(bdb1_cRecnum, "collect", bdb1_sary_collect, 0);
    rb_define_method(bdb1_cRecnum, "collect!", bdb1_sary_collect_bang, 0);
    rb_define_method(bdb1_cRecnum, "map!", bdb1_sary_collect_bang, 0);
    rb_define_method(bdb1_cRecnum, "filter", bdb1_sary_filter, 0);
    rb_define_method(bdb1_cRecnum, "delete", bdb1_sary_delete, 1);
    rb_define_method(bdb1_cRecnum, "delete_at", bdb1_sary_delete_at_m, 1);
    rb_define_method(bdb1_cRecnum, "delete_if", bdb1_sary_delete_if, 0);
    rb_define_method(bdb1_cRecnum, "reject!", bdb1_sary_reject_bang, 0);
    rb_define_method(bdb1_cRecnum, "replace", bdb1_sary_replace_m, 1);
    rb_define_method(bdb1_cRecnum, "clear", bdb1_sary_clear, 0);
    rb_define_method(bdb1_cRecnum, "fill", bdb1_sary_fill, -1);
    rb_define_method(bdb1_cRecnum, "include?", bdb1_has_value, 1);
    rb_define_method(bdb1_cRecnum, "<=>", bdb1_sary_cmp, 1);
    rb_define_method(bdb1_cRecnum, "slice", bdb1_sary_aref, -1);
    rb_define_method(bdb1_cRecnum, "slice!", bdb1_sary_slice_bang, -1);
/*
    rb_define_method(bdb1_cRecnum, "assoc", bdb1_sary_assoc, 1);
    rb_define_method(bdb1_cRecnum, "rassoc", bdb1_sary_rassoc, 1);
*/
    rb_define_method(bdb1_cRecnum, "+", bdb1_sary_plus, 1);
    rb_define_method(bdb1_cRecnum, "*", bdb1_sary_times, 1);

    rb_define_method(bdb1_cRecnum, "-", bdb1_sary_diff, 1);
    rb_define_method(bdb1_cRecnum, "&", bdb1_sary_and, 1);
    rb_define_method(bdb1_cRecnum, "|", bdb1_sary_or, 1);

/*
    rb_define_method(bdb1_cRecnum, "uniq", bdb1_sary_uniq, 0);
    rb_define_method(bdb1_cRecnum, "uniq!", bdb1_sary_uniq_bang, 0);
*/
    rb_define_method(bdb1_cRecnum, "compact", bdb1_sary_compact, 0);
    rb_define_method(bdb1_cRecnum, "compact!", bdb1_sary_compact_bang, 0);
/*
    rb_define_method(bdb1_cRecnum, "flatten", bdb1_sary_flatten, 0);
    rb_define_method(bdb1_cRecnum, "flatten!", bdb1_sary_flatten_bang, 0);
*/
    rb_define_method(bdb1_cRecnum, "nitems", bdb1_sary_nitems, 0);
    rb_define_method(bdb1_cRecnum, "to_a", bdb1_sary_to_a, 0);
    rb_define_method(bdb1_cRecnum, "to_ary", bdb1_sary_to_a, 0);
}
