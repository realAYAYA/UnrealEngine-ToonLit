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
 * \file opt_copy_propagation.cpp
 *
 * Moves usage of recently-copied variables to the previous copy of
 * the variable.
 *
 * This should reduce the number of MOV instructions in the generated
 * programs unless copy propagation is also done on the LIR, and may
 * help anyway by triggering other optimizations that live in the HIR.
 */

#include "ShaderCompilerCommon.h"
#include "ir.h"
#include "ir_visitor.h"
#include "ir_basic_block.h"
#include "ir_optimization.h"
#include "ir_rvalue_visitor.h"
#include "glsl_types.h"
#include "hash_table.h"

#include <map>
#include <set>

static bool Debug = false;

template<typename Class, size_t num>
struct Pool
{
	void* mem_ctx;
	exec_list FreeList;

	struct Type : public exec_node
	{
		Pool* PoolPtr;

		/* Callers of this ralloc-based new need not call delete. It's
		 * easier to just ralloc_free 'ctx' (or any of its ancestors). */
		static void* operator new(size_t size, void* ctx)
		{
			Pool* p = (Pool*)ctx;
			void* v = p->FreeList.pop_head();
			if (!v)
			{
				char* mem = (char*)ralloc_size(p->mem_ctx, size * num);
				v = mem;
				for (unsigned i = 1; i < num; i++)
				{
					Class* t = (Class*)(mem + (size * i));
					p->FreeList.push_tail(t);
				}
			}

			Class* t = (Class*)v;
			t->PoolPtr = p;
			return v;
		}

		/* If the user *does* call delete, that's OK, we will just
		 * ralloc_free in that case. */
		static void operator delete(void* node)
		{
			Class* t = (Class*)node;
			t->PoolPtr->FreeList.push_tail(t);
		}
	};
};

class acp_entry
{
public:
	acp_entry(ir_variable *lhs, ir_variable *rhs_var, ir_dereference_array *array_deref)
	{
		check(lhs);
		this->lhs = lhs;
		this->rhs_var = rhs_var;
		this->array_deref = array_deref;
	}

	ir_variable* GetVariableReferenced() const
	{
		return array_deref ? array_deref->variable_referenced() : rhs_var;
	}

	bool operator<(const acp_entry& other) const
	{
		if (lhs < other.lhs)
		{
			return true;
		}
		else if (lhs > other.lhs)
		{
			return false;
		}
		else if (rhs_var < other.rhs_var)
		{
			return true;
		}
		else if (rhs_var > other.rhs_var)
		{
			return false;
		}
		return array_deref < other.array_deref;
	}

	bool operator==(const acp_entry& rhs) const
	{
		return lhs == rhs.lhs
			&& rhs_var == rhs.rhs_var
			&& array_deref == rhs.array_deref;
	}

	bool references_var(const ir_variable* var) const
	{
		return lhs == var || (rhs_var && rhs_var == var) || (array_deref && array_deref->variable_referenced() == var);
	}

	ir_variable *lhs;
	ir_variable *rhs_var;
	ir_dereference_array *array_deref;
};

class ir_populate_scoped_acp_visitor : public ir_rvalue_visitor
{
public:
	ir_populate_scoped_acp_visitor(class acp_hash_table* in_acp)
		: acp(in_acp)
	{
	}

	virtual void handle_rvalue(ir_rvalue** rvalue) override;

private:
	class acp_hash_table* acp = nullptr;
};

class acp_hash_table
{
public:
	using acp_multimap = std::multimap<ir_variable*, acp_entry>;
	using acp_range = std::pair<acp_multimap::const_iterator, acp_multimap::const_iterator>;

	acp_hash_table(void* imem_ctx)
		: mem_ctx(imem_ctx)
		, killed_all(false)
	{
	}

	acp_hash_table(void* imem_ctx, acp_hash_table* in_parent)
		: mem_ctx(imem_ctx)
		, parent(in_parent)
		, killed_all(false)
	{
	}

	~acp_hash_table()
	{
	}

	static acp_hash_table* copy_for_loop_start(acp_hash_table const& other)
	{
		acp_hash_table* ht = new acp_hash_table(other.mem_ctx);
		
		/* Populate the initial acp with samplers & samplerstates so they propagate */
		for (const auto& EntryIt : other.acp_map)
		{
			const acp_entry* a = &EntryIt.second;
			auto* Var = a->GetVariableReferenced();
			if (Var && Var->type && (Var->type->is_sampler() || Var->type->IsSamplerState()))
			{
				if (Debug)
				{
					printf("ACP_Entry LOOP Block: LHS %d RHS_Var %d DeRef %d\n", a->lhs->id, a->rhs_var ? a->rhs_var->id : -1, a->array_deref ? a->array_deref->id : -1);
				}
				ht->add_acp(acp_entry(a->lhs, a->rhs_var, a->array_deref));
			}
		}

		return ht;
	}

	static acp_hash_table* clone_for_instructions(acp_hash_table& other, exec_list* instructions)
	{
		acp_hash_table* ht = new acp_hash_table(other.mem_ctx, &other);

		ir_populate_scoped_acp_visitor populate_visitor(ht);
		visit_list_elements(&populate_visitor, instructions);

		return ht;
	}

	void add_acp(const acp_entry& entry)
	{
		if (entry.lhs)
		{
			add_acp_hash(entry.lhs, entry);
		}
		if (entry.rhs_var)
		{
			add_acp_hash(entry.rhs_var, entry);
		}
		if (entry.array_deref && entry.array_deref->variable_referenced())
		{
			add_acp_hash(entry.array_deref->variable_referenced(), entry);
		}
	}

	void add_acp_hash(ir_variable* var, const acp_entry& entry)
	{
		const auto itEnd = acp_map.upper_bound(var);
		acp_map.insert(itEnd, std::make_pair(var, entry));
	}

	void add_acp(const acp_entry& entry, acp_multimap::const_iterator insertIt)
	{
		if (entry.lhs)
		{
			add_acp_hash(entry.lhs, entry, insertIt);
		}
		if (entry.rhs_var)
		{
			add_acp_hash(entry.rhs_var, entry, insertIt);
		}
		if (entry.array_deref && entry.array_deref->variable_referenced())
		{
			add_acp_hash(entry.array_deref->variable_referenced(), entry, insertIt);
		}
	}

	void add_acp_hash(ir_variable* var, const acp_entry& entry, acp_multimap::const_iterator insertIt)
	{
		const auto itEnd = acp_map.upper_bound(var);
		for (auto it = acp_map.lower_bound(var); it != itEnd; ++it)
		{
			if (it->second == entry)
			{
				return;
			}
		}

		acp_map.insert(insertIt, std::make_pair(var, entry));
	}

	void make_empty()
	{
		acp_map.clear();
	}

	acp_range find_range(ir_variable* var) const
	{
		return make_pair(acp_map.lower_bound(var), acp_map.upper_bound(var));
	}

	void erase_acp(ir_variable* var)
	{
		std::set<acp_entry> entries_to_invalidate;
		auto itEnd = acp_map.upper_bound(var);
		for (auto it = acp_map.lower_bound(var); it != itEnd; ++it)
		{
			if (it->second.references_var(var))
			{
				entries_to_invalidate.insert(it->second);
			}
		}

		auto RemoveEntriesForVar = [&](ir_variable* v, const acp_entry& e) -> void
		{
			auto itEnd = acp_map.upper_bound(v);
			for (auto it = acp_map.lower_bound(v); it != itEnd;)
			{
				if (it->second == e)
				{
					it = acp_map.erase(it);
					itEnd = acp_map.upper_bound(v);
				}
				else
				{
					++it;
				}
			}
		};

		for (const acp_entry& e : entries_to_invalidate)
		{
			RemoveEntriesForVar(e.lhs, e);
			if (ir_variable* v = e.GetVariableReferenced())
			{
				RemoveEntriesForVar(v, e);
			}
		}
	}

	void pull_acp(ir_variable* var)
	{
		if (parent)
		{
			if (kills.find(var) == kills.end())
			{
				parent->pull_acp(var);

				acp_multimap::const_iterator insertIt = acp_map.lower_bound(var);
				const acp_range parent_entries = parent->find_range(var);
				for (auto it = parent_entries.first; it != parent_entries.second; ++it)
				{
					add_acp(it->second, insertIt);
				}
			}
		}
	}

	void kill_all()
	{
		make_empty();
		killed_all = true;
	}

	/** List of acp_entry: The available copies to propagate */
	acp_multimap acp_map;
	void *mem_ctx;

	acp_hash_table* parent = nullptr;

	using kills_set = std::set<ir_variable*>;
	kills_set kills;

	bool killed_all;
};

void ir_populate_scoped_acp_visitor::handle_rvalue(ir_rvalue** rvalue)
{
	if (rvalue == 0 || *rvalue == 0)// || this->in_assignee)
	{
		return;
	}

	ir_variable* var = nullptr;

	if ((*rvalue)->ir_type == ir_type_dereference_variable)
	{
		ir_dereference_variable* deref_var = (ir_dereference_variable*)(*rvalue);
		var = deref_var->var;
	}
	else if ((*rvalue)->ir_type == ir_type_texture)
	{
		auto* TextureIR = (*rvalue)->as_texture();
		auto* DeRefVar = TextureIR->sampler->as_dereference_variable();
		if (DeRefVar)
		{
			var = DeRefVar->var;
		}
	}

	if (var != nullptr)
	{
		acp->pull_acp(var);
	}
}

class ir_copy_propagation_visitor : public ir_rvalue_visitor
{
public:
	ir_copy_propagation_visitor()
	{
		progress = false;
		mem_ctx = ralloc_context(0);
		this->acp = new acp_hash_table(mem_ctx);
		this->conservative_propagation = true;
	}
	~ir_copy_propagation_visitor()
	{
		delete acp;
		ralloc_free(mem_ctx);
	}

	virtual void handle_rvalue(ir_rvalue **rvalue);
	virtual ir_visitor_status visit_enter(class ir_loop *);
	virtual ir_visitor_status visit_enter(class ir_function_signature *);
	virtual ir_visitor_status visit_enter(class ir_function *);
	virtual ir_visitor_status visit_leave(class ir_assignment *);
	virtual ir_visitor_status visit_enter(class ir_call *);
	virtual ir_visitor_status visit_enter(class ir_if *);

	void add_copy(ir_assignment *ir);
	void kill(ir_variable *ir);
	void handle_if_block(exec_list *instructions);

	/** List of acp_entry: The available copies to propagate */
	acp_hash_table *acp;
	/**
	* List of kill_entry: The variables whose values were killed in this
	* block.
	*/

	bool progress;

	bool conservative_propagation;

	void *mem_ctx;
};

ir_visitor_status ir_copy_propagation_visitor::visit_enter(ir_function_signature *ir)
{
	/* Treat entry into a function signature as a completely separate
	* block.  Any instructions at global scope will be shuffled into
	* main() at link time, so they're irrelevant to us.
	*/
	acp_hash_table *orig_acp = this->acp;

	this->acp = new acp_hash_table(mem_ctx);

	visit_list_elements(this, &ir->body);

	delete acp;
	
	this->acp = orig_acp;

	return visit_continue_with_parent;
}

ir_visitor_status ir_copy_propagation_visitor::visit_leave(ir_assignment *ir)
{
	ir_visitor_status s = ir_rvalue_visitor::visit_leave(ir);

	kill(ir->lhs->variable_referenced());

	add_copy(ir);

	return s;
}

ir_visitor_status ir_copy_propagation_visitor::visit_enter(ir_function *ir)
{
	(void)ir;
	return visit_continue;
}

/**
* Replaces dereferences of ACP RHS variables with ACP LHS variables.
*
* This is where the actual copy propagation occurs.  Note that the
* rewriting of ir_dereference means that the ir_dereference instance
* must not be shared by multiple IR operations!
*/
void ir_copy_propagation_visitor::handle_rvalue(ir_rvalue **rvalue)
{
	if (rvalue == 0 || *rvalue == 0 || this->in_assignee)
	{
		return;
	}

	ir_dereference_variable* deref_var = nullptr;

	if ((*rvalue)->ir_type == ir_type_dereference_variable)
	{
		deref_var = (ir_dereference_variable*)(*rvalue);
	}
	else if ((*rvalue)->ir_type == ir_type_texture)
	{
		auto* TextureIR = (*rvalue)->as_texture();
		deref_var = TextureIR->sampler->as_dereference_variable();
	}
	
	// Shared variables should be considered volatile
	if (deref_var == nullptr || deref_var->var == nullptr || deref_var->var->mode == ir_var_shared || deref_var->var->precise)
	{
		return;
	}

	ir_variable* var = deref_var->var;

	const acp_hash_table::acp_range entry_range = acp->find_range(var);
	for (auto it = entry_range.first; it != entry_range.second; ++it)
	{
		const acp_entry* entry = &it->second;
		if (var == entry->lhs)
		{
			if (entry->rhs_var)
			{
				if (Debug)
				{
					printf("Change DeRef %d to %d\n", deref_var->id, entry->rhs_var->id);
				}

				// This is a full variable copy, so just change the dereference's variable.
				deref_var->var = entry->rhs_var;
				this->progress = true;
				break;
			}
			else if (entry->array_deref)
			{
				if (Debug)
				{
					printf("Replace ArrayDeRef %d to %d\n", deref_var->id, entry->array_deref->id);
				}

				// Propagate the array deref by replacing this variable deref with a clone of the array deref.
				void *ctx = ralloc_parent(*rvalue);
				*rvalue = entry->array_deref->clone(ctx, 0);
				this->progress = true;
				break;
			}
		}
	}
}

ir_visitor_status ir_copy_propagation_visitor::visit_enter(ir_call *ir)
{
	/* Do copy propagation on call parameters, but skip any out params */
	bool has_out_params = false;
	exec_list_iterator sig_param_iter = ir->callee->parameters.iterator();
	foreach_iter(exec_list_iterator, iter, ir->actual_parameters)
	{
		ir_variable *sig_param = (ir_variable *)sig_param_iter.get();
		ir_instruction *ir = (ir_instruction *)iter.get();
		if (sig_param->mode != ir_var_out && sig_param->mode != ir_var_inout)
		{
			ir->accept(this);
		}
		else
		{
			has_out_params = true;
			ir_variable* param_var = ir->as_variable();
			if (param_var && !conservative_propagation)
			{
				//todo: can probably be less aggressive here by tracking what components the function actually writes to.
				kill(param_var);
			}
		}
		sig_param_iter.next();
	}

	if (!ir->callee->is_builtin || (has_out_params && conservative_propagation))
	{
		/* Since we're unlinked, we don't (necessarily) know the side effects of
		* this call.  So kill all copies.
		*/
		acp->kill_all();
	}

	return visit_continue_with_parent;
}

void ir_copy_propagation_visitor::handle_if_block(exec_list *instructions)
{
	acp_hash_table *orig_acp = this->acp;

	/* Populate the initial acp with a copy of the original */
	this->acp = acp_hash_table::clone_for_instructions(*acp, instructions);

	visit_list_elements(this, instructions);

	if (acp->killed_all)
	{
		orig_acp->make_empty();
	}

	acp_hash_table* child_acp = acp;
	acp = orig_acp;
	acp->killed_all = acp->killed_all || child_acp->killed_all;

	for (ir_variable* v : child_acp->kills)
	{
		kill(v);
	}

	delete child_acp;
}

ir_visitor_status ir_copy_propagation_visitor::visit_enter(ir_if *ir)
{
	ir->condition->accept(this);

	handle_if_block(&ir->then_instructions);
	handle_if_block(&ir->else_instructions);

	/* handle_if_block() already descended into the children. */
	return visit_continue_with_parent;
}

ir_visitor_status ir_copy_propagation_visitor::visit_enter(ir_loop *ir)
{
	acp_hash_table *orig_acp = this->acp;

	/* FINISHME: For now, the initial acp for loops is totally empty.
	* We could go through once, then go through again with the acp
	* cloned minus the killed entries after the first run through.
	*/
	/* Populate the initial acp with samplers & samplerstates so they propagate */
	this->acp = acp_hash_table::copy_for_loop_start(*acp);

	visit_list_elements(this, &ir->body_instructions);

	if (acp->killed_all)
	{
		orig_acp->make_empty();
	}

	acp_hash_table* child_acp = acp;
	acp = orig_acp;
	acp->killed_all = acp->killed_all || child_acp->killed_all;

	for (ir_variable* v : child_acp->kills)
	{
		kill(v);
	}

	delete child_acp;

	/* Now retraverse with a safe acp list*/
	if (!acp->killed_all)
	{
		this->acp = acp_hash_table::clone_for_instructions(*acp, &ir->body_instructions);

		visit_list_elements(this, &ir->body_instructions);

		delete acp;
		this->acp = orig_acp;
	}

	/* already descended into the children. */
	return visit_continue_with_parent;
}

void ir_copy_propagation_visitor::kill(ir_variable *var)
{
	check(var != NULL);

	// Shared variables should be considered volatile
	if (var->mode == ir_var_shared || var->precise)
	{
		return;
	}

	/* Remove any entries currently in the ACP for this kill. */
	acp->erase_acp(var);

	/* Add the LHS variable to the list of killed variables in this block.
	*/
	if (Debug)
	{
		printf("Kill_Entry: Var %d\n", var->id);
	}

	acp->kills.insert(var);
}

/**
* Adds an entry to the available copy list if it's a plain assignment
* of a variable to a variable.
*/
void ir_copy_propagation_visitor::add_copy(ir_assignment *ir)
{
	if (ir->condition)
	{
		return;
	}

	ir_variable *lhs_var = ir->whole_variable_written();
	ir_variable *rhs_var = ir->rhs->whole_variable_referenced();
	ir_dereference_array *array_deref = ir->rhs->as_dereference_array();

	// Shared variables should be considered volatile
	if ((lhs_var != NULL && lhs_var->mode == ir_var_shared) ||
		(rhs_var != NULL && rhs_var->mode == ir_var_shared) ||
		(lhs_var != NULL && lhs_var->precise) ||
		(rhs_var != NULL && rhs_var->precise))
	{
		return;
	}

	if ((lhs_var != NULL) && (rhs_var != NULL))
	{
		if (lhs_var == rhs_var)
		{
			/* This is a dumb assignment, but we've conveniently noticed
			* it here.  Removing it now would mess up the loop iteration
			* calling us.  Just flag it to not execute, and someone else
			* will clean up the mess.
			*/
			ir->condition = new(ralloc_parent(ir)) ir_constant(false);
			this->progress = true;
		}
		else
		{
			if (Debug)
			{
				printf("ACP_Entry Assign %d Block: LHS %d RHS_Var %d\n", ir->id, lhs_var->id, rhs_var->id);
			}
			acp->add_acp(acp_entry(lhs_var, rhs_var, 0));
		}
	}
	else if (lhs_var && array_deref)
	{
		ir_dereference_variable *array_var_deref = array_deref->array->as_dereference_variable();
		if (array_var_deref)
		{
			ir_constant *const_array_index = array_deref->array_index->as_constant();
			if (const_array_index)
			{
				const_array_index = const_array_index->clone(mem_ctx, 0);
			}
			else
			{
				ir_constant *const_array_index_expr_value = array_deref->array_index->constant_expression_value();
				if (const_array_index_expr_value)
				{
					const_array_index = const_array_index_expr_value->clone(mem_ctx, 0);
					ralloc_free(const_array_index_expr_value);
				}
			}
			if (const_array_index)
			{
				ir_dereference_array *new_array_deref = new(this->mem_ctx) ir_dereference_array(array_var_deref->var, const_array_index);
				if (Debug)
				{
					printf("ACP_Entry Assign Block: LHS %d ArrayDeref %d [%d] \n", lhs_var->id, array_var_deref->id, const_array_index->id);
				}
				acp->add_acp(acp_entry(lhs_var, 0, new_array_deref));
			}
		}
	}
}

/**
* Does a copy propagation pass on the code present in the instruction stream.
*/
bool do_copy_propagation(exec_list *instructions, bool conservative_propagation)
{
	ir_copy_propagation_visitor v;
	v.conservative_propagation = conservative_propagation;

	visit_list_elements(&v, instructions);

	return v.progress;
}
