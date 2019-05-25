import numpy as np
import sys


print '%5s %5s %5s %5s %5s\n'%('thread', 'mtime', 'stime', 'mflop', 'sflop')
for i in [1, 2, 4, 8, 12, 16, 24, 32, 48]:
    times = []
    mflops = []
    filename = jobname = 'job_' + sys.argv[1] + '_' + str(i) + '.out'
    with open(filename, 'r') as f:
        for l in f:
            if 'megaflops' in l:
                mflops.append(float(l.split(':')[-1].strip()))
            elif 'Execution time' in l:
                times.append(float(l.split(':')[-1].strip()))
    #print times
    #print mflops
    print '%2d %5f %5f %5f %5f\n'%(i, np.mean(times), np.std(times), np.mean(mflops), np.std(mflops))

