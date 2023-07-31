// Copyright Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
* Copyright © 2010 Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* constant of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, constant, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above constantright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR CONSTANTRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

/**
* \file opt_constant_propagation.cpp
*
* Tracks assignments of constants to channels of variables, and
* usage of those constant channels with direct usage of the constants.
*
* This can lead to constant folding and algebraic optimizations in
* those later expressions, while causing no increase in instruction
* count (due to constants being generally free to load from a
* constant push buffer or as instruction immediate values) and
* possibly reducing register pressure.
*/

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "ir_visitor.h"
#include "ir_rvalue_visitor.h"
#include "ir_basic_block.h"
#include "ir_optimization.h"
#include "glsl_types.h"
#include "hash_table.h"

class cpv_entry : public exec_node
{
public:
	cpv_entry(ir_variable *var, unsigned write_mask, ir_constant *constant)
	{
		check(var);
		check(constant);
		this->var = var;
		this->write_mask = write_mask;
		this->constant = constant;
		this->initial_values = write_mask;
	}

	cpv_entry(const cpv_entry *src)
	{
		this->var = src->var;
		this->write_mask = src->write_mask;
		this->constant = src->constant;
		this->initial_values = src->initial_values;
	}

	ir_variable *var;
	ir_constant *constant;
	unsigned write_mask;

	/** Mask of values initially available in the constant. */
	unsigned initial_values;
};

class constprop_hash_table
{
public:
	constprop_hash_table(void *imem_ctx)
	: acp_ht(nullptr)
	, mem_ctx(imem_ctx)
	{
		this->acp_ht = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
	}
	
	~constprop_hash_table()
	{
		hash_table_dtor(acp_ht);
	}
	
	static void copy_function(const void *key, void *data, void *closure)
	{
		constprop_hash_table* ht = (constprop_hash_table*)closure;
		exec_list* list = new(ht->mem_ctx)exec_list;
		hash_table_insert(ht->acp_ht, list, key);
		exec_list* src_list = (exec_list*)data;
		foreach_iter(exec_list_iterator, iter, *src_list)
		{
			cpv_entry* a = (cpv_entry*)iter.get();
			if (a->get_next() && a->get_prev())
			{
                cpv_entry* entry = new(ht->mem_ctx) cpv_entry(*a);
				list->push_tail(entry);
			}
		}
	}
	
	static constprop_hash_table* copy(constprop_hash_table const& other)
	{
		constprop_hash_table* ht = new constprop_hash_table(other.mem_ctx);
		
		/* Populate the initial acp with a copy of the original */
		hash_table_call_foreach(other.acp_ht, &copy_function, ht);
		
		return ht;
	}
	
	void add_acp(cpv_entry* entry)
	{
		add_acp_hash(entry->var, entry);
	}
	
	void add_acp_hash(ir_variable* var, cpv_entry* entry)
	{
		exec_list* entries = (exec_list*)hash_table_find(acp_ht, var);
		if (!entries)
		{
			entries = new (mem_ctx)exec_list;
			hash_table_insert(acp_ht, entries, var);
		}
		bool bFound = false;
		foreach_iter(exec_list_iterator, iter, *entries)
		{
			cpv_entry* a = (cpv_entry *)iter.get();
			if (entry == a || (entry->var == a->var && entry->write_mask == a->write_mask && entry->constant == a->constant))
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			entries->push_tail(entry);
		}
	}
	
	exec_list* find_acp_hash_entry_list(ir_variable* var)
	{
		return (exec_list*)hash_table_find(acp_ht, var);
	}
	
	void make_empty()
	{
		hash_table_clear(acp_ht);
	}
	
private:
	/** List of cpv_entry: The available copies to propagate */
	hash_table* acp_ht;
	void *mem_ctx;
};

class kill_entry : public exec_node
{
public:
	kill_entry(ir_variable *var, unsigned write_mask)
	{
		check(var);
		this->var = var;
		this->write_mask = write_mask;
	}

	ir_variable *var;
	unsigned write_mask;
};

class ir_constant_propagation_visitor : public ir_rvalue_visitor
{
public:
	ir_constant_propagation_visitor()
	{
		progress = false;
		mem_ctx = ralloc_context(0);
		this->acp = new constprop_hash_table(mem_ctx);
        this->kills = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
		this->killed_all = false;
		this->conservative_propagation = true;
	}
	~ir_constant_propagation_visitor()
	{
        delete acp;
        hash_table_dtor(this->kills);
		ralloc_free(mem_ctx);
	}

	virtual ir_visitor_status visit_enter(class ir_loop *);
	virtual ir_visitor_status visit_enter(class ir_function_signature *);
	virtual ir_visitor_status visit_enter(class ir_function *);
	virtual ir_visitor_status visit_leave(class ir_assignment *);
	virtual ir_visitor_status visit_enter(class ir_call *);
	virtual ir_visitor_status visit_enter(class ir_if *);

	void add_constant(ir_assignment *ir);
	void kill(ir_variable *ir, unsigned write_mask);
	void handle_if_block(exec_list *instructions);
	void handle_rvalue(ir_rvalue **rvalue);

	/** List of cpv_entry: The available constants to propagate */
	constprop_hash_table *acp;

	/**
	* List of kill_entry: The masks of variables whose values were
	* killed in this block.
	*/
	hash_table *kills;

	bool progress;

	bool killed_all;

	bool conservative_propagation;

	void *mem_ctx;
};


void
ir_constant_propagation_visitor::handle_rvalue(ir_rvalue **rvalue)
{
	if (this->in_assignee || !*rvalue)
		return;

	const glsl_type *type = (*rvalue)->type;
	if (!type->is_scalar() && !type->is_vector())
		return;

	ir_swizzle *swiz = NULL;
	ir_dereference_variable *deref = (*rvalue)->as_dereference_variable();
	if (!deref)
	{
		swiz = (*rvalue)->as_swizzle();
		if (!swiz)
			return;

		deref = swiz->val->as_dereference_variable();
		if (!deref)
			return;
	}

	ir_constant_data data;
	memset(&data, 0, sizeof(data));

	for (unsigned int i = 0; i < type->components(); i++)
	{
		int channel;
		cpv_entry *found = NULL;

		if (swiz)
		{
			switch (i)
			{
			case 0: channel = swiz->mask.x; break;
			case 1: channel = swiz->mask.y; break;
			case 2: channel = swiz->mask.z; break;
			case 3: channel = swiz->mask.w; break;
			default: check(!"shouldn't be reached"); channel = 0; break;
			}
		}
		else
		{
			channel = i;
		}

        exec_list* list = acp->find_acp_hash_entry_list(deref->var);
        if (list)
        {
            foreach_iter(exec_list_iterator, iter, *list)
            {
                cpv_entry *entry = (cpv_entry *)iter.get();
                if (entry->write_mask & (1 << channel))
                {
                    found = entry;
                    break;
                }
            }
        }

		if (!found)
			return;

		int rhs_channel = 0;
		for (int j = 0; j < 4; j++)
		{
			if (j == channel)
				break;
			if (found->initial_values & (1 << j))
				rhs_channel++;
		}

		switch (type->base_type)
		{
		case GLSL_TYPE_HALF:
		case GLSL_TYPE_FLOAT:
			data.f[i] = found->constant->value.f[rhs_channel];
			break;
		case GLSL_TYPE_INT:
			data.i[i] = found->constant->value.i[rhs_channel];
			break;
		case GLSL_TYPE_UINT:
			data.u[i] = found->constant->value.u[rhs_channel];
			break;
		case GLSL_TYPE_BOOL:
			data.b[i] = found->constant->value.b[rhs_channel];
			break;
		default:
			check(!"not reached");
			break;
		}
	}

	ir_constant* Constant = new(ralloc_parent(deref)) ir_constant(type, &data);
	*rvalue = Constant;
	this->progress = true;
	if (!Constant->is_finite())
	{
		// Debug point
		int i = 0;
		++i;
	}
}

ir_visitor_status
ir_constant_propagation_visitor::visit_enter(ir_function_signature *ir)
{
	/* Treat entry into a function signature as a completely separate
	* block.  Any instructions at global scope will be shuffled into
	* main() at link time, so they're irrelevant to us.
	*/
	constprop_hash_table *orig_acp = this->acp;
	hash_table *orig_kills = this->kills;
	bool orig_killed_all = this->killed_all;

	this->acp = new constprop_hash_table(mem_ctx);
	this->kills = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
	this->killed_all = false;

	visit_list_elements(this, &ir->body);

    hash_table_dtor(this->kills);
    delete acp;
    
	this->kills = orig_kills;
	this->acp = orig_acp;
	this->killed_all = orig_killed_all;

	return visit_continue_with_parent;
}

ir_visitor_status
ir_constant_propagation_visitor::visit_leave(ir_assignment *ir)
{
	ir_rvalue_visitor::visit_leave(ir);

	if (this->in_assignee)
	{
		return visit_continue;
	}

	kill(ir->lhs->variable_referenced(), ir->write_mask);

	add_constant(ir);

	return visit_continue;
}

ir_visitor_status
ir_constant_propagation_visitor::visit_enter(ir_function *ir)
{
	(void)ir;
	return visit_continue;
}

ir_visitor_status
ir_constant_propagation_visitor::visit_enter(ir_call *ir)
{
	/* Do constant propagation on call parameters, but skip any out params */
	bool has_out_params = false;
	exec_list_iterator sig_param_iter = ir->callee->parameters.iterator();
	foreach_iter(exec_list_iterator, iter, ir->actual_parameters)
	{
		ir_variable *sig_param = (ir_variable *)sig_param_iter.get();
		ir_rvalue *param = (ir_rvalue *)iter.get();
		if (sig_param->mode != ir_var_out && sig_param->mode != ir_var_inout)
		{
			ir_rvalue *new_param = param;
			handle_rvalue(&new_param);
			if (new_param != param)
				param->replace_with(new_param);
			else
				param->accept(this);
		}
		else
		{
			has_out_params = true;
			ir_variable* param_var = param->as_variable();
			if (param_var && !conservative_propagation)
			{
				//todo: can probably be less aggressive here by tracking what components the function actually writes to.
				kill(param_var, ~0);
			}
		}
		sig_param_iter.next();
	}

	if (!ir->callee->is_builtin || (has_out_params && conservative_propagation))
	{
		/* Since we're unlinked, we don't (necssarily) know the side effects of
		* this call.  So kill all copies.
		*/
		acp->make_empty();
		this->killed_all = true;
	}

	return visit_continue_with_parent;
}

static void kill_function(const void *key, void *data, void *closure)
{
    ir_constant_propagation_visitor *visitor = (ir_constant_propagation_visitor*)closure;
    kill_entry *k = (kill_entry*)data;
    visitor->kill(k->var, k->write_mask);
}

void
ir_constant_propagation_visitor::handle_if_block(exec_list *instructions)
{
	constprop_hash_table *orig_acp = this->acp;
	hash_table *orig_kills = this->kills;
	bool orig_killed_all = this->killed_all;

    /* Populate the initial acp with a constant of the original */
    this->acp = constprop_hash_table::copy(*orig_acp);
	this->kills = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
	this->killed_all = false;

	visit_list_elements(this, instructions);

	if (this->killed_all)
	{
		orig_acp->make_empty();
	}
    
    delete acp;

	hash_table *new_kills = this->kills;
	this->kills = orig_kills;
	this->acp = orig_acp;
	this->killed_all = this->killed_all || orig_killed_all;

    hash_table_call_foreach(new_kills, &kill_function, this);
    
    hash_table_dtor(new_kills);
}

ir_visitor_status
ir_constant_propagation_visitor::visit_enter(ir_if *ir)
{
	ir->condition->accept(this);
	handle_rvalue(&ir->condition);

	handle_if_block(&ir->then_instructions);
	handle_if_block(&ir->else_instructions);

	/* handle_if_block() already descended into the children. */
	return visit_continue_with_parent;
}

ir_visitor_status
ir_constant_propagation_visitor::visit_enter(ir_loop *ir)
{
	constprop_hash_table *orig_acp = this->acp;
	hash_table *orig_kills = this->kills;
	bool orig_killed_all = this->killed_all;

	/* FINISHME: For now, the initial acp for loops is totally empty.
	* We could go through once, then go through again with the acp
	* cloned minus the killed entries after the first run through.
	*/
    this->acp = new constprop_hash_table(mem_ctx);
    this->kills = hash_table_ctor(1543, ir_hash_table_pointer_hash, ir_hash_table_pointer_compare);
	this->killed_all = false;

	visit_list_elements(this, &ir->body_instructions);

	if (this->killed_all)
	{
		orig_acp->make_empty();
	}
    
    delete acp;

	hash_table *new_kills = this->kills;
	this->kills = orig_kills;
	this->acp = orig_acp;
	this->killed_all = this->killed_all || orig_killed_all;

	hash_table_call_foreach(new_kills, &kill_function, this);
    
    hash_table_dtor(new_kills);

	/* already descended into the children. */
	return visit_continue_with_parent;
}


void
ir_constant_propagation_visitor::kill(ir_variable *var, unsigned write_mask)
{
	check(var != NULL);

	/* We don't track non-vectors. */
	if (!var->type->is_vector() && !var->type->is_scalar())
		return;

	/* Remove any entries currently in the ACP for this kill. */
    exec_list* list = acp->find_acp_hash_entry_list(var);
    if (list)
    {
        foreach_iter(exec_list_iterator, iter, *list)
        {
            cpv_entry *entry = (cpv_entry *)iter.get();

            if (entry->var == var)
            {
                entry->write_mask &= ~write_mask;
                if (entry->write_mask == 0)
                {
                    entry->remove();
                }
            }
        }
    }

	/* Add this writemask of the variable to the list of killed
	* variables in this block.
	*/
    kill_entry *entry = (kill_entry *)hash_table_find(kills, var);
    if (entry)
    {
        entry->write_mask |= write_mask;
        return;
    }

    /* Not already in the list.  Make new entry. */
    hash_table_insert(kills, new(this->mem_ctx) kill_entry(var, write_mask), var);
}

/**
* Adds an entry to the available constant list if it's a plain assignment
* of a variable to a variable.
*/
void
ir_constant_propagation_visitor::add_constant(ir_assignment *ir)
{
	cpv_entry *entry;

	if (ir->condition)
		return;

	if (!ir->write_mask)
		return;

	ir_dereference_variable *deref = ir->lhs->as_dereference_variable();
	ir_constant *constant = ir->rhs->as_constant();

	if (!deref || !constant)
		return;

	/* Only do constant propagation on vectors.  Constant matrices,
	* arrays, or structures would require more work elsewhere.
	*/
	if (!deref->var->type->is_vector() && !deref->var->type->is_scalar())
		return;

	// Shared variables should be considered volatile
	if (deref->var->mode == ir_var_shared)
		return;

	entry = new(this->mem_ctx) cpv_entry(deref->var, ir->write_mask, constant);
	this->acp->add_acp(entry);
}

/**
* Does a constant propagation pass on the code present in the instruction stream.
*/
bool do_constant_propagation(exec_list *instructions, bool conservative_propagation)
{
	ir_constant_propagation_visitor v;
	v.conservative_propagation = conservative_propagation;

	visit_list_elements(&v, instructions);

	return v.progress;
}
