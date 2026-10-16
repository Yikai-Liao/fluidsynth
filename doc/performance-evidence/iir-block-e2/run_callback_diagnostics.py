#!/usr/bin/env python3
"""Separate instrumented process-call latency runs; never used for render-time ratios."""
import datetime, json, subprocess, time
from pathlib import Path
from run_expansion import CASES, OUT, command, run_one

LAT=OUT/'callback'
LAT.mkdir(exist_ok=True)

def main():
    out=(OUT/'callback_runs.jsonl').open('w')
    for case in CASES:
        for rep in range(5):
            order=['baseline','candidate'] if rep%2==0 else ['candidate','baseline']
            for arm in order:
                csv=LAT/f"{case['name']}-{arm}-{rep:02d}.csv"
                argv,env=command(case,arm,'-',csv)
                started=time.monotonic()
                cp=subprocess.run(argv,capture_output=True,text=True,env=env,check=True,timeout=300)
                if 'Failed to allocate a synthesis process' in cp.stderr:
                    raise RuntimeError(f"voice allocation failure in {case['name']} {arm}")
                row={'case':case['name'],'arm':arm,'repeat':rep,'order':order,
                  'csv_path':str(csv),'instrumented_render_wall_s':json.loads(cp.stdout)['render_wall'],
                  'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
                out.write(json.dumps(row,sort_keys=True)+'\n'); out.flush()
            print('latency',case['name'],rep+1,'/5',order,flush=True)
    out.close()

if __name__=='__main__': main()
