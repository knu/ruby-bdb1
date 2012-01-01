#include "bdb1.h"

static ID id_send;
VALUE bdb1_cDelegate;

void
bdb1_deleg_free(delegst)
    struct deleg_class *delegst;
{
    free(delegst);
}

void
bdb1_deleg_mark(delegst)
    struct deleg_class *delegst;
{
    bdb1_DB *dbst;

    if (delegst->db) {
	Data_Get_Struct(delegst->db, bdb1_DB, dbst);
	if (dbst->dbp) {
	    rb_gc_mark(delegst->db);
	    if (delegst->key) rb_gc_mark(delegst->key);
	}
    }
    if (delegst->obj) rb_gc_mark(delegst->obj);
}

static VALUE
bdb1_deleg_missing(argc, argv, obj)
    int argc;
    VALUE *argv, obj;
{
    struct deleg_class *delegst, *newst;
    bdb1_DB *dbst;
    VALUE res, new;

    Data_Get_Struct(obj, struct deleg_class, delegst);
    if (rb_block_given_p()) {
	res = rb_block_call(delegst->obj, id_send, argc, argv, rb_yield, 0);
    }
    else {
	res = rb_funcall2(delegst->obj, id_send, argc, argv);
    }
    Data_Get_Struct(delegst->db, bdb1_DB, dbst);
    if (dbst->dbp) {
	VALUE nargv[2];

	if (!SPECIAL_CONST_P(res) &&
	    (TYPE(res) != T_DATA || 
	     RDATA(res)->dmark != (RUBY_DATA_FUNC)bdb1_deleg_mark)) {
	    new = Data_Make_Struct(bdb1_cDelegate, struct deleg_class, 
				   bdb1_deleg_mark, bdb1_deleg_free, newst);
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
	bdb1_put(2, nargv, delegst->db);
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

static VALUE bdb1_deleg_inspect(obj) DELEG_0(rb_intern("inspect"))
static VALUE bdb1_deleg_to_s(obj)    DELEG_0(rb_intern("to_s"))
static VALUE bdb1_deleg_to_str(obj)  DELEG_0(rb_intern("to_str"))
static VALUE bdb1_deleg_to_a(obj)    DELEG_0(rb_intern("to_a"))
static VALUE bdb1_deleg_to_ary(obj)  DELEG_0(rb_intern("to_ary"))
static VALUE bdb1_deleg_to_i(obj)    DELEG_0(rb_intern("to_i"))
static VALUE bdb1_deleg_to_int(obj)  DELEG_0(rb_intern("to_int"))
static VALUE bdb1_deleg_to_f(obj)    DELEG_0(rb_intern("to_f"))
static VALUE bdb1_deleg_to_hash(obj) DELEG_0(rb_intern("to_hash"))
static VALUE bdb1_deleg_to_io(obj)   DELEG_0(rb_intern("to_io"))
static VALUE bdb1_deleg_to_proc(obj) DELEG_0(rb_intern("to_proc"))

VALUE
bdb1_deleg_to_orig(obj)
    VALUE obj;
{
    struct deleg_class *delegst;
    Data_Get_Struct(obj, struct deleg_class, delegst);
    return delegst->obj;
}

static VALUE
bdb1_deleg_orig(obj)
    VALUE obj;
{
    return obj;
}

static VALUE
bdb1_deleg_dump(obj, limit)
    VALUE obj, limit;
{
    struct deleg_class *delegst;
    bdb1_DB *dbst;
    Data_Get_Struct(obj, struct deleg_class, delegst);
    Data_Get_Struct(delegst->db, bdb1_DB, dbst);
    return rb_funcall(dbst->marshal, rb_intern("dump"), 1, delegst->obj);
}

static VALUE
bdb1_deleg_load(obj, str)
    VALUE obj, str;
{
    bdb1_DB *dbst;

    if (NIL_P(obj = rb_thread_local_aref(rb_thread_current(), bdb1_id_current_db))) {
	rb_raise(bdb1_eFatal, "BUG : current_db not set");
    }
    Data_Get_Struct(obj, bdb1_DB, dbst);
    return rb_funcall(dbst->marshal, rb_intern("load"), 1, str);
}

void bdb1_init_delegator()
{
    id_send = rb_intern("send");
    bdb1_cDelegate = rb_define_class_under(bdb1_mDb, "Delegate", rb_cObject);
    {
	VALUE ary, tmp = Qfalse;
	int i;
	ID  id_eq = rb_intern("=="),
	    id_eqq = rb_intern("==="),
	    id_match = rb_intern("=~"),
	    id_not = rb_intern("!"),
	    id_neq = rb_intern("!="),
	    id_notmatch = rb_intern("!~");

	ary = rb_class_instance_methods(1, &tmp, rb_mKernel);
	for (i = 0; i < RARRAY_LEN(ary); i++) {
	    VALUE method = RARRAY_PTR(ary)[i];
	    VALUE mid;
	    if (!SYMBOL_P(method)) {
		Check_Type(method, T_STRING);
		mid = rb_intern(RSTRING_PTR(method));
	    }
	    else
		mid = SYM2ID(method);
	    if (mid == id_eq ||
		mid == id_eqq ||
		mid == id_match ||
		mid == id_not ||
		mid == id_neq ||
		mid == id_notmatch) continue;
	    rb_undef_method(bdb1_cDelegate, rb_id2name(mid));
	}
    }
    rb_define_method(bdb1_cDelegate, "method_missing", bdb1_deleg_missing, -1);
    rb_define_method(bdb1_cDelegate, "inspect", bdb1_deleg_inspect, 0);
    rb_define_method(bdb1_cDelegate, "to_s", bdb1_deleg_to_s, 0);
    rb_define_method(bdb1_cDelegate, "to_str", bdb1_deleg_to_str, 0);
    rb_define_method(bdb1_cDelegate, "to_a", bdb1_deleg_to_a, 0);
    rb_define_method(bdb1_cDelegate, "to_ary", bdb1_deleg_to_ary, 0);
    rb_define_method(bdb1_cDelegate, "to_i", bdb1_deleg_to_i, 0);
    rb_define_method(bdb1_cDelegate, "to_int", bdb1_deleg_to_int, 0);
    rb_define_method(bdb1_cDelegate, "to_f", bdb1_deleg_to_f, 0);
    rb_define_method(bdb1_cDelegate, "to_hash", bdb1_deleg_to_hash, 0);
    rb_define_method(bdb1_cDelegate, "to_io", bdb1_deleg_to_io, 0);
    rb_define_method(bdb1_cDelegate, "to_proc", bdb1_deleg_to_proc, 0);
    rb_define_method(bdb1_cDelegate, "_dump", bdb1_deleg_dump, 1);
    rb_define_singleton_method(bdb1_cDelegate, "_load", bdb1_deleg_load, 1);
    /* don't use please */
    rb_define_method(bdb1_cDelegate, "to_orig", bdb1_deleg_to_orig, 0);
    rb_define_method(rb_mKernel, "to_orig", bdb1_deleg_orig, 0);
}
