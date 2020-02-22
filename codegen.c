#include "zxcc.h"

// labelの通し番号
int label_seq_num;

// nodeを左辺値として評価し、そのアドレスをスタックにpushするコードを生成する。
// nodeが変数ではない場合エラー終了させる。
static void gen_lval(Node *node) {
    if(node->kind != ND_LVAR)
        error("代入の左辺値が変数ではありません");

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset);
    printf("  push rax\n");
}

// 抽象構文木の根ノードを受け取りスタックマシンのコードを生成する
static void gen(Node *node) {
    int label_num;
    switch(node->kind) {
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_LVAR:
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        gen_lval(node->lhs);
        gen(node->rhs);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    case ND_RETURN:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    case ND_IF:
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
        return;
    case ND_WHILE:
        label_num = label_seq_num++;
        printf(".Lbegin%03d:\n", label_num);
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lend%03d\n", label_num);
        gen(node->then);
        printf("  jmp .Lbegin%03d\n", label_num);
        printf(".Lend%03d:\n", label_num);
        return;
    case ND_FOR:
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
        return;
    }

    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch(node->kind) {
    case ND_ADD:
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
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