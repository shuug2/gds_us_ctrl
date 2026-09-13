#!/usr/bin/env python3
import re, sys

def strip(src):
    out, i, n = [], 0, len(src)
    while i < n:
        if src.startswith('//', i):
            j = src.find('\n', i); j = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2); j = n if j < 0 else j + 2
        elif src[i] in '"\'':
            q, j = src[i], i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(q + re.sub(r'[^\n]', ' ', src[i + 1:j - 1]) + q); i = j; continue
        else:
            out.append(src[i]); i += 1; continue
        out.append(re.sub(r'[^\n]', ' ', src[i:j])); i = j
    return ''.join(out)

def report(path):
    s = strip(open(path, encoding='utf-8', errors='replace').read())
    lines = s.split('\n'); i, n = 0, len(s)
    while i < n:
        if s[i] != '{': i += 1; continue
        d, j = 0, i
        while j < n:
            d += (s[j] == '{') - (s[j] == '}')
            if d == 0: break
            j += 1
        k = i - 1
        while k >= 0 and s[k].isspace(): k -= 1
        m = re.search(r'([A-Za-z_]\w*)\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*$', s[:k + 1])
        if k >= 0 and s[k] == ')' and m and m.group(1) not in ('if', 'while', 'for', 'switch', 'return'):
            a = s.count('\n', 0, m.start(1)) + 1; b = s.count('\n', 0, j) + 1
            code = sum(1 for ln in lines[a - 1:b] if ln.strip())
            print(f'{m.group(1)} {b - a + 1} (code {code})')
        i = j + 1

for p in sys.argv[1:]: report(p)
