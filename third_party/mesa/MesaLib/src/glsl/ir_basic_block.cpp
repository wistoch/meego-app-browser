/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file ir_basic_block.cpp
 *
 * Basic block analysis of instruction streams.
 */

#include "ir.h"
#include "ir_visitor.h"
#include "ir_basic_block.h"
#include "glsl_types.h"

class ir_has_call_visitor : public ir_hierarchical_visitor {
public:
   ir_has_call_visitor()
   {
      has_call = false;
   }

   virtual ir_visitor_status visit_enter(ir_call *ir)
   {
      (void) ir;
      has_call = true;
      return visit_stop;
   }

   bool has_call;
};

bool
ir_has_call(ir_instruction *ir)
{
   ir_has_call_visitor v;
   ir->accept(&v);
   return v.has_call;
}

/**
 * Calls a user function for every basic block in the instruction stream.
 *
 * Basic block analysis is pretty easy in our IR thanks to the lack of
 * unstructured control flow.  We've got:
 *
 * ir_loop (for () {}, while () {}, do {} while ())
 * ir_loop_jump (
 * ir_if () {}
 * ir_return
 * ir_call()
 *
 * Note that the basic blocks returned by this don't encompass all
 * operations performed by the program -- for example, if conditions
 * don't get returned, nor do the assignments that will be generated
 * for ir_call parameters.
 */
void call_for_basic_blocks(exec_list *instructions,
			   void (*callback)(ir_instruction *first,
					    ir_instruction *last,
					    void *data),
			   void *data)
{
   ir_instruction *leader = NULL;
   ir_instruction *last = NULL;

   foreach_iter(exec_list_iterator, iter, *instructions) {
      ir_instruction *ir = (ir_instruction *)iter.get();
      ir_if *ir_if;
      ir_loop *ir_loop;
      ir_function *ir_function;

      if (!leader)
	 leader = ir;

      if ((ir_if = ir->as_if())) {
	 callback(leader, ir, data);
	 leader = NULL;

	 call_for_basic_blocks(&ir_if->then_instructions, callback, data);
	 call_for_basic_blocks(&ir_if->else_instructions, callback, data);
      } else if ((ir_loop = ir->as_loop())) {
	 callback(leader, ir, data);
	 leader = NULL;
	 call_for_basic_blocks(&ir_loop->body_instructions, callback, data);
      } else if (ir->as_return() || ir->as_call()) {
	 callback(leader, ir, data);
	 leader = NULL;
      } else if ((ir_function = ir->as_function())) {
	 /* A function definition doesn't interrupt our basic block
	  * since execution doesn't go into it.  We should process the
	  * bodies of its signatures for BBs, though.
	  *
	  * Note that we miss an opportunity for producing more
	  * maximal BBs between the instructions that precede main()
	  * and the body of main().  Perhaps those instructions ought
	  * to live inside of main().
	  */
	 foreach_iter(exec_list_iterator, fun_iter, *ir_function) {
	    ir_function_signature *ir_sig;

	    ir_sig = (ir_function_signature *)fun_iter.get();

	    call_for_basic_blocks(&ir_sig->body, callback, data);
	 }
      } else if (ir->as_assignment()) {
	 /* If there's a call in the expression tree being assigned,
	  * then that ends the BB too.
	  *
	  * The assumption is that any consumer of the basic block
	  * walker is fine with the fact that the call is somewhere in
	  * the tree even if portions of the tree may be evaluated
	  * after the call.
	  *
	  * A consumer that has an issue with this could not process
	  * the last instruction of the basic block.  If doing so,
	  * expression flattener may be useful before using the basic
	  * block finder to get more maximal basic blocks out.
	  */
	 if (ir_has_call(ir)) {
	    callback(leader, ir, data);
	    leader = NULL;
	 }
      }
      last = ir;
   }
   if (leader) {
      callback(leader, last, data);
   }
}
