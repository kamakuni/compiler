#include "kmcc.h"

static int label_count = 0;
static char *funcname;
char* argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void gen_lval(Node *node) {
    switch (node->kind) {
      /*case ND_IDENT:{
        int offset = var_get_offset(variables, node->name);
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", offset);
        printf("  push rax\n");
        return;
	}*/
    case ND_VAR: {
      printf("  lea rax, [rbp-%d]\n", node->var->offset);
      printf("  push rax\n");
      return;
    }
    case ND_DEREF:
        gen(node->lhs);
        return;
    }

    error("not a left value");
}

void gen_block(Node *node){
    for (int i = 0; i < node->stmts->len; i++) {
        gen((Node *)vec_get(node->stmts, i));
        printf("  pop rax\n");
    }
}

void gen_func(Function *prog, Node *node){
    if(node->kind != ND_FUNC)
        error("関数でありません。");
    printf("%s:\n",node->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");

    printf("  sub rsp, %d\n", prog->stack_size);
    //for (int i = 0; i < node->args->len; i++) {

    for (Node *arg = node->args; arg; arg = arg->next) {
    int offset = var_get_offset(variables,vec_get(node->args,i));
      if (offset != 0) {
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", offset);
            printf("  mov [rax], %s\n", argregs[i]);
        }
    }
    if(node->block->kind != ND_BLOCK)
        error("ブロックでありません。");
    gen_block(node->block);

    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen(Node *node) {

    if(node->kind == ND_CALL) {
      /*int len = node->args->len;
        for (int i = 0; i < len; i++) {
            gen((Node *) vec_get(node->args,i));
            printf("  pop rax\n");
            printf("  mov %s, rax\n", argregs[i]);
	    }*/
      int nargs = 0;
      for (Node *arg = node->args; arg; arg = arg->next) {
	gen(arg);
	nargs++;
      }

      for (int i = nargs - 1; i >= 0; i--)
        printf("  pop %s\n", argregs[i]);
      // We need to align RSP to a 16 byte boundary before
      // calling a function because it is an ABI requirement.
      // RAX is set to 0 for variadic function
      int seq = label_count++;
      printf("  mov rax, rsp\n");
      printf("  and rax, 15\n");
      printf("  jnz .L.call.%d\n", seq);
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->name);
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.call.%d:\n", seq);
      printf("  sub rsp, 8\n");
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->name);
      printf("  add rsp, 8\n");
      printf(".L.end.%d:\n", seq);
      printf("  push rax\n");
      return;
    }

    if (node->kind == ND_BLOCK) {
        gen_block(node->block);
        return;
    }

    if (node->kind == ND_NUM) {
        printf("  push %d\n", node->val);
        return;
    }
    
    if (node->kind == ND_VAR) {
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }
    
    if (node->kind == ND_IF) {
        int label_count_if = label_count;
        label_count += 1;
        gen(node->cond);
        //printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        if(node->elseBody == NULL){
            printf("  je  .Lend%d\n", label_count_if);
            if(node->body->kind == ND_BLOCK){
                gen_block(node->body);
            } else {
                gen(node->body);
            }
            printf(".Lend%d:\n", label_count_if);
        } else {
            printf("  je  .Lelse%d\n", label_count_if);
            if(node->body->kind == ND_BLOCK){
                gen_block(node->body);
            } else {
                gen(node->body);
            }
            printf("  jmp  .Lend%d\n", label_count_if);
            printf(".Lelse%d:\n", label_count_if);
            if(node->elseBody->kind == ND_BLOCK){
                gen_block(node->elseBody);
            } else {
                gen(node->elseBody);
            }
            printf(".Lend%d:\n", label_count_if);
        }
        return;
    }

    if (node->kind == ND_WHILE) {
        int label_count_while = label_count;
        label_count += 1;
        printf(".Lbegin%d:\n", label_count_while);
        gen(node->cond);
        //printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%d\n", label_count_while);
        if(node->body->kind == ND_BLOCK){
            gen_block(node->body);
        } else {
            gen(node->body);
        }
        printf("  jmp  .Lbegin%d\n", label_count_while);
        printf(".Lend%d:\n", label_count_while);
        return;
    }

    if (node->kind == ND_FOR) {
        int label_count_for = label_count;
        label_count += 1;
        if(node->init) {
            gen(node->init);
        }
        printf(".Lbegin%d:\n", label_count_for);
        if(node->cond) {
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lend%d\n", label_count_for);
        }
        gen(node->body);
        if(node->incdec)
          gen(node->incdec);
        printf("  jmp  .Lbegin%d\n", label_count_for);
        printf(".Lend%d:\n", label_count_for);
        return;
    }

    if (node->kind == ND_RETURN) {
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    }
    
    if (node->kind == ND_ASSIGN) {
        gen_lval(node->lhs);    
        gen(node->rhs);
        
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    }

    if (node->kind == ND_ADDR) {
        gen_lval(node->lhs);
        return;
    }

    if (node->kind == ND_DEREF) {
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

    gen(node->lhs);
    gen(node->rhs);
    
    printf("  pop rdi\n");
    printf("  pop rax\n");
    
    switch (node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL:
            printf("  mul rdi\n");
            break;
        case ND_DIV:
            printf("  mov rdx, 0\n");
            printf("  div rdi\n");
            break;
        case ND_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
    }
    
    printf("  push rax\n");
}

void codegen(Function *prog){
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  
  for (Node *node = prog->node; node; node = node->next)
    gen_func(prog,node);
}
