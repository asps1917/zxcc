#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./zxcc "$input" > tmp.s
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

try 0 "return 0;"
try 42 "return 42;"
try 21 "return 5+20-4;"
try 41 "return 12 + 34 - 5 ;"
try 47 'return 5+6*7;'
try 15 'return 5*(9-6);'
try 4 'return (3+5)/2;'
try 10 'return -10+20;'
try 20 'return -(-3*+5)+5;'

try 0 'return 0==1;'
try 1 'return 42==42;'
try 1 'return 0!=1;'
try 0 'return 42!=42;'
try 1 'return 0<1;'
try 0 'return 1<1;'
try 0 'return 2<1;'
try 1 'return 0<=1;'
try 1 'return 1<=1;'
try 0 'return 2<=1;'
try 1 'return 1>0;'
try 0 'return 1>1;'
try 0 'return 1>2;'
try 1 'return 1>=0;'
try 1 'return 1>=1;'
try 0 'return 1>=2;'
try 1 'return 1;3;return 5;'
try 3 '1;return 3;5;'
try 5 '1;3;return 5;'
try 3 'return a = 3;'
try 14 'a = 3; b = 5 * 6 - 8; return a + b / 2;'
try 6 'foo = 1;bar = 2 + 3;return foo + bar;'
try 24 'foo_123 = 12;_aiueo99 = -1 + 3;return foo_123*_aiueo99;'
try 3 'if (1) return 3;'
try 3 'if (1-1) return 2; return 3;'
try 2 'if (2-1) return 2; return 3;'
try 2 'foo=3; foo=foo-3; if (foo) return 1; else return 2;'
try 1 'while(1) return 1; return 2;'
try 2 'while(0) return 1; return 2;'
try 8 'foo = 20; while(foo>=10) foo = foo-3; return foo;'
echo OK