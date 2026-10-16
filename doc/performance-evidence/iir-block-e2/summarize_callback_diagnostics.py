#!/usr/bin/env python3
import csv, json, math, statistics
from pathlib import Path
from run_expansion import CASES, OUT

DIR=OUT/'callback'; T5=2.7764451051977987

def percentile(values,p):
    v=sorted(values); return v[max(0,math.ceil(p*len(v))-1)]

def main():
    summaries=[]
    for case in CASES:
        runs={arm:[] for arm in ('baseline','candidate')}
        for rep in range(5):
            for arm in ('baseline','candidate'):
                path=DIR/f"{case['name']}-{arm}-{rep:02d}.csv"
                with path.open() as f: values=[int(r['latency_ns']) for r in csv.DictReader(f)]
                deadline=case['block']/case['rate']*1e9
                runs[arm].append({'repeat':rep,'p50_ns':percentile(values,.50),'p95_ns':percentile(values,.95),
                  'p99_ns':percentile(values,.99),'max_ns':max(values),'calls':len(values),
                  'over_deadline_calls':sum(v>deadline for v in values),'deadline_ns':deadline})
        ratios=[runs['candidate'][i]['p99_ns']/runs['baseline'][i]['p99_ns'] for i in range(5)]
        logs=list(map(math.log,ratios)); mean=statistics.mean(logs); half=T5*statistics.stdev(logs)/math.sqrt(5)
        summary={'case':case['name'],'block':case['block'],'sample_rate':case['rate'],
          'deadline_ns':case['block']/case['rate']*1e9,'baseline_runs':runs['baseline'],'candidate_runs':runs['candidate'],
          'baseline_median':{k:statistics.median(r[k] for r in runs['baseline']) for k in ('p50_ns','p95_ns','p99_ns','max_ns')},
          'candidate_median':{k:statistics.median(r[k] for r in runs['candidate']) for k in ('p50_ns','p95_ns','p99_ns','max_ns')},
          'p99_candidate_baseline_ratios':ratios,'p99_ratio_geomean':math.exp(mean),
          'p99_ratio_95ci':[math.exp(mean-half),math.exp(mean+half)],
          'repeatable_over_5pct_p99_regression':math.exp(mean-half)>1.05,
          'over_deadline_calls':{'baseline':sum(r['over_deadline_calls'] for r in runs['baseline']),
                                 'candidate':sum(r['over_deadline_calls'] for r in runs['candidate'])}}
        summaries.append(summary)
    result={'note':'Offline fluid_synth_write_float process-call timing with per-call clock and CSV overhead; diagnostic only, not an audio-device/xrun guarantee.',
      'all_no_repeatable_over_5pct_p99_regression':all(not s['repeatable_over_5pct_p99_regression'] for s in summaries),
      'cases':summaries}
    (OUT/'callback-summary.json').write_text(json.dumps(result,indent=2)+'\n')
    for s in summaries:
        print(s['case'],'p99 ns',round(s['baseline_median']['p99_ns']),round(s['candidate_median']['p99_ns']),
          'ratio',round(s['p99_ratio_geomean'],4),'CI',tuple(round(x,4) for x in s['p99_ratio_95ci']),
          'deadline',s['over_deadline_calls'],'regress>5%',s['repeatable_over_5pct_p99_regression'])
    print('gate',result['all_no_repeatable_over_5pct_p99_regression'])

if __name__=='__main__': main()
