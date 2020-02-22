#include "zxcc.h"

// labelの通し番号
int label_seq_num;

static void debug_printf(char *fmt, ...);

// nodeを左辺値として評価し、そのアドレスをスタックにpushするコードを生成する。
// nodeが変数ではない場合エラー終了させる。
static void gen_lval(Node *node) {
    debug_printf("gen_lval");
    if(node->kind != ND_LVAR)
        error("代入の左辺値が変数ではありません");

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset);
    printf("  push rax\n");
    debug_printf("gen_lval end");
}

// 抽象構文木の根ノードを受け取りスタックマシンのコードを生成する
static void gen(Node *node) {
    int label_num;
    switch(node->kind) {
    case ND_NUM:
        debug_printf("gen - ND_NUM");
        printf("  push %d\n", node->val);
        return;
    case ND_LVAR:
        debug_printf("gen - ND_LVAR");
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        debug_printf("gen - ND_LVAR end");
        return;
    case ND_ASSIGN:
        debug_printf("gen - ND_ASSIGN");
        gen_lval(node->lhs); // 左辺: 変数のアドレスをpush
        gen(node->rhs);      // 右辺: 数値をpush
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        debug_printf("gen - ND_ASSIGN end");
        return;
    case ND_RETURN:
        debug_printf("gen - ND_RETURN");
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
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
    case ND_SUB:
        debug_printf("gen - ND_SUB");
        printf("  sub rax, rdi\n");
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

void codegen() {
    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    // プロローグ
    // localsにパース処理中に最後に割り当てたローカル変数のポインタが代入されているため、
    // locals->offsetが必要なローカル変数用領域のサイズと等しい。
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", locals->offset);

    // 先頭の式から順にコード生成
    for(int i = 0; code[i]; i++) {
        gen(code[i]);

        // 式の評価結果としてスタックに一つの値が残っている
        // はずなので、スタックが溢れないようにポップしておく
        printf("  pop rax\n");
    }

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
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