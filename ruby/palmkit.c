#include <stdlib.h>
#include <slm.h>
#include <ruby.h>

static VALUE cNgram;  /* N-gram class */
static SLMBOStatus bo_status;

static VALUE
rb_Ngram_new(int argc, VALUE *argv, VALUE self)
{
    SLMNgram *ng;
    VALUE filename, filetype, verbosity, retval;
    int type;

    if (rb_scan_args(argc, argv, "21", &filename, &filetype, &verbosity) == 2)
	verbosity = INT2FIX(2); /* default value */
    else
	Check_Type(verbosity, T_FIXNUM);

    Check_Type(filename,T_STRING);
    Check_Type(filetype,T_STRING);

    if (strcmp(RSTRING(filetype)->ptr,"SLM_LM_ARPA") == 0)
      type = SLM_LM_ARPA;
    else if (strcmp(RSTRING(filetype)->ptr,"SLM_LM_BINARY") == 0)
      type = SLM_LM_BINARY;
    else {
      rb_raise(rb_eArgError,"Ngram::new : unknown LM type");
    }

    ng = SLMReadLM(RSTRING(filename)->ptr,type,FIX2INT(verbosity));
    retval = Data_Wrap_Struct(cNgram, 0, SLMFreeLM, ng);
    return retval;
}

static VALUE
rb_Ngram_to_id(VALUE self, VALUE word)
{
    SLMWordID id;
    SLMNgram *ng;

    Check_Type(word,T_STRING);
    Data_Get_Struct(self,SLMNgram,ng);
    id = SLMWord2ID(ng,RSTRING(word)->ptr);
    return INT2FIX(id);
}

static VALUE
rb_Ngram_to_word(VALUE self, VALUE id)
{
    SLMNgram *ng;
    SLMWordID wid;

    Check_Type(id,T_FIXNUM);
    wid = FIX2INT(id);
    Data_Get_Struct(self,SLMNgram,ng);
    if (wid >= ng->n_unigram) {
	rb_raise(rb_eArgError,"Invalid ID value");
    }
    if (wid == 0)
	return rb_str_new2("<UNK>");
    else
	return rb_str_new2(ng->vocab[wid]);
}

static VALUE
rb_Ngram_prob(VALUE self, VALUE vidarray)
{
    SLMNgram *ng;
    SLMWordID *wids;
    struct RArray *idarray;
    double prob;
    int i;

    Check_Type(vidarray,T_ARRAY);
    idarray = RARRAY(vidarray);
    Data_Get_Struct(self,SLMNgram,ng);
    wids = ALLOCA_N(SLMWordID,idarray->len);
    for (i = 0; i < idarray->len; i++) {
	if (TYPE(idarray->ptr[i]) == T_FIXNUM)
	    wids[i] = FIX2INT(idarray->ptr[i]);
	else if (TYPE(idarray->ptr[i]) == T_STRING)
	    wids[i] = SLMWord2ID(ng,RSTRING(idarray->ptr[i])->ptr);
	else {
	    rb_raise(rb_eTypeError,"Ngram::prob : value is not a Fixnum nor String");
	}
    }
    prob = SLMGetBOProb(ng,idarray->len, wids, &bo_status);
    return rb_float_new(prob);
}

static VALUE
rb_Ngram_status(VALUE self)
{
    VALUE res,hit;
    int i;
    res = rb_ary_new2(4);
    rb_ary_push(res,INT2FIX(bo_status.len));
    hit = rb_ary_new2(bo_status.len);
    for (i = 0; i < bo_status.len; i++) {
        VALUE st;
        switch(bo_status.hit[i]) {
	case SLM_STAT_HIT:
	    st = rb_str_new2("hit");
	    break;
	case SLM_STAT_BO_WITH_ALPHA:
	    st = rb_str_new2("bo");
	    break;
	case SLM_STAT_BO:
	    st = rb_str_new2("none");
	    break;
	}
        rb_ary_push(hit,st);
    }
    rb_ary_push(res,hit);
    rb_ary_push(res,rb_float_new(bo_status.ng_prob));
    rb_ary_push(res,rb_float_new(bo_status.ug_prob));
    return res;
}

static VALUE
rb_Ngram_length(VALUE self)
{
    int n;
    SLMNgram *ng;

    Data_Get_Struct(self,SLMNgram,ng);
    n = SLMNgramLength(ng);
    return INT2FIX(n);
}

static VALUE
rb_Ngram_context_length(VALUE self)
{
    int n;
    SLMNgram *ng;

    Data_Get_Struct(self,SLMNgram,ng);
    n = SLMContextLength(ng);
    return INT2FIX(n);
}

static VALUE
rb_Ngram_vocab_size(VALUE self)
{
    int n;
    SLMNgram *ng;

    Data_Get_Struct(self,SLMNgram,ng);
    n = SLMVocabSize(ng);
    return INT2FIX(n);
}

void
Init_palmkit()
{
    /* class definition */
    cNgram = rb_define_class("Ngram",rb_cObject);
    
    /* factory method definition */
    rb_define_singleton_method(cNgram, "new", rb_Ngram_new, -1);

    /* instance method definition */
    rb_define_method(cNgram, "to_id", rb_Ngram_to_id, 1);
    rb_define_method(cNgram, "to_word", rb_Ngram_to_word, 1);
    rb_define_method(cNgram, "prob", rb_Ngram_prob, 1);
    rb_define_method(cNgram, "status", rb_Ngram_status, 0);
    rb_define_method(cNgram, "length", rb_Ngram_length, 0);
    rb_define_method(cNgram, "n", rb_Ngram_length, 0);
    rb_define_method(cNgram, "context_length", rb_Ngram_context_length, 0);
    rb_define_method(cNgram, "vocab_size", rb_Ngram_vocab_size, 0);
}
