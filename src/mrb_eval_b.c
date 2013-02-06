
// #define DISABLE_EVAL_T

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/proc.h>
#include <mruby/dump.h>
#include "opcode.h"
#include <setjmp.h>

#ifndef DISABLE_EVAL_T
#include <mruby/compile.h>
#endif


mrb_value
mrb_yield_internal(mrb_state *mrb, mrb_value b, int argc, mrb_value *argv, mrb_value self, struct RClass *c);

static void
replace_stop_with_return(mrb_state *mrb, mrb_irep *irep)
{
  if (irep->iseq[irep->ilen - 1] == MKOP_A(OP_STOP, 0)) {
    irep->iseq = mrb_realloc(mrb, irep->iseq, (irep->ilen + 1) * sizeof(mrb_code));
    irep->iseq[irep->ilen - 1] = MKOP_A(OP_LOADNIL, 0);
    irep->iseq[irep->ilen] = MKOP_AB(OP_RETURN, 0, OP_R_NORMAL);
    irep->ilen++;
  }
}

static mrb_value
mrb_eval_run(mrb_state *mrb, int n)
{
  mrb_value result = mrb_nil_value();
  
  if (n >= 0) {
    struct RProc *proc;
    mrb_irep *irep = mrb->irep[n];
    
    replace_stop_with_return(mrb, irep);
    proc = mrb_proc_new(mrb, irep);
    proc->target_class = mrb->object_class;
    result = mrb_yield_internal(mrb, mrb_obj_value(proc), 0, NULL, mrb_top_self(mrb), mrb->object_class);
  } else if (mrb->exc) {
    // fail to load.
    longjmp(*(jmp_buf*)mrb->jmp, 1);
  }
  
  return result;
}


static mrb_value
mrb_eval_b(mrb_state *mrb, mrb_value self)
{
  int arena_idx;
  int n = -1;
  mrb_value code = mrb_nil_value();
  
  mrb_get_args(mrb, "o", &code);
  if (mrb_nil_p(code) || mrb_type(code) != MRB_TT_STRING){
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid argument");
  }
  
  arena_idx = mrb_gc_arena_save(mrb);
  n = mrb_read_irep(mrb, RSTRING_PTR(code));
  mrb_gc_arena_restore(mrb, arena_idx);
  
  return mrb_eval_run(mrb, n);
}

#ifndef DISABLE_EVAL_T
static mrb_value
mrb_eval_t(mrb_state *mrb, mrb_value self)
{
  int arena_idx;
  int n = -1;
  mrb_value code = mrb_nil_value();
  struct mrb_parser_state *parser;
  mrbc_context *cxt = NULL;
  mrb_value result = mrb_nil_value();
  
  mrb_get_args(mrb, "o", &code);
  if (mrb_nil_p(code) || mrb_type(code) != MRB_TT_STRING){
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid argument");
  }
  
  parser = mrb_parser_new(mrb);
  parser->s    = RSTRING_PTR(code);
  parser->send = RSTRING_PTR(code) + strlen(RSTRING_PTR(code));
  parser->lineno = 1;
  mrb_parser_parse(parser, cxt);
  if (0 < parser->nerr) {
    /* syntax error */
  }
  else {
    /* generate bytecode */
    arena_idx = mrb_gc_arena_save(mrb);
    n = mrb_generate_code(mrb, parser);
    mrb_gc_arena_restore(mrb, arena_idx);
    result = mrb_eval_run(mrb, n);
  }
  mrb_parser_free(parser);
  
  return result;
}
#endif


/*********************************************************
 * register
 *********************************************************/

void
mrb_mruby_eval_b_gem_init(mrb_state* mrb)
{
  mrb_define_method(mrb, mrb->kernel_module, "eval_b", mrb_eval_b, ARGS_REQ(1));
#ifndef DISABLE_EVAL_T
  mrb_define_method(mrb, mrb->kernel_module, "eval_t", mrb_eval_t, ARGS_REQ(1));
#endif
}
