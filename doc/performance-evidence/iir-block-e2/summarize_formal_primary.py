#!/usr/bin/env python3
import json, math, statistics
from pathlib import Path
root=Path(__file__).resolve().parent/'formal-primary'

def load(name):
    return [json.loads(line) for line in (root/name).read_text().splitlines() if line.strip()]

def t_summary(rows, arm_left, arm_right, expected_n, tcrit):
    grouped={}
    for r in rows:
        if r.get('warmup'): continue
        grouped.setdefault((r.get('batch',1),r['pair']),{})[r['label']]=r
    ratios=[]; setup_render=[]; cpu=[]; hashes=[]; slow=[]
    for p in sorted(grouped):
        g=grouped[p]
        if set(g)!={arm_left,arm_right}: raise ValueError((p,set(g)))
        l=g[arm_left]; r=g[arm_right]
        ratios.append(r['render_wall_s']/l['render_wall_s'])
        lv=json.loads(l['stdout']); rv=json.loads(r['stdout'])
        setup_render.append((rv['setup_wall']+rv['render_wall'])/(lv['setup_wall']+lv['render_wall']))
        cpu.append(r['render_cpu_s']/l['render_cpu_s'])
        hashes.append(l['hash']==r['hash'])
        slow.append(max(r['render_wall_s']/l['render_wall_s'],l['render_wall_s']/r['render_wall_s']))
    if len(ratios)!=expected_n: raise ValueError(f'expected {expected_n} pairs, got {len(ratios)}')
    logs=[math.log(x) for x in ratios]
    m=statistics.mean(logs); sd=statistics.stdev(logs); half=tcrit*sd/math.sqrt(len(logs))
    setup_logs=[math.log(x) for x in setup_render]
    sm=statistics.mean(setup_logs); shalf=tcrit*statistics.stdev(setup_logs)/math.sqrt(len(setup_logs))
    gm=math.exp(m)
    return {
        'n_pairs':len(ratios),
        'ratio_median':statistics.median(ratios),
        'ratio_geomean':gm,
        'ratio_95ci':[math.exp(m-half),math.exp(m+half)],
        'setup_plus_render_ratio_median':statistics.median(setup_render),
        'setup_plus_render_ratio_geomean':math.exp(sm),
        'setup_plus_render_ratio_95ci':[math.exp(sm-shalf),math.exp(sm+shalf)],
        'ratio_range':[min(ratios),max(ratios)],
        'pairs_faster':sum(x<1 for x in ratios),
        'ratio_cpu_geomean':math.exp(statistics.mean(math.log(x) for x in cpu)),
        'ratio_cpu_95ci':[math.exp(statistics.mean(math.log(x) for x in cpu)-tcrit*statistics.stdev(math.log(x) for x in cpu)/math.sqrt(len(cpu))),math.exp(statistics.mean(math.log(x) for x in cpu)+tcrit*statistics.stdev(math.log(x) for x in cpu)/math.sqrt(len(cpu)))],
        'all_output_hashes_equal':all(hashes),
        'worst_pairwise_slowdown_ratio':max(slow),
        'ratios':ratios,
    }

b1=t_summary(load('candidate-batch1.jsonl'),'baseline','candidate',15,2.1447866879169273)
b2=t_summary(load('candidate-batch2.jsonl'),'baseline','candidate',15,2.1447866879169273)
allrows=load('candidate-batch1.jsonl')+load('candidate-batch2.jsonl')
combined=t_summary(allrows,'baseline','candidate',30,2.045229642132703)
aa=t_summary(load('aa.jsonl'),'baseline','baseline_repeat',15,2.1447866879169273)
preregister=json.loads((root/'preregister.json').read_text())
checks={'combined_ci_upper_below_0_98':combined['ratio_95ci'][1]<0.98,
        'combined_setup_plus_render_ci_upper_below_0_98':combined['setup_plus_render_ratio_95ci'][1]<0.98,
        'batch1_median_below_1':b1['ratio_median']<1,
        'batch2_median_below_1':b2['ratio_median']<1,
        'batch1_setup_plus_render_median_below_1':b1['setup_plus_render_ratio_median']<1,
        'batch2_setup_plus_render_median_below_1':b2['setup_plus_render_ratio_median']<1,
        'all_primary_hashes_equal':combined['all_output_hashes_equal'],
        'aa_ci_contains_1':aa['ratio_95ci'][0]<=1<=aa['ratio_95ci'][1]}
summary={'preregistered':preregister,'aa_control':aa,'batch1':b1,'batch2':b2,'combined_candidate_baseline':combined,
         'checks':checks,'primary_gate_pass':all(checks[k] for k in (
             'combined_ci_upper_below_0_98','combined_setup_plus_render_ci_upper_below_0_98',
             'batch1_median_below_1','batch2_median_below_1',
             'batch1_setup_plus_render_median_below_1','batch2_setup_plus_render_median_below_1',
             'all_primary_hashes_equal'))}
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for k,v in [('A/A',aa),('batch1',b1),('batch2',b2),('combined',combined)]:
    print(k, 'n',v['n_pairs'],'median',round(v['ratio_median'],5),'geo',round(v['ratio_geomean'],5),'95ci',tuple(round(x,5) for x in v['ratio_95ci']),'faster',v['pairs_faster'],'hash',v['all_output_hashes_equal'])
print('checks',json.dumps(checks,sort_keys=True))
print('primary_gate_pass',summary['primary_gate_pass'])
