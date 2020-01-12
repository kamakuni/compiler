#include "kmcc.h"

// All local variable instances created during parsing are
// accumulated to this list.
static VarList *locals;

static Type *basetype(void) {
  expect("int");
  Type *ty = int_type;
  while (consume("*"))
    ty = pointer_to(ty);
  return ty;
}

// Find a local variable by name
static Var *find_var(Token *tok) {
  for (VarList *vl = locals; vl; vl = vl->next) {
    Var *var = vl->var;
    if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len))
      return var;
  }
  return NULL;
}

Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1,sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind,tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

/*Node *new_node_block(Vector *stmts) {
    Node *node = new_node(ND_BLOCK);
    node->stmts = stmts;
    return node;
    }*/

/*Node *new_node_call(char *name, Node *args) {
    Node *node = new_node(ND_CALL);
    node->name = name;
    node->args = args;
    return node;
    }*/
/*
Node *new_node_function(char *name, Vector *args, Node *block) {
    Node *node = new_node(ND_FUNC);
    node->name = name;
    node->args = args;
    node->block = block;
    return node;
}
*/
Node *new_num(long val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}
/*
Node *new_node_ident(Type *ty, char *name) {
    Node *node = new_node(ND_IDENT);
    node->ty = ty;
    node->name = strndup(name, strlen(name));
    var_append(variables, ty, name);
    return node;
}
*/
/*
Node *new_node_for(Node *init, Node *cond, Node * inc, Node *body) {
    Node *node = new_node(ND_FOR);
    node->init = init;
    node->cond = cond;
    node->inc = inc;
    node->body = body;
    return node;
}
*/
/*
Node *new_node_if(Node *cond, Node *body, Node *els) {
    Node *node = new_node(ND_IF);
    node->cond = cond;
    node->body = body;
    node->els = els;
    return node;
}
*/
/*
Node *new_node_while(Node *cond, Node *body) {
    Node *node = new_node(ND_WHILE);
    node->cond = cond;
    node->body = body;
    return node;
}
*/
static Node *new_var_node(Var *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Var *new_lvar(char *name, Type *ty){
  Var *var = calloc(1,sizeof(Var));
  var->name = name;
  var->ty = ty;
  
  VarList *vl = calloc(1,sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok){
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

Node *code[100];

static Function *function();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *add();
static Node *equality();
static Node *relational();
static Node *primary();
static Node *mul();
static Node *unary();      

// program = function*
Function *program() {
  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    cur->next = function();
    cur = cur->next;
  }
  return head.next;
}

static VarList *read_func_param() {
  VarList *vl = calloc(1 , sizeof(VarList));
  Type *ty = basetype();
  vl->var = new_lvar(expect_ident(),ty);
  return vl;
}

static VarList *read_func_params() {
  if (consume(")"))
    return NULL;

  VarList *head = read_func_param();
  VarList *cur = head;

  while (!consume(")")) {
    expect(",");
    cur->next = read_func_param();
    cur = cur->next;
  }

  return head;
}

// function = ident "(" params? ")" "{" stmt* "}"
// params = ident ("," ident)*
static Function *function() {
  locals = NULL;

  Function *fn = calloc(1,sizeof(Function));
  basetype();
  fn->name = expect_ident();
  expect("(");
  fn->params = read_func_params();
  expect("{");
  
  Node head = {};
  Node *cur = &head;
  while(!consume("}")){
    cur->next = stmt();
    cur = cur->next;
  }

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// declaration = basetype ident ("=" expr) ";"
static Node *declaration(){
  Token *tok = token;
  Type *ty = basetype();
  Var *var = new_lvar(expect_ident(), ty);

  if (consume(";"))
    return new_node(ND_NULL, tok);

  expect("=");
  Node *lhs = new_var_node(var, tok);
  Node *rhs = expr();
  expect(";");
  Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
  return new_unary(ND_EXPR_STMT, node, tok);
}

static Node *read_expr_stmt(){
  Token *tok = token;
  return new_unary(ND_EXPR_STMT, expr(), tok);
}

static Node *assign() {
    Node *node = equality();
    Token *tok;
    if (tok = consume("="))
      node = new_binary(ND_ASSIGN, node, assign(), tok);
    return node;
}

// expr = assign
static Node *expr() {
    return assign();
}

static Node *stmt() {
  Node *node = stmt2();
  add_type(node);
  return node;
}

// stmt2 = "return" expr ";"
//       | "if" "(" expr ")" stmt ("else" stmt)?
//       | "while" "(" expr ")" stmt
//       | "for" "(" expr? ";" expr? ";" expr? ")" stmt  
//       | "{" stmt* "}"
//       | expr ";"
static Node *stmt2() {
  Token *tok;
  if (tok = consume("return")) {
    Node *node = new_unary(ND_RETURN, expr(), tok);
    expect(";");
    return node;
  }

  if (tok = consume("if")) {
    Node *node = new_node(ND_IF,tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if(consume("else"))
      node->els = stmt();
    return node;
  }

  if(tok = consume("while")) {
    Node *node = new_node(ND_WHILE, tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  }

  if (tok = consume("for")) {
    Node *node = new_node(ND_FOR, tok);
    expect("(");
    if (!consume(";")) {
      node->init = read_expr_stmt();
      expect(";");
    }
    if (!consume(")")) {
      node->inc = read_expr_stmt();
      expect(")");
    }
    node->then = stmt();
    return node;
  }

  if (tok = consume("{")) {
    Node head = {};
    Node *cur = &head;

    while(!consume("}")){
      cur->next = stmt();
      cur = cur->next;
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    return node;
  }

  if (tok = peek("int"))
    return declaration();
  
  Node *node = read_expr_stmt();
  expect(";");
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
    Node *node = relational();
    Token *tok;
    
    for (;;) {
        if (tok = consume("=="))
	  node = new_binary(ND_EQ, node, relational(), tok);
        else if (tok = consume("!="))
	  node = new_binary(ND_NE, node, relational(), tok);
        else
          return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
    // lhs
    Node *node = add();
    Token *tok;
    
    for (;;) {
        if (tok = consume("<"))
	  node = new_binary(ND_LT, node, add(), tok);
        else if (tok = consume("<="))
	  node = new_binary(ND_LE, node, add(), tok);
        else if (tok = consume(">"))
	  node = new_binary(ND_LT, add(), node, tok);
        else if (tok = consume(">="))
	  node = new_binary(ND_LE, add(), node, tok);
        else
            return node;
    }
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs, tok);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_ADD, lhs, rhs, tok);
  if (is_integer(lhs->ty) && rhs->ty->base)
    return new_binary(ND_PTR_ADD, rhs, lhs, tok);
  error_tok(tok, "invalid operands");
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs, tok);
  if (lhs->ty->base && is_integer(rhs->ty))
    return new_binary(ND_PTR_SUB, lhs, rhs, tok);
  if (lhs->ty->base && rhs->ty->base)
    return new_binary(ND_PTR_DIFF, lhs, rhs,tok);
  error_tok(tok,"invalid operands");
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
    // lhs
    Node *node = mul();
    Token *tok;
    for (;;) {
        if (tok = consume("+"))
	  node = new_binary(ND_ADD, node, mul(), tok);
        else if (tok = consume("-"))
	  node = new_binary(ND_SUB, node, mul(), tok);
        else
            return node;
    }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul() {
    Node *node = unary();
    Token *tok;
    for (;;) {
        if (tok = consume("*"))
	  node = new_binary(ND_MUL, node, unary(), tok);
        else if (tok = consume("/"))
	  node = new_binary(ND_DIV, node, unary(), tok);
        else
            return node;
        
    }
}

// unary = ("+" | "-" | "*" | "&")? unary
//       | primary
static Node *unary() {
  Token *tok;
  if (consume("+")) {
    return unary();
  }
  if (tok = consume("-")) {
    // -x => 0-x
    return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
  }
  if (tok = consume("*")) {
    return new_unary(ND_DEREF, unary(), tok);
  }
  if (tok = consume("&")) {
    return new_unary(ND_ADDR, unary(), tok);
  }
  return primary();
}

// func-args = "(" (assign ("," assign)*)? ")"
static Node *func_args() {
  if (consume(")"))
    return NULL;

  Node *head = assign();
  Node *cur = head;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

// primary = "(" expr ")" | ident func-args? | num
static Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  Token *tok;
  if (tok = consume_ident()) {
    // Function call
    if (consume("(")) {
      Node *node = new_node(ND_FUNCALL, tok);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args();
      return node;
    }

    // Variable
    Var *var = find_var(tok);
    if(!var)
      error_tok(tok, "undefined variable");
    return new_var_node(var, tok);
  }

  tok = token;
  if (tok->kind != TK_NUM)
    error_tok(tok, "expected expression");
  return new_num(expect_number(), tok);
}
