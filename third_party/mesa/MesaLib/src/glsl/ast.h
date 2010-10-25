/* -*- c++ -*- */
/*
 * Copyright © 2009 Intel Corporation
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

#pragma once
#ifndef AST_H
#define AST_H

#include "list.h"
#include "glsl_parser_extras.h"

struct _mesa_glsl_parse_state;

struct YYLTYPE;

/**
 * \defgroup AST Abstract syntax tree node definitions
 *
 * An abstract syntax tree is generated by the parser.  This is a fairly
 * direct representation of the gramma derivation for the source program.
 * No symantic checking is done during the generation of the AST.  Only
 * syntactic checking is done.  Symantic checking is performed by a later
 * stage that converts the AST to a more generic intermediate representation.
 *
 *@{
 */
/**
 * Base class of all abstract syntax tree nodes
 */
class ast_node {
public:
   /* Callers of this talloc-based new need not call delete. It's
    * easier to just talloc_free 'ctx' (or any of its ancestors). */
   static void* operator new(size_t size, void *ctx)
   {
      void *node;

      node = talloc_zero_size(ctx, size);
      assert(node != NULL);

      return node;
   }

   /* If the user *does* call delete, that's OK, we will just
    * talloc_free in that case. */
   static void operator delete(void *table)
   {
      talloc_free(table);
   }

   /**
    * Print an AST node in something approximating the original GLSL code
    */
   virtual void print(void) const;

   /**
    * Convert the AST node to the high-level intermediate representation
    */
   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   /**
    * Retrieve the source location of an AST node
    *
    * This function is primarily used to get the source position of an AST node
    * into a form that can be passed to \c _mesa_glsl_error.
    *
    * \sa _mesa_glsl_error, ast_node::set_location
    */
   struct YYLTYPE get_location(void) const
   {
      struct YYLTYPE locp;

      locp.source = this->location.source;
      locp.first_line = this->location.line;
      locp.first_column = this->location.column;
      locp.last_line = locp.first_line;
      locp.last_column = locp.first_column;

      return locp;
   }

   /**
    * Set the source location of an AST node from a parser location
    *
    * \sa ast_node::get_location
    */
   void set_location(const struct YYLTYPE &locp)
   {
      this->location.source = locp.source;
      this->location.line = locp.first_line;
      this->location.column = locp.first_column;
   }

   /**
    * Source location of the AST node.
    */
   struct {
      unsigned source;    /**< GLSL source number. */
      unsigned line;      /**< Line number within the source string. */
      unsigned column;    /**< Column in the line. */
   } location;

   exec_node link;

protected:
   /**
    * The only constructor is protected so that only derived class objects can
    * be created.
    */
   ast_node(void);
};


/**
 * Operators for AST expression nodes.
 */
enum ast_operators {
   ast_assign,
   ast_plus,        /**< Unary + operator. */
   ast_neg,
   ast_add,
   ast_sub,
   ast_mul,
   ast_div,
   ast_mod,
   ast_lshift,
   ast_rshift,
   ast_less,
   ast_greater,
   ast_lequal,
   ast_gequal,
   ast_equal,
   ast_nequal,
   ast_bit_and,
   ast_bit_xor,
   ast_bit_or,
   ast_bit_not,
   ast_logic_and,
   ast_logic_xor,
   ast_logic_or,
   ast_logic_not,

   ast_mul_assign,
   ast_div_assign,
   ast_mod_assign,
   ast_add_assign,
   ast_sub_assign,
   ast_ls_assign,
   ast_rs_assign,
   ast_and_assign,
   ast_xor_assign,
   ast_or_assign,

   ast_conditional,

   ast_pre_inc,
   ast_pre_dec,
   ast_post_inc,
   ast_post_dec,
   ast_field_selection,
   ast_array_index,

   ast_function_call,

   ast_identifier,
   ast_int_constant,
   ast_uint_constant,
   ast_float_constant,
   ast_bool_constant,

   ast_sequence
};

/**
 * Representation of any sort of expression.
 */
class ast_expression : public ast_node {
public:
   ast_expression(int oper, ast_expression *,
		  ast_expression *, ast_expression *);

   ast_expression(const char *identifier) :
      oper(ast_identifier)
   {
      subexpressions[0] = NULL;
      subexpressions[1] = NULL;
      subexpressions[2] = NULL;
      primary_expression.identifier = (char *) identifier;
   }

   static const char *operator_string(enum ast_operators op);

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   virtual void print(void) const;

   enum ast_operators oper;

   ast_expression *subexpressions[3];

   union {
      char *identifier;
      int int_constant;
      float float_constant;
      unsigned uint_constant;
      int bool_constant;
   } primary_expression;


   /**
    * List of expressions for an \c ast_sequence or parameters for an
    * \c ast_function_call
    */
   exec_list expressions;
};

class ast_expression_bin : public ast_expression {
public:
   ast_expression_bin(int oper, ast_expression *, ast_expression *);

   virtual void print(void) const;
};

/**
 * Subclass of expressions for function calls
 */
class ast_function_expression : public ast_expression {
public:
   ast_function_expression(ast_expression *callee)
      : ast_expression(ast_function_call, callee,
		       NULL, NULL),
	cons(false)
   {
      /* empty */
   }

   ast_function_expression(class ast_type_specifier *type)
      : ast_expression(ast_function_call, (ast_expression *) type,
		       NULL, NULL),
	cons(true)
   {
      /* empty */
   }

   bool is_constructor() const
   {
      return cons;
   }

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

private:
   /**
    * Is this function call actually a constructor?
    */
   bool cons;
};


/**
 * Number of possible operators for an ast_expression
 *
 * This is done as a define instead of as an additional value in the enum so
 * that the compiler won't generate spurious messages like "warning:
 * enumeration value ‘ast_num_operators’ not handled in switch"
 */
#define AST_NUM_OPERATORS (ast_sequence + 1)


class ast_compound_statement : public ast_node {
public:
   ast_compound_statement(int new_scope, ast_node *statements);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   int new_scope;
   exec_list statements;
};

class ast_declaration : public ast_node {
public:
   ast_declaration(char *identifier, int is_array, ast_expression *array_size,
		   ast_expression *initializer);
   virtual void print(void) const;

   char *identifier;
   
   int is_array;
   ast_expression *array_size;

   ast_expression *initializer;
};


enum {
   ast_precision_high = 0, /**< Default precision. */
   ast_precision_medium,
   ast_precision_low
};

struct ast_type_qualifier {
   unsigned invariant:1;
   unsigned constant:1;
   unsigned attribute:1;
   unsigned varying:1;
   unsigned in:1;
   unsigned out:1;
   unsigned centroid:1;
   unsigned uniform:1;
   unsigned smooth:1;
   unsigned flat:1;
   unsigned noperspective:1;

   /** \name Layout qualifiers for GL_ARB_fragment_coord_conventions */
   /*@{*/
   unsigned origin_upper_left:1;
   unsigned pixel_center_integer:1;
   /*@}*/
};

class ast_struct_specifier : public ast_node {
public:
   ast_struct_specifier(char *identifier, ast_node *declarator_list);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   char *name;
   exec_list declarations;
};


enum ast_types {
   ast_void,
   ast_float,
   ast_int,
   ast_uint,
   ast_bool,
   ast_vec2,
   ast_vec3,
   ast_vec4,
   ast_bvec2,
   ast_bvec3,
   ast_bvec4,
   ast_ivec2,
   ast_ivec3,
   ast_ivec4,
   ast_uvec2,
   ast_uvec3,
   ast_uvec4,
   ast_mat2,
   ast_mat2x3,
   ast_mat2x4,
   ast_mat3x2,
   ast_mat3,
   ast_mat3x4,
   ast_mat4x2,
   ast_mat4x3,
   ast_mat4,
   ast_sampler1d,
   ast_sampler2d,
   ast_sampler2drect,
   ast_sampler3d,
   ast_samplercube,
   ast_sampler1dshadow,
   ast_sampler2dshadow,
   ast_sampler2drectshadow,
   ast_samplercubeshadow,
   ast_sampler1darray,
   ast_sampler2darray,
   ast_sampler1darrayshadow,
   ast_sampler2darrayshadow,
   ast_isampler1d,
   ast_isampler2d,
   ast_isampler3d,
   ast_isamplercube,
   ast_isampler1darray,
   ast_isampler2darray,
   ast_usampler1d,
   ast_usampler2d,
   ast_usampler3d,
   ast_usamplercube,
   ast_usampler1darray,
   ast_usampler2darray,

   ast_struct,
   ast_type_name
};


class ast_type_specifier : public ast_node {
public:
   ast_type_specifier(int specifier);

   /** Construct a type specifier from a type name */
   ast_type_specifier(const char *name) 
      : type_specifier(ast_type_name), type_name(name), structure(NULL),
	is_array(false), array_size(NULL), precision(ast_precision_high)
   {
      /* empty */
   }

   /** Construct a type specifier from a structure definition */
   ast_type_specifier(ast_struct_specifier *s)
      : type_specifier(ast_struct), type_name(s->name), structure(s),
	is_array(false), array_size(NULL), precision(ast_precision_high)
   {
      /* empty */
   }

   const struct glsl_type *glsl_type(const char **name,
				     struct _mesa_glsl_parse_state *state)
      const;

   virtual void print(void) const;

   ir_rvalue *hir(exec_list *, struct _mesa_glsl_parse_state *);

   enum ast_types type_specifier;

   const char *type_name;
   ast_struct_specifier *structure;

   int is_array;
   ast_expression *array_size;

   unsigned precision:2;
};


class ast_fully_specified_type : public ast_node {
public:
   virtual void print(void) const;
   bool has_qualifiers() const;

   ast_type_qualifier qualifier;
   ast_type_specifier *specifier;
};


class ast_declarator_list : public ast_node {
public:
   ast_declarator_list(ast_fully_specified_type *);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_fully_specified_type *type;
   exec_list declarations;

   /**
    * Special flag for vertex shader "invariant" declarations.
    *
    * Vertex shaders can contain "invariant" variable redeclarations that do
    * not include a type.  For example, "invariant gl_Position;".  This flag
    * is used to note these cases when no type is specified.
    */
   int invariant;
};


class ast_parameter_declarator : public ast_node {
public:
   ast_parameter_declarator()
   {
      this->identifier = NULL;
      this->is_array = false;
      this->array_size = 0;
   }

   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_fully_specified_type *type;
   char *identifier;
   int is_array;
   ast_expression *array_size;

   static void parameters_to_hir(exec_list *ast_parameters,
				 bool formal, exec_list *ir_parameters,
				 struct _mesa_glsl_parse_state *state);

private:
   /** Is this parameter declaration part of a formal parameter list? */
   bool formal_parameter;

   /**
    * Is this parameter 'void' type?
    *
    * This field is set by \c ::hir.
    */
   bool is_void;
};


class ast_function : public ast_node {
public:
   ast_function(void);

   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_fully_specified_type *return_type;
   char *identifier;

   exec_list parameters;

private:
   /**
    * Is this prototype part of the function definition?
    *
    * Used by ast_function_definition::hir to process the parameters, etc.
    * of the function.
    *
    * \sa ::hir
    */
   bool is_definition;

   /**
    * Function signature corresponding to this function prototype instance
    *
    * Used by ast_function_definition::hir to process the parameters, etc.
    * of the function.
    *
    * \sa ::hir
    */
   class ir_function_signature *signature;

   friend class ast_function_definition;
};


class ast_declaration_statement : public ast_node {
public:
   ast_declaration_statement(void);

   enum {
      ast_function,
      ast_declaration,
      ast_precision
   } mode;

   union {
      class ast_function *function;
      ast_declarator_list *declarator;
      ast_type_specifier *type;
      ast_node *node;
   } declaration;
};


class ast_expression_statement : public ast_node {
public:
   ast_expression_statement(ast_expression *);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_expression *expression;
};


class ast_case_label : public ast_node {
public:

   /**
    * An expression of NULL means 'default'.
    */
   ast_expression *expression;
};

class ast_selection_statement : public ast_node {
public:
   ast_selection_statement(ast_expression *condition,
			   ast_node *then_statement,
			   ast_node *else_statement);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_expression *condition;
   ast_node *then_statement;
   ast_node *else_statement;
};


class ast_switch_statement : public ast_node {
public:
   ast_expression *expression;
   exec_list statements;
};

class ast_iteration_statement : public ast_node {
public:
   ast_iteration_statement(int mode, ast_node *init, ast_node *condition,
			   ast_expression *rest_expression, ast_node *body);

   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *, struct _mesa_glsl_parse_state *);

   enum ast_iteration_modes {
      ast_for,
      ast_while,
      ast_do_while
   } mode;
   

   ast_node *init_statement;
   ast_node *condition;
   ast_expression *rest_expression;

   ast_node *body;

private:
   /**
    * Generate IR from the condition of a loop
    *
    * This is factored out of ::hir because some loops have the condition
    * test at the top (for and while), and others have it at the end (do-while).
    */
   void condition_to_hir(class ir_loop *, struct _mesa_glsl_parse_state *);
};


class ast_jump_statement : public ast_node {
public:
   ast_jump_statement(int mode, ast_expression *return_value);
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   enum ast_jump_modes {
      ast_continue,
      ast_break,
      ast_return,
      ast_discard
   } mode;

   ast_expression *opt_return_value;
};


class ast_function_definition : public ast_node {
public:
   virtual void print(void) const;

   virtual ir_rvalue *hir(exec_list *instructions,
			  struct _mesa_glsl_parse_state *state);

   ast_function *prototype;
   ast_compound_statement *body;
};
/*@}*/

extern void
_mesa_ast_to_hir(exec_list *instructions, struct _mesa_glsl_parse_state *state);

extern ir_rvalue *
_mesa_ast_field_selection_to_hir(const ast_expression *expr,
				 exec_list *instructions,
				 struct _mesa_glsl_parse_state *state);

#endif /* AST_H */
