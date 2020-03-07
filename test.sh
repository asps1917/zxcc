#!/bin/bash
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }

int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

try() {
  expected="$1"
  input="$2"

  ./zxcc "$input" > tmp.s
  gcc -static -o tmp tmp.s tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 0 'main() { return 0; }'
try 42 'main() { return 42; }'
try 21 'main() { return 5+20-4; }'
try 41 'main() { return 12 + 34 - 5 ; }'
try 47 'main() { return 5+6*7; }'
try 15 'main() { return 5*(9-6); }'
try 4 'main() { return (3+5)/2; }'
try 10 'main() { return -10+20; }'
try 20 'main() { return -(-3*+5)+5; }'

try 0 'main() { return 0==1; }'
try 1 'main() { return 42==42; }'
try 1 'main() { return 0!=1; }'
try 0 'main() { return 42!=42; }'

try 1 'main() { return 0<1; }'
try 0 'main() { return 1<1; }'
try 0 'main() { return 2<1; }'
try 1 'main() { return 0<=1; }'
try 1 'main() { return 1<=1; }'
try 0 'main() { return 2<=1; }'
try 1 'main() { return 1>0; }'
try 0 'main() { return 1>1; }'
try 0 'main() { return 1>2; }'
try 1 'main() { return 1>=0; }'
try 1 'main() { return 1>=1; }'
try 0 'main() { return 1>=2; }'

try 1 'main() { return 1;3;return 5; }'
try 3 'main() { 1;return 3;5; }'
try 5 'main() { 1;3;return 5; }'

try 3 'main() { int a; a = 3; return a; }'
try 14 'main() { int a; int b; a = 3; b = 5 * 6 - 8; return a + b / 2; }'
try 6 'main() { int foo; int bar; foo = 1;bar = 2 + 3;return foo + bar; }'
try 24 'main() { int foo_123; int _aiueo99; foo_123 = 12;_aiueo99 = -1 + 3;return foo_123*_aiueo99; }'

try 3 'main() { if (1) return 3; }'
try 3 'main() { if (1-1) return 2; return 3; }'
try 2 'main() { if (2-1) return 2; return 3; }'
try 2 'main() { int foo; foo=3; foo=foo-3; if (foo) return 1; else return 2; }'

try 1 'main() { while(1) return 1; return 2; }'
try 2 'main() { while(0) return 1; return 2; }'
try 8 'main() { int foo; foo = 20; while(foo>=10) foo = foo-3; return foo; }'

try 10 'main() { int limit; limit = 10; int a; int b; for(a=0; a<limit; a = a+1) b=1; return a; }'
try 10 'main() { int limit; limit = 10; int a ;a = 0; int b; for(; a<limit; a = a+1) b=1; return a; }'
try 0 'main() { int limit ; int a; limit = 10;for(a = 0;; a = a+1) return a; }'
try 10 'main() { int limit; limit = 10; int a; for(a=0; a<limit;) a = a+1; return a; }'
try 10 'main() { int limit; limit = 10; for(;;) return limit; return 0; }'

try 30 'main() { int limit; int b;{limit = 10; b=0;} int a; for(a=0; a<limit; a = a+1) {b=b+1; b=b+2; } return b; }'
try 10 'main() { int a; a = 0; while(1) {if(a==10) return a; a =a+1;} }'
try 20 'main() { int a; int b; a = 0; b=0; while(1) {if(a==10) {return b; } a =a+1; b = b+2;} }'
try 21 'main() { int a; int b; a = 0; b=0; while(1) {if(a==10) b = b+1; if(a==20) return b; a =a+1; b=b+1;} }'

try 3 'main() { return ret3(); }'
try 5 'main() { return ret5(); }'
try 8 'main() { return add(3, 5); }'
try 2 'main() { return sub(5, 3); }'
try 21 'main() { return add6(1,2,3,4,5,6); }'

try 10 'main() { int a; int b; a = 1; b =2; b = a+b; return b + ret7(); } ret7() { int a; int b; a = 3; b = 4; a = a+b; return a;}'

try 7 'main() { return add2(3,4); } add2(x,y) { return x+y; }'
try 1 'main() { return sub2(4,3); } sub2(x,y) { return x-y; }'
try 55 'main() { return fib(9); } fib(x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'
try 17 'main() { return foo(6,4); } foo(x,y) { int a; int b; a = 1; b =2; a = a + b; return x + y - a + add2(3, 7); } add2(x,y) { return x+y; }'
try 21 'main() {return add_6(1,2,3,4,5,6);} add_6(a,b,c,d,e,f) {return a+b+c+d+e+f; }'

try 3 'main() { int x; int y; x = 3; y = &x; return *y;}'
try 3 'main() { int x; int y; int z; x = 3; y = 5; z = &y - 8; return *z;}'
try 5 'main() { int x; int y; int z; x = 3; y = 5; z = &x + 8; return *z;}'
try 3 'main() { int x; int y; int z; x=3; y=&x; z=&y; return **z; }'
try 5 'main() { int x; int y; x=3; y=&x; *y=5; return x; }'
try 7 'main() { int x; int y; x=3; y=5; *(&x+8)=7; return y; }'
try 7 'main() { int x; int y; x=3; y=5; *(&y-8)=7; return x; }'

echo OK