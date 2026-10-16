#!/usr/bin/env python3
import array, hashlib, json, math, statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parent
DIR=ROOT/'expansion'
T15=2.1447866879169273

def load(path): return [json.loads(s) for s in path.read_text().splitlines() if s.strip()]

def paired_summary(rows):
    by={}
    for r in rows:
        if not r['warmup']: by.setdefault(r['pair'],{})[r['arm']]=r
    if len(by)!=15 or any(set(g)!= {'baseline','candidate'} for g in by.values()):
        raise ValueError(f'incomplete fixed sample: {len(by)} pairs')
    ratios=[by[i]['candidate']['render_wall_s']/by[i]['baseline']['render_wall_s'] for i in sorted(by)]
    setup_ratios=[(by[i]['candidate']['setup_wall_s']+by[i]['candidate']['render_wall_s'])/
      (by[i]['baseline']['setup_wall_s']+by[i]['baseline']['render_wall_s']) for i in sorted(by)]
    cpu=[by[i]['candidate']['render_cpu_s']/by[i]['baseline']['render_cpu_s'] for i in sorted(by)]
    logs=list(map(math.log,ratios)); m=statistics.mean(logs); half=T15*statistics.stdev(logs)/math.sqrt(len(logs))
    setup_logs=list(map(math.log,setup_ratios)); sm=statistics.mean(setup_logs); shalf=T15*statistics.stdev(setup_logs)/math.sqrt(len(setup_logs))
    clog=list(map(math.log,cpu)); cm=statistics.mean(clog); ch=T15*statistics.stdev(clog)/math.sqrt(len(clog))
    return {'n_pairs':15,'ratio_median':statistics.median(ratios),'ratio_geomean':math.exp(m),
      'ratio_95ci':[math.exp(m-half),math.exp(m+half)],'ratio_range':[min(ratios),max(ratios)],
      'setup_plus_render_ratio_median':statistics.median(setup_ratios),
      'setup_plus_render_ratio_geomean':math.exp(sm),
      'setup_plus_render_ratio_95ci':[math.exp(sm-shalf),math.exp(sm+shalf)],
      'pairs_candidate_faster':sum(x<1 for x in ratios),'ratio_cpu_geomean':math.exp(cm),
      'ratio_cpu_95ci':[math.exp(cm-ch),math.exp(cm+ch)],
      'pairs_candidate_hash_equal':sum(by[i]['candidate']['hash']==by[i]['baseline']['hash'] for i in by),
      'worst_pairwise_slowdown_ratio':max(max(r,1/r) for r in ratios),'ratios':ratios}

def pcm_quality(name, rate, seconds):
    a=DIR/f'pcm-{name}-baseline.f32'; b=DIR/f'pcm-{name}-candidate.f32'
    ba=a.read_bytes(); bb=b.read_bytes()
    if len(ba)!=len(bb) or len(ba)%4: raise ValueError('PCM size mismatch')
    x=array.array('f'); x.frombytes(ba); y=array.array('f'); y.frombytes(bb)
    if len(x)%2: raise ValueError('interleaved PCM size mismatch')
    seg_samples=rate*2; segments=[]
    for start in range(0,len(x),seg_samples):
        stop=min(start+seg_samples,len(x)); n=stop-start
        er2=ref2=0.0; maxerr=0.0
        for i in range(start,stop):
            e=float(y[i])-float(x[i]); v=float(x[i]); er2+=e*e; ref2+=v*v; maxerr=max(maxerr,abs(e))
        er=math.sqrt(er2/n); ref=math.sqrt(ref2/n)
        refdb=20*math.log10(max(ref,1e-300))
        errdb=20*math.log10(er) if er else None
        rel=20*math.log10(er/ref) if er and ref else None
        quality_pass=True if er==0 else (rel<=-60 if refdb>=-60 else errdb<=-80)
        segments.append({'segment_index':len(segments),'frames':n//2,'reference_rms_dbfs':refdb,
          'error_rms_dbfs':errdb,'relative_rms_error_db':rel,'max_abs_error':maxerr,
          'quality_pass':quality_pass})
    return {'baseline_pcm_sha256':hashlib.sha256(ba).hexdigest(),
      'candidate_pcm_sha256':hashlib.sha256(bb).hexdigest(),'bitwise_equal':ba==bb,
      'frames':len(x)//2,'segments':segments,'all_segments_pass':all(s['quality_pass'] for s in segments)}

def main():
    cases=[]
    manifest=json.loads((DIR/'manifest.json').read_text())
    for case_def in manifest['cases']:
        p=DIR/f"{case_def['name']}.jsonl"
        rows=load(p); s=paired_summary(rows)
        baseline=next(r for r in rows if r['pair']==0 and r['arm']=='baseline')
        quality=pcm_quality(baseline['case'],baseline['sample_rate'],baseline['duration_s'])
        s.update({'case':baseline['case'],'block':baseline['block'],'sample_rate':baseline['sample_rate'],
          'duration_s':baseline['duration_s'],'polyphony_limit':baseline['polyphony_limit'],
          'custom_filter_type':baseline['custom_filter_type'],'quality':quality,
          'repeatable_over_3pct_regression':(s['ratio_95ci'][0]>1.03 or s['setup_plus_render_ratio_95ci'][0]>1.03)})
        cases.append(s)
    result={'cases':cases,
      'all_quality_segments_pass':all(c['quality']['all_segments_pass'] for c in cases),
      'no_repeatable_over_3pct_total_slowdown':all(not c['repeatable_over_3pct_regression'] for c in cases)}
    result['expansion_gate_pass']=result['all_quality_segments_pass'] and result['no_repeatable_over_3pct_total_slowdown']
    (DIR/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    for c in cases:
        print(c['case'],'render',round(c['ratio_geomean'],5),tuple(round(x,5) for x in c['ratio_95ci']),
          'setup+render',round(c['setup_plus_render_ratio_geomean'],5),tuple(round(x,5) for x in c['setup_plus_render_ratio_95ci']),
          'faster',c['pairs_candidate_faster'],'hashpairs',c['pairs_candidate_hash_equal'],'pcm',c['quality']['bitwise_equal'],
          'quality',c['quality']['all_segments_pass'],'regress>3%',c['repeatable_over_3pct_regression'])
    print('gate',result['expansion_gate_pass'])

if __name__=='__main__': main()
