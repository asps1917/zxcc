#include "zxcc.h"

// labelの通し番号
int label_seq_num;
static char *func_name;

static char *regs_for_args_8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *regs_for_args_4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *regs_for_args_2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *regs_for_args_1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};

static void debug_printf(char *fmt, ...);
static void gen(Node *node);

// nodeを左辺値として評価し、そのアドレスをスタックにpushするコードを生成する。
// nodeが評価不可能な場合エラー終了させる。
static void gen_lval(Node *node) {
    debug_printf("gen_lval");
    switch(node->kind) {
        case ND_VAR:
            if(node->var->is_local) {
                printf("  mov rax, rbp\n");
                printf("  sub rax, %d\n", node->var->offset);
                printf("  push rax\n");
            } else {
                // グローバル変数 or 文字列リテラル
                printf("  push offset %s\n", node->var->name);
            }
            break;
        case ND_DEREF:
            gen(node->lhs);
            break;
        case ND_MEMBER:
            gen_lval(node->lhs);
            printf("  pop rax\n");
            printf("  add rax, %d\n", node->member->offset);
            printf("  push rax\n");
            break;
        default:
            error("引数が左辺値として評価不可能なノードです");
    }
    debug_printf("gen_lval end");
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

// 抽象構文木の根ノードを受け取りスタックマシンのコードを生成する
static void gen(Node *node) {
    int label_num;
    switch(node->kind) {
        case ND_NULL:
            // 何もしない
            return;
        case ND_NUM:
            debug_printf("gen - ND_NUM");
            if(node->val == (int)node->val) {
                printf("  push %d\n", node->val);
            } else {
                printf("  movabs rax, %ld\n", node->val);
                printf("  push rax\n");
            }
            return;
        case ND_EXPR_STMT:
            debug_printf("gen - ND_EXPR_STMT");
            gen(node->lhs);
            printf("  add rsp, 8\n");
            return;
        case ND_VAR:
        case ND_MEMBER:
            debug_printf("gen - ND_VAR");
            gen_lval(node);
            if(node->type->ty != ARRAY) {
                load(node->type);
            }
            debug_printf("gen - ND_VAR end");
            return;
        case ND_ASSIGN:
            debug_printf("gen - ND_ASSIGN");
            gen_lval(node->lhs);  // 左辺: 変数のアドレスをpush
            gen(node->rhs);       // 右辺: 数値をpush
            store(node->type);
            debug_printf("gen - ND_ASSIGN end");
            return;
        case ND_ADDR:
            debug_printf("gen - ND_ADDR");
            gen_lval(node->lhs);
            debug_printf("gen - ND_ADDR end");
            return;
        case ND_DEREF:
            debug_printf("gen - ND_DEREF");
            gen(node->lhs);
            if(node->type->ty != ARRAY) {
                load(node->type);
            }
            debug_printf("gen - ND_DEREF end");
            return;
        case ND_RETURN:
            debug_printf("gen - ND_RETURN");
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  jmp .L.return.%s\n", func_name);
            debug_printf("gen - ND_RETURN end");
            return;
        case ND_IF:
            debug_printf("gen - ND_IF");
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            label_num = label_seq_num++;
            if(node->els) {
                // elseあり
                printf("  je  .Lelse%03d\n", label_num);
                gen(node->then);
                printf("  jmp .Lend%03d\n", label_num);
                printf(".Lelse%03d:\n", label_num);
                gen(node->els);
                printf(".Lend%03d:\n", label_num);
            } else {
                // elseなし
                printf("  je  .Lend%03d\n", label_num);
                gen(node->then);
                printf(".Lend%03d:\n", label_num);
            }
            debug_printf("gen - ND_IF end");
            return;
        case ND_WHILE:
            debug_printf("gen - ND_WHILE");
            label_num = label_seq_num++;
            printf(".Lbegin%03d:\n", label_num);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lend%03d\n", label_num);
            gen(node->then);
            printf("  jmp .Lbegin%03d\n", label_num);
            printf(".Lend%03d:\n", label_num);
            debug_printf("gen - ND_WHILE end");
            return;
        case ND_FOR:
            debug_printf("gen - ND_FOR");
            label_num = label_seq_num++;
            if(node->init) {
                gen(node->init);
            }
            printf(".Lbegin%03d:\n", label_num);
            if(node->cond) {
                // cond==NULLの場合.LendXXXラベルへのジャンプ処理を出力しない(=無限ループ)
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je  .Lend%03d\n", label_num);
            }

            gen(node->then);
            if(node->post) {
                gen(node->post);
            }
            printf("  jmp .Lbegin%03d\n", label_num);
            printf(".Lend%03d:\n", label_num);
            debug_printf("gen - ND_FOR end");
            return;
        case ND_BLOCK:
        case ND_STMT_EXPR:
            debug_printf("gen - ND_BLOCK, ND_STMT_EXPR");
            for(Node *cur = node->block; cur; cur = cur->next) {
                gen(cur);
            }
            debug_printf("gen - ND_BLOCK, ND_STMT_EXPR end");
            return;
        case ND_FUNCCALL:
            debug_printf("gen - ND_FUNCCALL");
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
            printf("  push rax\n");
            debug_printf("gen - ND_FUNCCALL end");
            return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch(node->kind) {
        case ND_ADD:
            debug_printf("gen - ND_ADD");
            printf("  add rax, rdi\n");
            break;
        case ND_PTR_ADD:
            debug_printf("gen - ND_PTR_ADD");
            printf("  imul rdi, %d\n", node->type->ptr_to->size);
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            debug_printf("gen - ND_SUB");
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_SUB:
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
            debug_printf("gen - ND_MUL");
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            debug_printf("gen - ND_DIV");
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_EQ:
            debug_printf("gen - ND_EQ");
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_NE:
            debug_printf("gen - ND_NE");
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LT:
            debug_printf("gen - ND_LT");
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LE:
            debug_printf("gen - ND_LE");
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
    }

    printf("  push rax\n");
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
    printf(".global %s\n", func->name);
    printf("%s:\n", func->name);
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", func->stack_size);

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
    printf(".data\n");

    for(VarList *vlist = prog->globals; vlist; vlist = vlist->next) {
        Var *gvar = vlist->var;
        printf("%s:\n", gvar->name);

        if(gvar->contents) {
            // 文字列リテラル
            for(int i = 0; i < gvar->cont_len; i++) {
                printf("  .byte %d\n", gvar->contents[i]);
            }
        } else {
            // グローバル変数
            printf("  .zero %d\n", gvar->type->size);
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

int is_debug = 0;
// デバッグ用のコメントをアセンブリに出力する
static void debug_printf(char *fmt, ...) {
    if(!is_debug) {
        return;
    }
    va_list va;
    va_start(va, fmt);
    printf("#[DEBUG] ");
    vprintf(fmt, va);
    printf("\n");

    va_end(va);
}