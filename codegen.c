#include "zxcc.h"

// labelの通し番号
int label_seq_num;
static int brkseq;
static int contseq;
static char *func_name;

static char *regs_for_args_8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *regs_for_args_4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *regs_for_args_2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *regs_for_args_1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};

static void gen(Node *node);

// nodeを左辺値として評価し、そのアドレスをスタックにpushするコードを生成する。
// nodeが評価不可能な場合エラー終了させる。
static void gen_lval(Node *node) {
    switch(node->kind) {
        case ND_VAR:
            if(node->init) {
                gen(node->init);
            }

            if(node->var->is_local) {
                printf("  mov rax, rbp\n");
                printf("  sub rax, %d\n", node->var->offset);
                printf("  push rax\n");
            } else {
                // グローバル変数 or 文字列リテラル
                printf("  push offset %s\n", node->var->name);
            }
            return;
        case ND_DEREF:
            gen(node->lhs);
            return;
        case ND_MEMBER:
            gen_lval(node->lhs);
            printf("  pop rax\n");
            printf("  add rax, %d\n", node->member->offset);
            printf("  push rax\n");
            return;
        default:
            error("引数が左辺値として評価不可能なノードです");
    }
}

static void load(Type *type) {
    printf("  pop rax\n");

    if(type->size == 1) {
        // raxが指しているアドレスから1byteロードする(符号拡張あり)
        printf("  movsx rax, byte ptr [rax]\n");
    } else if(type->size == 2) {
        // raxが指しているアドレスから2byteロードする(符号拡張あり)
        printf("  movsx rax, word ptr [rax]\n");
    } else if(type->size == 4) {
        // raxが指しているアドレスから4byteロードする(符号拡張あり)
        printf("  movsxd rax, dword ptr [rax]\n");
    } else {
        assert(type->size == 8);
        printf("  mov rax, [rax]\n");
    }

    printf("  push rax\n");
}

static void store(Type *type) {
    printf("  pop rdi\n");
    printf("  pop rax\n");

    if(type->ty == BOOL) {
        printf("  cmp rdi, 0\n");
        printf("  setne dil\n");
        printf("  movzb rdi, dil\n");
    }

    if(type->size == 1) {
        // dilから1byteストアする
        printf("  mov [rax], dil\n");
    } else if(type->size == 2) {
        // diから2byteストアする
        printf("  mov [rax], di\n");
    } else if(type->size == 4) {
        // ediから4byteストアする
        printf("  mov [rax], edi\n");
    } else {
        assert(type->size == 8);
        printf("  mov [rax], rdi\n");
    }

    printf("  push rdi\n");
}

static void truncate(Type *ty) {
    printf("  pop rax\n");

    if(ty->ty == BOOL) {
        printf("  cmp rax, 0\n");
        printf("  setne al\n");
    }

    if(ty->size == 1) {
        printf("  movsx rax, al\n");
    } else if(ty->size == 2) {
        printf("  movsx rax, ax\n");
    } else if(ty->size == 4) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

static void inc(Type *ty) {
    printf("  pop rax\n");
    printf("  add rax, %d\n", ty->ptr_to ? ty->ptr_to->size : 1);
    printf("  push rax\n");
}

static void dec(Type *ty) {
    printf("  pop rax\n");
    printf("  sub rax, %d\n", ty->ptr_to ? ty->ptr_to->size : 1);
    printf("  push rax\n");
}

static void gen_binary(Node *node) {
    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch(node->kind) {
        case ND_ADD:
        case ND_ADD_EQ:
            printf("  add rax, rdi\n");
            break;
        case ND_PTR_ADD:
        case ND_PTR_ADD_EQ:
            printf("  imul rdi, %d\n", node->type->ptr_to->size);
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
        case ND_SUB_EQ:
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_SUB:
        case ND_PTR_SUB_EQ:
            printf("  imul rdi, %d\n", node->type->ptr_to->size);
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_DIFF:
            printf("  sub rax, rdi\n");
            printf("  cqo\n");
            printf("  mov rdi, %d\n", node->lhs->type->ptr_to->size);
            printf("  idiv rdi\n");
            break;
        case ND_MUL:
        case ND_MUL_EQ:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
        case ND_DIV_EQ:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_BITAND:
        case ND_BITAND_EQ:
            printf("  and rax, rdi\n");
            break;
        case ND_BITOR:
        case ND_BITOR_EQ:
            printf("  or rax, rdi\n");
            break;
        case ND_BITXOR:
        case ND_BITXOR_EQ:
            printf("  xor rax, rdi\n");
            break;
        case ND_SHL:
        case ND_SHL_EQ:
            printf("  mov cl, dil\n");
            printf("  shl rax, cl\n");
            break;
        case ND_SHR:
        case ND_SHR_EQ:
            printf("  mov cl, dil\n");
            printf("  sar rax, cl\n");
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
    }

    printf("  push rax\n");
}

// 抽象構文木の根ノードを受け取りスタックマシンのコードを生成する
static void gen(Node *node) {
    int label_num;
    switch(node->kind) {
        case ND_NULL:
            // 何もしない
            return;
        case ND_NUM:
            if(node->val == (int)node->val) {
                printf("  push %ld\n", node->val);
            } else {
                printf("  movabs rax, %ld\n", node->val);
                printf("  push rax\n");
            }
            return;
        case ND_EXPR_STMT:
            gen(node->lhs);
            printf("  add rsp, 8\n");
            return;
        case ND_VAR:
            if(node->init) {
                gen(node->init);
            }
            gen_lval(node);
            if(node->type->ty != ARRAY) {
                load(node->type);
            }
            return;
        case ND_MEMBER:
            gen_lval(node);
            if(node->type->ty != ARRAY) {
                load(node->type);
            }
            return;
        case ND_ASSIGN:
            gen_lval(node->lhs);  // 左辺: 変数のアドレスをpush
            gen(node->rhs);       // 右辺: 数値をpush
            store(node->type);
            return;
        case ND_TERNARY: {
            int seq = label_seq_num++;
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lelse%d\n", seq);
            gen(node->then);
            printf("  jmp .Lend%d\n", seq);
            printf(".Lelse%d:\n", seq);
            gen(node->els);
            printf(".Lend%d:\n", seq);
            return;
        }
        case ND_PRE_INC:
            gen_lval(node->lhs);
            printf("  push [rsp]\n");
            load(node->type);
            inc(node->type);
            store(node->type);
            return;
        case ND_PRE_DEC:
            gen_lval(node->lhs);
            printf("  push [rsp]\n");
            load(node->type);
            dec(node->type);
            store(node->type);
            return;
        case ND_POST_INC:
            gen_lval(node->lhs);
            printf("  push [rsp]\n");
            load(node->type);
            inc(node->type);
            store(node->type);
            dec(node->type);
            return;
        case ND_POST_DEC:
            gen_lval(node->lhs);
            printf("  push [rsp]\n");
            load(node->type);
            dec(node->type);
            store(node->type);
            inc(node->type);
            return;
        case ND_ADD_EQ:
        case ND_PTR_ADD_EQ:
        case ND_SUB_EQ:
        case ND_PTR_SUB_EQ:
        case ND_MUL_EQ:
        case ND_DIV_EQ:
        case ND_SHL_EQ:
        case ND_SHR_EQ:
        case ND_BITAND_EQ:
        case ND_BITOR_EQ:
        case ND_BITXOR_EQ:
            gen_lval(node->lhs);
            printf("  push [rsp]\n");
            load(node->lhs->type);
            gen(node->rhs);
            gen_binary(node);
            store(node->type);
            return;
        case ND_COMMA:
            gen(node->lhs);
            gen(node->rhs);
            return;
        case ND_ADDR:
            gen_lval(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            if(node->type->ty != ARRAY) {
                load(node->type);
            }
            return;
        case ND_NOT:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            printf("  push rax\n");
            return;
        case ND_BITNOT:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  not rax\n");
            printf("  push rax\n");
            return;
        case ND_LOGAND: {
            int seq = label_seq_num++;
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .L.false.%d\n", seq);
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .L.false.%d\n", seq);
            printf("  push 1\n");
            printf("  jmp .L.end.%d\n", seq);
            printf(".L.false.%d:\n", seq);
            printf("  push 0\n");
            printf(".L.end.%d:\n", seq);
            return;
        }
        case ND_LOGOR: {
            int seq = label_seq_num++;
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  jne .L.true.%d\n", seq);
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  jne .L.true.%d\n", seq);
            printf("  push 0\n");
            printf("  jmp .L.end.%d\n", seq);
            printf(".L.true.%d:\n", seq);
            printf("  push 1\n");
            printf(".L.end.%d:\n", seq);
            return;
        }
        case ND_RETURN:
            if(node->lhs) {
                gen(node->lhs);
                printf("  pop rax\n");
            }
            printf("  jmp .L.return.%s\n", func_name);
            return;
        case ND_IF:
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            label_num = label_seq_num++;
            if(node->els) {
                // elseあり
                printf("  je  .Lelse%d\n", label_num);
                gen(node->then);
                printf("  jmp .Lend%d\n", label_num);
                printf(".Lelse%d:\n", label_num);
                gen(node->els);
                printf(".Lend%d:\n", label_num);
            } else {
                // elseなし
                printf("  je  .Lend%d\n", label_num);
                gen(node->then);
                printf(".Lend%d:\n", label_num);
            }
            return;
        case ND_WHILE: {
            label_num = label_seq_num++;
            int brk = brkseq;
            int cont = contseq;
            brkseq = contseq = label_num;

            printf(".Lcontinue%d:\n", label_num);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lbreak%d\n", label_num);
            gen(node->then);
            printf("  jmp .Lcontinue%d\n", label_num);
            printf(".Lbreak%d:\n", label_num);

            brkseq = brk;
            contseq = cont;
            return;
        }
        case ND_FOR: {
            label_num = label_seq_num++;
            int brk = brkseq;
            int cont = contseq;
            brkseq = contseq = label_num;

            if(node->init) {
                gen(node->init);
            }
            printf(".Lbegin%d:\n", label_num);
            if(node->cond) {
                // cond==NULLの場合.LendXXXラベルへのジャンプ処理を出力しない(=無限ループ)
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je  .Lbreak%d\n", label_num);
            }

            gen(node->then);
            printf(".Lcontinue%d:\n", label_num);
            if(node->post) {
                gen(node->post);
            }
            printf("  jmp .Lbegin%d\n", label_num);
            printf(".Lbreak%d:\n", label_num);

            brkseq = brk;
            contseq = cont;
            return;
        }
        case ND_DO: {
            int seq = label_seq_num++;
            int brk = brkseq;
            int cont = contseq;
            brkseq = contseq = seq;

            printf(".Lbegin%d:\n", seq);
            gen(node->then);
            printf(".Lcontinue%d:\n", seq);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  jne .Lbegin%d\n", seq);
            printf(".Lbreak%d:\n", seq);

            brkseq = brk;
            contseq = cont;
            return;
        }
        case ND_SWITCH: {
            int seq = label_seq_num++;
            int brk = brkseq;
            brkseq = seq;
            node->case_label = seq;

            gen(node->cond);
            printf("  pop rax\n");

            for(Node *n = node->case_next; n; n = n->case_next) {
                n->case_label = label_seq_num++;
                n->case_end_label = seq;
                printf("  cmp rax, %ld\n", n->val);
                printf("  je .Lcase%d\n", n->case_label);
            }

            if(node->default_case) {
                int i = label_seq_num++;
                node->default_case->case_end_label = seq;
                node->default_case->case_label = i;
                printf("  jmp .Lcase%d\n", i);
            }

            printf("  jmp .Lbreak%d\n", seq);
            gen(node->then);
            printf(".Lbreak%d:\n", seq);

            brkseq = brk;
            return;
        }
        case ND_CASE:
            printf(".Lcase%d:\n", node->case_label);
            gen(node->lhs);
            return;
        case ND_BLOCK:
        case ND_STMT_EXPR:
            for(Node *cur = node->block; cur; cur = cur->next) {
                gen(cur);
            }
            return;
        case ND_BREAK:
            if(brkseq == 0) {
                error("不正なbreakです");
            }
            printf("  jmp .Lbreak%d\n", brkseq);
            return;
        case ND_CONTINUE:
            if(contseq == 0) {
                error("不正なcontinueです");
            }
            printf("  jmp .Lcontinue%d\n", contseq);
            return;
        case ND_GOTO:
            printf("  jmp .Llabel.%s.%s\n", func_name, node->label_name);
            return;
        case ND_LABEL:
            printf(".Llabel.%s.%s:\n", func_name, node->label_name);
            gen(node->lhs);
            return;
        case ND_FUNCCALL:
            if(!strcmp(node->func_name, "__builtin_va_start")) {
                printf("  pop rax\n");
                printf("  mov edi, dword ptr [rbp-8]\n");
                printf("  mov dword ptr [rax], 0\n");
                printf("  mov dword ptr [rax+4], 0\n");
                printf("  mov qword ptr [rax+8], rdi\n");
                printf("  mov qword ptr [rax+16], 0\n");
                return;
            }

            int args_count = 0;
            for(Node *cur = node->args; cur; cur = cur->next) {
                gen(cur);
                args_count++;
            }
            if(args_count > 6) {
                error("%s: , 7個以上の引数を持つ関数です", node->func_name);
            }
            for(int i = args_count - 1; i >= 0; i--) {
                printf("  pop %s\n", regs_for_args_8[i]);
            }

            // x86-64のABIに従ってcall命令実行前にrspを16バイトでアライメントする必要がある
            label_num = label_seq_num++;
            printf("  mov rax, rsp\n");
            printf("  and rax, 15\n");
            printf("  jnz .L.call.%d\n", label_num);
            printf("  mov rax, 0\n");
            printf("  call %s\n", node->func_name);
            printf("  jmp .L.end.%d\n", label_num);
            printf(".L.call.%d:\n", label_num);
            printf("  sub rsp, 8\n");
            printf("  mov rax, 0\n");
            printf("  call %s\n", node->func_name);
            printf("  add rsp, 8\n");
            printf(".L.end.%d:\n", label_num);
            if(node->type->ty == BOOL) {
                printf("  movzb rax, al\n");
            }
            printf("  push rax\n");
            return;
        case ND_CAST:
            gen(node->lhs);
            truncate(node->type);
            return;
    }

    gen(node->lhs);
    gen(node->rhs);
    gen_binary(node);
}

// レジスタ上の引数をスタック領域にコピーする処理をアセンブリに出力する
static void load_arg(Var *var, int idx) {
    int size = var->type->size;
    if(size == 1) {
        printf("  mov [rbp-%d], %s\n", var->offset, regs_for_args_1[idx]);
    } else if(size == 2) {
        printf("  mov [rbp-%d], %s\n", var->offset, regs_for_args_2[idx]);
    } else if(size == 4) {
        printf("  mov [rbp-%d], %s\n", var->offset, regs_for_args_4[idx]);
    } else {
        assert(size == 8);
        printf("  mov [rbp-%d], %s\n", var->offset, regs_for_args_8[idx]);
    }
}

// 関数をアセンブリとして出力する
static void funcgen(Function *func) {
    func_name = func->name;
    // 関数ラベル、プロローグ出力
    if(!func->is_static) {
        printf(".global %s\n", func->name);
    }
    printf("%s:\n", func->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", func->stack_size);

    // 可変長引数の関数の場合、引数用レジスタの値を保存する
    if(func->has_varargs) {
        int n = 0;
        for(VarList *vl = func->args; vl; vl = vl->next) {
            n++;
        }

        printf("mov dword ptr [rbp-8], %d\n", n * 8);
        printf("mov [rbp-16], r9\n");
        printf("mov [rbp-24], r8\n");
        printf("mov [rbp-32], rcx\n");
        printf("mov [rbp-40], rdx\n");
        printf("mov [rbp-48], rsi\n");
        printf("mov [rbp-56], rdi\n");
    }

    // レジスタ上の引数をスタック領域にコピー
    int i = 0;
    for(VarList *arg = func->args; arg; arg = arg->next) {
        load_arg(arg->var, i++);
    }

    // 先頭の式から順にコード生成
    for(Node *node = func->node; node; node = node->next) {
        gen(node);
    }

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf(".L.return.%s:\n", func_name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

// データセグメントをアセンブリに出力する
static void gen_data_seg(Program *prog) {
    for(VarList *vlist = prog->globals; vlist; vlist = vlist->next) {
        if(!vlist->var->is_static) {
            printf(".global %s\n", vlist->var->name);
        }
    }

    printf(".bss\n");

    for(VarList *vlist = prog->globals; vlist; vlist = vlist->next) {
        Var *gvar = vlist->var;
        if(gvar->initializer) {
            continue;
        }

        printf(".align %d\n", gvar->type->align);
        printf("%s:\n", gvar->name);
        printf("  .zero %d\n", gvar->type->size);
    }

    printf(".data\n");

    for(VarList *vlist = prog->globals; vlist; vlist = vlist->next) {
        Var *gvar = vlist->var;
        if(!gvar->initializer) {
            continue;
        }

        printf(".align %d\n", gvar->type->align);
        printf("%s:\n", gvar->name);

        for(Initializer *init = gvar->initializer; init; init = init->next) {
            if(init->label) {
                printf("  .quad %s%+ld\n", init->label, init->addend);
            } else if(init->sz == 1) {
                printf("  .byte %ld\n", init->val);
            } else {
                printf("  .%dbyte %ld\n", init->sz, init->val);
            }
        }
    }
}

// テキストセグメントをアセンブリに出力する
static void gen_text_seg(Program *prog) {
    printf(".text\n");

    // 関数を出力
    for(Function *func = prog->funcs; func; func = func->next) {
        funcgen(func);
    }
}

void codegen(Program *prog) {
    label_seq_num = 0;

    printf(".intel_syntax noprefix\n");
    gen_data_seg(prog);
    gen_text_seg(prog);
}
