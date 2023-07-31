// Copyright Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

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
* \file opt_constant_variable.cpp
*
* Marks variables assigned a single constant value over the course
* of the program as constant.
*
* The goal here is to trigger further constant folding and then dead
* code elimination.  This is common with vector/matrix constructors
* and calls to builtin functions.
*/

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "ir_visitor.h"
#include "ir_optimization.h"
#include "glsl_types.h"
#include "hash_table.h"

struct assignment_entry
{
	exec_node link;
	int assignment_count;
	ir_variable *var;
	ir_constant *constval;
	bool our_scope;
};

class ir_constant_variable_visitor : public ir_hierarchical_visitor
{
public:
	ir_constant_variable_visitor();
	virtual ~ir_constant_variable_visitor();
	
	virtual ir_visitor_status visit(ir_variable *) override;
	virtual ir_visitor_status visit_enter(ir_assignment *) override;
	virtual ir_visitor_status visit_enter(ir_call *) override;

	hash_table* ht;
	exec_list list;
};

ir_constant_variable_visitor::ir_constant_variable_visitor()
: ht(nullptr)
{
	ht = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
}

ir_constant_variable_visitor::~ir_constant_variable_visitor()
{
	hash_table_dtor(ht);
}

static struct assignment_entry * get_assignment_entry(ir_variable *var, hash_table *ht, exec_list* list)
{
	struct assignment_entry *entry = (struct assignment_entry *)hash_table_find(ht, var);
	if(!entry)
	{
		entry = (struct assignment_entry *)calloc(1, sizeof(*entry));
		entry->var = var;
		hash_table_insert(ht, entry, var);
		list->push_head(&entry->link);
	}
	return entry;
}

ir_visitor_status ir_constant_variable_visitor::visit(ir_variable *ir)
{
	struct assignment_entry *entry = get_assignment_entry(ir, ht, &this->list);

	// Shared variables should be considered volatile
	if (ir->mode != ir_var_shared)
		entry->our_scope = true;

	return visit_continue;
}

ir_visitor_status ir_constant_variable_visitor::visit_enter(ir_assignment *ir)
{
	ir_constant *constval;
	struct assignment_entry *entry;

	entry = get_assignment_entry(ir->lhs->variable_referenced(), ht, &this->list);
	check(entry);
	entry->assignment_count++;

	/* If it's already constant, don't do the work. */
	if (entry->var->constant_value)
		return visit_continue;

	/* OK, now find if we actually have all the right conditions for
	* this to be a constant value assigned to the var.
	*/
	if (ir->condition)
		return visit_continue;

	ir_variable *var = ir->whole_variable_written();
	if (!var)
		return visit_continue;

	// This avoid this pattern that fails:
	//
	//	int SimulationStageIndex;
	//	RWBuffer<float> RWOutputFloat;
	//	[numthreads(32, 1, 1)]
	//	void SimulateMainComputeCS(float f : TEXCOORD)
	//	{
	//		if (SimulationStageIndex != 0)
	//		{
	//			f = 0;
	//		}

	//		RWOutputFloat[0] = f;
	//	}
	// The optimizer doesn't take into consideration the argument from the function could has an initial value. This causes
	// other consequences such as some expressions can't get removed. We might need to make sure that has no other side effects.

	if (var->mode == ir_var_in)
	{
		return visit_continue;
	}

	constval = ir->rhs->constant_expression_value();
	if (!constval)
		return visit_continue;

	/* Mark this entry as having a constant assignment (if the
	* assignment count doesn't go >1).  do_constant_variable will fix
	* up the variable with the constant value later.
	*/
	entry->constval = constval;

	return visit_continue;
}

ir_visitor_status ir_constant_variable_visitor::visit_enter(ir_call *ir)
{
	/* Mark any out parameters as assigned to */
	exec_list_iterator sig_iter = ir->callee->parameters.iterator();
	foreach_iter(exec_list_iterator, iter, *ir)
	{
		ir_rvalue *param_rval = (ir_rvalue *)iter.get();
		ir_variable *param = (ir_variable *)sig_iter.get();

		if (param->mode == ir_var_out ||
			param->mode == ir_var_inout)
		{
			ir_variable *var = param_rval->variable_referenced();
			struct assignment_entry *entry;

			check(var);
			entry = get_assignment_entry(var, ht, &this->list);
			entry->assignment_count++;
		}
		sig_iter.next();
	}

	/* Mark the return storage as having been assigned to */
	if (ir->return_deref != NULL)
	{
		ir_variable *var = ir->return_deref->variable_referenced();
		struct assignment_entry *entry;

		check(var);
		entry = get_assignment_entry(var, ht, &this->list);
		entry->assignment_count++;
	}

	return visit_continue;
}

/**
* Does a copy propagation pass on the code present in the instruction stream.
*/
bool do_constant_variable(exec_list *instructions)
{
	bool progress = false;
	ir_constant_variable_visitor v;

	v.run(instructions);

	while (!v.list.is_empty())
	{
		struct assignment_entry *entry;
		entry = exec_node_data(struct assignment_entry, v.list.head, link);

		if (entry->assignment_count == 1 && entry->constval && entry->our_scope)
		{
			entry->var->constant_value = entry->constval;
			progress = true;
		}
		entry->link.remove();
		free(entry);
	}

	return progress;
}

bool do_constant_variable_unlinked(exec_list *instructions)
{
	bool progress = false;

	foreach_iter(exec_list_iterator, iter, *instructions)
	{
		ir_instruction *ir = (ir_instruction *)iter.get();
		ir_function *f = ir->as_function();
		if (f)
		{
			foreach_iter(exec_list_iterator, sigiter, *f)
			{
				ir_function_signature *sig =
					(ir_function_signature *)sigiter.get();
				if (do_constant_variable(&sig->body))
				{
					progress = true;
				}
			}
		}
	}

	return progress;
}
