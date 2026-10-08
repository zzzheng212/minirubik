"""Validation of the student's captured assembly, not the earlier reference solver.
AI-assisted test tooling. Uses only Python standard library and Ripes.
"""
import argparse
import ast
import concurrent.futures
import csv
import hashlib
import json
import random
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT/'source'/'test.s'
NAMES = ['R', 'R2', "R'", 'B', 'B2', "B'", 'D', 'D2', "D'"]
SRC = [[1, 4, 2, 0, 3, 5, 6], [0, 1, 2, 4, 5, 6, 3], [0, 2, 5, 3, 1, 4, 6]]
TWIST = [[1, 2, 0, 2, 1, 0, 0], [0, 0, 0, 1, 2, 1, 2], [0]*7]


def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()


def write_json(p, v): p.write_text(json.dumps(
    v, ensure_ascii=False, indent=2), encoding='utf-8')


def cli_source():
    text = SOURCE.read_text(encoding='utf-8-sig')
    # Remove only the two complete argument-setup/call sequences for rendering.
    text, n = re.subn(r'(?m)^\s*mv\s+a0,\s*s0\s*\n\s*mv\s+a1,\s*s1\s*\n\s*jal\s+ra,\s*render_cube\s*$',
                      '\n    # Renderer compiled out.\n', text)
    if n != 2:
        raise ValueError(
            f'Expected exactly two renderer call sites, found {n}')
    if re.search(r'(?m)^\s*[^#\n]*\bLED_MATRIX_|^\s*jal\s+ra,\s*render_cube', text):
        raise ValueError('Unexpected remaining renderer reference')
    return text


def adapt(text, state, expected):
    text, n = re.subn(
        r'(\binput:\s*\.string\s*)"[^"\n]*"', lambda m: m[1]+json.dumps(state), text)
    if n != 1:
        raise ValueError('input label not unique')
    text, n = re.subn(r'(\bexpected_length:\s*\.word\s*)-?\d+',
                      lambda m: m[1]+str(expected), text)
    if n != 1:
        raise ValueError('expected_length not unique')
    return text


def replay(state, moves):
    p = [int(x)-1 for x in state[:7]]
    o = [int(x)-1 for x in state[7:]]
    for move in moves:
        f = move//3
        for _ in range(move % 3+1):
            p = [p[s] for s in SRC[f]]
            o = [(o[s]+TWIST[f][i]) % 3 for i, s in enumerate(SRC[f])]
    return p == list(range(7)) and o == [0]*7


def static_bytes(text):
    offsets = {'.data': 0, '.bss': 0, '.rodata': 0}
    section = '.text'
    for raw in text.splitlines():
        line = raw.split('#', 1)[0].strip()
        line = re.sub(r'^\w+:\s*', '', line)
        if line in ('.text', '.data', '.bss', '.rodata'):
            section = line
            continue
        if not line or section not in offsets:
            continue
        op, _, args = line.partition(' ')
        args = args.strip()
        if op == '.align':
            boundary = int(args, 0)
            offsets[section] = (
                (offsets[section]+boundary-1)//boundary)*boundary
        elif op == '.zero':
            offsets[section] += int(args, 0)
        elif op in ('.byte', '.half', '.word'):
            offsets[section] += len(args.split(',')) * \
                {'.byte': 1, '.half': 2, '.word': 4}[op]
        elif op == '.string':
            offsets[section] += len(ast.literal_eval(args).encode('utf-8'))+1
        else:
            raise ValueError('Unsupported data directive: '+line)
    return sum(offsets.values())


def run_one(args, text, case, out):
    state = case['input']
    expected = int(case['distance'])
    invalid = expected < 0
    key = case.get('name', state)
    src = out/(key+'.s')
    logpath = out/(key+'.log')
    resultpath = out/(key+'.json')
    code = adapt(text, state, expected)
    digest = hashlib.sha256(code.encode()).hexdigest()
    if args.resume and resultpath.exists():
        old = json.loads(resultpath.read_text(encoding='utf-8'))
        if old.get('ok') and old.get('source_sha256') == digest and old.get('ripes_sha256') == args.ripes_hash:
            return old
    src.write_text(code, encoding='utf-8')
    command = [args.ripes, '--mode', 'cli', '--src', str(src), '-t', 'asm', '--proc', args.model,
               '--reginit', 'gpr:2=0x7ffffff0', '--iret', '--exectime', '--regs', '--timeout', '600000']
    start = time.perf_counter()
    try:
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=650,
                              creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        raw = proc.stdout.decode('utf-8', errors='replace')
        rc = proc.returncode
    except subprocess.TimeoutExpired as e:
        raw = (e.stdout or b'').decode(
            'utf-8', errors='replace')+'\nHOST TIMEOUT'
        rc = -1
    logpath.write_text(raw, encoding='utf-8')
    log = raw.replace('\x00', '')

    def number(pattern):
        m = re.search(pattern, log)
        return int(m[1]) if m else None
    retired = number(r'instructions retired\s+(\d+)')
    length = number(r'Length:\s*(\d+)')
    match = re.search(r'Solution:[ \t]*([^\r\n]*)', log)
    tokens = match[1].split() if match else []
    valid_tokens = all(t in NAMES for t in tokens)
    moves = [NAMES.index(t) for t in tokens] if valid_tokens else []
    if invalid:
        ok = rc == 0 and 'FAIL: invalid input' in log and 'Program exited with code: 1' in log and number(
            r'x8:\s+(\d+)') == 0
    else:
        ok = (rc == 0 and 'PASS: replay solved' in log and 'Program exited with code: 0' in log
              and length == expected and len(moves) == expected and valid_tokens and replay(state, moves)
              and number(r'x8:\s+(\d+)') == 1 and retired is not None)
    row = dict(input=state, name=key, model=args.model, expected=expected, length=length, moves=tokens,
               ok=ok, retired=retired, within_budget=retired is not None and retired <= 50_000_000,
               model_ms=number(r'execution time \(ms\)\s+(\d+)'), wall_seconds=time.perf_counter()-start,
               source_sha256=digest, ripes_sha256=args.ripes_hash, command=command,
               same_path_as_C=(''.join(map(str, moves)) == case['path']) if not invalid and 'path' in case else None)
    write_json(resultpath, row)
    return row


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--ripes', required=True)
    p.add_argument(
        '--model', choices=['RV32_ISS', 'RV32_5S'], default='RV32_ISS')
    p.add_argument(
        '--suite', choices=['smoke', 'hard', 'invalid', 'random'], default='smoke')
    p.add_argument('--workers', type=int, default=1)
    p.add_argument('--resume', action='store_true')
    args = p.parse_args()
    args.ripes_hash = sha(Path(args.ripes))
    rows = list(csv.DictReader((ROOT/'cases.tsv').open(), delimiter='\t'))
    if args.suite == 'smoke':
        states = ['12345671111111', '25346712313322',
                  '21345671111111', '54721631111111']
        if args.model == 'RV32_5S':
            states = states[:3]
        cases = [next(r for r in rows if r['input'] == s) for s in states]
    elif args.suite == 'hard':
        cases = [r for r in rows if r['distance'] == '11']
        assert len(cases) == 2644 and len({r['input'] for r in cases}) == 2644
    elif args.suite == 'random':
        exact = (ROOT/'exact.bin').read_bytes()
        rng = random.Random(20261007)
        cases = []
        for rank in rng.sample(range(3674160), 128):
            pr, orient = divmod(rank, 729)
            available = list(range(7))
            perm = []
            for fact in [720, 120, 24, 6, 2, 1, 1]:
                q, pr = divmod(pr, fact)
                perm.append(available.pop(q))
            ori = [0]*7
            for i in range(5, -1, -1):
                orient, ori[i] = divmod(orient, 3)
            ori[6] = -sum(ori) % 3
            cases.append(dict(input=''.join(str(v+1)
                         for v in perm+ori), distance=exact[rank]))
    else:
        states = ['11345671111111', '12345672111111', '1234567111111',
                  '123456711111111', '02345671111111', '12345674111111']
        cases = [dict(input=s, distance=-1, name='invalid_'+str(i))
                 for i, s in enumerate(states)]
    text = cli_source()
    (ROOT/'solver_cli.s').write_text(text, encoding='utf-8')
    out = ROOT/'results'/args.model/args.suite
    out.mkdir(parents=True, exist_ok=True)
    manifest = dict(original_sha256=sha(SOURCE), renderer_sha256=sha(ROOT/'source'/'renderer.s'),
                    cli_sha256=hashlib.sha256(text.encode()).hexdigest(), cases_sha256=sha(ROOT/'cases.tsv'),
                    ripes=args.ripes, ripes_sha256=args.ripes_hash, workers=args.workers,
                    static_bytes=static_bytes(text), led_static_bytes=static_bytes(text+'\n'+(ROOT/'source'/'renderer.s').read_text(encoding='utf-8-sig')))
    write_json(out/'manifest.json', manifest)
    results = []
    start = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(run_one, args, text, c, out) for c in cases]
        for f in concurrent.futures.as_completed(futures):
            r = f.result()
            results.append(r)
            if len(cases) <= 6 or len(results) % 25 == 0 or not r['ok']:
                print(
                    f"{args.model} {args.suite}: {len(results)}/{len(cases)}; last={r['input']}; ok={r['ok']}; retired={r['retired']}; elapsed={time.perf_counter()-start:.1f}s", flush=True)
            write_json(out/'progress.json', dict(completed=len(results),
                       total=len(cases), failures=sum(not x['ok'] for x in results)))
    valid = [r for r in results if r['retired'] is not None]
    summary = dict(completed=len(results), total=len(cases), all_pass=all(r['ok'] for r in results),
                   all_within_budget=all(r['within_budget'] for r in results),
                   wall_seconds=time.perf_counter()-start,
                   worst=max(
                       valid, key=lambda r: r['retired']) if valid else None,
                   over_budget=[r['input']
                                for r in results if not r['within_budget']],
                   failures=[r['input'] for r in results if not r['ok']])
    write_json(out/'summary.json', summary)
    with (out/'measurements.csv').open('w', newline='', encoding='utf-8') as stream:
        keys = ['input', 'model', 'expected', 'length', 'ok', 'retired',
                'within_budget', 'model_ms', 'wall_seconds', 'same_path_as_C', 'source_sha256']
        writer = csv.DictWriter(stream, fieldnames=keys, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(sorted(results, key=lambda r: r['input']))
    print(json.dumps(summary, ensure_ascii=False), flush=True)
    if not summary['all_pass'] or (args.suite == 'hard' and not summary['all_within_budget']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
