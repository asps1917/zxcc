#include "zxcc.h"

int main(int argc, char **argv) {
    if(argc != 2) {
        error("引数の個数が正しくありません");
        return 1;
    }

    // トークナイズしてパースする
    user_input = argv[1];
    token = tokenize();
    Function *prog = program();

    // ローカル変数分のスタック領域を確保
    //   パース処理中に最後に割り当てたローカル変数のポインタがlocalsに代入されているため、
    //   locals->offsetが必要なローカル変数用領域のサイズと等しい。
    prog->stack_size = prog->locals->offset;

    label_seq_num = 0;
    codegen(prog);

    return 0;
}