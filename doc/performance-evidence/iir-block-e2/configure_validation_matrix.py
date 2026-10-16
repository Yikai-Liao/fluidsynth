#!/usr/bin/env python3
import os, shlex, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parent
SOURCE=Path(os.environ['FLUIDSYNTH_E2_SOURCE']) if os.environ.get('FLUIDSYNTH_E2_SOURCE') else None
if SOURCE is None:
    raise SystemExit('set FLUIDSYNTH_E2_SOURCE to the FluidSynth worktree being validated')
tokens=shlex.split((ROOT/'configure-baseline.command').read_text().strip())
variants=[('debug-double','Debug','OFF','OFF'),('debug-float','Debug','ON','OFF'),
          ('asan-double','Debug','OFF','ON'),('asan-float','Debug','ON','ON')]
for name,build_type,floats,ubsan in variants:
    args=list(tokens)
    for i,t in enumerate(args):
        if t=='-S': args[i+1]=str(SOURCE)
        elif t=='-B': args[i+1]=str(ROOT/f'build-{name}')
        elif t.startswith('-DCMAKE_BUILD_TYPE='): args[i]=f'-DCMAKE_BUILD_TYPE={build_type}'
        elif t.startswith('-Denable-floats='): args[i]=f'-Denable-floats={floats}'
        elif t.startswith('-Denable-ubsan='): args[i]=f'-Denable-ubsan={ubsan}'
    out=ROOT/f'configure-{name}.log'
    with out.open('w') as f:
        print('configure',name,flush=True)
        cp=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT)
    if cp.returncode:
        print(out.read_text()[-4000:])
        raise SystemExit(cp.returncode)
    print('configured',name,flush=True)
