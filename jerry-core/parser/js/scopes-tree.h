/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SCOPES_TREE_H
#define SCOPES_TREE_H

#include "linked-list.h"
#include "lexer.h"
#include "ecma-globals.h"
#include "hash-table.h"
#include "opcodes.h"
#include "lit-id-hash-table.h"
#include "lit-literal.h"

#define NOT_A_LITERAL (lit_cpointer_t::null_cp ())

typedef struct
{
  lit_cpointer_t lit_id[3];
  vm_instr_t op;
} op_meta;

typedef struct tree_header
{
  struct tree_header *parent;
  linked_list children;
  uint8_t children_num;
} tree_header;

/**
 * Structure for holding scope information during parsing
 */
typedef struct
{
  tree_header t; /**< header */
  linked_list instrs; /**< instructions */
  vm_instr_counter_t instrs_count; /**< count of instructions */
  linked_list var_decls; /**< instructions for variable declarations */
  uint8_t var_decls_cout; /**< number of instructions for variable declarations */
  bool strict_mode: 1; /**< flag, indicating that scope's code should be executed in strict mode */
  bool ref_arguments: 1; /**< flag, indicating that "arguments" variable is used inside the scope */
  bool ref_eval: 1; /**< flag, indicating that "eval" is used inside the scope */
} scopes_tree_int;

typedef scopes_tree_int * scopes_tree;

scopes_tree scopes_tree_init (scopes_tree);
void scopes_tree_free (scopes_tree);
vm_instr_counter_t scopes_tree_instrs_num (scopes_tree);
vm_instr_counter_t scopes_tree_var_decls_num (scopes_tree);
void scopes_tree_add_op_meta (scopes_tree, op_meta);
void scopes_tree_add_var_decl (scopes_tree, op_meta);
void scopes_tree_set_op_meta (scopes_tree, vm_instr_counter_t, op_meta);
void scopes_tree_set_instrs_num (scopes_tree, vm_instr_counter_t);
op_meta scopes_tree_op_meta (scopes_tree, vm_instr_counter_t);
op_meta scopes_tree_var_decl (scopes_tree, vm_instr_counter_t);
size_t scopes_tree_count_literals_in_blocks (scopes_tree);
vm_instr_counter_t scopes_tree_count_instructions (scopes_tree);
vm_instr_t *scopes_tree_raw_data (scopes_tree, uint8_t *, size_t, lit_id_hash_table *);
void scopes_tree_set_strict_mode (scopes_tree, bool);
void scopes_tree_set_arguments_used (scopes_tree);
void scopes_tree_set_eval_used (scopes_tree);
bool scopes_tree_strict_mode (scopes_tree);

#endif /* SCOPES_TREE_H */
